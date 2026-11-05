/**
 * Copyright (C) 2021-2024 Saturneric <eric@bktus.com>
 *
 * This file is part of GpgFrontend.
 *
 * GpgFrontend is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GpgFrontend is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GpgFrontend. If not, see <https://www.gnu.org/licenses/>.
 *
 * The initial version of the source code is inherited from
 * the gpg4usb project, which is under GPL-3.0-or-later.
 *
 * All the source code of GpgFrontend was modified and released by
 * Saturneric <eric@bktus.com> starting on May 12, 2021.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#include "NotePageView.h"

#include <GFModule.h>
#include <GFModuleCommand.h>
#include <GFModuleTheme.h>

#include <GFSDK.hpp>
#include <GFSDKHostCommands.hpp>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLocale>
#include <QLockFile>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <array>

#include "GFModuleTr.h"
#include "NoteFiles.h"
#include "NoteImage.h"
#include "NoteStyle.h"

using namespace Qt::StringLiterals;

namespace gf::notes {

namespace {

/// Where Encrypt puts its snapshots, which Save copies to the note's file.
auto RevisionRoot() -> QString {
  return QStandardPaths::writableLocation(
             QStandardPaths::AppLocalDataLocation) +
         u"/notes/revisions"_s;
}

/// The pack's own budget, and room for the ZIP and OpenPGP framing around it.
constexpr qint64 kMaxFileBytes = Budgets{}.max_total_bytes + 44LL * 1024 * 1024;
/// As the page itself allows (editor.js): more pasted at once waits there.
constexpr int kMaxPendingImages = 4;
/// How long a snapshot waits for images still being stored.
constexpr int kSnapshotRetries = 20;
constexpr int kSnapshotRetryMs = 250;
/// How long closing waits for work still in flight.
constexpr qint64 kWaitLimitMs = 60 * 1000;
constexpr auto kSuffix = ".textpack.gpg";
constexpr auto kPlainSuffix = ".textpack";  ///< a note saved without encryption

/// A note file saved without encryption: "x.textpack", not "x.textpack.gpg".
auto IsPlainPath(const QString& path) -> bool {
  return path.endsWith(QString::fromLatin1(kPlainSuffix), Qt::CaseInsensitive);
}

auto Now() -> qint64 { return QDateTime::currentMSecsSinceEpoch(); }

/// What a job needs, copied at snapshot time and never shared with the UI.
struct Payload {
  Note note;
  QStringList drop;
  ProtectionConfig config;
  QString bundle;
  int channel = 0;
};

// Overwrites the bytes only when this is their last reference: on a shared
// buffer fill() would zero a fresh copy and leave the original to be freed
// with the plaintext in it. A shared buffer is just released; whoever still
// holds it is the one to wipe it.
template <typename Bytes>
void Wipe(Bytes& bytes) {
  if (bytes.isDetached()) bytes.fill({});
  bytes.clear();
}

auto DecryptFailure(uint32_t error) -> QString {
  switch (static_cast<CryptoError>(error)) {
    case CryptoError::kBAD_PASSPHRASE:
      return GTrC::tr("That passphrase does not open this note.");
    case CryptoError::kNO_SECKEY:
      return GTrC::tr("None of your keys can open this note.");
    case CryptoError::kNO_PASSPHRASE:
      return GTrC::tr("This note has no passphrase to open it with.");
    case CryptoError::kCANCELED:
      return GTrC::tr("Cancelled.");
    case CryptoError::kINV_DATA:
      return GTrC::tr("This file is not an encrypted note.");
    default:
      return GTrC::tr(
          "The note could not be opened. If its key is on a card, make "
          "sure the card is present.");
  }
}

}  // namespace

NotePageView::NotePageView(QWidget* parent)
    : QWidget(parent), persistence_(make_persistence()) {
  channel_ = CurrentChannel();
  build_ui();
  tick_ = new QTimer(this);
  tick_->setInterval(1000);
  connect(tick_, &QTimer::timeout, this, &NotePageView::autosave_tick);
  tick_->start();

  {
    // A new note: open straight away, empty and unencrypted -- unless the
    // Host opened a file into this tab meanwhile (OpenPath runs right after
    // the tab is made, before this does).
    QTimer::singleShot(0, this, [this] {
      if (state_ != State::kNEW) return;
      note_ = {};
      note_.info = QJsonObject{{u"version"_s, 2}};
      note_id_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
      persistence_ = make_persistence();
      start_editor();
    });
  }
}

NotePageView::~NotePageView() {
  // Nothing in flight may finish into a page that is gone.
  wait_until([this] { return !persistence_.Busy() && jobs_in_flight_ == 0; });
  forget();
  // Revisions belong to this tab; what was saved is a copy.
  if (!revision_dir_.isEmpty()) QDir(revision_dir_).removeRecursively();
}

// --- the widgets -------------------------------------------------------------

namespace {

/// The note's name, as a person would say it: no ".textpack.gpg".
auto DisplayName(const QString& path) -> QString {
  auto name = QFileInfo(path).fileName();
  for (const auto* suffix : {kSuffix, kPlainSuffix}) {
    if (name.endsWith(QString::fromLatin1(suffix), Qt::CaseInsensitive)) {
      name.chop(static_cast<int>(qstrlen(suffix)));
    }
  }
  return name;
}

/// "0x1D70 14AD 87F5 A975", easier to compare by eye than one run.
auto GroupedId(const QString& id) -> QString {
  const auto tail = id.right(16).toUpper();
  QStringList parts;
  for (int i = 0; i < tail.size(); i += 4) parts << tail.mid(i, 4);
  return parts.join(u' ');
}

}  // namespace

void NotePageView::build_ui() {
  stack_ = new QStackedWidget(this);
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->addWidget(stack_);

  // Encrypted: what the note is, and that Decrypt in the toolbar opens it.
  auto* encrypted = new QWidget();
  auto* page = new QVBoxLayout(encrypted);
  page->setContentsMargins(48, 48, 48, 48);
  page->addStretch(2);
  page->addWidget(new NoteGlyph(NoteGlyph::Kind::kLOCK, 72, true), 0,
                  Qt::AlignHCenter);
  page->addSpacing(18);
  encrypted_title_ = new QLabel();
  encrypted_title_->setTextFormat(Qt::PlainText);
  encrypted_title_->setAlignment(Qt::AlignHCenter);
  encrypted_title_->setWordWrap(true);
  NoteScaleFont(encrypted_title_, 1.6, QFont::DemiBold);
  page->addWidget(encrypted_title_);
  page->addSpacing(4);
  encrypted_facts_ = new QLabel();
  encrypted_facts_->setTextFormat(Qt::PlainText);
  encrypted_facts_->setAlignment(Qt::AlignHCenter);
  encrypted_facts_->setWordWrap(true);
  encrypted_facts_->setObjectName(u"muted"_s);
  page->addWidget(encrypted_facts_);
  page->addSpacing(22);
  encrypted_hint_ = new QLabel();
  encrypted_hint_->setTextFormat(Qt::PlainText);
  encrypted_hint_->setWordWrap(true);
  encrypted_hint_->setAlignment(Qt::AlignHCenter);
  page->addWidget(encrypted_hint_);
  page->addSpacing(12);
  // Who the file is encrypted for, as the file itself says: a plain list,
  // a few rows tall, that scrolls when there are more.
  readers_table_ = new QTreeWidget();
  readers_table_->setColumnCount(2);
  readers_table_->setHeaderLabels({GTrC::tr("Key ID"), GTrC::tr("User ID")});
  readers_table_->setRootIsDecorated(false);
  readers_table_->setSelectionMode(QAbstractItemView::NoSelection);
  readers_table_->setFocusPolicy(Qt::NoFocus);
  readers_table_->header()->setStretchLastSection(true);
  readers_table_->header()->setSectionsMovable(false);
  auto* readers_row = new QHBoxLayout();
  readers_row->addStretch(1);
  readers_row->addWidget(readers_table_, 6);
  readers_row->addStretch(1);
  readers_table_->setMaximumWidth(620);
  page->addLayout(readers_row);
  page->addSpacing(10);
  encrypted_status_ = new QLabel();
  encrypted_status_->setWordWrap(true);
  encrypted_status_->setAlignment(Qt::AlignHCenter);
  encrypted_status_->setTextFormat(Qt::PlainText);
  page->addWidget(encrypted_status_);
  page->addStretch(3);
  stack_->addWidget(encrypted);

  // Open: a slim header and the editor page. On the left, whether the note
  // is saved; on the right, the status button: whether the note is
  // encrypted, and for whom, with a menu that says what that means and what
  // to do next. Anything else worth saying goes there too.
  auto* open = new QWidget();
  auto* o = new QVBoxLayout(open);
  o->setContentsMargins(0, 0, 0, 0);
  o->setSpacing(0);
  header_ = new QWidget();
  auto* hl = new QHBoxLayout(header_);
  hl->setContentsMargins(8, 4, 8, 2);
  hl->setSpacing(6);
  save_state_ = new QLabel();
  save_state_->setObjectName(u"muted"_s);
  save_state_->setTextFormat(Qt::PlainText);
  NoteScaleFont(save_state_, 0.92);
  hl->addWidget(save_state_);
  hl->addStretch(1);
  status_button_ = new NoteStatusButton();
  status_menu_ = new QMenu(status_button_);
  status_button_->setMenu(status_menu_);
  connect(status_menu_, &QMenu::aboutToShow, this,
          [this] { rebuild_status_menu(); });
  hl->addWidget(status_button_);
  o->addWidget(header_);
  notice_timer_ = new QTimer(this);
  notice_timer_->setSingleShot(true);
  notice_timer_->setInterval(8000);
  connect(notice_timer_, &QTimer::timeout, this, [this] { clear_notice(); });

  slot_ = new QWidget();
  slot_->setObjectName(u"note_editor_slot"_s);
  o->addWidget(slot_, 1);
  stack_->addWidget(open);

  // Unavailable
  auto* gone = new QWidget();
  auto* gl = new QVBoxLayout(gone);
  gl->setContentsMargins(48, 48, 48, 48);
  gl->addStretch(2);
  gl->addWidget(new NoteGlyph(NoteGlyph::Kind::kWARNING, 56, true), 0,
                Qt::AlignHCenter);
  gl->addSpacing(16);
  unavailable_ = new QLabel();
  unavailable_->setWordWrap(true);
  unavailable_->setAlignment(Qt::AlignCenter);
  unavailable_->setMaximumWidth(480);
  gl->addWidget(unavailable_, 0, Qt::AlignHCenter);
  gl->addStretch(3);
  stack_->addWidget(gone);

  restyle();
}

void NotePageView::restyle() {
  // Setting a palette sends PaletteChange again: once is enough.
  if (restyling_) return;
  restyling_ = true;
  const auto muted = gf::ui::MutedColor(this);
  for (auto* l : findChildren<QLabel*>(u"muted"_s))
    gf::ui::SetLabelColor(l, muted);
  gf::ui::SetLabelColor(encrypted_status_, gf::ui::WarningColor(this));

  restyling_ = false;
  if (state_ == State::kOPEN) {
    refresh_strip();
    post_ui();
  }
}

void NotePageView::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);
  if (gf::ui::IsRestyle(event) && stack_ != nullptr) restyle();
}

void NotePageView::show_encrypted(const QString& status) {
  state_ = State::kENCRYPTED;
  const QFileInfo info(path_);
  encrypted_title_->setText(DisplayName(path_));
  encrypted_facts_->setText(QStringList{
      GTrC::tr("Encrypted note"), QLocale().formattedDataSize(info.size()),
      GTrC::tr("modified %1")
          .arg(QLocale().toString(info.lastModified(), QLocale::ShortFormat))}
                                .join(u"  \u00B7  "_s));
  fill_readers();
  encrypted_status_->setText(status);
  encrypted_status_->setVisible(!status.isEmpty());
  stack_->setCurrentIndex(0);
  // Decrypt, and nothing else: the tab was made before it knew what it holds.
  NotifyOpsChanged();
}

void NotePageView::fill_readers() {
  readers_table_->clear();
  // The keys that can open it here first, then the others it names.
  auto keys = file_readers_.keys;
  std::stable_sort(keys.begin(), keys.end(),
                   [](const FileRecipient& a, const FileRecipient& b) {
                     return a.has_secret > b.has_secret;
                   });
  const auto muted = gf::ui::MutedColor(this);
  int mine = 0;
  for (const auto& k : keys) {
    auto* item = new QTreeWidgetItem(readers_table_);
    if (k.hidden) {
      item->setText(0, GTrC::tr("Hidden"));
      item->setText(1, GTrC::tr("The file does not say whose key this is"));
    } else {
      item->setText(0, GroupedId(k.key_id));
      item->setText(1, !k.found       ? GTrC::tr("Not in your key database")
                       : k.has_secret ? GTrC::tr("%1 (your key)").arg(k.uid)
                                      : k.uid);
    }
    if (k.has_secret) {
      ++mine;
      auto font = item->font(1);
      font.setBold(true);
      item->setFont(1, font);
    } else if (k.hidden || !k.found) {
      item->setForeground(0, muted);
      item->setForeground(1, muted);
    }
  }
  if (file_readers_.password) {
    auto* item = new QTreeWidgetItem(readers_table_);
    item->setText(0, GTrC::tr("Password"));
    item->setText(1, GTrC::tr("Anyone who knows the note's password"));
  }
  // Rows with room to breathe, and a gap after the key IDs; set as sizes,
  // since a style sheet would take over the table's palette.
  const int row_height = fontMetrics().height() + 10;
  for (int i = 0; i < readers_table_->topLevelItemCount(); ++i) {
    readers_table_->topLevelItem(i)->setSizeHint(1, QSize(0, row_height));
  }
  readers_table_->resizeColumnToContents(0);
  readers_table_->setColumnWidth(0, readers_table_->columnWidth(0) + 24);
  // One height for two readers or a thousand: six rows, then it scrolls.
  constexpr int kVisibleRows = 6;
  readers_table_->setVisible(readers_table_->topLevelItemCount() > 0);
  readers_table_->setFixedHeight(readers_table_->header()->sizeHint().height() +
                                 kVisibleRows * row_height +
                                 2 * readers_table_->frameWidth());

  // What the list means for the person looking at it.
  QString hint;
  if (mine > 0) {
    hint = GTrC::tr(
        "This note is encrypted for the keys below, and one of "
        "them is yours. Click Decrypt in the toolbar to open it.");
  } else if (file_readers_.password && keys.isEmpty()) {
    hint = GTrC::tr(
        "This note is encrypted with a password. Click Decrypt in "
        "the toolbar and enter it to open the note.");
  } else if (file_readers_.password) {
    hint = GTrC::tr(
        "None of these keys is yours, but the note also opens "
        "with its password. Click Decrypt in the toolbar to "
        "enter it.");
  } else if (!keys.isEmpty()) {
    hint = GTrC::tr(
        "This note is encrypted for the keys below, and none of "
        "them is in your key database with its secret key. Only "
        "their holders can open it.");
  } else {
    hint = GTrC::tr(
        "This note is encrypted. Click Decrypt in the toolbar to "
        "open it.");
  }
  encrypted_hint_->setText(hint);
}

void NotePageView::show_open() {
  state_ = State::kOPEN;
  set_status(persistence_.IsClean() ? QString() : GTrC::tr("Unsaved changes"));
  refresh_strip();
  stack_->setCurrentIndex(1);
  NotifyOpsChanged();
  // Once more when the event loop turns: a tab the Host is still putting
  // together (restored from its recovery cache, say) is not listening yet.
  QTimer::singleShot(0, this, [this] {
    if (state_ == State::kOPEN) NotifyOpsChanged();
  });
}

void NotePageView::show_unavailable(const QString& why) {
  state_ = State::kUNAVAILABLE;
  unavailable_->setText(why);
  stack_->setCurrentIndex(2);
  NotifyOpsChanged();
}

void NotePageView::describe_readers() {
  const auto revision = persistence_.Revision();
  if (described_revision_ == revision.path) return;
  described_revision_ = revision.path;
  readers_text_.clear();
  readers_lines_.clear();
  if (!revision.exists) return;
  const auto& readers = revision.readers;
  if (readers.passphrase) {
    readers_text_ = GTrC::tr("Encrypted");
    readers_lines_ << GTrC::tr("Anyone who knows the password");
    return;
  }
  // Just "Encrypted": a note can be for a hundred keys, which no button can
  // name. Who they are is in the status menu.
  readers_text_ = GTrC::tr("Encrypted");
  const auto facts = DescribeKeys(channel_, readers.recipients);
  for (const auto& f : readers.recipients) {
    readers_lines_ << u"%1  (%2)"_s.arg(facts.value(f).DisplayName(),
                                        GroupedId(f));
  }
  // A menu a thousand rows long would not fit on any screen.
  constexpr int kMaxLines = 10;
  if (const int left_out = static_cast<int>(readers_lines_.size()) - kMaxLines;
      left_out > 0) {
    readers_lines_ = readers_lines_.first(kMaxLines);
    readers_lines_ << GTrC::tr("and %n more", "", left_out);
  }
}

void NotePageView::refresh_strip() {
  const auto revision = persistence_.Revision();
  describe_readers();
  const bool selection_changed = revision.exists && checked_.has_value() &&
                                 !(*checked_ == revision.readers);

  // As the e-mail view states a message: quiet when there is nothing to
  // protect, the accent when it is protected, the warning colour when it
  // needs the user. What it means for Save is in the tooltip.
  QString text;
  QString hint;
  auto icon = u":/icons/unlock.png"_s;
  auto colour = gf::ui::MutedColor(this);
  if (!revision.exists) {
    text = GTrC::tr("Not encrypted");
    hint = GTrC::tr(
        "Save writes the note as a plain TextPack, which anyone with the file "
        "can read. To encrypt it, check who may read it in the Key ToolBox "
        "(or no one, for a password), then click Encrypt.");
  } else if (!persistence_.RevisionCurrent()) {
    // The text moved on; what Save writes is the older, encrypted version.
    text = GTrC::tr("Encryption out of date");
    hint = GTrC::tr(
        "Save writes the encrypted version, which does not have your latest "
        "changes. Encrypt again to include them.");
    icon = u":/icons/warning.png"_s;
    colour = gf::ui::WarningColor(this);
  } else {
    text = readers_text_;
    hint = revision.readers.passphrase
               ? GTrC::tr(
                     "Encrypted with a password. Save writes this "
                     "encrypted version.")
               : GTrC::tr("Save writes this encrypted version.");
    if (selection_changed) {
      hint += u" "_s + GTrC::tr(
                           "The Key ToolBox now has other keys checked; "
                           "encrypt again to use them.");
    }
    icon = u":/icons/lock.png"_s;
    colour = gf::ui::ThemeColor(this, GF_UI_COLOR_ACCENT_POSITIVE);
  }
  // Something just happened that the user should hear about: said here, in
  // place of the state, until it is dealt with or goes by itself.
  if (!notice_.isEmpty()) {
    hint = (notice_detail_.isEmpty() ? QString() : notice_detail_ + u"\n\n"_s) +
           text + u": "_s + hint;
    text = notice_;
    if (notice_warning_) {
      icon = u":/icons/warning.png"_s;
      colour = gf::ui::WarningColor(this);
    }
  }
  if (!content_note_.isEmpty()) hint += u"\n\n"_s + content_note_;

  status_button_->SetFullText(text);
  // As rich text, which Qt wraps; a plain one runs off the screen.
  status_button_->setToolTip(u"<qt>%1<br><br>%2</qt>"_s.arg(
      text.toHtmlEscaped(), hint.toHtmlEscaped().replace(u"\n"_s, u"<br>"_s)));
  status_button_->setIcon(QIcon(icon));
  auto pal = status_button_->palette();
  pal.setColor(QPalette::ButtonText, colour);
  status_button_->setPalette(pal);
}

void NotePageView::rebuild_status_menu() {
  // Actions only, as the e-mail view's menu: what the state means is the
  // button's tooltip.
  status_menu_->clear();
  const auto revision = persistence_.Revision();
  const bool outdated = revision.exists && !persistence_.RevisionCurrent();
  const bool selection_changed = revision.exists && checked_.has_value() &&
                                 !(*checked_ == revision.readers);

  QString encrypt_text = GTrC::tr("Encrypt...");
  if (outdated) {
    encrypt_text = GTrC::tr("Update Encryption");
  } else if (selection_changed) {
    encrypt_text = GTrC::tr("Encrypt for the Checked Keys");
  } else if (revision.exists) {
    encrypt_text = GTrC::tr("Encrypt Again");
  }
  auto* encrypt_action = status_menu_->addAction(encrypt_text);
  encrypt_action->setEnabled(!encrypting_);
  connect(encrypt_action, &QAction::triggered, this, [this] { encrypt(); });
  if (!revision.exists) return;

  // Who can read the encrypted version: the snapshot's readers, not what
  // the Key ToolBox has checked now.
  auto* readers = status_menu_->addMenu(GTrC::tr("Can Be Read By"));
  for (const auto& line : readers_lines_)
    readers->addAction(line)->setEnabled(false);
  if (!revision.readers.passphrase) {
    readers->addSeparator();
    connect(readers->addAction(GTrC::tr("Check These Keys in the Key ToolBox")),
            &QAction::triggered, this, [this] { restore_checked_keys(); });
  }
  status_menu_->addSeparator();
  auto* remove = status_menu_->addAction(GTrC::tr("Remove Encryption"));
  remove->setToolTip(GTrC::tr(
      "Remove encryption from this note. Keep the current text and save as "
      ".textpack."));
  connect(remove, &QAction::triggered, this, [this] { remove_encryption(); });
}

void NotePageView::notice(const QString& text, bool warning, bool sticky,
                          const QString& detail) {
  notice_ = text;
  notice_detail_ = detail;
  notice_warning_ = warning;
  if (sticky) {
    notice_timer_->stop();
  } else {
    notice_timer_->start();
  }
  refresh_strip();
}

void NotePageView::clear_notice() {
  notice_timer_->stop();
  if (notice_.isEmpty()) return;
  notice_.clear();
  refresh_strip();
}

void NotePageView::set_status(const QString& text, const QString& detail) {
  save_state_->setText(text);
  save_state_->setToolTip(detail);
}

void NotePageView::post_ui() {
  if (!editor_.IsLive()) return;
  const auto pal = palette();
  const auto window = pal.color(QPalette::Window);
  const QJsonObject theme{
      {u"dark"_s, window.lightnessF() < 0.5},
      {u"base"_s, NoteHex(pal.color(QPalette::Base))},
      {u"text"_s, NoteHex(pal.color(QPalette::Text))},
      {u"window"_s, NoteHex(window)},
      {u"muted"_s, NoteHex(gf::ui::MutedColor(this))},
      {u"border"_s, NoteHex(gf::ui::BorderColor(this))},
      {u"accent"_s, NoteHex(pal.color(QPalette::Highlight))},
      {u"accentText"_s, NoteHex(pal.color(QPalette::HighlightedText))},
      {u"warning"_s, NoteHex(gf::ui::WarningColor(this))},
  };
  // The page's words, in the application's language.
  static const std::array<std::pair<const char*, const char*>, 39> kWords = {{
      {"formatting", QT_TRANSLATE_NOOP("ModuleNotes", "Formatting")},
      {"paragraph", QT_TRANSLATE_NOOP("ModuleNotes", "Normal text")},
      {"heading1", QT_TRANSLATE_NOOP("ModuleNotes", "Heading 1")},
      {"heading2", QT_TRANSLATE_NOOP("ModuleNotes", "Heading 2")},
      {"heading3", QT_TRANSLATE_NOOP("ModuleNotes", "Heading 3")},
      {"heading4", QT_TRANSLATE_NOOP("ModuleNotes", "Heading 4")},
      {"heading5", QT_TRANSLATE_NOOP("ModuleNotes", "Heading 5")},
      {"heading6", QT_TRANSLATE_NOOP("ModuleNotes", "Heading 6")},
      {"textStyle", QT_TRANSLATE_NOOP("ModuleNotes", "Text style")},
      {"bold", QT_TRANSLATE_NOOP("ModuleNotes", "Bold")},
      {"italic", QT_TRANSLATE_NOOP("ModuleNotes", "Italic")},
      {"strike", QT_TRANSLATE_NOOP("ModuleNotes", "Strikethrough")},
      {"code", QT_TRANSLATE_NOOP("ModuleNotes", "Inline code")},
      {"link", QT_TRANSLATE_NOOP("ModuleNotes", "Link")},
      {"bulletList", QT_TRANSLATE_NOOP("ModuleNotes", "Bulleted list")},
      {"orderedList", QT_TRANSLATE_NOOP("ModuleNotes", "Numbered list")},
      {"taskList", QT_TRANSLATE_NOOP("ModuleNotes", "Checklist")},
      {"quote", QT_TRANSLATE_NOOP("ModuleNotes", "Quote")},
      {"codeBlock", QT_TRANSLATE_NOOP("ModuleNotes", "Code block")},
      {"rule", QT_TRANSLATE_NOOP("ModuleNotes", "Divider")},
      {"table", QT_TRANSLATE_NOOP("ModuleNotes", "Table")},
      {"rowAdd", QT_TRANSLATE_NOOP("ModuleNotes", "Add row below")},
      {"colAdd", QT_TRANSLATE_NOOP("ModuleNotes", "Add column to the right")},
      {"rowDelete", QT_TRANSLATE_NOOP("ModuleNotes", "Delete row")},
      {"colDelete", QT_TRANSLATE_NOOP("ModuleNotes", "Delete column")},
      {"tableDelete", QT_TRANSLATE_NOOP("ModuleNotes", "Delete table")},
      {"undo", QT_TRANSLATE_NOOP("ModuleNotes", "Undo")},
      {"redo", QT_TRANSLATE_NOOP("ModuleNotes", "Redo")},
      {"find", QT_TRANSLATE_NOOP("ModuleNotes", "Find")},
      {"more", QT_TRANSLATE_NOOP("ModuleNotes", "More")},
      {"findPlaceholder", QT_TRANSLATE_NOOP("ModuleNotes", "Find in note")},
      {"noResults", QT_TRANSLATE_NOOP("ModuleNotes", "No results")},
      {"previous", QT_TRANSLATE_NOOP("ModuleNotes", "Previous")},
      {"next", QT_TRANSLATE_NOOP("ModuleNotes", "Next")},
      {"close", QT_TRANSLATE_NOOP("ModuleNotes", "Close")},
      {"linkPlaceholder",
       QT_TRANSLATE_NOOP("ModuleNotes", "Paste or type a web address")},
      {"apply", QT_TRANSLATE_NOOP("ModuleNotes", "Apply")},
      {"removeLink", QT_TRANSLATE_NOOP("ModuleNotes", "Remove link")},
      {"placeholder", QT_TRANSLATE_NOOP("ModuleNotes", "Start writing...")},
  }};
  QJsonObject strings;
  for (const auto& [key, text] : kWords) {
    strings.insert(QString::fromLatin1(key), GTrC::tr(text));
  }
  post_json(
      {{u"type"_s, u"ui"_s}, {u"theme"_s, theme}, {u"strings"_s, strings}});
}

// --- opening -----------------------------------------------------------------

auto NotePageView::OpenPath(const QString& path) -> bool {
  QByteArray bytes;
  if (!read_file(path, &bytes)) return false;
  if (IsPlainPath(path)) {
    // Saved without encryption: nothing to unlock, it opens as it is.
    path_ = path;
    plain_ = true;
    opened(bytes);
    return true;
  }
  if (!take_encrypted(bytes)) return false;
  path_ = path;
  show_encrypted();
  return true;
}

auto NotePageView::read_file(const QString& path, QByteArray* bytes) -> bool {
  QString why;
  if (!ReadNoteFile(path, kMaxFileBytes, bytes, &why)) {
    QMessageBox::warning(this, GTrC::tr("Note"),
                         GTrC::tr("The note could not be read: %1").arg(why));
    return false;
  }
  return true;
}

auto NotePageView::take_encrypted(const QByteArray& bytes) -> bool {
  if (!IsEncryptedMessage(bytes)) {
    QMessageBox::warning(
        this, GTrC::tr("Encrypted Note"),
        GTrC::tr("This file is not an OpenPGP-encrypted note."));
    return false;
  }
  file_ = bytes;
  file_readers_ = ReadersOf(channel_, bytes);
  return true;
}

void NotePageView::decrypt_file() {
  if (state_ != State::kENCRYPTED) return;
  state_ = State::kDECRYPTING;
  encrypted_status_->setText(GTrC::tr("Decrypting..."));
  encrypted_status_->show();
  NotifyOpsChanged();

  QPointer<NotePageView> self(this);
  const auto cipher = file_;
  const auto channel = channel_;
  ++jobs_in_flight_;  // keeps the destructor waiting for this too
  // As the Host's Decrypt: a key here, or the passphrase, which it asks for.
  (void)QtConcurrent::run([self, cipher, channel] {
    auto result = Decrypt(channel, cipher);
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [self, result = std::move(result)]() mutable {
          // On every way out: a tab closed before the worker returned left
          // the decrypted note to be freed as it was.
          const auto wipe = qScopeGuard([&result]() { Wipe(result.data); });
          if (self.isNull()) return;
          --self->jobs_in_flight_;
          if (!result.ok) {
            self->report_crypto(GF_GPG_ANALYSE_DECRYPT, result);
            // Still encrypted, and Decrypt can be tried again.
            self->show_encrypted(DecryptFailure(result.error));
            return;
          }
          const auto why = self->opened(result.data);
          Wipe(result.data);
          // Decrypted, and opened in this tab -- or not a note after all.
          self->report_crypto(GF_GPG_ANALYSE_DECRYPT, result,
                              why.isEmpty() ? GF_DOCUMENT_OUTPUT_APPLIED
                                            : GF_DOCUMENT_OUTPUT_APPLY_FAILED,
                              why);
        },
        Qt::QueuedConnection);
  });
}

void NotePageView::report_crypto(int operation, const CryptoResult& result,
                                 int output, const QString& reason) {
  CryptoReport report;
  report.channel = channel_;
  report.operation = operation;
  report.err = result.error;
  report.capsule_id = result.capsule_id;
  report.input_hash = result.input_hash;
  report.output = output;
  report.reason = reason;
  if (!result.ok) {
    report.output = GF_DOCUMENT_OUTPUT_NOT_PRODUCED;
    report.reason.clear();
    if (result.capsule_id.isEmpty()) {
      // Refused before OpenPGP ran: the note's own reason is all there is.
      report.reason = result.why;
    } else if (result.error == 0) {
      // OpenPGP succeeded, but the note would not keep what it made (a
      // reader left out, say): the output was not used.
      report.output = GF_DOCUMENT_OUTPUT_APPLY_FAILED;
      report.reason = result.why;
    }
    // Otherwise OpenPGP failed, and the Host's analysis says why.
  }
  NotifyResult(report);
}

auto NotePageView::opened(const QByteArray& zip) -> QString {
  QString why;
  const auto entries = ZipRead(zip, Budgets{}, &why);
  const auto note = entries.has_value() ? ReadPack(*entries, Budgets{}, &why)
                                        : std::optional<Note>{};
  if (!note.has_value()) {
    if (plain_) {
      const auto text = GTrC::tr("This file is not a note: %1").arg(why);
      show_unavailable(text);
      return text;
    }
    const auto text = GTrC::tr(
                          "The note decrypted, but its contents are not "
                          "a note: %1")
                          .arg(why);
    show_encrypted(text);
    return text;
  }
  open_note(*note, false);
  return {};
}

void NotePageView::open_note(const Note& note, bool unsaved) {
  note_ = note;
  const auto app = note_.App();
  note_id_ = app.value(u"note_id"_s).toString();
  if (note_id_.isEmpty()) {
    note_id_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
  }
  managed_.clear();
  for (const auto& v : app.value(u"managed_assets"_s).toArray()) {
    managed_ << v.toString();
  }

  const auto front = FrontMatterLength(note_.markdown);
  front_matter_ = QString::fromUtf8(note_.markdown.left(front));

  // Decrypted, it is ordinary text: the file's ciphertext is not kept as
  // the note's encrypted snapshot, and the next save is a plain one.
  persistence_ = make_persistence();
  if (unsaved) {
    // Restored from the Host's recovery cache: nothing of it is saved yet.
    persistence_.Edited(1, Now());
    persistence_.Captured(1);
  } else {
    persistence_.Opened(0, plain_);
  }
  Wipe(file_);
  start_editor();
}

void NotePageView::start_editor() {
  if (gf::ui::WebEditorAvailability() != gf::ui::WebEditorStatus::kOK) {
    show_unavailable(
        GTrC::tr("The note editor needs the web engine component, which is "
                 "not available. The note itself is standard OpenPGP and can "
                 "still be decrypted with any OpenPGP tool."));
    return;
  }
  gf::ui::WebEditor::Handlers h;
  h.message = [this](const QByteArray& json) { on_message(json); };
  h.lost = [this] {
    show_unavailable(
        GTrC::tr("The note editor stopped. Your last changes "
                 "are kept in memory; close the tab to save "
                 "or discard them."));
  };
  show_open();
  if (editor_.Create(*this, slot_, std::move(h)) !=
      gf::ui::WebEditorStatus::kOK) {
    show_unavailable(GTrC::tr("The note editor could not be started."));
    return;
  }
  // Images the note already carries, served from memory, checked first.
  for (auto it = note_.assets.cbegin(); it != note_.assets.cend(); ++it) {
    QString mime;
    if (IsDisplayableImage(it.value(), &mime)) {
      editor_.PutResource(it.key(), mime, it.value());
    }
  }
}

// --- the page ----------------------------------------------------------------

void NotePageView::on_message(const QByteArray& json) {
  const auto msg = ParseMessage(json);
  const auto type = msg.value(u"type"_s).toString();
  if (type == u"ready"_s) {
    on_ready();
  } else if (type == u"loaded"_s) {
    if (!bridge_.Accept(msg).isEmpty()) on_loaded(msg);
  } else if (type == u"dirty"_s) {
    const bool was_current = persistence_.RevisionCurrent();
    persistence_.Edited(
        page_gen_base_ + static_cast<quint64>(msg.value(u"gen"_s).toInteger()),
        Now());
    // The snapshot just fell behind the text.
    if (was_current && !persistence_.RevisionCurrent()) refresh_strip();
    if (!notified_modified_) {
      notified_modified_ = true;
      NotifyModified();
      set_status(GTrC::tr("Unsaved changes"));
    }
  } else if (type == u"markdown"_s) {
    if (!bridge_.Accept(msg).isEmpty()) finish_snapshot(msg);
  } else if (type == u"image.add"_s) {
    on_image(msg);
  } else if (type == u"link.open"_s) {
    on_link(QUrl(msg.value(u"url"_s).toString(), QUrl::StrictMode));
  } else if (type == u"notice"_s) {
    on_notice(msg.value(u"text"_s).toString());
  } else if (type == u"engine.crashed"_s) {
    // The page lost the document; on_ready() gives back its last snapshot.
    notice(GTrC::tr("Editor restarted"), true, false,
           GTrC::tr("The editor restarted; your last snapshot was restored."));
    NotifyModified();
  }
}

void NotePageView::on_ready() {
  bridge_.NewEpoch();
  // The page counts its edits from 0 on every load; ours go on from where
  // the note is (a recovered note starts edited, a restarted page mid-way).
  page_gen_base_ = persistence_.EditGen();
  post_ui();
  // A restarted page gets back what it last handed over, not the file.
  const auto body =
      last_markdown_.isEmpty()
          ? QString::fromUtf8(note_.markdown.mid(front_matter_.toUtf8().size()))
          : last_markdown_;
  editor_.Post(bridge_.Request(u"load"_s, {{u"markdown"_s, body}}, Now()));
  last_markdown_ = body;
}

void NotePageView::on_loaded(const QJsonObject& msg) {
  static const QHash<QString, const char*> kNames = {
      {u"footnotes"_s, QT_TRANSLATE_NOOP("ModuleNotes", "footnotes")},
      {u"html"_s, QT_TRANSLATE_NOOP("ModuleNotes", "HTML")},
      {u"math"_s, QT_TRANSLATE_NOOP("ModuleNotes", "math")},
  };
  QStringList what;
  for (const auto& v : msg.value(u"unsupported"_s).toArray()) {
    if (const auto* name = kNames.value(v.toString())) what << GTrC::tr(name);
  }
  if (what.isEmpty()) return;
  // Kept in the status menu for as long as the note is open.
  content_note_ = GTrC::tr(
                      "This note uses %1, which the editor shows as plain "
                      "text. Editing and saving may change how it looks "
                      "in other apps.")
                      .arg(what.join(u", "_s));
  notice(GTrC::tr("Some content shown as text"), false, false, content_note_);
}

void NotePageView::on_link(const QUrl& url) {
  const auto scheme = url.scheme().toLower();
  if (scheme != u"http"_s && scheme != u"https"_s && scheme != u"mailto"_s) {
    return;
  }
  const auto answer = QMessageBox::question(
      this, GTrC::tr("Open Link?"),
      GTrC::tr("Open this address in your browser?\n\n%1")
          .arg(url.toString(QUrl::FullyEncoded)),
      QMessageBox::Open | QMessageBox::Cancel, QMessageBox::Cancel);
  if (answer == QMessageBox::Open) QDesktopServices::openUrl(url);
}

void NotePageView::on_notice(const QString& text) {
  static const QHash<QString, const char*> kNotices = {
      {u"image-too-large"_s,
       QT_TRANSLATE_NOOP("ModuleNotes", "That image is larger than 20 MiB.")},
      {u"unsupported-image"_s,
       QT_TRANSLATE_NOOP("ModuleNotes",
                         "Only PNG, JPEG, GIF and WebP images can be pasted.")},
      {u"images-busy"_s,
       QT_TRANSLATE_NOOP("ModuleNotes", "Still storing the previous images.")},
      {u"link-scheme"_s,
       QT_TRANSLATE_NOOP("ModuleNotes",
                         "Links may only be web or e-mail addresses.")},
  };
  if (const auto* t = kNotices.value(text)) notice(GTrC::tr(t), true);
}

void NotePageView::on_image(const QJsonObject& msg) {
  const auto token = msg.value(u"token"_s).toString();
  const auto data =
      QByteArray::fromBase64(msg.value(u"data"_s).toString().toLatin1());
  if (token.isEmpty() || jobs_in_flight_ >= kMaxPendingImages) {
    reject_image(token, GTrC::tr("Still storing the previous images."));
    return;
  }
  ++jobs_in_flight_;
  QPointer<NotePageView> self(this);
  const auto epoch = bridge_.Epoch();
  (void)QtConcurrent::run([self, data, token, epoch] {
    QString why;
    auto stored = PrepareImage(data, &why);
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [self, stored = std::move(stored), why, token, epoch]() mutable {
          // The image left unused when the page is gone; one kept in the note
          // is shared, and Wipe() only lets go of it.
          const auto wipe = qScopeGuard([&stored]() {
            if (stored.has_value()) Wipe(stored->data);
          });
          if (self.isNull()) return;
          --self->jobs_in_flight_;
          // Locked or reloaded meanwhile: the page that asked is gone.
          if (self->bridge_.Epoch() != epoch || self->state_ != State::kOPEN)
            return;
          if (!stored.has_value()) {
            self->reject_image(
                token, GTrC::tr("The image was not added: %1").arg(why));
            return;
          }
          // Registered before the page is told, so it never shows a hole.
          self->note_.assets.insert(stored->path, stored->data);
          if (!self->managed_.contains(stored->path))
            self->managed_ << stored->path;
          self->editor_.PutResource(stored->path, stored->mime, stored->data);
          self->post_json({{u"type"_s, u"image.ready"_s},
                           {u"token"_s, token},
                           {u"path"_s, stored->path}});
        },
        Qt::QueuedConnection);
  });
}

// --- encryption --------------------------------------------------------------

void NotePageView::encrypt() {
  if (state_ != State::kOPEN || encrypting_) return;
  encrypting_ = true;
  clear_notice();
  QPointer<NotePageView> self(this);
  const auto removals = removals_;
  const auto give_up = [self](const QString& why) {
    if (self.isNull()) return;
    self->encrypting_ = false;
    self->notice(GTrC::tr("Encryption failed"), true, true, why);
  };
  const auto asked = Commands().Invoke<gf::cmd::host::KeysChecked>(
      {}, this,
      [self, removals,
       give_up](const gf::cmd::Outcome<gf::cmd::host::KeysChecked::Result>& r) {
        if (self.isNull()) return;
        auto* v = self.data();
        if (!r.Ok() || v->state_ != State::kOPEN) {
          give_up(GTrC::tr("The Key ToolBox could not be read."));
          return;
        }
        // The keys checked right now, in the database the ToolBox shows; or,
        // with none checked, a passphrase, which the Host asks for.
        QStringList fingerprints;
        for (const auto& k : r.value.keys) fingerprints << k.fingerprint;
        const auto readers = ProtectionConfig::ForCheckedKeys(fingerprints);
        if (!readers.passphrase) {
          v->channel_ = static_cast<int>(r.value.channel);
          const auto facts = DescribeKeys(v->channel_, readers.recipients);
          QStringList bad;
          for (const auto& f : readers.recipients) {
            const auto k = facts.value(f);
            if (!k.Usable()) bad << k.DisplayName();
          }
          if (!bad.isEmpty()) {
            give_up(GTrC::tr("These checked keys cannot be encrypted to "
                             "(expired, revoked or disabled): %1")
                        .arg(bad.join(u", "_s)));
            return;
          }
        }
        v->checked_ = readers;
        v->set_status(GTrC::tr("Encrypting..."));
        // The text as it is now: edits made meanwhile leave the snapshot
        // behind, which the strip then says.
        v->request_snapshot(
            [self, removals, readers, give_up](std::optional<Snapshot> s) {
              if (self.isNull()) return;
              auto* w = self.data();
              if (!s.has_value()) {
                give_up(GTrC::tr("The editor did not answer."));
                return;
              }
              // The encryption was taken off while this waited: it stays off.
              if (w->removals_ != removals || w->state_ != State::kOPEN) {
                w->encrypting_ = false;
                return;
              }
              auto payload = std::make_shared<Payload>(
                  *std::static_pointer_cast<const Payload>(s->payload));
              payload->config = readers;
              s->readers = readers;
              s->payload = std::move(payload);
              // Named by time, so the newest sorts last.
              const auto file = w->revision_dir() + u"/"_s +
                                QDateTime::currentDateTimeUtc().toString(
                                    u"yyyyMMdd-HHmmsszzz"_s) +
                                QString::fromLatin1(kSuffix);
              w->persistence_.Revise(*s, file);
            });
      });
  if (!asked.Ok()) give_up(GTrC::tr("The Key ToolBox could not be read."));
}

void NotePageView::remove_encryption() {
  if (state_ != State::kOPEN) return;
  ++removals_;
  // The text stays exactly as it is; only the encrypted snapshot goes, and
  // the Key ToolBox keeps its checks for the next Encrypt.
  if (const auto file = persistence_.DropRevision(); !file.isEmpty()) {
    QFile::remove(file);
  }
  clear_notice();
  refresh_strip();
  NotifyOpsChanged();
}

auto NotePageView::EncryptRequested() -> bool {
  if (state_ != State::kOPEN) return false;
  encrypt();
  return true;
}

auto NotePageView::DecryptRequested() -> bool {
  if (state_ == State::kENCRYPTED) {
    decrypt_file();
    return true;
  }
  // Open, Decrypt takes the encryption off; never puts an older snapshot
  // back over the text.
  if (state_ == State::kOPEN && persistence_.Revision().exists) {
    remove_encryption();
    return true;
  }
  return false;
}

auto NotePageView::CryptoOperationTip(uint32_t op) const
    -> std::optional<QString> {
  if (state_ != State::kOPEN) return std::nullopt;
  if (op == GF_CRYPTO_OP_DECRYPT && persistence_.Revision().exists) {
    return GTrC::tr(
        "Remove encryption from this note. Keep the current text and save as "
        ".textpack.");
  }
  if (op == GF_CRYPTO_OP_ENCRYPT) {
    return GTrC::tr(
        "Encrypt this note for the keys checked in the Key ToolBox, or with a "
        "password when none are checked. Save then writes the encrypted "
        "version.");
  }
  return std::nullopt;
}

auto NotePageView::SaveAllowed() const -> bool {
  return state_ == State::kOPEN;
}

auto NotePageView::choose_destination(bool save_as, bool encrypted) -> QString {
  const auto suffix = QString::fromLatin1(encrypted ? kSuffix : kPlainSuffix);
  auto destination = path_;
  // An encrypted save never lands on the plain file, nor the other way round.
  const bool fits = encrypted
                        ? destination.endsWith(suffix, Qt::CaseInsensitive)
                        : IsPlainPath(destination);
  if (save_as || destination.isEmpty() || !fits) {
    const auto dir =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    // Windows appends the filter's extension itself, and takes ".gpg" alone
    // for the extension of a name already ending in ".textpack.gpg".
#ifdef Q_OS_WIN
    const auto name = BundleNameFor(QFileInfo(path_).fileName());
#else
    const auto name = BundleNameFor(QFileInfo(path_).fileName()) + suffix;
#endif
    const auto offered = path_.isEmpty()
                             ? QDir(dir).filePath(name)
                             : QFileInfo(path_).dir().filePath(name);
    destination = QFileDialog::getSaveFileName(
        this,
        encrypted ? GTrC::tr("Save Encrypted Note")
                  : GTrC::tr("Save Note (Not Encrypted)"),
        offered,
        encrypted ? GTrC::tr("Encrypted Note (*.textpack.gpg)")
                  : GTrC::tr("TextPack (*.textpack)"));
    if (destination.isEmpty()) return {};
    destination = WithNoteSuffix(destination, suffix);
  }
  return destination;
}

void NotePageView::restore_checked_keys() {
  const auto revision = persistence_.Revision();
  if (state_ != State::kOPEN || !revision.exists) return;
  // A password snapshot checks nothing: Encrypt then asks for a password.
  Commands().Invoke<gf::cmd::host::KeysSetChecked>(
      {static_cast<qint64>(channel_), revision.readers.passphrase
                                          ? QStringList{}
                                          : revision.readers.recipients});
}

void NotePageView::refresh_checked_keys() {
  if (state_ != State::kOPEN || checked_asked_ ||
      !persistence_.Revision().exists) {
    return;
  }
  checked_asked_ = true;
  QPointer<NotePageView> self(this);
  const auto asked = Commands().Invoke<gf::cmd::host::KeysChecked>(
      {}, this,
      [self](const gf::cmd::Outcome<gf::cmd::host::KeysChecked::Result>& r) {
        if (self.isNull()) return;
        self->checked_asked_ = false;
        if (!r.Ok()) return;
        QStringList fingerprints;
        for (const auto& k : r.value.keys) fingerprints << k.fingerprint;
        const auto now = ProtectionConfig::ForCheckedKeys(fingerprints);
        if (self->checked_ == now) return;
        self->checked_ = now;
        if (self->state_ == State::kOPEN) self->refresh_strip();
      });
  if (!asked.Ok()) checked_asked_ = false;
}

auto NotePageView::revision_dir() -> QString {
  if (revision_dir_.isEmpty()) {
    const auto root = RevisionRoot();
    static bool swept = false;
    if (!swept) {
      swept = true;
      RemoveOrphanedRevisions();
    }
    const auto make_owner_only = [](const QString& dir) {
      QDir().mkpath(dir);
      QFile::setPermissions(dir, QFileDevice::ReadOwner |
                                     QFileDevice::WriteOwner |
                                     QFileDevice::ExeOwner);
    };
    make_owner_only(root);
    revision_dir_ =
        root + u"/"_s + QUuid::createUuid().toString(QUuid::WithoutBraces);
    make_owner_only(revision_dir_);
    auto lock = std::make_unique<QLockFile>(revision_dir_ + u".lock"_s);
    lock->setStaleLockTime(0);
    if (lock->tryLock(0)) revision_lock_ = std::move(lock);
  }
  return revision_dir_;
}

void NotePageView::keep_only_revision(const QString& current) {
  if (revision_dir_.isEmpty()) return;
  // The new snapshot is in place before the one it replaces goes.
  for (const auto& f :
       QDir(revision_dir_)
           .entryInfoList({u"*"_s + QString::fromLatin1(kSuffix)},
                          QDir::Files)) {
    if (f.absoluteFilePath() != QFileInfo(current).absoluteFilePath()) {
      QFile::remove(f.absoluteFilePath());
    }
  }
}

void NotePageView::RemoveOrphanedRevisions() {
  const QDir root(RevisionRoot());
  for (const auto& d : root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    QLockFile probe(d.absoluteFilePath() + u".lock"_s);
    probe.setStaleLockTime(0);
    if (!probe.tryLock(0)) continue;  // a running tab's
    QDir(d.absoluteFilePath()).removeRecursively();
    probe.unlock();
  }
}

// --- snapshots and jobs ------------------------------------------------------

void NotePageView::request_snapshot(
    std::function<void(std::optional<Snapshot>)> done) {
  snapshot_waiters_.append(std::move(done));
  if (snapshot_in_progress_) return;
  snapshot_in_progress_ = true;
  snapshot_retries_ = 0;
  ask_for_snapshot();
}

void NotePageView::ask_for_snapshot() {
  const auto req = bridge_.Request(u"get"_s, {}, Now());
  if (req.isEmpty() || editor_.Post(req) != gf::ui::WebEditorStatus::kOK) {
    finish_snapshot({});
  }
}

void NotePageView::finish_snapshot(const QJsonObject& reply) {
  // Images still being stored: wait for them, a little.
  if (!reply.isEmpty() && reply.value(u"pending"_s).toInt() > 0 &&
      snapshot_retries_ < kSnapshotRetries) {
    ++snapshot_retries_;
    QTimer::singleShot(kSnapshotRetryMs, this, [this] { ask_for_snapshot(); });
    return;
  }
  snapshot_in_progress_ = false;
  auto waiters = std::exchange(snapshot_waiters_, {});
  const auto snap =
      reply.isEmpty() ? std::optional<Snapshot>{} : build_snapshot(reply);
  for (auto& w : waiters) w(snap);
}

auto NotePageView::build_snapshot(const QJsonObject& reply)
    -> std::optional<Snapshot> {
  const auto markdown = reply.value(u"markdown"_s).toString();
  last_markdown_ = markdown;
  QStringList referenced;
  for (const auto& v : reply.value(u"assets"_s).toArray())
    referenced << v.toString();
  // Never a snapshot that points at an image it does not carry.
  for (const auto& a : referenced) {
    if (!note_.assets.contains(a)) return std::nullopt;
  }

  auto payload = std::make_shared<Payload>();
  payload->note = note_;
  payload->note.markdown = front_matter_.toUtf8() + markdown.toUtf8();
  QStringList kept_managed;
  for (const auto& m : managed_) {
    (referenced.contains(m) ? kept_managed : payload->drop) << m;
  }
  payload->note.MergeApp(
      {{u"schema"_s, 1},
       {u"note_id"_s, note_id_},
       {u"managed_assets"_s, QJsonArray::fromStringList(kept_managed)}});

  payload->bundle = BundleNameFor(QFileInfo(path_).fileName());
  payload->channel = channel_;
  return Snapshot{
      page_gen_base_ + static_cast<quint64>(reply.value(u"gen"_s).toInteger()),
      {},
      payload};
}

void NotePageView::run_job(const Job& job) {
  QPointer<NotePageView> self(this);
  const auto id = job.id;
  const auto kind = job.kind;
  const auto payload =
      std::static_pointer_cast<const Payload>(job.snapshot.payload);
  const QString target = job.destination;
  const QString source = job.source;

  (void)QtConcurrent::run([self, id, kind, payload, target, source] {
    bool ok = false;
    QString why;
    std::optional<CryptoResult> crypto;  ///< set once Encrypt ran
    if (kind == JobKind::kSAVE) {
      // The snapshot, copied: it stays the note's encrypted version.
      QByteArray cipher;
      ok =
          ReadNoteFile(source, kMaxFileBytes, &cipher, &why) &&
          WriteNoteFile(target, EncryptedPackage::FromCiphertext(cipher), &why);
    } else if (payload != nullptr) {
      const auto entries =
          WritePack(payload->note, payload->bundle, payload->drop);
      if (const auto over = CheckBudgets(entries, Budgets{});
          over.has_value()) {
        why = *over;
      } else if (auto zip = ZipWrite(entries); !zip.has_value()) {
        why = u"the note could not be packed"_s;
      } else {
        // The pack must read back as what was written, under the same
        // budgets, or it is not saved.
        QString read_why;
        const auto back = ZipRead(*zip, Budgets{}, &read_why);
        if (!back.has_value() ||
            !ReadPack(*back, Budgets{}, &read_why).has_value()) {
          why = u"the packed note did not read back: "_s + read_why;
        } else {
          if (kind == JobKind::kSAVE_PLAIN) {
            // A note without an encrypted snapshot: as it is.
            ok = WriteNoteFile(target, PlainTextPack::FromUnencryptedNote(*zip),
                               &why);
          } else {
            crypto = Encrypt(payload->channel, payload->config, *zip);
            if (!crypto->ok) {
              why =
                  crypto->why.isEmpty() ? u"encryption failed"_s : crypto->why;
            } else {
              ok = WriteNoteFile(
                  target, EncryptedPackage::FromCiphertext(crypto->data), &why);
            }
            Wipe(crypto->data);  // the ciphertext is on disk, or not wanted
          }
          Wipe(*zip);
        }
      }
    }
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [self, id, ok, why, crypto = std::move(crypto)] {
          if (self.isNull()) return;
          if (crypto.has_value()) {
            // Kept as the note's encrypted version, or not written.
            self->report_crypto(GF_GPG_ANALYSE_ENCRYPT, *crypto,
                                ok ? GF_DOCUMENT_OUTPUT_APPLIED
                                   : GF_DOCUMENT_OUTPUT_APPLY_FAILED,
                                why);
          }
          self->persistence_.Finished(id, ok, why);
        },
        Qt::QueuedConnection);
  });
}

void NotePageView::job_done(const Job& job, bool ok, const QString& why) {
  if (job.kind == JobKind::kREVISION) {
    encrypting_ = false;
    const auto current = persistence_.Revision().path;
    if (ok && current == job.destination) {
      keep_only_revision(current);
      clear_notice();
    } else {
      // Failed, or made before the encryption was taken off: not wanted.
      // The snapshot before it, if any, stays as it was.
      QFile::remove(job.destination);
      if (!ok) notice(GTrC::tr("Encryption failed"), true, true, why);
    }
    set_status(persistence_.IsClean() ? QString()
                                      : GTrC::tr("Unsaved changes"));
    refresh_strip();
    NotifyOpsChanged();
    return;
  }
  if (job.kind == JobKind::kSAVE || job.kind == JobKind::kSAVE_PLAIN) {
    if (ok) {
      const auto previous = path_;
      path_ = job.destination;
      plain_ = job.kind == JobKind::kSAVE_PLAIN;
      NotifySaved(path_, QFileInfo(path_).fileName());
      // Edits the saved file does not have are still unsaved.
      notified_modified_ = !persistence_.IsClean();
      if (notified_modified_) {
        NotifyModified();
        set_status(GTrC::tr("Unsaved changes"));
      } else {
        set_status(GTrC::tr("Saved %1")
                       .arg(QLocale().toString(QTime::currentTime(),
                                               QLocale::ShortFormat)),
                   plain_ ? GTrC::tr("Saved, not encrypted, to %1").arg(path_)
                          : GTrC::tr("Saved, encrypted, to %1").arg(path_));
      }
      if (job.kind == JobKind::kSAVE && !persistence_.RevisionCurrent()) {
        // Said for a while; the state then still says the text moved on.
        notice(GTrC::tr("Saved older encrypted version"), true, false,
               GTrC::tr("Saved the last encrypted version. Encrypt again "
                        "to include your latest changes."));
      } else {
        clear_notice();
      }
      refresh_strip();
      // Encrypted now: the plain copy it was saved as before is still there.
      if (!plain_ && IsPlainPath(previous) && previous != path_ &&
          QFileInfo::exists(previous)) {
        const auto answer = QMessageBox::question(
            this, GTrC::tr("Unencrypted Copy"),
            GTrC::tr("The note is now saved encrypted. Its earlier, "
                     "unencrypted copy is still on disk:\n\n%1\n\nDelete it?")
                .arg(previous),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (answer == QMessageBox::Yes) QFile::remove(previous);
      }
    } else {
      set_status(GTrC::tr("Not saved"), why);
      notice(GTrC::tr("Not saved"), true, true, why);
      QMessageBox::warning(this, GTrC::tr("Note"),
                           GTrC::tr("The note was not saved. The file on disk "
                                    "is unchanged, and your changes are still "
                                    "here.\n\n%1")
                               .arg(why));
    }
  }
}

void NotePageView::autosave_tick() {
  bridge_.Expire(Now());
  // The Key ToolBox has no event for its checks; a look every few seconds,
  // while there is a snapshot they could differ from.
  if (state_ == State::kOPEN && isVisible() && ++checked_tick_ % 3 == 0) {
    refresh_checked_keys();
  }
  if (state_ != State::kOPEN || snapshot_in_progress_) return;
  if (!persistence_.CaptureDue(Now())) return;
  // The note as it is now, for the Host to keep with its other unsaved tabs.
  request_snapshot([this](std::optional<Snapshot> s) {
    if (!s.has_value()) return;
    recovery_ = s->payload;
    recovery_gen_ = s->edit_gen;
    persistence_.Captured(s->edit_gen);
    NotifyModified();
  });
}

// --- the host's requests -----------------------------------------------------

void NotePageView::Load(const QByteArray& bytes) {
  // Only ever the record Save() gave the Host's recovery cache: a tab it
  // brings back after the application ended with this note unsaved.
  if (bytes.isEmpty() || state_ != State::kNEW) return;
  auto record = RecoveryFromJson(bytes);
  if (!record.has_value()) return;
  path_ = record->path;
  plain_ = record->plain;
  open_note(record->note, true);
}

auto NotePageView::Save() -> std::optional<QByteArray> {
  if (state_ != State::kOPEN || recovery_ == nullptr ||
      recovery_gen_ <= handed_gen_) {
    return std::nullopt;
  }
  const auto& r = *std::static_pointer_cast<const Payload>(recovery_);
  handed_gen_ = recovery_gen_;
  return RecoveryToJson({r.note, path_, plain_}, r.drop);
}

auto NotePageView::IsDirty() const -> bool {
  // Whether the Host's copy is behind: what Save() would hand over is newer.
  return state_ == State::kOPEN && recovery_ != nullptr &&
         recovery_gen_ > handed_gen_;
}

auto NotePageView::CryptoOperations() const -> std::optional<uint32_t> {
  // Done here (EncryptRequested, DecryptRequested): the Host holds nothing.
  switch (state_) {
    case State::kENCRYPTED:
      return GF_CRYPTO_OP_DECRYPT;
    case State::kOPEN:
      // Decrypt, once there is an encrypted snapshot, takes it off.
      return GF_CRYPTO_OP_ENCRYPT |
             (persistence_.Revision().exists ? GF_CRYPTO_OP_DECRYPT : 0U);
    default:
      return 0U;
  }
}

auto NotePageView::SuggestedFileName() const -> QString {
  // The format Save would write now.
  return BundleNameFor(QFileInfo(path_).fileName()) +
         QString::fromLatin1(persistence_.Revision().exists ? kSuffix
                                                            : kPlainSuffix);
}

auto NotePageView::SaveRequested(bool save_as) -> bool {
  if (state_ != State::kOPEN) return false;
  if (persistence_.SaveInFlight()) return true;
  // Nothing new to write: the text is in the file, in the form wanted.
  if (!save_as && persistence_.IsClean() && persistence_.WrittenIsCurrent()) {
    return true;
  }

  // With an encrypted snapshot, exactly that; without one, the text as a
  // plain TextPack. Never an encryption, and never a passphrase asked for.
  // Either way a file of the other kind is kept: Save As, to a new name.
  const bool encrypted = persistence_.Revision().exists;
  const auto destination = choose_destination(save_as, encrypted);
  if (destination.isEmpty()) return true;
  clear_notice();
  set_status(GTrC::tr("Saving..."));
  if (encrypted) {
    persistence_.Save(destination);
    return true;
  }
  QPointer<NotePageView> self(this);
  request_snapshot([self, destination](std::optional<Snapshot> s) {
    if (self.isNull()) return;
    if (!s.has_value()) {
      self->set_status(GTrC::tr("Not saved"));
      self->notice(GTrC::tr("Not saved"), true, true,
                   GTrC::tr("The editor did not answer."));
      return;
    }
    self->persistence_.SavePlain(*s, destination);
  });
  return true;
}

void NotePageView::wait_until(const std::function<bool()>& ready) {
  const auto deadline = Now() + kWaitLimitMs;
  while (!ready() && Now() < deadline) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
  }
}

auto NotePageView::ask_and_close() -> bool {
  if (state_ != State::kOPEN || !persistence_.HasUnsavedWork()) {
    wait_until([this] { return !persistence_.Busy(); });
    return true;
  }
  const auto answer = QMessageBox::warning(
      this, GTrC::tr("Unsaved Note"),
      GTrC::tr("This note has unsaved changes. Save them before closing?"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
      QMessageBox::Save);
  if (answer == QMessageBox::Cancel) return false;
  return close_as_decided(answer == QMessageBox::Save);
}

auto NotePageView::close_as_decided(bool save) -> bool {
  if (state_ != State::kOPEN || !persistence_.HasUnsavedWork()) {
    wait_until([this] { return !persistence_.Busy(); });
    return true;
  }
  if (save) {
    if (!SaveRequested(false)) return false;
    wait_until([this] {
      return !encrypting_ && !snapshot_in_progress_ && !persistence_.Busy();
    });
    // An encrypted snapshot older than the text saves only what it holds:
    // the newer edits are still unsaved, so the note stays open.
    return persistence_.IsClean();
  }
  // Discard: nothing queued runs, and what runs finishes.
  persistence_.CancelQueued();
  wait_until([this] { return !persistence_.Busy(); });
  return true;
}

auto NotePageView::CloseRequested() -> bool { return ask_and_close(); }

auto NotePageView::CloseDecided(bool save) -> std::optional<bool> {
  return close_as_decided(save);
}

void NotePageView::FindRequested() {
  if (state_ != State::kOPEN) return;
  editor_.Post(R"({"type":"find.open"})");
}

auto NotePageView::make_persistence() -> NotePersistence {
  return NotePersistence(
      [this](const Job& j) { run_job(j); },
      [this](const Job& j, bool ok, const QString& w) { job_done(j, ok, w); });
}

void NotePageView::post_json(const QJsonObject& message) {
  editor_.Post(QJsonDocument(message).toJson(QJsonDocument::Compact));
}

void NotePageView::reject_image(const QString& token, const QString& why) {
  post_json({{u"type"_s, u"image.rejected"_s}, {u"token"_s, token}});
  notice(GTrC::tr("Image not added"), true, false, why);
}

void NotePageView::forget() {
  // As far as can be done: the page is told to drop everything, the editor
  // and its resources go, the buffers here are overwritten, and anything the
  // page still sends is void.
  recovery_.reset();
  recovery_gen_ = 0;
  handed_gen_ = 0;
  if (editor_.IsLive()) {
    editor_.Post(R"({"type":"wipe"})");
    editor_.Destroy();
  }
  bridge_.NewEpoch();
  Wipe(note_.markdown);
  for (auto& a : note_.assets) Wipe(a);
  note_ = {};
  Wipe(last_markdown_);
  front_matter_.clear();
  managed_.clear();

  notified_modified_ = false;
  notice_.clear();
  content_note_.clear();
}

void NotePageView::WipeContent() {
  wait_until([this] { return !persistence_.Busy() && jobs_in_flight_ == 0; });
  forget();
  Wipe(file_);
  file_readers_ = {};
}

}  // namespace gf::notes
