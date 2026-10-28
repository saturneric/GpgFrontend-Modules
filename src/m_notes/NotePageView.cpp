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
#include <GFSDKStorage.h>

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
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLocale>
#include <QLockFile>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>
#include <array>

#include "GFModuleTr.h"
#include "NoteFiles.h"
#include "NoteImage.h"
#include "NoteStyle.h"

using namespace Qt::StringLiterals;

namespace gf::notes {

namespace {

/// Where Encrypt puts revisions until Save moves them.
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
/// How long closing or locking waits for work still in flight.
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

auto ConfirmedKey(const QString& note_id) -> QString {
  return u"confirmed/"_s + note_id;
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
    // A new note: open straight away, empty, with nothing confirmed yet --
    // unless the Host opened a file into this tab meanwhile (OpenPath runs
    // right after the tab is made, before this does).
    QTimer::singleShot(0, this, [this] {
      if (state_ != State::kNEW) return;
      note_ = {};
      note_.info = QJsonObject{{u"version"_s, 2}};
      note_id_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
      persistence_ = make_persistence();
      assess();
      start_editor();
    });
  }
}

NotePageView::~NotePageView() {
  // Nothing in flight may finish into a page that is gone.
  wait_until([this] { return !persistence_.Busy() && jobs_in_flight_ == 0; });
  forget();
  // Revisions belong to this tab; whatever was saved has been moved already.
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

/// "Alice" from "Alice <alice@example.org>".
auto ShortName(const QString& label) -> QString {
  const auto at = label.indexOf(u" <"_s);
  return at > 0 ? label.left(at) : label;
}

/// "0x1D70 14AD 87F5 A975", easier to compare by eye than one run.
auto GroupedId(const QString& id) -> QString {
  const auto tail = id.right(16).toUpper();
  QStringList parts;
  for (int i = 0; i < tail.size(); i += 4) parts << tail.mid(i, 4);
  return parts.join(u' ');
}

auto Row(QWidget* glyph, const QString& title, const QString& detail,
         const QString& badge) -> QWidget* {
  auto* row = new QWidget();
  auto* h = new QHBoxLayout(row);
  h->setContentsMargins(14, 10, 14, 10);
  h->setSpacing(12);
  h->addWidget(glyph, 0, Qt::AlignVCenter);
  auto* text = new QVBoxLayout();
  text->setSpacing(1);
  auto* t = new QLabel(title);
  t->setTextFormat(Qt::PlainText);
  t->setTextInteractionFlags(Qt::TextSelectableByMouse);
  text->addWidget(t);
  if (!detail.isEmpty()) {
    auto* d = new QLabel(detail);
    d->setTextFormat(Qt::PlainText);
    d->setTextInteractionFlags(Qt::TextSelectableByMouse);
    NoteScaleFont(d, 0.9);
    gf::ui::SetLabelColor(d, gf::ui::MutedColor(row));
    d->setObjectName(u"muted"_s);
    text->addWidget(d);
  }
  h->addLayout(text, 1);
  if (!badge.isEmpty()) {
    auto* b = new QLabel(badge);
    NoteScaleFont(b, 0.85, QFont::DemiBold);
    gf::ui::SetLabelColor(b, row->palette().color(QPalette::Highlight));
    h->addWidget(b, 0, Qt::AlignVCenter);
  }
  return row;
}

auto Rule(QWidget* parent) -> QFrame* {
  auto* rule = new QFrame(parent);
  rule->setFrameShape(QFrame::HLine);
  rule->setFrameShadow(QFrame::Plain);
  rule->setFixedHeight(1);
  auto pal = rule->palette();
  pal.setColor(QPalette::WindowText, gf::ui::BorderColor(rule));
  rule->setPalette(pal);
  return rule;
}

auto FlatButton(const QString& text) -> QToolButton* {
  auto* b = new QToolButton();
  b->setText(text);
  b->setAutoRaise(true);
  b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  b->setCursor(Qt::PointingHandCursor);
  b->setFocusPolicy(Qt::TabFocus);
  return b;
}

}  // namespace

void NotePageView::build_ui() {
  stack_ = new QStackedWidget(this);
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->addWidget(stack_);

  // Locked: one column, centred, in the upper part of the page. What the
  // note is, the way in, and below that who it is encrypted for.
  auto* locked_scroll = new QScrollArea();
  locked_scroll->setFrameShape(QFrame::NoFrame);
  locked_scroll->setWidgetResizable(true);
  auto* locked = new QWidget();
  locked_scroll->setWidget(locked);
  auto* page = new QVBoxLayout(locked);
  page->setContentsMargins(24, 24, 24, 24);
  page->addStretch(2);
  auto* column_holder = new QHBoxLayout();
  auto* column_widget = new QWidget();
  column_widget->setMaximumWidth(460);
  column_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  auto* column = new QVBoxLayout(column_widget);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(0);
  column_holder->addStretch(1);
  column_holder->addWidget(column_widget, 4);
  column_holder->addStretch(1);
  page->addLayout(column_holder);
  page->addStretch(3);

  auto* glyph = new NoteGlyph(NoteGlyph::Kind::kLOCK, 72, true);
  locked_glyph_ = glyph;
  column->addWidget(glyph, 0, Qt::AlignHCenter);
  column->addSpacing(18);
  locked_title_ = new QLabel();
  locked_title_->setTextFormat(Qt::PlainText);
  locked_title_->setAlignment(Qt::AlignHCenter);
  locked_title_->setWordWrap(true);
  NoteScaleFont(locked_title_, 1.6, QFont::DemiBold);
  column->addWidget(locked_title_);
  column->addSpacing(4);
  locked_facts_ = new QLabel();
  locked_facts_->setTextFormat(Qt::PlainText);
  locked_facts_->setAlignment(Qt::AlignHCenter);
  locked_facts_->setWordWrap(true);
  locked_facts_->setObjectName(u"muted"_s);
  column->addWidget(locked_facts_);
  column->addSpacing(22);

  unlock_ = new QPushButton();
  unlock_->setMinimumWidth(200);
  unlock_->setMinimumHeight(unlock_->sizeHint().height() + 8);
  unlock_->setDefault(true);
  unlock_->setAutoDefault(true);
  column->addWidget(unlock_, 0, Qt::AlignHCenter);
  column->addSpacing(6);

  locked_status_ = new QLabel();
  locked_status_->setWordWrap(true);
  locked_status_->setAlignment(Qt::AlignHCenter);
  locked_status_->setTextFormat(Qt::PlainText);
  column->addSpacing(6);
  column->addWidget(locked_status_);
  column->addSpacing(28);

  recipients_heading_ = new QLabel(GTrC::tr("ENCRYPTED FOR"));
  NoteScaleFont(recipients_heading_, 0.82, QFont::DemiBold);
  recipients_heading_->setObjectName(u"muted"_s);
  column->addWidget(recipients_heading_);
  column->addSpacing(8);
  auto* card = new NoteCard();
  recipients_ = new QVBoxLayout(card);
  recipients_->setContentsMargins(1, 1, 1, 1);
  recipients_->setSpacing(0);
  column->addWidget(card);
  column->addSpacing(8);
  recipients_note_ = new QLabel(GTrC::tr(
      "Read from the file itself, so not yet verified. Once the note is open "
      "you can see and change who can read it."));
  recipients_note_->setWordWrap(true);
  recipients_note_->setObjectName(u"muted"_s);
  NoteScaleFont(recipients_note_, 0.9);
  column->addWidget(recipients_note_);
  connect(unlock_, &QPushButton::clicked, this, [this] { unlock(); });
  stack_->addWidget(locked_scroll);

  // Open: a slim header (who can read it; how far it is saved; Lock), a
  // banner for anything that needs saying, and the editor page.
  auto* open = new QWidget();
  auto* o = new QVBoxLayout(open);
  o->setContentsMargins(0, 0, 0, 0);
  o->setSpacing(0);
  header_ = new QWidget();
  auto* hl = new QHBoxLayout(header_);
  hl->setContentsMargins(8, 4, 8, 2);
  hl->setSpacing(6);
  protection_button_ = FlatButton(QString());
  protection_button_->setIconSize(QSize(16, 16));
  hl->addWidget(protection_button_);
  hl->addStretch(1);
  save_state_ = new QLabel();
  save_state_->setObjectName(u"muted"_s);
  save_state_->setTextFormat(Qt::PlainText);
  NoteScaleFont(save_state_, 0.92);
  hl->addWidget(save_state_);
  hl->addSpacing(6);
  lock_ = FlatButton(GTrC::tr("Lock"));
  lock_->setIconSize(QSize(16, 16));
  lock_->setToolTip(
      GTrC::tr("Close the note and forget what is in memory. "
               "Its file stays encrypted where it is."));
  hl->addWidget(lock_);
  o->addWidget(header_);

  banner_ = new QWidget();
  banner_->setAutoFillBackground(true);
  auto* bl = new QHBoxLayout(banner_);
  bl->setContentsMargins(14, 8, 8, 8);
  bl->setSpacing(10);
  auto* bglyph = new NoteGlyph(NoteGlyph::Kind::kINFO, 16);
  banner_glyph_ = bglyph;
  bl->addWidget(bglyph, 0, Qt::AlignTop);
  banner_text_ = new QLabel();
  banner_text_->setWordWrap(true);
  banner_text_->setTextFormat(Qt::PlainText);
  bl->addWidget(banner_text_, 1);
  auto* dismiss = FlatButton(QString());
  dismiss->setToolTip(GTrC::tr("Dismiss"));
  dismiss->setObjectName(u"dismiss"_s);
  dismiss->setIconSize(QSize(14, 14));
  bl->addWidget(dismiss, 0, Qt::AlignTop);
  banner_->setVisible(false);
  o->addWidget(banner_);
  banner_timer_ = new QTimer(this);
  banner_timer_->setSingleShot(true);
  banner_timer_->setInterval(8000);
  connect(banner_timer_, &QTimer::timeout, banner_, &QWidget::hide);
  connect(dismiss, &QToolButton::clicked, banner_, &QWidget::hide);

  slot_ = new QWidget();
  slot_->setObjectName(u"note_editor_slot"_s);
  o->addWidget(slot_, 1);
  // The Key ToolBox is where readers are chosen: this shows them there.
  connect(protection_button_, &QToolButton::clicked, this,
          [this] { restore_checked_keys(); });
  connect(lock_, &QToolButton::clicked, this, [this] {
    if (ask_and_close()) lock();
  });
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
  gf::ui::SetLabelColor(locked_status_, gf::ui::WarningColor(this));

  const auto window = palette().color(QPalette::Window);
  const auto tone = banner_warning_ ? gf::ui::WarningColor(this)
                                    : palette().color(QPalette::Highlight);
  auto bp = banner_->palette();
  bp.setColor(QPalette::Window, gf::ui::Blend(window, tone, 0.12));
  banner_->setPalette(bp);
  banner_glyph_->SetKind(banner_warning_ ? NoteGlyph::Kind::kWARNING
                                         : NoteGlyph::Kind::kINFO);
  banner_glyph_->SetColor(tone);
  if (auto* d = banner_->findChild<QToolButton*>(u"dismiss"_s)) {
    d->setIcon(NoteGlyph::Icon(NoteGlyph::Kind::kCLOSE,
                               palette().color(QPalette::WindowText), 14));
  }
  lock_->setIcon(NoteGlyph::Icon(NoteGlyph::Kind::kLOCK,
                                 palette().color(QPalette::WindowText), 16));
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

void NotePageView::show_locked(const QString& status) {
  state_ = State::kLOCKED;
  const QFileInfo info(path_);
  locked_title_->setText(DisplayName(path_));
  locked_facts_->setText(QStringList{
      GTrC::tr("Encrypted note"), QLocale().formattedDataSize(info.size()),
      GTrC::tr("modified %1")
          .arg(QLocale().toString(info.lastModified(), QLocale::ShortFormat))}
                             .join(u"  \u00B7  "_s));
  locked_glyph_->SetKind(NoteGlyph::Kind::kLOCK);

  // Who it is encrypted for, as far as the packets say.
  while (auto* item = recipients_->takeAt(0)) {
    delete item->widget();
    delete item;
  }
  int passphrases = 0;
  bool first = true;
  auto add = [this, &first](NoteGlyph::Kind kind, const QString& title,
                            const QString& detail, const QString& badge) {
    if (!first) recipients_->addWidget(Rule(nullptr));
    first = false;
    recipients_->addWidget(
        Row(new NoteGlyph(kind, 32, true), title, detail, badge));
  };
  for (const auto& p : packets_) {
    if (!p.pkesk) {
      ++passphrases;
      continue;
    }
    if (p.hidden) {
      add(NoteGlyph::Kind::kHIDDEN, GTrC::tr("Hidden recipient"),
          GTrC::tr("The file does not say whose key this is."), {});
    } else if (p.key_found) {
      const auto name = p.label.isEmpty() ? p.primary_fingerprint : p.label;
      add(NoteGlyph::Kind::kPERSON, ShortName(name),
          GTrC::tr("Key %1").arg(GroupedId(p.key_id)),
          p.has_secret ? GTrC::tr("Your key") : QString());
    } else {
      add(NoteGlyph::Kind::kKEY, GTrC::tr("Unknown key"),
          GTrC::tr("Key %1, not in your key database").arg(GroupedId(p.key_id)),
          {});
    }
  }
  if (passphrases > 0) {
    add(NoteGlyph::Kind::kPASSPHRASE, GTrC::tr("Passphrase"),
        GTrC::tr("Anyone who knows the note's passphrase"), {});
  }
  if (first) {
    add(NoteGlyph::Kind::kWARNING, GTrC::tr("Nothing readable"),
        GTrC::tr("The file names no recipients."), {});
  }

  const bool by_passphrase =
      std::none_of(packets_.cbegin(), packets_.cend(),
                   [](const DetectedPacket& p) { return p.pkesk; });
  unlock_->setText(GTrC::tr("Unlock"));
  unlock_->setIcon(NoteGlyph::Icon(
      by_passphrase ? NoteGlyph::Kind::kPASSPHRASE : NoteGlyph::Kind::kKEY,
      palette().color(QPalette::ButtonText), 16));
  unlock_->setToolTip(
      GTrC::tr("Opened as the Decrypt button would open it: "
               "your key's passphrase or PIN, or the note's "
               "passphrase, may be asked for."));
  unlock_->setEnabled(true);
  locked_status_->setText(status);
  locked_status_->setVisible(!status.isEmpty());
  stack_->setCurrentIndex(0);
  unlock_->setFocus();
  // The tab was made before it knew what it holds: let the Host ask again.
  NotifyOpsChanged();
}

void NotePageView::show_open() {
  state_ = State::kOPEN;
  banner_->hide();
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

void NotePageView::refresh_strip() {
  if (protection_.IsEmpty()) {
    protection_button_->setText(
        plain_ ? GTrC::tr("Not encrypted: a plain TextPack")
               : GTrC::tr("Not encrypted"));
    protection_button_->setIcon(NoteGlyph::Icon(
        NoteGlyph::Kind::kWARNING, gf::ui::WarningColor(this), 16));
    protection_button_->setToolTip(
        GTrC::tr("Save keeps it as a plain TextPack, which anyone with the "
                 "file can read.\n\nTo encrypt it, check who can read it in "
                 "the Key ToolBox (or no one, for a passphrase) and press "
                 "Encrypt."));
    lock_->setEnabled(false);
    lock_->setToolTip(GTrC::tr("Only an encrypted note can be locked."));
    return;
  }
  const auto facts = DescribeKeys(channel_, protection_.recipients);
  QStringList names;
  QStringList lines;
  for (const auto& f : protection_.recipients) {
    const auto k = facts.value(f);
    names << (k.label.isEmpty() ? GroupedId(f) : ShortName(k.label));
    lines << u"%1  (%2)"_s.arg(k.DisplayName(), GroupedId(f));
  }
  if (protection_.passphrase) {
    names << GTrC::tr("passphrase");
    lines << GTrC::tr("Anyone who knows the note's passphrase");
  }
  if (protection_.recipients.isEmpty()) names.clear();
  QString who;
  const int more = static_cast<int>(names.size()) - 2;
  if (more <= 0) {
    who = names.join(GTrC::tr(" and "));
  } else {
    who =
        GTrC::tr("%1, %2 and %n more", "", more).arg(names.at(0), names.at(1));
  }
  protection_button_->setText(names.isEmpty()
                                  ? GTrC::tr("Encrypted with a passphrase")
                                  : GTrC::tr("Encrypted for %1").arg(who));
  protection_button_->setIcon(NoteGlyph::Icon(
      NoteGlyph::Kind::kLOCK, palette().color(QPalette::Highlight), 16));
  protection_button_->setToolTip(
      GTrC::tr("Saves and recovery copies can be read by:\n%1\n\nClick to "
               "check these keys in the Key ToolBox. Change the checks there, "
               "then press Encrypt.")
          .arg(lines.join(u"\n"_s)));
  const bool lockable = path_.endsWith(QString::fromLatin1(kSuffix));
  lock_->setEnabled(lockable);
  lock_->setToolTip(!lockable
                        ? GTrC::tr("Save the note encrypted first; then it "
                                   "can be locked.")
                        : GTrC::tr("Close the note and forget what is "
                                   "in memory. Its file stays "
                                   "encrypted where it is."));
}

void NotePageView::set_status(const QString& text, const QString& detail) {
  save_state_->setText(text);
  save_state_->setToolTip(detail);
}

void NotePageView::show_banner(const QString& text, bool warning,
                               bool transient) {
  banner_warning_ = warning;
  banner_text_->setText(text);
  restyle();
  banner_->show();
  if (transient) {
    banner_timer_->start();
  } else {
    banner_timer_->stop();
  }
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
  static const std::array<std::pair<const char*, const char*>, 36> kWords = {{
      {"formatting", QT_TRANSLATE_NOOP("ModuleNotes", "Formatting")},
      {"paragraph", QT_TRANSLATE_NOOP("ModuleNotes", "Normal text")},
      {"heading1", QT_TRANSLATE_NOOP("ModuleNotes", "Heading 1")},
      {"heading2", QT_TRANSLATE_NOOP("ModuleNotes", "Heading 2")},
      {"heading3", QT_TRANSLATE_NOOP("ModuleNotes", "Heading 3")},
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
  if (!sniff_file(bytes)) return false;
  path_ = path;
  show_locked();
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

auto NotePageView::sniff_file(const QByteArray& bytes) -> bool {
  const auto packets = SniffPackets(channel_, bytes);
  if (!packets.has_value()) {
    QMessageBox::warning(
        this, GTrC::tr("Encrypted Note"),
        GTrC::tr("This file is not an OpenPGP-encrypted note."));
    return false;
  }
  file_ = bytes;
  packets_ = *packets;
  return true;
}

void NotePageView::unlock() {
  if (state_ != State::kLOCKED) return;
  state_ = State::kUNLOCKING;
  unlock_->setEnabled(false);
  unlock_->setText(GTrC::tr("Unlocking..."));
  locked_status_->hide();

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
          if (self.isNull()) return;
          --self->jobs_in_flight_;
          if (!result.ok) {
            self->show_locked(DecryptFailure(result.error));
            return;
          }
          self->opened(result.data);
          Wipe(result.data);
        },
        Qt::QueuedConnection);
  });
}

void NotePageView::opened(const QByteArray& zip) {
  QString why;
  const auto entries = ZipRead(zip, Budgets{}, &why);
  const auto note = entries.has_value() ? ReadPack(*entries, Budgets{}, &why)
                                        : std::optional<Note>{};
  if (!note.has_value()) {
    if (plain_) {
      show_unavailable(GTrC::tr("This file is not a note: %1").arg(why));
      return;
    }
    show_locked(GTrC::tr("The note decrypted, but its contents are not a "
                         "note: %1")
                    .arg(why));
    return;
  }
  open_note(*note, false);
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

  persistence_ = make_persistence();
  assess();
  if (plain_) {
    // Whatever its info.json remembers, this file is not encrypted.
    protection_ = {};
    protection_confirmed_ = false;
  }
  if (unsaved) {
    // Restored from the Host's recovery cache: nothing of it is saved yet.
    persistence_.Edited(1, Now());
    persistence_.Captured(1);
  } else {
    persistence_.Opened(0, rev_);
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
    persistence_.Edited(
        page_gen_base_ + static_cast<quint64>(msg.value(u"gen"_s).toInteger()),
        Now());
    if (!notified_modified_) {
      notified_modified_ = true;
      NotifyModified();
      set_status(persistence_.Revision().exists
                     ? GTrC::tr("Changed since Encrypt")
                     : GTrC::tr("Unsaved changes"));
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
    show_banner(
        GTrC::tr("The editor restarted; your last snapshot was restored."),
        true);
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
  show_banner(GTrC::tr("This note uses %1, which the editor shows as plain "
                       "text. Editing and saving may change how it looks in "
                       "other apps.")
                  .arg(what.join(u", "_s)),
              false);
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
  if (const auto* t = kNotices.value(text))
    show_banner(GTrC::tr(t), true, true);
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
        [self, stored = std::move(stored), why, token, epoch] {
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

// --- protection --------------------------------------------------------------

void NotePageView::assess() {
  std::optional<ProtectionConfig> recorded;
  const auto app = note_.App();
  if (app.contains(u"protection"_s)) {
    recorded =
        ProtectionConfig::FromJson(app.value(u"protection"_s).toObject());
  }
  QStringList fprs =
      recorded.has_value() ? recorded->recipients : QStringList{};
  for (const auto& p : packets_) {
    if (p.key_found) fprs << p.primary_fingerprint;
  }
  fprs.removeDuplicates();
  QList<QByteArray> confirmed;
  for (const auto& h : gf::sdk::Setting(GFModuleSdkContext(), GF_SETTING_MODULE,
                                        ConfirmedKey(note_id_))
                           .toStringList()) {
    confirmed << h.toLatin1();
  }
  assessment_ =
      Assess(recorded, packets_, DescribeKeys(channel_, fprs), confirmed);
  protection_ = assessment_.proposal;
  // Keys and a passphrase together is what older saves did; the next
  // Encrypt chooses one, and asks before dropping the other.
  protection_confirmed_ = assessment_.Resolved() && protection_.IsExclusive();
}

void NotePageView::choose_protection(
    std::function<void(std::optional<ProtectionConfig>)> done) {
  QPointer<NotePageView> self(this);
  const auto asked = Commands().Invoke<gf::cmd::host::KeysChecked>(
      {}, this,
      [self,
       done](const gf::cmd::Outcome<gf::cmd::host::KeysChecked::Result>& r) {
        if (self.isNull()) return;
        auto* v = self.data();
        if (!r.Ok()) {
          v->show_banner(GTrC::tr("The Key ToolBox could not be read."), true);
          done(std::nullopt);
          return;
        }
        ProtectionConfig next;
        if (!r.value.keys.isEmpty()) {
          // The keys checked right now, in the database the ToolBox shows.
          v->channel_ = static_cast<int>(r.value.channel);
          for (const auto& k : r.value.keys) next.recipients << k.fingerprint;
          next.Normalise();
          const auto facts = DescribeKeys(v->channel_, next.recipients);
          QStringList bad;
          for (const auto& f : next.recipients) {
            const auto k = facts.value(f);
            if (!k.Usable()) bad << k.DisplayName();
          }
          if (!bad.isEmpty()) {
            QMessageBox::warning(
                v, GTrC::tr("Encrypt Note"),
                GTrC::tr("These checked keys cannot be encrypted to (expired, "
                         "revoked or disabled):\n\n%1\n\nUncheck them in the "
                         "Key ToolBox, then encrypt again.")
                    .arg(bad.join(u"\n"_s)));
            done(std::nullopt);
            return;
          }
        } else {
          // No key checked: a passphrase, and only a passphrase.
          next.passphrase = true;
        }

        // Nobody loses access without being named first.
        if (!v->protection_.IsEmpty()) {
          const auto lost = LostBetween(v->protection_, next);
          if (!lost.IsEmpty()) {
            const auto facts = DescribeKeys(v->channel_, lost.recipients);
            QStringList names;
            for (const auto& f : lost.recipients) {
              const auto k = facts.value(f);
              names << k.DisplayName();
            }
            if (lost.passphrase) {
              names << GTrC::tr("anyone who knows the note's passphrase");
            }
            const auto answer = QMessageBox::warning(
                v, GTrC::tr("Encrypt Note"),
                GTrC::tr("These will no longer be able to read new saves of "
                         "this note:\n\n%1\n\nContinue?")
                    .arg(names.join(u"\n"_s)),
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (answer != QMessageBox::Yes) {
              done(std::nullopt);
              return;
            }
          }
        }
        // A passphrase is asked for by the Host when it encrypts.
        done(next);
      });
  if (!asked.Ok()) {
    show_banner(GTrC::tr("The Key ToolBox could not be read."), true);
    done(std::nullopt);
  }
}

void NotePageView::adopt_protection(const ProtectionConfig& config) {
  if (config == protection_ && protection_confirmed_) return;
  protection_ = config;
  protection_confirmed_ = true;
  assessment_.proposal = config;
  assessment_.confirmed = true;
  assessment_.issues.clear();
  record_confirmation();
  ++rev_;
  persistence_.ProtectionChanged(rev_);
  refresh_strip();
}

void NotePageView::encrypt(bool then_save, bool save_as) {
  if (state_ != State::kOPEN || encrypting_) return;
  encrypting_ = true;
  QPointer<NotePageView> self(this);
  choose_protection([self, then_save,
                     save_as](std::optional<ProtectionConfig> config) {
    if (self.isNull()) return;
    auto* v = self.data();
    if (!config.has_value() || v->state_ != State::kOPEN) {
      v->encrypting_ = false;
      return;
    }
    QString destination;
    if (then_save) {
      destination = v->choose_destination(save_as, true);
      if (destination.isEmpty()) {
        v->encrypting_ = false;
        return;
      }
    }
    v->adopt_protection(*config);
    v->set_status(GTrC::tr("Encrypting..."));
    v->request_snapshot([self, destination](std::optional<Snapshot> s) {
      if (self.isNull()) return;
      auto* w = self.data();
      w->encrypting_ = false;
      if (!s.has_value()) {
        w->set_status(GTrC::tr("Not encrypted"));
        w->show_banner(GTrC::tr("Not encrypted: the editor did not answer."),
                       true);
        return;
      }
      // Named by time, so the newest sorts last and the oldest is pruned.
      const auto file =
          w->revision_dir() + u"/"_s +
          QDateTime::currentDateTimeUtc().toString(u"yyyyMMdd-HHmmsszzz"_s) +
          QString::fromLatin1(kSuffix);
      w->persistence_.Revise(*s, file);
      if (!destination.isEmpty()) w->persistence_.Save(destination);
    });
  });
}

auto NotePageView::EncryptRequested() -> bool {
  if (state_ != State::kOPEN) return false;
  encrypt(false, false);
  return true;
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
    const auto name = BundleNameFor(QFileInfo(path_).fileName()) + suffix;
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
    if (!destination.endsWith(suffix, Qt::CaseInsensitive)) {
      destination += suffix;
    }
  }
  return destination;
}

void NotePageView::restore_checked_keys() {
  if (state_ != State::kOPEN || protection_.IsEmpty()) return;
  // A passphrase note checks nothing: Encrypt then asks for a passphrase.
  Commands().Invoke<gf::cmd::host::KeysSetChecked>(
      {static_cast<qint64>(channel_),
       protection_.passphrase ? QStringList{} : protection_.recipients});
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

void NotePageView::prune_revisions() {
  if (revision_dir_.isEmpty()) return;
  auto files = QDir(revision_dir_)
                   .entryInfoList({u"*"_s + QString::fromLatin1(kSuffix)},
                                  QDir::Files, QDir::Name | QDir::Reversed);
  for (qsizetype i = kKeptRevisions; i < files.size(); ++i) {
    QFile::remove(files.at(i).absoluteFilePath());
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

void NotePageView::record_confirmation() {
  auto* ctx = GFModuleSdkContext();
  auto hashes = gf::sdk::Setting(ctx, GF_SETTING_MODULE, ConfirmedKey(note_id_))
                    .toStringList();
  const auto h = QString::fromLatin1(protection_.Hash());
  if (!hashes.contains(h)) hashes << h;
  gf::sdk::SetSetting(ctx, GF_SETTING_MODULE, ConfirmedKey(note_id_), hashes);
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
       {u"protection"_s, protection_.ToJson()},
       {u"managed_assets"_s, QJsonArray::fromStringList(kept_managed)}});
  payload->config = protection_;

  payload->bundle = BundleNameFor(QFileInfo(path_).fileName());
  payload->channel = channel_;
  return Snapshot{
      page_gen_base_ + static_cast<quint64>(reply.value(u"gen"_s).toInteger()),
      rev_, payload};
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
    if (kind == JobKind::kSAVE) {
      // The revision, moved: written to its place first, removed after.
      QByteArray cipher;
      ok =
          ReadNoteFile(source, kMaxFileBytes, &cipher, &why) &&
          WriteNoteFile(target, EncryptedPackage::FromCiphertext(cipher), &why);
      if (ok) QFile::remove(source);
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
            // The user saved a note they never encrypted: as it is.
            ok = WriteNoteFile(target, PlainTextPack::FromUnencryptedNote(*zip),
                               &why);
          } else if (auto cipher =
                         Encrypt(payload->channel, payload->config, *zip);
                     !cipher.ok) {
            why = cipher.why.isEmpty() ? u"encryption failed"_s : cipher.why;
          } else {
            ok = WriteNoteFile(
                target, EncryptedPackage::FromCiphertext(cipher.data), &why);
          }
          Wipe(*zip);
        }
      }
    }
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [self, id, ok, why] {
          if (!self.isNull()) self->persistence_.Finished(id, ok, why);
        },
        Qt::QueuedConnection);
  });
}

void NotePageView::job_done(const Job& job, bool ok, const QString& why) {
  if (job.kind == JobKind::kREVISION) {
    if (ok) {
      prune_revisions();
      if (!persistence_.SaveInFlight()) {
        set_status(GTrC::tr("Encrypted, not saved"),
                   GTrC::tr("Save puts this encrypted revision in the note's "
                            "file."));
      }
    } else {
      // A save waiting for this revision must not move an older one.
      persistence_.CancelQueued();
      set_status(GTrC::tr("Not encrypted"), why);
      show_banner(GTrC::tr("Not encrypted: %1").arg(why), true);
    }
    return;
  }
  if (job.kind == JobKind::kSAVE || job.kind == JobKind::kSAVE_PLAIN) {
    if (ok) {
      const auto previous = path_;
      path_ = job.destination;
      plain_ = job.kind == JobKind::kSAVE_PLAIN;
      NotifySaved(path_, QFileInfo(path_).fileName());
      // Edits made while saving are still unsaved.
      notified_modified_ = !persistence_.IsClean();
      if (notified_modified_) {
        NotifyModified();
        set_status(GTrC::tr("Changed since Encrypt"),
                   GTrC::tr("The saved file has the note as it was at the "
                            "last Encrypt."));
      } else {
        set_status(GTrC::tr("Saved %1")
                       .arg(QLocale().toString(QTime::currentTime(),
                                               QLocale::ShortFormat)),
                   plain_ ? GTrC::tr("Saved, not encrypted, to %1").arg(path_)
                          : GTrC::tr("Saved, encrypted, to %1").arg(path_));
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
      show_banner(GTrC::tr("Not saved: %1").arg(why), true);
      QMessageBox::warning(this, GTrC::tr("Encrypted Note"),
                           GTrC::tr("The note was not saved. The file on disk "
                                    "is unchanged, and your changes are still "
                                    "here.\n\n%1")
                               .arg(why));
    }
  }
}

void NotePageView::autosave_tick() {
  bridge_.Expire(Now());
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
  // Encrypt only, and done here (EncryptRequested): the Host holds nothing.
  return state_ == State::kOPEN ? GF_CRYPTO_OP_ENCRYPT : 0U;
}

auto NotePageView::SuggestedFileName() const -> QString {
  return BundleNameFor(QFileInfo(path_).fileName()) +
         QString::fromLatin1(protection_.IsEmpty() ? kPlainSuffix : kSuffix);
}

auto NotePageView::SaveRequested(bool save_as) -> bool {
  if (state_ != State::kOPEN) return false;
  if (encrypting_ || persistence_.SaveInFlight()) return true;
  if (!save_as && persistence_.IsClean()) return true;

  // Never encrypted: saved as it is, a plain TextPack, where the user says.
  if (protection_.IsEmpty() && !path_.endsWith(QString::fromLatin1(kSuffix))) {
    const auto destination = choose_destination(save_as, false);
    if (destination.isEmpty()) return true;
    set_status(GTrC::tr("Saving..."));
    request_snapshot([this, destination](std::optional<Snapshot> s) {
      if (!s.has_value()) {
        set_status(GTrC::tr("Not saved"));
        show_banner(GTrC::tr("Not saved: the editor did not answer."), true);
        return;
      }
      persistence_.SavePlain(*s, destination);
    });
    return true;
  }

  // What is saved is the last revision -- the note as it was at Encrypt.
  if (!persistence_.Revision().exists) {
    QMessageBox box(
        QMessageBox::Information, GTrC::tr("Encrypt First"),
        GTrC::tr("A note is saved as it was when you last pressed "
                 "Encrypt, and this one has not been encrypted "
                 "yet.\n\nCheck who can read it in the Key ToolBox, "
                 "or check no one to use a passphrase."),
        QMessageBox::Cancel, this);
    auto* now =
        box.addButton(GTrC::tr("Encrypt and Save"), QMessageBox::AcceptRole);
    box.setDefaultButton(now);
    box.exec();
    if (box.clickedButton() == now) encrypt(true, save_as);
    return true;
  }
  if (!persistence_.RevisionCurrent()) {
    QMessageBox box(QMessageBox::Warning, GTrC::tr("Changed Since Encrypting"),
                    GTrC::tr("The note changed after you last pressed Encrypt. "
                             "The revision that would be saved does not have "
                             "those changes."),
                    QMessageBox::Cancel, this);
    auto* update = box.addButton(GTrC::tr("Encrypt Changes and Save"),
                                 QMessageBox::AcceptRole);
    auto* older = box.addButton(GTrC::tr("Save the Older Revision"),
                                QMessageBox::DestructiveRole);
    box.setDefaultButton(update);
    box.exec();
    if (box.clickedButton() == update) {
      encrypt(true, save_as);
      return true;
    }
    if (box.clickedButton() != older) return true;
  }
  const auto destination = choose_destination(save_as, true);
  if (destination.isEmpty()) return true;
  set_status(GTrC::tr("Saving..."));
  persistence_.Save(destination);
  return true;
}

void NotePageView::wait_until(const std::function<bool()>& ready) {
  const auto deadline = Now() + kWaitLimitMs;
  while (!ready() && Now() < deadline) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
  }
}

auto NotePageView::ask_and_close() -> bool {
  if (state_ != State::kOPEN || persistence_.IsClean()) {
    wait_until([this] { return !persistence_.Busy(); });
    return true;
  }
  const auto answer = QMessageBox::warning(
      this, GTrC::tr("Unsaved Note"),
      GTrC::tr("This note has unsaved changes. Save them before closing?"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
      QMessageBox::Save);
  if (answer == QMessageBox::Cancel) return false;
  if (answer == QMessageBox::Save) {
    if (!SaveRequested(false)) return false;
    wait_until([this] {
      return !encrypting_ && !snapshot_in_progress_ && !persistence_.Busy();
    });
    return persistence_.IsClean();
  }
  // Discard: nothing queued runs, and what runs finishes.
  persistence_.CancelQueued();
  wait_until([this] { return !persistence_.Busy(); });
  return true;
}

auto NotePageView::CloseRequested() -> bool { return ask_and_close(); }

void NotePageView::FindRequested() {
  if (state_ != State::kOPEN) return;
  editor_.Post(R"({"type":"find.open"})");
}

void NotePageView::lock() {
  if (path_.isEmpty()) return;
  forget();
  if (QByteArray bytes; read_file(path_, &bytes) && sniff_file(bytes)) {
    show_locked();
  } else {
    show_unavailable(GTrC::tr("The note file could not be read again."));
  }
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
  show_banner(why, true, true);
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
}

void NotePageView::WipeContent() {
  wait_until([this] { return !persistence_.Busy() && jobs_in_flight_ == 0; });
  forget();
  Wipe(file_);
}

}  // namespace gf::notes
