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

#include "EMailPageView.h"

#include <GFSDKGpg.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QCompleter>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QSet>
#include <QShortcut>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <functional>

#include "EMailAccountStore.h"
#include "EMailBasicGpgOpera.h"
#include "EMailBodyView.h"
#include "EMailHeaderView.h"
#include "EMailHelper.h"
#include "EMailSecurityView.h"
#include "EMailSendDialog.h"
#include "EMailStructureView.h"
#include "EMailVerificationPayload.h"
#include "EMailViewLayout.h"
#include "EMailViewStyle.h"
#include "GFModuleCommonUtils.hpp"

namespace {

/// How far a locked surface moves from the page's own background towards its
/// muted foreground. Enough to read as a different material, not enough to
/// compete with the message sitting on it.
constexpr double kLockedTint = 0.10;
constexpr double kLockedFieldTint = 0.05;

/// How long a one-off note stays in the attachment heading before it goes back
/// to describing the list. Long enough to read a short sentence without having
/// gone looking for it, short enough not to become the label.
constexpr int kStatusNoteMs = 4000;

/// How long the splitter has to sit still before its width is written to
/// settings. A drag emits a position per pixel, and each of those would
/// otherwise be a settings write.
constexpr int kPersistDelayMs = 500;

/// The attachment list never gets less than this, however short the window:
/// below it the list is smaller than its own header plus a row.
constexpr int kMinAttachmentHeight = 120;

/// Names the completer so it can be found again on the line edit it belongs
/// to. Needed because a multi-value completer is not reachable through
/// QLineEdit::completer(): it is driven by hand and only parented to the edit.
constexpr auto kCompleterName = "EMailAddressCompleter";

/// Property holding the option list a completer was built from, so an
/// unchanged keyring rebuilds nothing.
constexpr auto kCompleterOptions = "EMailAddressCompleterOptions";

/**
 * @brief Offers @p options on @p edit as the user types.
 *
 * @param multi the field holds a ';'-separated list, so completion applies to
 *        the entry being typed rather than to the whole line. Without this the
 *        completer matches against "alice@x.org; bo" and never fires again
 *        after the first address.
 *
 * Case-insensitive and matching anywhere in the entry, not just at the start:
 * people remember a correspondent by name or by domain at least as often as by
 * the first letter of their local part.
 */
void InstallAddressCompleter(QLineEdit* edit, const QStringList& options,
                             bool multi) {
  if (edit == nullptr) return;

  // Nothing is rebuilt unless the keyring actually changed. This runs on every
  // focus, and tearing a completer down and building it back identical is both
  // wasted work and a chance to destroy one that is in use.
  const auto fingerprint = options.join('\n');
  if (edit->property(kCompleterOptions) == fingerprint) return;
  edit->setProperty(kCompleterOptions, fingerprint);

  // Two different ownerships have to be undone here, and getting either wrong
  // is a crash rather than a leak.
  //
  // setCompleter(nullptr) DELETES the completer outright when the line edit is
  // its parent, which ours is -- so the old pointer is dangling the instant
  // this returns and must not be touched again.
  if (edit->completer() != nullptr) edit->setCompleter(nullptr);

  // A hand-driven completer was never handed to setCompleter() at all, so the
  // call above knows nothing about it. It is found by name and deleted here.
  if (auto* previous = edit->findChild<QCompleter*>(
          kCompleterName, Qt::FindDirectChildrenOnly)) {
    delete previous;
  }

  if (options.isEmpty()) return;

  auto* completer = new QCompleter(options, edit);
  completer->setObjectName(kCompleterName);
  completer->setCaseSensitivity(Qt::CaseInsensitive);
  completer->setFilterMode(Qt::MatchContains);
  completer->setCompletionMode(QCompleter::PopupCompletion);

  if (!multi) {
    edit->setCompleter(completer);
    return;
  }

  // A multi-value field drives its completer by hand. QCompleter has no notion
  // of a separator, so the prefix is set from the entry under the cursor and
  // the chosen value is written back over just that entry.
  completer->setWidget(edit);

  QObject::connect(edit, &QLineEdit::textEdited, completer,
                   [edit, completer](const QString& text) {
                     const auto start = text.lastIndexOf(';') + 1;
                     const auto entry = text.mid(start).trimmed();
                     if (entry.isEmpty()) {
                       completer->popup()->hide();
                       return;
                     }
                     completer->setCompletionPrefix(entry);
                     if (completer->completionCount() == 0) {
                       completer->popup()->hide();
                       return;
                     }
                     completer->complete();
                   });

  QObject::connect(completer,
                   QOverload<const QString&>::of(&QCompleter::activated), edit,
                   [edit](const QString& chosen) {
                     const auto text = edit->text();
                     const auto start = text.lastIndexOf(';') + 1;
                     // The separator and the spacing around it are the field's,
                     // not the entry's, so everything up to and including the
                     // last ';' is kept exactly as the user left it.
                     auto head = text.left(start);
                     if (!head.isEmpty() && !head.endsWith(' ')) head += ' ';
                     edit->setText(head + chosen);
                     // Emitted by hand: setText() is a programmatic change and
                     // does not raise textEdited, which is what the Send state
                     // listens on.
                     emit edit->textChanged(edit->text());
                   });
}

// Shared with the other views in this module; see EMailViewStyle.h.
const auto& ThemeColor = EMailThemeColor;
const auto& MutedColor = EMailMutedColor;
const auto& AccentColor = EMailAccentColor;
const auto& HumanSize = EMailHumanSize;
const auto& Blend = EMailBlend;

/**
 * @brief A tool button sized by its full text, drawn with as much of it as
 * fits.
 *
 * The two have to be separated. If the size came from what is drawn, then
 * shortening the text would shrink the button, the layout would hand the
 * reclaimed space to whatever else is in the row, and widening the window
 * afterwards would never give it back -- the text would stay shortened for
 * the rest of the session.
 */
class ElidingToolButton : public QToolButton {
 public:
  using QToolButton::QToolButton;

  /// Sets the wording this button stands for, whatever it ends up drawing.
  void SetFullText(const QString& text) {
    if (full_ == text) return;
    full_ = text;
    // Always the whole thing, however little of it is on screen.
    setToolTip(text);
    updateGeometry();
    fit();
  }

  [[nodiscard]] auto sizeHint() const -> QSize override {
    auto hint = QToolButton::sizeHint();
    if (full_.isEmpty()) return hint;

    // Measured from the full wording rather than from what is currently
    // drawn, which is the whole point of this class.
    const QFontMetrics metrics(font());
    hint.setWidth(hint.width() + metrics.horizontalAdvance(full_) -
                  metrics.horizontalAdvance(text()));
    return hint;
  }

 protected:
  void resizeEvent(QResizeEvent* event) override {
    QToolButton::resizeEvent(event);
    fit();
  }

 private:
  void fit() {
    if (full_.isEmpty()) return;

    // The icon and the spacing around it are not the text's to use, so they
    // come off the width before anything is measured against it.
    const int reserved =
        iconSize().width() +
        (style()->pixelMetric(QStyle::PM_ToolBarItemSpacing, nullptr, this) *
         2);

    const QFontMetrics metrics(font());
    const int available = std::max(0, contentsRect().width() - reserved);
    const auto elided = metrics.elidedText(full_, Qt::ElideRight, available);

    // Guarded: setText re-runs the layout, which can resize this button again.
    if (elided != text()) setText(elided);
  }

  QString full_;
};

constexpr int kColName = 0;
constexpr int kColType = 1;
constexpr int kColSize = 2;
constexpr int kColSigned = 3;

auto GuessMimeType(const QString& path) -> QString {
  static const QMap<QString, QString> kByExtension = {
      {"txt", "text/plain"},
      {"html", "text/html"},
      {"pdf", "application/pdf"},
      {"png", "image/png"},
      {"jpg", "image/jpeg"},
      {"jpeg", "image/jpeg"},
      {"gif", "image/gif"},
      {"zip", "application/zip"},
      {"asc", "application/pgp-keys"},
      {"eml", "message/rfc822"}};
  const auto suffix = QFileInfo(path).suffix().toLower();
  return kByExtension.value(suffix, "application/octet-stream");
}

}  // namespace

EMailPageView::EMailPageView(QWidget* parent) : QWidget(parent) {
  build_ui();
  // Files can be dropped anywhere on the message to attach them, which is
  // what people try before they look for a button.
  setAcceptDrops(true);
}

void EMailPageView::build_ui() {
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(8, 6, 8, 6);
  outer->setSpacing(6);

  // The message, and nothing else. This view is already mounted inside the
  // host's document tabs, and a second row of tabs under the first reads as two
  // competing sets of destinations rather than as one message. What is KNOWN
  // about the message -- its structure, its signatures, its headers -- is
  // inspection rather than content, so it is reached through a window of its
  // own instead of taking a place beside the message itself.
  message_surface_ = build_message_surface();
  outer->addWidget(message_surface_, 1);

  persist_timer_ = new QTimer(this);
  persist_timer_->setSingleShot(true);
  persist_timer_->setInterval(kPersistDelayMs);
  connect(persist_timer_, &QTimer::timeout, this,
          &EMailPageView::persist_details_state);

  build_details_dialog();

  // Three keys rather than the five the tabs had: there are two ways to look at
  // the document, and one window that describes it.
  //
  // Only within this widget: several of these views can be open at once, and a
  // key pressed in one must not move another.
  const auto add_shortcut = [this](const QKeySequence& keys,
                                   const std::function<void()>& action) {
    auto* shortcut = new QShortcut(keys, this);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcut, &QShortcut::activated, this, action);
  };

  add_shortcut(QKeySequence(Qt::ALT | Qt::Key_1),
               [this]() { set_source_mode(false); });
  add_shortcut(QKeySequence(Qt::ALT | Qt::Key_2),
               [this]() { set_source_mode(true); });
  add_shortcut(QKeySequence(Qt::ALT | Qt::Key_3), [this]() { open_details(); });

  apply_colors();
}

void EMailPageView::build_details_dialog() {
  // Built with the view rather than on first use. The three inspection views
  // have to exist from the start -- everything that loads or clears a message
  // writes to them -- and a widget that exists without a layout to sit in
  // gets a default geometry and keeps the stale paint that goes with it. Going
  // straight into the tab widget costs one hidden dialog and avoids all of it.
  //
  // Parented to this view, so it closes with the tab it describes and never
  // outlives the message it is describing. Deliberately NOT modal: it says
  // what the message IS, and reading that while scrolling the message is the
  // whole point.
  details_dialog_ = new QDialog(this);
  details_dialog_->setWindowTitle(tr("Message Details"));

  auto* layout = new QVBoxLayout(details_dialog_);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(6);

  // A tab widget is the right shape HERE, where it was the wrong one inside
  // the page: in a window of its own there is no outer row of tabs for it to
  // compete with, and the three are genuinely peers.
  details_tabs_ = new QTabWidget(details_dialog_);
  details_tabs_->setDocumentMode(true);
  layout->addWidget(details_tabs_, 1);

  structure_view_ = new EMailStructureView(details_tabs_);
  security_view_ = new EMailSecurityView(details_tabs_);
  header_view_ = new EMailHeaderView(details_tabs_);

  // Most-asked first. The old order was the order the views happened to be
  // constructed in.
  details_tabs_->addTab(security_view_, tr("Security"));
  details_tabs_->addTab(structure_view_, tr("Structure"));
  details_tabs_->addTab(header_view_, tr("Headers"));

  // The security view can ask for the two things it used to only describe.
  // Asking is all it does: the answer arrives at ApplyVerificationResult()
  // and repaints everything, whichever of the ways to ask was used.
  connect(security_view_, &EMailSecurityView::SignalVerifyAgainRequested, this,
          [this]() { request_verification(); });
  connect(security_view_, &EMailSecurityView::SignalImportMessageKeysRequested,
          this, &EMailPageView::import_message_keys);

  auto* buttons =
      new QDialogButtonBox(QDialogButtonBox::Close, details_dialog_);
  connect(buttons, &QDialogButtonBox::rejected, details_dialog_,
          &QDialog::reject);
  layout->addWidget(buttons);

  connect(details_tabs_, &QTabWidget::currentChanged, this, [this](int index) {
    auto* page = details_tabs_->widget(index);

    // Focus follows the tab. Without this it stays on whatever was clicked, so
    // the filter box and the trees on the tab just opened are only reachable
    // by tabbing across the whole window first.
    if (page != nullptr) page->setFocus(Qt::OtherFocusReason);

    sync_details();
    schedule_persist();
  });

  connect(details_dialog_, &QDialog::finished, this,
          [this](int) { persist_details_state(); });

  // Restored before the window is ever shown, so it opens where it was left
  // rather than jumping into place afterwards.
  auto* settings = EMailViewSettings();
  if (settings == nullptr) {
    details_dialog_->resize(kEMailDetailsDefaultWidth,
                            kEMailDetailsDefaultHeight);
    return;
  }

  const auto size =
      EMailClampDetailsSize(settings->value(kEMailDetailsWidthKey, 0).toInt(),
                            settings->value(kEMailDetailsHeightKey, 0).toInt());
  details_dialog_->resize(size.first, size.second);
  details_tabs_->setCurrentIndex(EMailClampDetailsTab(
      settings->value(kEMailDetailsTabKey, 0).toInt(), details_tabs_->count()));
}

void EMailPageView::apply_colors() {
  // The one place any colour in this view is decided. Called at the end of
  // build_ui() and again on every theme change, which is what keeps a single
  // definition from having to exist twice -- and what makes the tab follow a
  // live light/dark switch at all. Nothing here may touch a font: the font
  // scaling done at build time is not idempotent.
  for (auto* caption : captions_) {
    if (caption != nullptr) EMailMakeMuted(caption);
  }

  if (envelope_rule_ != nullptr) EMailPaintRule(envelope_rule_);
  if (action_separator_ != nullptr) EMailPaintRule(action_separator_);

  if (locked_banner_ != nullptr) {
    EMailTintBanner(locked_banner_, MutedColor(this), kLockedTint);
  }
  if (locked_notice_ != nullptr) {
    EMailSetLabelColor(locked_notice_, MutedColor(this));
  }
  if (locked_recipients_ != nullptr) {
    EMailSetLabelColor(locked_recipients_, MutedColor(this));
  }
  if (attachment_heading_ != nullptr) {
    EMailSetLabelColor(attachment_heading_, MutedColor(this));
  }
  if (unsigned_notice_ != nullptr) {
    EMailSetLabelColor(unsigned_notice_, ThemeColor(this, &GFUIWarningColor));
  }

  if (attachment_list_ != nullptr) {
    EMailPaintTreeHeader(attachment_list_);
    EMailRepaintTree(attachment_list_);
  }

  // These decide a colour from state rather than holding a fixed one, so they
  // are asked to decide again rather than being recoloured from here.
  if (security_button_ != nullptr) paint_security_button();
  if (from_edit_ != nullptr) {
    style_as_locked(content_lock() != EMailLockReason::kNONE);
  }
}

void EMailPageView::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);
  if (EMailIsRestyle(event)) apply_colors();
}

auto EMailPageView::build_message_surface() -> QWidget* {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 6, 0, 0);
  layout->setSpacing(6);

  auto* form = new QFormLayout();
  form->setLabelAlignment(Qt::AlignRight);
  form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
  // Three points of leading ran the rows together into one block of boxes; a
  // wider gutter stops the captions touching the fields they name.
  form->setHorizontalSpacing(10);
  form->setVerticalSpacing(4);
  form->setContentsMargins(0, 0, 0, 0);

  from_edit_ = new QLineEdit(this);
  to_edit_ = new QLineEdit(this);
  cc_edit_ = new QLineEdit(this);
  bcc_edit_ = new QLineEdit(this);
  subject_edit_ = new QLineEdit(this);

  // Send follows what is actually in the fields, so the reason it is off is
  // never one edit out of date.
  for (auto* edit :
       {from_edit_, to_edit_, cc_edit_, bcc_edit_, subject_edit_}) {
    connect(edit, &QLineEdit::textChanged, this,
            [this]() { refresh_send_state(); });
  }

  // Rebuilt on entry rather than once at construction: a key imported while
  // this tab was open should be offered the next time an address is typed,
  // and the keyring is already in memory so the rebuild costs nothing.
  for (auto* edit : {from_edit_, to_edit_, cc_edit_, bcc_edit_}) {
    edit->installEventFilter(this);
  }

  // Separator is shown rather than assumed: the old dialog explained it in a
  // tips label the user had to find first.
  const auto multi = tr("Separate several addresses with \";\"");
  to_edit_->setPlaceholderText(multi);
  cc_edit_->setToolTip(multi);
  // Said plainly, because the behaviour is deliberate and would otherwise
  // look like data loss: what is typed here never becomes a header.
  bcc_edit_->setToolTip(
      tr("%1.\n\nBlind recipients are never written into the message: a Bcc "
         "header would tell every recipient who was blind-copied. They are "
         "used to choose encryption recipients and are not saved with the "
         "file.")
          .arg(multi));

  // Captions are quieter than the values beside them, so the eye lands on the
  // addresses rather than on the word "From".
  const auto caption = [this](const QString& text) {
    auto* label = new QLabel(text, this);
    // Quieter AND smaller than the value beside it, which is the convention
    // everywhere else in this module. The colour is not set here:
    // apply_colors() owns every colour in this view, so a theme change repaints
    // these too.
    EMailMakeSecondary(label);
    captions_.append(label);
    return label;
  };

  form->addRow(caption(tr("From:")), from_edit_);

  // "To" carries the toggle for the two rows most messages never use, so the
  // common case is four rows rather than six. The toggle only shows and hides:
  // it used to clear the fields as well, which threw away addresses whenever
  // the row was collapsed by accident.
  auto* to_row = new QWidget(this);
  auto* to_row_layout = new QHBoxLayout(to_row);
  to_row_layout->setContentsMargins(0, 0, 0, 0);
  to_row_layout->setSpacing(6);
  to_row_layout->addWidget(to_edit_, 1);

  cc_bcc_toggle_ = new QToolButton(this);
  cc_bcc_toggle_->setText(tr("Cc/Bcc"));
  cc_bcc_toggle_->setCheckable(true);
  cc_bcc_toggle_->setAutoRaise(true);
  cc_bcc_toggle_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
  cc_bcc_toggle_->setToolTip(
      tr("Show carbon copy and blind carbon copy (%1)")
          .arg(cc_bcc_toggle_->shortcut().toString(QKeySequence::NativeText)));
  to_row_layout->addWidget(cc_bcc_toggle_);

  form->addRow(caption(tr("To:")), to_row);
  form->addRow(caption(tr("Cc:")), cc_edit_);
  form->addRow(caption(tr("Bcc:")), bcc_edit_);

  header_form_ = form;
  connect(cc_bcc_toggle_, &QToolButton::toggled, this,
          [this](bool on) { set_cc_bcc_visible(on); });
  set_cc_bcc_visible(false);

  // The subject is the message's title, so it carries the weight of one.
  auto subject_font = subject_edit_->font();
  subject_font.setBold(true);
  subject_edit_->setFont(subject_font);
  form->addRow(caption(tr("Subject:")), subject_edit_);

  // Wrapped rather than added as a bare layout, so the raw source mode can
  // take the whole envelope off screen in one call. What the form says
  // describes the message; the raw mode shows octets it does not necessarily
  // describe any more.
  envelope_box_ = new QWidget(page);
  envelope_box_->setLayout(form);
  layout->addWidget(envelope_box_);

  // A hairline between the envelope and the message, so the two stop running
  // together into one undifferentiated column of boxes. Given room on both
  // sides: a rule pressed against what it divides reads as an underline.
  envelope_rule_ = EMailRule(this);
  layout->addSpacing(2);
  layout->addWidget(envelope_rule_);
  layout->addSpacing(2);

  auto* body_row = new QHBoxLayout();
  body_row->setContentsMargins(0, 0, 0, 0);
  body_row->setSpacing(2);

  // Message / Raw Source. Flat and adjacent, the same shape the host uses for
  // the pair when a view does NOT adopt its editor -- two views of one
  // document, rather than two buttons that do something.
  //
  // It sits in the action row rather than in a strip of its own because the
  // row stays on screen in both modes: a switcher that lives above everything
  // is the second tab bar this redesign exists to remove.
  view_switcher_ = new QWidget(this);
  auto* switcher_row = new QHBoxLayout(view_switcher_);
  switcher_row->setContentsMargins(0, 0, 0, 0);
  switcher_row->setSpacing(2);

  const auto make_mode = [this, switcher_row](const QString& text,
                                              const QString& tip, bool checked,
                                              const QKeySequence& shortcut) {
    auto* button = new QToolButton(view_switcher_);
    button->setText(text);
    button->setCheckable(true);
    button->setChecked(checked);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::PointingHandCursor);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setToolTip(
        tr("%1 (%2)").arg(tip, shortcut.toString(QKeySequence::NativeText)));
    // Held at its natural width. Which view you are looking at is not
    // something to work out from "Me...ge"; when the row runs out of room it is
    // the message ACTIONS that give way, in refresh_action_density().
    button->setMinimumWidth(button->sizeHint().width());
    switcher_row->addWidget(button);
    return button;
  };

  message_mode_button_ = make_mode(tr("Message"), tr("Show the message"), true,
                                   QKeySequence(Qt::ALT | Qt::Key_1));
  source_mode_button_ = make_mode(tr("Raw Source"), tr("Show the raw document"),
                                  false, QKeySequence(Qt::ALT | Qt::Key_2));

  auto* mode_group = new QButtonGroup(view_switcher_);
  mode_group->setExclusive(true);
  mode_group->addButton(message_mode_button_);
  mode_group->addButton(source_mode_button_);

  connect(message_mode_button_, &QToolButton::clicked, this,
          [this]() { set_source_mode(false); });
  connect(source_mode_button_, &QToolButton::clicked, this,
          [this]() { set_source_mode(true); });

  // A host that kept its own switcher never hands over an editor, and then
  // there is no second mode to offer.
  view_switcher_->setVisible(false);
  body_row->addWidget(view_switcher_);
  body_row->addSpacing(4);

  // Message-level actions. Each produces a NEW document in a new tab; none of
  // them writes to this one.
  //
  // Icons come from the desktop theme where there is one, because these three
  // actions have long-settled, instantly recognisable icons that no bundled
  // substitute would improve on. The bundled files are the fallback for the
  // platforms that have no icon theme at all.
  const auto make_action =
      [this, body_row](const QString& theme_icon, const QString& fallback_icon,
                       const QString& text, const QString& tip,
                       const QKeySequence& shortcut) {
        auto* button = new QToolButton(this);
        button->setIcon(QIcon::fromTheme(theme_icon, QIcon(fallback_icon)));
        button->setText(text);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setAutoRaise(true);
        button->setVisible(false);

        // The key and the sentence advertising it are set together, so they
        // cannot drift apart. A shortcut nothing mentions is a shortcut only
        // the person who wrote it knows about.
        button->setShortcut(shortcut);
        button->setToolTip(
            shortcut.isEmpty()
                ? tip
                : tr("%1 (%2)").arg(
                      tip, shortcut.toString(QKeySequence::NativeText)));

        body_row->addWidget(button);
        return button;
      };

  reply_button_ = make_action(
      QStringLiteral("mail-reply-sender"), ":/icons/reply.png", tr("Reply"),
      tr("Write a reply to the sender."), QKeySequence(Qt::CTRL | Qt::Key_R));
  reply_all_button_ = make_action(
      QStringLiteral("mail-reply-all"), ":/icons/reply-all.png",
      tr("Reply All"),
      tr("Write a reply to the sender and everyone else who was addressed. "
         "Blind recipients are not included."),
      QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
  forward_button_ = make_action(
      QStringLiteral("mail-forward"), ":/icons/redo.png", tr("Forward"),
      tr("Pass this message on, with its attachments."),
      QKeySequence(Qt::CTRL | Qt::Key_L));

  connect(reply_button_, &QToolButton::clicked, this, [this]() {
    slot_derive_message(static_cast<int>(EMailReplyMode::kREPLY));
  });
  connect(reply_all_button_, &QToolButton::clicked, this, [this]() {
    slot_derive_message(static_cast<int>(EMailReplyMode::kREPLY_ALL));
  });
  connect(forward_button_, &QToolButton::clicked, this, [this]() {
    slot_derive_message(static_cast<int>(EMailReplyMode::kFORWARD));
  });

  send_button_ = make_action(QStringLiteral("mail-send"),
                             ":/icons/export-email.png", tr("Send..."),
                             tr("Send this message through a configured "
                                "mail account."),
                             QKeySequence(Qt::CTRL | Qt::Key_Return));
  connect(send_button_, &QToolButton::clicked, this,
          &EMailPageView::slot_send_message);

  // Locking the document is a different kind of act from deriving a new
  // message out of it, so it is set apart rather than lined up with them.
  auto* action_separator = EMailRule(this, Qt::Vertical);
  action_separator->setVisible(false);
  body_row->addSpacing(4);
  body_row->addWidget(action_separator);
  body_row->addSpacing(4);
  action_separator_ = action_separator;

  forensic_toggle_ = make_action(
      QStringLiteral("object-locked"), ":/icons/read-only.png", tr("Read-only"),
      tr("Lock this message so it cannot be edited or rewritten. Reply and "
         "Forward still work and produce new messages."),
      QKeySequence());
  forensic_toggle_->setCheckable(true);
  connect(forensic_toggle_, &QToolButton::toggled, this,
          [this](bool on) { SetForensicMode(on); });

  body_row->addStretch();

  // Opens the pane beside the message. Next to the security button because
  // the two are the same subject: what this message is, and the detail behind
  // that answer.
  details_button_ = new QToolButton(this);
  details_button_->setIcon(QIcon::fromTheme(
      QStringLiteral("dialog-information"), QIcon(":/icons/detail.png")));
  // The ellipsis is doing its usual job: this opens a window rather than
  // changing anything here.
  details_button_->setText(tr("Details..."));
  details_button_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  details_button_->setAutoRaise(true);
  details_button_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
  details_button_->setToolTip(tr("%1 (%2)").arg(
      tr("Show what is known about this message: its signatures, its "
         "structure and its headers."),
      details_button_->shortcut().toString(QKeySequence::NativeText)));
  details_button_->setMinimumWidth(details_button_->sizeHint().width());
  connect(details_button_, &QToolButton::clicked, this,
          [this]() { open_details(); });
  body_row->addWidget(details_button_);
  body_row->addSpacing(4);

  // What the message IS, and which action applies, on the same row as the
  // actions themselves rather than in a band of its own above the body. It is
  // the answer to "what do I do with this", so it belongs where the doing is.
  security_button_ = new ElidingToolButton(this);
  security_button_->setVisible(false);
  security_button_->setAutoRaise(true);
  security_button_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  // The whole control opens the menu: what a message IS and what can be done
  // about it are the same question, so there is no separate arrow to find.
  security_button_->setPopupMode(QToolButton::InstantPopup);
  security_menu_ = new QMenu(security_button_);
  security_button_->setMenu(security_menu_);
  {
    auto font = security_button_->font();
    font.setPointSizeF(font.pointSizeF() * 0.92);
    security_button_->setFont(font);
  }
  // Sized to what it says and no wider. Maximum rather than Preferred so it
  // never grows into the empty middle of the row -- stretched across it, a
  // button with a menu indicator reads as a drop-down list of the whole row's
  // worth of nothing -- while still being allowed to SHRINK when the window
  // gets narrow, which is what keeps it from pushing the other actions off.
  // The text then gives way instead, with the full wording in the tooltip.
  security_button_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
  body_row->addWidget(security_button_);

  layout->addLayout(body_row);

  body_edit_ = new QPlainTextEdit(this);
  body_edit_->setPlaceholderText(tr("Write your message here."));

  body_view_ = new EMailBodyView(this);

  // The body view decides for itself when it has given up on rendering and is
  // showing source instead, so it is the thing that gets asked. Answering the
  // signal rather than re-deriving the same condition here keeps the sentence
  // the user reads tied to what was actually done to the body.
  connect(body_view_, &EMailBodyView::SignalHtmlShownAsSource, this,
          [this]() { body_is_html_ = true; });

  // The reason the message is locked is not a footnote: it is the first thing
  // to read when the fields will not take input. A tinted strip with the state
  // icon beside it says so at a glance, where small grey text under the body
  // did not.
  locked_banner_ = EMailTintedBanner(this, MutedColor(this), kLockedTint);
  locked_banner_->setVisible(false);

  auto* banner_row = new QHBoxLayout(locked_banner_);
  banner_row->setContentsMargins(8, 6, 8, 6);
  banner_row->setSpacing(8);

  locked_banner_icon_ = new QLabel(locked_banner_);
  locked_banner_icon_->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
  banner_row->addWidget(locked_banner_icon_);

  locked_notice_ = new QLabel(locked_banner_);
  locked_notice_->setWordWrap(true);
  {
    auto font = locked_notice_->font();
    font.setPointSizeF(font.pointSizeF() * 0.92);
    locked_notice_->setFont(font);
  }
  // Colour belongs to apply_colors(), like every other colour in this view.
  banner_row->addWidget(locked_notice_, 1);

  layout->addWidget(locked_banner_);

  locked_panel_ = build_locked_panel();

  body_stack_ = new QStackedWidget(this);
  body_stack_->addWidget(body_edit_);
  body_stack_->addWidget(body_view_);
  body_stack_->addWidget(locked_panel_);
  layout->addWidget(body_stack_, 1);

  // Everything about attachments, wrapped for the same reason as the envelope:
  // the raw source mode hides it whole.
  attachment_box_ = new QWidget(page);
  auto* attachment_column = new QVBoxLayout(attachment_box_);
  attachment_column->setContentsMargins(0, 0, 0, 0);
  attachment_column->setSpacing(6);
  layout->addWidget(attachment_box_);

  attachment_heading_ = new QLabel(this);
  {
    auto font = attachment_heading_->font();
    font.setPointSizeF(font.pointSizeF() * 0.92);
    attachment_heading_->setFont(font);
  }
  attachment_heading_->setVisible(false);
  // Not welded to the body above it: this is a new group, not a caption on the
  // editor.
  attachment_column->addSpacing(2);
  attachment_column->addWidget(attachment_heading_);

  attachment_list_ = new QTreeWidget(this);
  attachment_list_->setRootIsDecorated(false);
  attachment_list_->setAlternatingRowColors(true);
  attachment_list_->setUniformRowHeights(true);
  attachment_list_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  attachment_list_->setHeaderLabels(
      {tr("Name"), tr("Type"), tr("Size"), tr("Signed")});
  attachment_list_->header()->setStretchLastSection(false);
  attachment_list_->header()->setSectionResizeMode(kColName,
                                                   QHeaderView::Stretch);
  EMailPolishTree(attachment_list_);
  attachment_list_->setVisible(false);
  attachment_column->addWidget(attachment_list_);

  unsigned_notice_ = new QLabel(this);
  unsigned_notice_->setWordWrap(true);
  unsigned_notice_->setVisible(false);
  {
    auto font = unsigned_notice_->font();
    font.setPointSizeF(font.pointSizeF() * 0.92);
    unsigned_notice_->setFont(font);
  }
  attachment_column->addWidget(unsigned_notice_);

  // The same shape as the actions above: flat, icon beside text. These used
  // to be raised push buttons, which gave the attachment list a heavier
  // footer than the message itself.
  auto* buttons = new QHBoxLayout();
  buttons->setContentsMargins(0, 0, 0, 0);
  buttons->setSpacing(2);

  const auto make_attachment_action =
      [this](const QString& theme_icon, const QString& fallback_icon,
             const QString& text, const QString& tip,
             const QKeySequence& shortcut = {}) {
        auto* button = new QToolButton(this);
        button->setIcon(QIcon::fromTheme(theme_icon, QIcon(fallback_icon)));
        button->setText(text);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setAutoRaise(true);

        button->setShortcut(shortcut);
        button->setToolTip(
            shortcut.isEmpty()
                ? tip
                : tr("%1 (%2)").arg(
                      tip, shortcut.toString(QKeySequence::NativeText)));
        return button;
      };

  add_button_ = make_attachment_action(
      QStringLiteral("mail-attachment"), ":/icons/attachment.png",
      tr("Attach File..."),
      tr("Add one or more files to this message. Files can also be dropped "
         "onto the message."),
      QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
  // Removing takes a part out of the message being composed; it deletes
  // nothing on disk, and the wording and icon both stay away from suggesting
  // it does.
  remove_button_ = make_attachment_action(
      QStringLiteral("list-remove"), ":/icons/remove.png", tr("Remove"),
      tr("Take the selected attachments out of this message."));
  save_button_ = make_attachment_action(
      QStringLiteral("document-save"), ":/icons/filesave.png", tr("Save..."),
      tr("Write the selected attachments to a folder."));
  save_all_button_ = make_attachment_action(
      QStringLiteral("document-save-all"), ":/icons/save-all.png",
      tr("Save All..."), tr("Write every attachment to a folder."));

  buttons->addWidget(add_button_);
  buttons->addWidget(remove_button_);
  buttons->addStretch();
  buttons->addWidget(save_button_);
  buttons->addWidget(save_all_button_);
  attachment_column->addLayout(buttons);

  connect(add_button_, &QToolButton::clicked, this,
          &EMailPageView::slot_add_attachment);
  connect(remove_button_, &QToolButton::clicked, this,
          &EMailPageView::slot_remove_attachment);
  connect(save_button_, &QToolButton::clicked, this,
          &EMailPageView::slot_save_attachment);
  connect(save_all_button_, &QToolButton::clicked, this,
          &EMailPageView::slot_save_all_attachments);
  connect(attachment_list_, &QTreeWidget::itemSelectionChanged, this,
          &EMailPageView::slot_selection_changed);

  // Everything the four buttons below do, on the row the user is pointing at.
  // The buttons stay: this is the second way to reach them, for the people who
  // look for it on the thing itself.
  attachment_list_->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(attachment_list_, &QTreeWidget::customContextMenuRequested, this,
          &EMailPageView::slot_attachment_menu);

  // What a double-click on a file has meant everywhere else for thirty years.
  connect(attachment_list_, &QTreeWidget::itemDoubleClicked, this,
          [this](QTreeWidgetItem* item, int) { open_attachment(item); });

  // The keys a list of files is expected to answer to. Delete goes through
  // slot_remove_attachment(), so it asks the same question the button does
  // rather than being a quieter way to destroy the same bytes.
  auto* del = new QShortcut(QKeySequence::Delete, attachment_list_);
  del->setContext(Qt::WidgetShortcut);
  connect(del, &QShortcut::activated, this,
          &EMailPageView::slot_remove_attachment);

  connect(attachment_list_, &QTreeWidget::itemActivated, this,
          [this](QTreeWidgetItem* item, int) { open_attachment(item); });

  for (auto* edit :
       {from_edit_, to_edit_, cc_edit_, bcc_edit_, subject_edit_}) {
    connect(edit, &QLineEdit::textEdited, this, [this]() { mark_dirty(); });
  }
  connect(body_edit_, &QPlainTextEdit::textChanged, this, [this]() {
    // textChanged also fires while loading, which must not count as an edit.
    if (!loading_) mark_dirty();
  });

  slot_selection_changed();
  return page;
}

auto EMailPageView::details_visible() const -> bool {
  return details_dialog_ != nullptr && details_dialog_->isVisible();
}

void EMailPageView::open_details() {
  // show() on a window that is already open does nothing useful -- it may be
  // behind the main window, or minimised -- so it is always raised and given
  // the keyboard as well. Asking for it twice means "bring it to me".
  details_dialog_->show();
  details_dialog_->raise();
  details_dialog_->activateWindow();

  sync_details();
}

void EMailPageView::open_details(const QString& section) {
  if (security_view_ == nullptr) return;

  details_tabs_->setCurrentWidget(security_view_);

  // Opened and brought up to date BEFORE the reveal: the refresh rebuilds the
  // tree, and a rebuild after the scroll throws away the row scrolled to.
  open_details();
  security_view_->RevealSection(section);
}

void EMailPageView::sync_details() {
  if (!details_visible()) return;

  // Whatever was typed into the message is part of it now, so the tab being
  // looked at has to describe that rather than what was loaded.
  sync_inspection();

  if (!EMailNeedsSecurityRefresh(
          true, details_tabs_->currentWidget() == security_view_)) {
    return;
  }

  // Shows what is known; it does not go and find out. Opening a tab used to
  // verify the message, which meant looking at it started a crypto operation
  // -- a wait dialog and a status report the user never asked for -- and gave
  // this view a verification of its own, disagreeing with the host's. Until
  // the user asks, the honest answer is that nobody has checked.
  refresh_security();
}

void EMailPageView::schedule_persist() {
  if (persist_timer_ != nullptr) persist_timer_->start();
}

void EMailPageView::persist_details_state() {
  auto* settings = EMailViewSettings();
  if (settings == nullptr || details_dialog_ == nullptr) return;

  settings->setValue(kEMailDetailsWidthKey, details_dialog_->width());
  settings->setValue(kEMailDetailsHeightKey, details_dialog_->height());
  settings->setValue(kEMailDetailsTabKey, details_tabs_->currentIndex());
  settings->sync();
}

void EMailPageView::set_source_mode(bool on) {
  // Nothing was adopted, so there is only one mode and this cannot be entered.
  if (raw_tab_ == nullptr || source_mode_ == on) {
    if (message_mode_button_ != nullptr) {
      message_mode_button_->setChecked(!source_mode_);
      source_mode_button_->setChecked(source_mode_);
    }
    return;
  }

  if (on) {
    // The host answers by writing pending edits into the document. It has to
    // happen before the editor is on screen, or the octets the user reads as
    // the raw source are behind what this view already has.
    emit SignalSourceViewRequested();
  }

  source_mode_ = on;
  message_mode_button_->setChecked(!on);
  source_mode_button_->setChecked(on);

  // The envelope and the attachment list describe the parsed message. Beside
  // the raw octets they would be describing something the user is no longer
  // looking at, so they go away and the document gets the whole area.
  envelope_box_->setVisible(!on);
  envelope_rule_->setVisible(!on);
  attachment_box_->setVisible(!on);

  if (on) {
    body_stack_->setCurrentWidget(raw_tab_);
    refresh_raw_lock_ui();
    source_view_->setFocus(Qt::OtherFocusReason);
    return;
  }

  // Back to whichever of the editor, the rendered body or the locked panel the
  // message actually calls for.
  refresh_body_view();
}

void EMailPageView::refresh_action_density() {
  if (message_surface_ == nullptr || reply_button_ == nullptr) return;

  const auto compact = EMailShouldCompactActions(width(), actions_compact_);
  if (compact == actions_compact_) return;
  actions_compact_ = compact;

  const auto style =
      compact ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon;
  for (auto* button : {reply_button_, reply_all_button_, forward_button_,
                       send_button_, forensic_toggle_}) {
    if (button != nullptr) button->setToolButtonStyle(style);
  }
}

void EMailPageView::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  refresh_action_density();
}

void EMailPageView::set_cc_bcc_visible(bool visible) {
  if (header_form_ == nullptr) return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  header_form_->setRowVisible(cc_edit_, visible);
  header_form_->setRowVisible(bcc_edit_, visible);
#else
  // Older Qt has no setRowVisible; hiding both cells collapses the row.
  const auto set_row_visible = [this, visible](QWidget* field) {
    if (auto* label = header_form_->labelForField(field)) {
      label->setVisible(visible);
    }
    field->setVisible(visible);
  };
  set_row_visible(cc_edit_);
  set_row_visible(bcc_edit_);
#endif

  if (cc_bcc_toggle_ != nullptr && cc_bcc_toggle_->isChecked() != visible) {
    QSignalBlocker blocker(cc_bcc_toggle_);
    cc_bcc_toggle_->setChecked(visible);
  }
}

auto EMailPageView::content_lock() const -> EMailLockReason {
  if (forensic_) return EMailLockReason::kFORENSIC;

  // A signature covers exact octets. Editing the content it covers does not
  // produce a message with a broken signature -- it produces one that is
  // indistinguishable from a forgery to whoever verifies it next. So it is not
  // offered at all; the signature comes off first, deliberately, and then the
  // message is an ordinary one that can be edited.
  //
  // Encrypted content cannot be edited for the simpler reason that what is
  // here is ciphertext, and typing into it would destroy the message.
  if (security_state_ != EMailSecurityState::kPLAIN) {
    return EMailLockReason::kPROTECTED;
  }

  return EMailLockReason::kNONE;
}

void EMailPageView::apply_content_lock() {
  const auto reason = content_lock();
  const bool locked = reason != EMailLockReason::kNONE;

  // Editing is disabled at the widget level as well as at the dirty flag:
  // a read-only field cannot produce an edit to refuse in the first place,
  // and the user can see the document is locked rather than discovering it.
  for (auto* edit :
       {from_edit_, to_edit_, cc_edit_, bcc_edit_, subject_edit_}) {
    edit->setReadOnly(locked);
  }
  body_edit_->setReadOnly(locked);

  // Attaching and removing change the message; saving a part out does not.
  add_button_->setEnabled(!locked);
  remove_button_->setEnabled(!locked);

  // The raw editor writes straight into the document, so leaving it writable
  // would be a hole right through the lock. Forensic mode locks it outright;
  // otherwise it follows the unlock the user asked for on the Raw Source tab.
  if (source_view_ != nullptr) {
    source_view_->setProperty("readOnly", forensic_ || !raw_unlocked_);
  }

  // The icon travels with the wording: the same reason, said twice, so the
  // state is recognisable before the sentence is read.
  QString notice;
  QString icon;
  switch (reason) {
    case EMailLockReason::kNONE:
      break;
    case EMailLockReason::kFORENSIC:
      icon = ":/icons/read-only.png";
      notice =
          tr("This message is open for inspection only. It cannot be edited or "
             "rewritten; Reply and Forward still work, and produce new "
             "messages.");
      break;
    case EMailLockReason::kPROTECTED:
      switch (security_state_) {
        case EMailSecurityState::kENCRYPTED:
          icon = ":/icons/lock.png";
          notice =
              tr("This message is encrypted. Decrypt it before editing it.");
          break;
        case EMailSecurityState::kSIGNED_ENCRYPTED:
          icon = ":/icons/lock.png";
          notice = tr(
              "This message is encrypted and signed. Decrypt it, then remove "
              "the signature, before editing it.");
          break;
        default:
          icon = ":/icons/signature.png";
          notice =
              tr("This message is signed. Remove the signature before editing "
                 "it. An edit under a signature reads as a forgery.");
          break;
      }
      break;
  }

  setToolTip(notice);

  // An HTML body is shown as its source, deliberately, and the user is owed
  // that sentence -- without it the body looks like a rendering that failed.
  // It shares the banner rather than getting one of its own: two tinted strips
  // stacked above the message would cost more room than either is worth. The
  // lock wins, because it is the reason the message will not take input, and
  // the HTML note falls back to the body's tooltip.
  const auto html_notice =
      body_is_html_
          ? tr("This message was written in HTML. It is shown as its source "
               "rather than rendered: nothing here loads images or follows "
               "links.")
          : QString();

  if (!locked && body_is_html_) {
    icon = ":/icons/code.png";
    notice = html_notice;
  }

  // Said on the body itself in the case the banner could not take: a locked
  // HTML message is both things at once, and only one of them fits up there.
  if (body_view_ != nullptr) {
    body_view_->setToolTip(locked ? html_notice : QString());
  }

  if (locked_notice_ != nullptr) locked_notice_->setText(notice);
  if (locked_banner_icon_ != nullptr && !icon.isEmpty()) {
    locked_banner_icon_->setPixmap(QIcon(icon).pixmap(16, 16));
  }
  if (locked_banner_ != nullptr) {
    locked_banner_->setVisible(!notice.isEmpty());
  }

  style_as_locked(locked);
  refresh_raw_lock_ui();
}

void EMailPageView::style_as_locked(bool locked) {
  // Unset rather than recoloured when the lock lifts: a default-constructed
  // palette resolves nothing of its own, so the widget goes back to following
  // the user's theme instead of to whatever this function last decided the
  // theme was.
  const auto field_palette = [this, locked]() {
    if (!locked) return QPalette();

    QPalette p;
    p.setColor(QPalette::Base, Blend(palette().color(QPalette::Window),
                                     MutedColor(this), kLockedFieldTint));
    return p;
  }();

  // Losing the frame is what does the work. A framed box is an invitation to
  // type in it, and the invitation is the confusing part -- without it the
  // envelope reads as the message's addresses rather than as a form.
  for (auto* edit :
       {from_edit_, to_edit_, cc_edit_, bcc_edit_, subject_edit_}) {
    edit->setFrame(!locked);
    edit->setPalette(field_palette);
  }

  body_edit_->setFrameShape(locked ? QFrame::NoFrame : QFrame::StyledPanel);
  body_edit_->setPalette(field_palette);

  // The text itself keeps its normal colour. It is the thing the user came to
  // read, and greying it out to signal "read-only" would make the message
  // harder to read in order to say that it can be read.

  // Shown exactly when the body is empty, which is when a promise to take
  // writing is least true.
  body_edit_->setPlaceholderText(locked ? QString()
                                        : tr("Write your message here."));
}

void EMailPageView::SetForensicMode(bool on) {
  forensic_ = on;

  // Forensic mode revokes an unlock already in force. Leaving it does NOT
  // unlock: the raw editor is locked by default either way, and has to be
  // asked for deliberately each time.
  if (on) {
    raw_unlocked_ = false;
    if (raw_unlock_button_ != nullptr) {
      const QSignalBlocker blocker(raw_unlock_button_);
      raw_unlock_button_->setChecked(false);
    }
  }

  if (on) dirty_ = false;

  apply_content_lock();
}

void EMailPageView::mark_dirty() {
  if (loading_) return;
  // In forensic mode the document is never modified, so it can never become
  // dirty -- and since SaveToSource() is only ever called for a dirty view,
  // making this unreachable is what makes reserialization impossible rather
  // than merely unused.
  if (forensic_) return;
  const bool was_clean = !dirty_;
  dirty_ = true;
  // The inspection tabs are built from the document, and the document just
  // changed. They are not rebuilt here: that waits until one is actually
  // looked at, so typing does not pay for a full reserialization per keystroke.
  inspection_stale_ = true;
  // Once per load is enough: the host only needs to learn that the tab became
  // modified, and it stays modified until it is saved.
  if (was_clean) emit SignalContentModified();
}

void EMailPageView::LoadFromSource(const QByteArray& source) {
  loading_ = true;

  message_ = EMailMetaData{};

  // The host round-trips our own bytes back through here: FlushPrimaryView()
  // takes SaveToSource()'s output, puts it in the document, and a later
  // ReloadPrimaryView() hands the very same octets back. That is not a new
  // document, and re-deriving from it would launder a draft into a received
  // message -- which is exactly what re-enabled Reply on a draft and cost the
  // message its Message-ID, by making the send path preserve bytes we wrote
  // ourselves.
  //
  // The comparison is sound because the round trip is byte-exact: the host
  // remembers a CRLF document as CRLF and restores it in DocumentBytes().
  const bool same_document = !last_source_.isEmpty() && source == last_source_;

  vmime::shared_ptr<vmime::message> parsed;
  const bool is_eml = CheckIfEMLMessage(source, parsed);
  if (is_eml) {
    GetEMLMetaData(parsed, message_);
    if (ExtractParts(parsed, message_) != 0) {
      MLogWarn("message exceeds the supported parsing limits");
    }
  } else {
    // Not a message yet -- a new tab, or something the user is still typing.
    // Treat the whole document as the body so nothing is lost.
    message_.body = source;
  }

  // Set before the inspection views refresh: they read the original bytes, and
  // must read the ones just loaded rather than the previous document's.
  last_source_ = source;

  // Both seeded from the same fact, and then they part company. Only a message
  // can carry octets a signature covers, so plain text opened in a tab is not
  // treated as bytes to preserve. Handed back our own bytes, neither is
  // touched: what this document IS did not change just because it made a trip
  // through the host and came home.
  if (!same_document) {
    source_is_original_ = is_eml;
    document_is_received_ = is_eml;
  }

  refresh_fields();
  refresh_attachments();
  refresh_structure();
  refresh_body_view();
  refresh_security_button();
  refresh_security();

  // Last, and after refresh_structure(): what may be edited follows from what
  // the message turned out to BE, so the classification has to exist first.
  apply_content_lock();

  loading_ = false;

  // A freshly loaded view matches the document exactly, which is what lets the
  // host hand the original bytes to a verify untouched.
  dirty_ = false;
  inspection_stale_ = false;

  focus_first_field();
}

void EMailPageView::focus_first_field() {
  // Nothing set focus at all before this, so it landed on the first widget in
  // the form -- From -- which on a received message is a box that will not
  // take input. Where it goes now follows what the tab is FOR: a draft is
  // going to be addressed, and a message that arrived is going to be read.
  if (document_is_received_) {
    if (body_stack_ != nullptr && body_stack_->currentWidget() != nullptr) {
      body_stack_->currentWidget()->setFocus(Qt::OtherFocusReason);
    }
    return;
  }

  // Not From: it is normally already filled in from the account, and the
  // first thing anyone actually types is the recipient.
  if (to_edit_ != nullptr) to_edit_->setFocus(Qt::OtherFocusReason);
}

void EMailPageView::refresh_structure() {
  tree_root_ = EMailPart{};
  regions_.clear();
  security_state_ = EMailSecurityState::kPLAIN;
  // Results belong to the document that produced them. A new load has not
  // verified anything yet, and carrying the previous message's verdicts over
  // would be worse than showing none.
  discard_verification();
  recipient_rows_.clear();

  vmime::shared_ptr<vmime::message> parsed;
  if (!CheckIfEMLMessage(last_source_, parsed)) {
    // A new tab or something still being typed: there is no structure to show
    // yet, and inventing one would be worse than an empty view.
    structure_view_->Clear();
    header_view_->Clear();
    return;
  }

  if (ParseMimeTree(parsed, last_source_, tree_root_, regions_) != 0) {
    MLogWarn("message tree exceeds the supported parsing limits");
    structure_view_->Clear();
    header_view_->Clear();
    return;
  }

  security_state_ = ClassifyOpenPGPStructure(tree_root_, regions_);

  structure_view_->SetTree(tree_root_, regions_);
  header_view_->SetMessage(tree_root_, last_source_);
}

void EMailPageView::AdoptSourceView(QWidget* source) {
  if (source == nullptr || source_view_ != nullptr) return;

  source_view_ = source;

  // The editor is not shown bare: it gets a row above it saying whether it may
  // be written to, and the control that changes that.
  raw_tab_ = new QWidget(this);
  auto* layout = new QVBoxLayout(raw_tab_);
  layout->setContentsMargins(0, 4, 0, 0);
  layout->setSpacing(4);

  auto* bar = new QHBoxLayout();
  bar->setContentsMargins(0, 0, 0, 0);

  raw_notice_ = new QLabel(raw_tab_);
  raw_notice_->setWordWrap(true);
  {
    auto font = raw_notice_->font();
    font.setPointSizeF(font.pointSizeF() * 0.92);
    raw_notice_->setFont(font);
  }
  bar->addWidget(raw_notice_, 1);

  raw_unlock_button_ = new QToolButton(raw_tab_);
  raw_unlock_button_->setCheckable(true);
  raw_unlock_button_->setAutoRaise(true);
  raw_unlock_button_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  bar->addWidget(raw_unlock_button_);

  layout->addLayout(bar);
  layout->addWidget(source, 1);

  connect(raw_unlock_button_, &QToolButton::toggled, this,
          &EMailPageView::slot_toggle_raw_edit);

  // A page of the body stack rather than a place of its own, so the action row
  // above it -- and with it the switcher back -- stays on screen while the raw
  // document is being read. Appended, so the indices the rest of this file
  // uses for the editor, the rendered body and the locked panel do not move.
  body_stack_->addWidget(raw_tab_);

  // There is a second mode now, so the switch to it becomes reachable.
  view_switcher_->setVisible(true);

  // Read-only by default, and not only in forensic mode: these are the exact
  // octets a signature covers, and an accidental keystroke here is
  // indistinguishable from a forgery to whoever verifies the message next.
  // Set through the property system because the page hands over a plain
  // QWidget; an editor that does not carry the property simply does not gain a
  // way to be written to.
  raw_unlocked_ = false;
  source_view_->setProperty("readOnly", true);
  refresh_raw_lock_ui();
}

void EMailPageView::slot_toggle_raw_edit(bool on) {
  if (!on) {
    raw_unlocked_ = false;
    if (source_view_ != nullptr) source_view_->setProperty("readOnly", true);
    refresh_raw_lock_ui();
    return;
  }

  // Should be unreachable -- the control is disabled -- but the document must
  // not become writable through a path that skipped the check.
  if (forensic_) {
    const QSignalBlocker blocker(raw_unlock_button_);
    raw_unlock_button_->setChecked(false);
    return;
  }

  if (content_lock() == EMailLockReason::kPROTECTED) {
    QMessageBox::information(
        this, tr("Protected Message"),
        tr("These bytes are covered by a signature, or are ciphertext. Remove "
           "the signature, or decrypt the message, before editing its "
           "source."));
    const QSignalBlocker blocker(raw_unlock_button_);
    raw_unlock_button_->setChecked(false);
    return;
  }

  raw_unlocked_ = true;
  if (source_view_ != nullptr) source_view_->setProperty("readOnly", false);
  refresh_raw_lock_ui();
}

void EMailPageView::refresh_raw_lock_ui() {
  if (raw_unlock_button_ == nullptr || raw_notice_ == nullptr) return;

  const auto reason = content_lock();
  const bool unlockable = reason == EMailLockReason::kNONE;

  raw_unlock_button_->setEnabled(unlockable || raw_unlocked_);
  raw_unlock_button_->setIcon(
      QIcon(raw_unlocked_ ? ":/icons/unlock.png" : ":/icons/read-only.png"));
  raw_unlock_button_->setText(raw_unlocked_ ? tr("Stop Editing")
                                            : tr("Edit Raw Source"));

  QString notice;
  QColor colour = MutedColor(this);
  if (raw_unlocked_) {
    notice =
        tr("You are editing the raw message source. What you type here is the "
           "document.");
    colour = EMailWarningColor(this);
  } else {
    switch (reason) {
      case EMailLockReason::kFORENSIC:
        notice =
            tr("This message is locked for inspection. Its source cannot be "
               "edited.");
        break;
      case EMailLockReason::kPROTECTED:
        notice =
            tr("Read-only. These bytes are protected. Remove the signature, or "
               "decrypt the message, to edit them.");
        break;
      case EMailLockReason::kNONE:
        notice = tr("Read-only. Unlock to edit the message source directly.");
        break;
    }
  }

  raw_notice_->setText(notice);
  auto palette = raw_notice_->palette();
  palette.setColor(QPalette::WindowText, colour);
  raw_notice_->setPalette(palette);
}

void EMailPageView::ApplyEditorFont(const QFont& font) {
  // The body is the document's text, so it follows the text editor font. The
  // header fields, buttons and tables are chrome and deliberately do not.
  if (body_edit_ != nullptr) body_edit_->setFont(font);
  if (body_view_ != nullptr) body_view_->setFont(font);
}

void EMailNotifyKeyringChanged() {
  // The keyring is refreshed on a worker thread, so this can arrive from one;
  // everything below touches widgets and must not.
  QMetaObject::invokeMethod(
      qApp,
      []() {
        for (auto* widget : QApplication::allWidgets()) {
          auto* view = qobject_cast<EMailPageView*>(widget);
          if (view != nullptr) view->NotifyKeyringChanged();
        }
      },
      Qt::QueuedConnection);
}

void EMailPageView::NotifyKeyringChanged() {
  // Whether this message can be opened is a statement ABOUT the key database,
  // so it stops being true the moment that database changes: import the secret
  // key it was encrypted to, or switch to a profile that holds it, and
  // "cannot be opened on this computer" is simply wrong.
  //
  // Recomputed rather than discarded, unlike the verification below. This
  // costs one packet read and one key lookup, runs no crypto and asks for no
  // passphrase, so there is nothing to make the user ask for a second time.
  // It also has to happen before the early return: an encrypted message that
  // carries no signature has no regions at all, and that is precisely the
  // message whose locked panel is on screen.
  if (locked_capability_ != nullptr &&
      (security_state_ == EMailSecurityState::kENCRYPTED ||
       security_state_ == EMailSecurityState::kSIGNED_ENCRYPTED)) {
    refresh_locked_panel();
  }

  if (regions_.isEmpty()) return;

  // The previous answer was about a keyring that no longer exists: a signature
  // reported as unverifiable because the key was missing becomes verifiable
  // the moment that key is imported, and showing the old answer afterwards is
  // showing something that is no longer true.
  //
  // DISCARDED, not redone. Importing a key is not asking to verify, and a
  // keyring refresh reaching every open message must not set a crypto
  // operation running behind each of them. The surface goes back to saying
  // nobody has checked, which is now the case, and the user can ask.
  discard_verification();
  refresh_security_button();
  refresh_attachments();

  if (EMailNeedsSecurityRefresh(
          details_visible(),
          details_tabs_ != nullptr &&
              details_tabs_->currentWidget() == security_view_)) {
    refresh_security();
  }
}

void EMailPageView::sync_inspection() {
  // Nothing to do for a document that has not been edited -- and nothing that
  // MAY be done, either. Reserializing a clean message would rewrite
  // boundaries, header order and encodings, which is exactly what breaks a
  // PGP/MIME signature over the original octets.
  if (!inspection_stale_) return;

  collect_fields();

  QByteArray eml;
  // Re-encodes every attachment, on this thread. Long enough on a message with
  // a few large parts that the window would otherwise sit there looking dead.
  const EMailBusyCursor busy;

  if (BuildMimeEML(message_, message_.body, message_.attachments, eml) != 0) {
    MLogWarn("failed to serialize the edited message for inspection: " +
             QString::fromUtf8(eml));
    // Show nothing rather than the previous document's structure: a stale tree
    // presented as the current one is worse than an empty tab. The flag stays
    // set, so the next visit tries again.
    //
    // Said rather than left blank: three empty tabs after a tab switch read as
    // a message with nothing in it, which is a different and much worse claim
    // than "this could not be read".
    const auto why =
        tr("This message could not be assembled for inspection, so there is "
           "nothing to show here. The message itself is unchanged.");
    structure_view_->ShowNotice(why);
    header_view_->ShowNotice(why);
    security_view_->ShowNotice(why);
    return;
  }

  inspection_stale_ = false;
  // The inspection views read raw byte ranges out of this, so it has to be the
  // document they are describing. Still dirty: the host has yet to be given
  // these bytes, and only SaveToSource() settles that.
  last_source_ = eml;
  source_is_original_ = false;

  refresh_structure();
  refresh_security_button();
  refresh_security();
  apply_content_lock();
}

void EMailPageView::discard_verification() {
  cached_verification_ = {};

  // Whatever was in flight was about the document as it was. Cleared with the
  // answer it would have filled, so a request that is now meaningless cannot
  // leave this page unable to make another one.
  verification_pending_ = false;
}

void EMailPageView::request_verification() {
  if (verification_pending_) return;
  if (regions_.isEmpty() || last_source_.isEmpty()) return;

  // Asked for through the host, like every other crypto operation this view
  // offers. There is ONE verification in this application and the host owns
  // starting it: a second one here, over this view's own copy of the bytes,
  // is what let the status report and this surface describe the same message
  // differently.
  verification_pending_ = true;
  emit SignalCryptoOperationRequested("verify");
}

void EMailPageView::ApplyVerificationResult(const QByteArray& payload) {
  // Terminal either way: whatever happens below, this page is free to ask
  // again afterwards. A request that could never be answered must not leave
  // the view permanently silent.
  verification_pending_ = false;

  if (payload.isEmpty()) {
    // The request ended without an answer -- refused before it reached the
    // engine, or cancelled. Nothing to render; what matters is that the page
    // has been released to ask again, which happened above.
    MLogDebug("a verification request ended without a result");
    return;
  }

  EMailVerificationResult result;
  if (!DecodeVerificationPayload(payload, result)) {
    // A payload from a build that means something different by it, or one that
    // did not survive the trip. Half of a verdict is worse than none: it would
    // be rendered as though it were the whole answer.
    MLogWarn("discarding a verification result that could not be read");
    return;
  }

  // An answer about bytes that are no longer here. The document can be edited,
  // or the tab switched, between asking and being answered, and painting the
  // old verdict onto the new bytes tells the user something about a message
  // that is not in front of them.
  if (!VerificationMatchesSource(result, last_source_)) {
    MLogWarn("discarding a verification result for a different document");
    return;
  }

  cached_verification_ = result;

  // What the message surface is entitled to say has just changed. Without
  // this the badge kept the structural wording it was given at load time, so
  // a signature that turned out to be bad went on reading as "Signed" for as
  // long as the tab stayed open.
  refresh_security_button();
  refresh_attachments();
  refresh_security();
}

void EMailPageView::slot_derive_message(int mode) {
  // What the user asked for, for the failure notices below. A refusal that
  // does not name the action the user just clicked leaves them guessing which
  // of the three buttons it came from.
  const auto what = [mode]() -> QString {
    switch (static_cast<EMailReplyMode>(mode)) {
      case EMailReplyMode::kREPLY:
        return tr("Cannot Reply");
      case EMailReplyMode::kREPLY_ALL:
        return tr("Cannot Reply to All");
      case EMailReplyMode::kFORWARD:
        break;
    }
    return tr("Cannot Forward");
  }();

  // Nothing below this point changes THIS message, so every failure can say
  // so. It is the one reassurance worth giving: the user has just watched an
  // action on a message they care about not happen.
  const auto intact = tr("\n\nThis message itself has not been changed.");

  // Walk up to the tab widget that owns this page. The module cannot link the
  // UI library, so the host is reached by invokable name rather than by type.
  QObject* edit = parent();
  while (edit != nullptr &&
         edit->metaObject()->indexOfMethod(
             "SlotNewCustomTab(QString,QString,QIcon,QString)") < 0) {
    edit = edit->parent();
  }

  if (edit == nullptr) {
    MLogWarn("cannot derive a message: no tab host above this view");
    QMessageBox::warning(
        this, what,
        tr("A new message cannot be opened from here, because this view is "
           "not inside a window that holds tabs.") +
            intact);
    return;
  }

  // Whichever of our own addresses the message was sent to; used to keep the
  // user out of their own reply-all.
  QString self;
  for (const auto& candidate : message_.to + message_.cc) {
    if (!candidate.trimmed().isEmpty()) {
      self = candidate;
      break;
    }
  }

  EMailMetaData derived;
  BuildDerivedMetaData(message_, static_cast<EMailReplyMode>(mode), self,
                       derived);
  const auto quoted =
      BuildQuotedBody(message_, static_cast<EMailReplyMode>(mode));

  QByteArray eml;
  if (BuildMimeEML(derived, quoted, derived.attachments, eml) != 0) {
    MLogWarn("failed to build the derived message");
    QMessageBox::warning(
        this, what,
        tr("The new message could not be assembled. This usually means a part "
           "of the original could not be re-encoded, most often an "
           "attachment.") +
            intact);
    return;
  }

  // Name the tab after what it is. The subject already carries the "Re: " or
  // "Fwd: " prefix, so a reply is told apart from its original at a glance,
  // which "untitled.eml" never allowed. The host does not sanitize the title
  // it is handed, so it is sanitized here.
  auto title = TabTitleForSubject(derived.subject);
  if (title.isEmpty()) title = tr("untitled.eml");

  QWidget* page = nullptr;
  QMetaObject::invokeMethod(edit, "SlotNewCustomTab", Qt::DirectConnection,
                            Q_RETURN_ARG(QWidget*, page),
                            Q_ARG(QString, "email"), Q_ARG(QString, title),
                            Q_ARG(QIcon, QIcon(":/icons/email.png")),
                            Q_ARG(QString, ":/icons/email.png"));

  if (page == nullptr) {
    MLogWarn("the host did not create a tab for the derived message");
    QMessageBox::warning(
        this, what,
        tr("A tab for the new message could not be opened.") + intact);
    return;
  }

  // The new tab mounts a view of this same type, so it can be addressed
  // directly rather than through the host's document.
  auto* view = page->findChild<EMailPageView*>();
  if (view == nullptr) {
    MLogWarn("the new tab has no message view to fill");
    QMessageBox::warning(
        this, what,
        tr("The new tab opened, but it is not showing a message view, so "
           "there is nowhere to put the reply. The tab can be closed.") +
            intact);
    return;
  }

  view->LoadFromSource(eml);
  // It is a draft the user has not saved, and the tab should say so.
  view->mark_dirty();
}

auto EMailPageView::build_locked_panel() -> QWidget* {
  auto* panel = new QWidget(this);
  auto* layout = new QVBoxLayout(panel);
  layout->setAlignment(Qt::AlignCenter);
  layout->setSpacing(10);

  auto* icon = new QLabel(panel);
  icon->setAlignment(Qt::AlignCenter);
  icon->setPixmap(QIcon(":/icons/lock.png").pixmap(48, 48));
  layout->addWidget(icon);

  locked_heading_ = new QLabel(tr("This message is encrypted"), panel);
  locked_heading_->setAlignment(Qt::AlignCenter);
  {
    auto font = locked_heading_->font();
    font.setPointSizeF(font.pointSizeF() * 1.15);
    font.setBold(true);
    locked_heading_->setFont(font);
  }
  layout->addWidget(locked_heading_);

  locked_recipients_ = new QLabel(panel);
  locked_recipients_->setAlignment(Qt::AlignCenter);
  locked_recipients_->setWordWrap(true);
  locked_recipients_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(locked_recipients_);

  // Whether this user can open it at all, which is the question the panel was
  // silent on. Pressing Decrypt with no matching private key produces an
  // error from the host and a panel that comes back looking exactly as it did,
  // so the only way to learn it was never going to work was to try it.
  locked_capability_ = new QLabel(panel);
  locked_capability_->setAlignment(Qt::AlignCenter);
  locked_capability_->setWordWrap(true);
  layout->addWidget(locked_capability_);

  auto* row = new QHBoxLayout();
  row->addStretch();
  locked_decrypt_button_ =
      new QPushButton(QIcon(":/icons/unlock.png"), tr("Decrypt"), panel);
  row->addWidget(locked_decrypt_button_);
  row->addStretch();
  layout->addLayout(row);

  connect(locked_decrypt_button_, &QPushButton::clicked, this, [this]() {
    // Verifying costs nothing extra once the message is open, and a signature
    // inside the ciphertext is the only kind worth anything here.
    emit SignalCryptoOperationRequested(
        security_state_ == EMailSecurityState::kSIGNED_ENCRYPTED
            ? QStringLiteral("decrypt_verify")
            : QStringLiteral("decrypt"));
  });

  return panel;
}

void EMailPageView::refresh_locked_panel() {
  locked_heading_->setText(security_state_ ==
                                   EMailSecurityState::kSIGNED_ENCRYPTED
                               ? tr("This message is encrypted and signed")
                               : tr("This message is encrypted"));

  // Who the message was ADDRESSED to is all that is knowable before it is
  // opened. Once an operation has run, the engine can say who it was actually
  // encrypted to, which is the better answer and not the same question.
  QStringList named;
  if (!recipient_rows_.isEmpty()) {
    for (const auto& row : recipient_rows_) {
      if (!row.address.isEmpty()) {
        named.append(row.address);
      } else if (!row.info.uid.isEmpty()) {
        named.append(row.info.uid);
      } else if (!row.info.key_id.isEmpty()) {
        named.append(row.info.key_id);
      }
    }
  } else {
    named = message_.to + message_.cc;
  }
  named.removeAll({});
  named.removeDuplicates();

  locked_recipients_->setText(
      named.isEmpty() ? tr("The recipients are not named in the headers.")
                      : tr("Addressed to %1").arg(named.join(", ")));

  refresh_locked_capability(named);

  locked_decrypt_button_->setEnabled(true);
}

void EMailPageView::refresh_locked_capability(const QStringList& named) {
  // What the MESSAGE says, not what the headers say. A message is encrypted to
  // a key, and the key it names is usually an encryption subkey whose UID
  // address need never appear in To or Cc -- so matching addresses answered a
  // different question than the one asked, and answered it wrongly in both
  // directions: it claimed a readable message could not be opened whenever the
  // key's address differed from the header, and claimed the user held the key
  // for mail encrypted to someone else who happened to share an address.
  const auto* ciphertext = FindEncryptedCiphertextPart(tree_root_);
  if (ciphertext != nullptr) {
    GFGpgEncRecipient* raw = nullptr;
    int count = 0;
    if (GFGpgSniffEncryptedRecipients(
            GFGpgCurrentGpgContextChannel(), ciphertext->data.constData(),
            static_cast<int>(ciphertext->data.size()), &raw, &count) == 0) {
      QList<EMailEncRecipient> recipients;
      recipients.reserve(count);
      for (int i = 0; i < count; ++i) {
        // Copied rather than taken: these strings belong to the array that
        // GFGpgFreeEncRecipients releases as a whole.
        recipients.append(EMailEncRecipient{
            QString::fromUtf8(raw[i].key_id),
            QString::fromUtf8(raw[i].pub_algo),
            QString::fromUtf8(raw[i].fingerprint),
            QString::fromUtf8(raw[i].uid),
            raw[i].key_found != 0,
            raw[i].has_secret != 0,
            raw[i].hidden != 0,
        });
      }
      GFGpgFreeEncRecipients(raw, count);

      if (!recipients.isEmpty()) {
        show_decrypt_capability(DescribeDecryptCapability(recipients));
        return;
      }
    }
  }

  // Nothing could be read out of the message itself -- it is not an RFC 3156
  // encrypted message, or the engine could not parse its packets. Fall back to
  // the addresses, which is a guess, and worded as one.
  refresh_locked_capability_by_address(named);
}

void EMailPageView::show_decrypt_capability(
    const EMailDecryptCapability& capability) {
  locked_capability_->setVisible(true);

  switch (capability.verdict) {
    case EMailDecryptVerdict::kCAN_OPEN:
      EMailSetLabelColor(locked_capability_, AccentColor(this, true));
      locked_capability_->setText(
          capability.holding.size() == 1
              ? tr("You hold the private key for %1, so this message can be "
                   "opened here.")
                    .arg(capability.holding.front())
              : tr("You hold private keys for %1, so this message can be "
                   "opened here.")
                    .arg(capability.holding.join(", ")));
      return;

    case EMailDecryptVerdict::kCANNOT_OPEN:
      EMailSetLabelColor(locked_capability_,
                         ThemeColor(this, &GFUIWarningColor));
      locked_capability_->setText(
          tr("This message is encrypted to keys you do not hold the private "
             "half of, so it cannot be opened on this computer."));
      return;

    case EMailDecryptVerdict::kUNKNOWN:
      EMailSetLabelColor(locked_capability_, MutedColor(this));
      locked_capability_->setText(
          capability.has_hidden
              ? tr("Some recipients of this message were deliberately not "
                   "named, so there is no way to tell whether you can open it "
                   "until you try.")
              : tr("The recipients of this message cannot be read, so there is "
                   "no way to tell whether you can open it until you try."));
      return;
  }
}

void EMailPageView::refresh_locked_capability_by_address(
    const QStringList& named) {
  // The addresses this user holds the secret half for. A far weaker signal
  // than the recipient key ids above -- it can only ever be a guess, because
  // the key a message is encrypted to need not carry any of these addresses --
  // so it is reached only when the message itself could not be read, and every
  // wording below is hedged accordingly.
  char** addresses = nullptr;
  int count = 0;
  if (GFGpgListKeyAddresses(GFGpgCurrentGpgContextChannel(), 1, &addresses,
                            &count) != 0) {
    locked_capability_->setVisible(false);
    return;
  }

  QStringList mine;
  for (int i = 0; i < count; ++i) {
    const auto address = AddressOfUid(QString::fromUtf8(addresses[i]));
    if (!address.isEmpty()) mine.append(address.toLower());
  }
  GFGpgFreeStringArray(addresses, count);

  // Nothing knowable either way: no recipient is named, or the keyring holds
  // no private key at all. Saying something in that case would be inventing
  // an answer to a question that was not asked.
  if (named.isEmpty() || mine.isEmpty()) {
    locked_capability_->setVisible(!mine.isEmpty());
    if (!mine.isEmpty()) {
      EMailSetLabelColor(locked_capability_, MutedColor(this));
      locked_capability_->setText(
          tr("The recipients are not named, so there is no way to tell "
             "whether you can open this until you try."));
    }
    return;
  }

  QStringList matched;
  for (const auto& entry : named) {
    const auto address = AddressOfUid(entry).toLower();
    if (!address.isEmpty() && mine.contains(address)) matched.append(address);
  }

  locked_capability_->setVisible(true);
  if (matched.isEmpty()) {
    EMailSetLabelColor(locked_capability_, ThemeColor(this, &GFUIWarningColor));
    locked_capability_->setText(
        tr("You do not hold a private key for any of these addresses. Unless "
           "the message was also encrypted to a key that is not named here, "
           "it cannot be opened on this computer."));
    return;
  }

  EMailSetLabelColor(locked_capability_, AccentColor(this, true));
  locked_capability_->setText(
      matched.size() == 1
          ? tr("You hold the private key for %1.").arg(matched.front())
          : tr("You hold private keys for %1.").arg(matched.join(", ")));
}

auto EMailPageView::AttachPublicKey(const QByteArray& key_data,
                                    const QString& suggested_name) -> int {
  if (key_data.isEmpty()) return kEMAIL_ADD_NOT_HANDLED;

  if (!refuse_when_locked(tr("attach a public key"))) {
    return kEMAIL_ADD_REFUSED;
  }

  EMailAttachment attachment;
  attachment.filename =
      suggested_name.trimmed().isEmpty() ? "publickey.asc" : suggested_name;
  attachment.mime_type = "application/pgp-keys";
  attachment.disposition = "attachment";
  attachment.is_openpgp_key = true;
  attachment.data = key_data;

  message_.attachments.append(attachment);
  refresh_attachments();
  mark_dirty();
  return kEMAIL_ADD_DONE;
}

auto EMailPageView::AppendBodyText(const QString& text) -> int {
  if (text.isEmpty()) return kEMAIL_ADD_NOT_HANDLED;

  if (!refuse_when_locked(tr("add text to this message"))) {
    return kEMAIL_ADD_REFUSED;
  }

  // The message being composed, not the document: for a structured message
  // "put this in what I am writing" means the body and nothing else.
  body_edit_->appendPlainText(text);
  // No mark_dirty() here: the body editor's own textChanged already did it.
  return kEMAIL_ADD_DONE;
}

auto EMailPageView::refuse_when_locked(const QString& what) -> bool {
  const auto reason = content_lock();
  if (reason == EMailLockReason::kNONE) return true;

  if (reason == EMailLockReason::kFORENSIC) {
    QMessageBox::information(
        this, tr("Message Locked"),
        tr("This message is open for inspection only, so it is not possible "
           "to %1.")
            .arg(what));
    return false;
  }

  if (security_state_ == EMailSecurityState::kENCRYPTED ||
      security_state_ == EMailSecurityState::kSIGNED_ENCRYPTED) {
    QMessageBox::information(
        this, tr("Message Is Encrypted"),
        tr("Decrypt this message before trying to %1.").arg(what));
    return false;
  }

  // Signed, and therefore editable only once the signature is gone. Offered
  // rather than merely refused: the user asked to change the message, and
  // removing the signature is how that becomes possible.
  if (QMessageBox::question(
          this, tr("Message Is Signed"),
          tr("This message is signed, so it cannot be changed: an edit "
             "under a signature reads as a forgery.\n\nRemove the signature "
             "and make it an ordinary message?"),
          QMessageBox::Yes | QMessageBox::Cancel,
          QMessageBox::Cancel) == QMessageBox::Yes) {
    slot_remove_protection_layer();
  }
  return false;
}

void EMailPageView::refresh_body_view() {
  // The raw document is on screen, and this function's whole job is to decide
  // which of the OTHER pages belongs there. Without this guard the host's own
  // reload -- which it runs on every content change, including the user's own
  // keystrokes in that editor -- would pull the editor out from under them.
  if (source_mode_) return;

  // Cleared before the body view is told anything, so the flag can only be set
  // again by the view itself saying it fell back to source.
  // apply_content_lock() runs after this function in LoadFromSource() and is
  // what puts the sentence on screen.
  body_is_html_ = false;

  // The plain-text part is the body whenever the message has one, and the
  // choice is the message's own rather than a mode the user picks. Only a
  // message that offers nothing but HTML falls through to its source, which
  // is read-only: this workspace composes plain text and nothing else.
  const auto* body = SelectBodyPart(tree_root_, false);
  const bool html_only = body != nullptr && body->content_type == "text/html";

  // Only offered for a message that parsed: there is nothing to reply to in a
  // draft the user is still typing.
  // Whether this document came from SOMEONE ELSE, which is the only thing
  // Reply and Forward mean. Deliberately not "does it currently parse as
  // MIME": a draft parses the moment anything serializes it -- opening the
  // Structure, Security or Headers tab is enough -- and gating on that made
  // these three switch themselves on partway through writing a message, with
  // nothing to reply to.
  const bool is_message = document_is_received_;

  // Every action stays where it is and is turned off instead of taken away. A
  // control that disappears sends the user looking for a feature they think
  // they have lost; one that is merely grey, with a tooltip, tells them what
  // to do next. The reasons here -- no account, no recipient, nothing parsed
  // yet -- are all things they can act on once told.
  send_button_->setVisible(true);
  refresh_send_state();

  const auto not_a_message =
      tr("This is a draft you are still writing, not a message that was "
         "received, so there is nothing here to act on yet.");

  reply_button_->setVisible(true);
  reply_all_button_->setVisible(true);
  forward_button_->setVisible(true);
  forensic_toggle_->setVisible(true);
  if (action_separator_ != nullptr) action_separator_->setVisible(true);

  for (auto* button : {reply_button_, reply_all_button_, forward_button_}) {
    set_action_available(button, is_message, not_a_message);
  }

  // Read-only is offered only where it CHANGES something. A signed or
  // encrypted message is already locked by content_lock(), so on those -- the
  // common case in this program -- the toggle appeared to do nothing at all,
  // which is how a real protection came to look like a dead control.
  //
  // What it uniquely does, and only on a plain received message: it locks the
  // Raw Source tab outright, and it stops SaveToSource() rebuilding the
  // message. That second one is the point. A rebuild rewrites header order and
  // encodings, and for a message whose HEADERS are the evidence -- a forged
  // Received chain, say -- one stray keystroke and a save would destroy the
  // very thing the message was opened to examine.
  if (!is_message) {
    set_action_available(forensic_toggle_, false, not_a_message);
  } else if (security_state_ != EMailSecurityState::kPLAIN && !forensic_) {
    set_action_available(
        forensic_toggle_, false,
        tr("This message cannot be edited anyway: it is signed or encrypted, "
           "so it is already protected from being rewritten."));
  } else {
    set_action_available(forensic_toggle_, true, {});
  }

  // Ciphertext is not text the user can read or edit, so it is not offered as
  // either. Deliberately after the block above: Reply and Forward derive a new
  // message and never touch this one, so they stay available on a message that
  // cannot be read.
  const bool locked = security_state_ == EMailSecurityState::kENCRYPTED ||
                      security_state_ == EMailSecurityState::kSIGNED_ENCRYPTED;
  if (locked) {
    refresh_locked_panel();
    body_view_->Clear();
    body_stack_->setCurrentIndex(2);
    return;
  }

  if (!html_only) {
    body_view_->Clear();
    body_stack_->setCurrentIndex(0);
    return;
  }

  body_view_->SetBody(body);
  body_stack_->setCurrentIndex(1);
}

void EMailPageView::refresh_security() {
  QStringList addresses;
  addresses += message_.to;
  addresses += message_.cc;
  addresses += message_.bcc_header;
  addresses += compose_.bcc;
  if (!message_.from.isEmpty()) addresses.append(message_.from);

  const auto findings = tree_root_.content_type.isEmpty()
                            ? QList<EMailFinding>{}
                            : InspectMessage(message_, tree_root_, regions_,
                                             cached_verification_.verdicts);

  security_view_->SetMessage(
      security_state_, regions_, cached_verification_.signatures,
      recipient_rows_, addresses, message_.from,
      GFGpgCurrentGpgContextChannel(), findings, cached_verification_.state,
      !message_key_parts().isEmpty());
}

/**
 * @brief Whether this message could go out, and if not, what to fix.
 *
 * Every refusal names the one thing standing in the way. A disabled control
 * that will not say why is the same dead end as a hidden one -- the user knows
 * something is wrong and not what -- so the reason goes in the tooltip, which
 * is where a disabled button can still speak.
 */
auto EMailPageView::eventFilter(QObject* watched, QEvent* event) -> bool {
  if (event->type() == QEvent::FocusIn &&
      (watched == from_edit_ || watched == to_edit_ || watched == cc_edit_ ||
       watched == bcc_edit_)) {
    install_address_hints();
  }
  return QWidget::eventFilter(watched, event);
}

/**
 * @brief Turns one action off without taking it away.
 *
 * A control that vanishes leaves the user hunting for a feature; one that is
 * merely grey leaves them guessing why. Neither is necessary: the button stays
 * where it is and its tooltip says what is missing, which is the only thing a
 * disabled control can still do.
 */
void EMailPageView::set_action_available(QToolButton* button, bool available,
                                         const QString& why) {
  if (button == nullptr) return;

  // Captured on first use rather than at construction, so this cannot fall out
  // of step with whatever wording make_action() gave the button.
  if (!action_tooltips_.contains(button)) {
    action_tooltips_.insert(button, button->toolTip());
  }

  button->setEnabled(available);
  button->setToolTip(available ? action_tooltips_.value(button) : why);
}

void EMailPageView::refresh_send_state() {
  // Read straight off the widgets rather than through collect_fields(). This
  // runs on every keystroke, and collect_fields() copies the entire body and
  // writes into message_ -- neither of which a button's enabled state has any
  // business doing.
  const auto split = [](const QString& text) {
    QStringList out;
    for (const auto& part : text.split(';', Qt::SkipEmptyParts)) {
      const auto trimmed = part.trimmed();
      if (!trimmed.isEmpty()) out.append(trimmed);
    }
    return out;
  };

  const auto from = from_edit_->text().trimmed();
  const auto to = split(to_edit_->text());
  const auto cc = split(cc_edit_->text());
  const auto bcc = split(bcc_edit_->text());
  const auto subject = subject_edit_->text().trimmed();

  QString blocker;

  if (!EMailSendDialog::HasUsableAccount()) {
    // First, because it is the only one the user cannot fix by typing. Said as
    // the setup step it is rather than as a fault in the message.
    blocker =
        tr("No mail account is set up yet. Add one in Settings, under Mail "
           "Accounts, and this message can be sent.");
  } else if (from.isEmpty()) {
    blocker = tr("Fill in who this message is from.");
  } else if (!MailIsPlausibleAddress(from)) {
    blocker = tr("The From address does not look like an e-mail address.");
  } else if (to.isEmpty() && cc.isEmpty() && bcc.isEmpty()) {
    blocker = tr("Add at least one recipient.");
  } else if (subject.isEmpty()) {
    blocker = tr("Give this message a subject.");
  } else {
    // Every recipient, across all three fields: one bad address in Cc stops
    // the message just as surely as a bad one in To, and finding out at the
    // server is finding out too late.
    for (const auto& list : {to, cc, bcc}) {
      for (const auto& address : list) {
        if (MailIsPlausibleAddress(address)) continue;
        blocker =
            tr("This does not look like an e-mail address: %1").arg(address);
        break;
      }
      if (!blocker.isEmpty()) break;
    }
  }

  set_action_available(send_button_, blocker.isEmpty(), blocker);
}

/**
 * @brief The keyring's addresses, offered as you type.
 *
 * A hint and nothing more. That an address is offered means the keyring has
 * seen it; it is not a claim that a usable key exists for it, still less that
 * the key belongs to whoever the name says. Anything acting on the choice
 * resolves it properly through the security view.
 */
void EMailPageView::install_address_hints() {
  const auto addresses = [](bool secret_only) {
    char** raw = nullptr;
    int count = 0;
    if (GFGpgListKeyAddresses(GFGpgCurrentGpgContextChannel(),
                              secret_only ? 1 : 0, &raw, &count) != 0) {
      return QStringList{};
    }

    QStringList out;
    out.reserve(count);
    // Copied rather than taken: UnStrDup FREES what it is handed, and these
    // strings belong to the array that GFGpgFreeStringArray releases as a
    // whole.
    for (int i = 0; i < count; ++i) out.append(QString::fromUtf8(raw[i]));
    GFGpgFreeStringArray(raw, count);
    return out;
  };

  // From is one identity, so it completes against what the user can send AS.
  // The configured mail accounts come FIRST and the keyring second: an account
  // is an address this program can actually send through, whereas a secret key
  // is only an identity it could sign as. When the two name the same address
  // the account's wording wins, because that is the one the user typed into
  // Settings.
  QStringList senders;
  QSet<QString> seen;
  const auto remember = [&senders, &seen](const QString& entry) {
    const auto address = MailAddressOnly(entry).toLower();
    if (address.isEmpty() || seen.contains(address)) return;
    seen.insert(address);
    senders.append(entry);
  };

  for (const auto& account : EMailAccountStore::Load()) {
    remember(account.Label());
  }
  for (const auto& entry : addresses(true)) remember(entry);

  InstallAddressCompleter(from_edit_, senders, false);

  const auto known = addresses(false);
  for (auto* edit : {to_edit_, cc_edit_, bcc_edit_}) {
    InstallAddressCompleter(edit, known, true);
  }
}

auto EMailPageView::security_badge() const -> EMailBadgeState {
  // Structure AND cryptography. The badge used to be derived from structure
  // alone, so a multipart/signed whose signature part held forty bytes of
  // garbage read as "Signed", in the same accent colour as a verified one,
  // and nothing on the message surface ever contradicted it.
  //
  // Quoted, never recomputed: a verification produced this, and the status
  // report quotes the same one. Deriving a second opinion here is exactly how
  // this surface came to call a message forged while the report called it
  // verified.
  if (cached_verification_.state != EMailVerifyState::kNOT_ATTEMPTED) {
    return cached_verification_.overall;
  }

  // Nothing has verified this document. Structure is all that can honestly be
  // said about it, which is what this answers with an empty result.
  return DeriveSecurityBadge(security_state_, EMailVerifyState::kNOT_ATTEMPTED,
                             {}, message_.from);
}

void EMailPageView::paint_security_button() {
  // Colour only. Split from refresh_security_button() because that one also
  // rebuilds the menu and announces that the available operations changed,
  // and a theme change must not claim either of those happened.
  QColor colour = MutedColor(this);
  switch (ToneForBadge(security_badge())) {
    case EMailBadgeTone::kGOOD:
      colour = AccentColor(this, true);
      break;
    case EMailBadgeTone::kWARN:
      colour = EMailWarningColor(this);
      break;
    case EMailBadgeTone::kDANGER:
      colour = EMailToneColor(this, EMailTone::kDANGER);
      break;
    case EMailBadgeTone::kMUTED:
      break;
  }

  auto palette = security_button_->palette();
  palette.setColor(QPalette::ButtonText, colour);
  security_button_->setPalette(palette);
}

void EMailPageView::refresh_security_button() {
  QString text;
  QString icon;

  const bool encrypted =
      security_state_ == EMailSecurityState::kENCRYPTED ||
      security_state_ == EMailSecurityState::kSIGNED_ENCRYPTED;

  switch (security_badge()) {
    case EMailBadgeState::kENCRYPTED_ONLY:
      text = tr("Encrypted");
      icon = ":/icons/lock.png";
      break;
    case EMailBadgeState::kSIGNED_UNVERIFIED:
      // What is actually known at this point: the message CARRIES a
      // signature. Whether it is any good is a separate question that has
      // not been asked yet, and the wording must not answer it.
      text = encrypted ? tr("Encrypted, signature not checked")
                       : tr("Signature not checked");
      icon = ":/icons/signature.png";
      break;
    case EMailBadgeState::kSIGNED_GOOD:
      text = encrypted ? tr("Encrypted, signature verified")
                       : tr("Signature verified");
      icon = ":/icons/signature.png";
      break;
    case EMailBadgeState::kSIGNED_MISMATCH:
      text = tr("Signed by a different address");
      icon = ":/icons/warning.png";
      break;
    case EMailBadgeState::kSIGNED_EXPIRED:
      text = tr("Signature or key expired");
      icon = ":/icons/warning.png";
      break;
    case EMailBadgeState::kSIGNED_UNKNOWN_KEY:
      text = tr("Signed by an unknown key");
      icon = ":/icons/warning.png";
      break;
    case EMailBadgeState::kSIGNED_BAD:
      text = tr("Bad signature");
      icon = ":/icons/warning.png";
      break;
    case EMailBadgeState::kSIGNED_ERROR:
      // Not the same claim as a bad signature, and not the same as one nobody
      // has looked at. The check was made and could not produce an answer,
      // which is the one outcome trying again might change.
      text = tr("Signature could not be checked");
      icon = ":/icons/warning.png";
      break;
    case EMailBadgeState::kMALFORMED:
      text = tr("Malformed OpenPGP structure");
      icon = ":/icons/warning.png";
      break;
    case EMailBadgeState::kNOT_PROTECTED:
      // Stated plainly and quietly. An unprotected message is the ordinary
      // case, not a fault, and painting it as a warning would train the user
      // to ignore the one that matters.
      text = tr("Not signed or encrypted");
      icon = ":/icons/email.png";
      break;
  }

  // Always shown, including on a tab the user is still composing. This is not
  // only a statement about what the message IS -- it is also where Sign and
  // Encrypt are reached from, and a new draft is exactly when someone wants
  // them. An empty draft is honestly "not signed or encrypted", which is what
  // the kPLAIN branch above already says.
  security_button_->setVisible(true);

  security_button_->setIcon(QIcon(icon));
  paint_security_button();

  // The button keeps the whole wording and draws as much of it as it was given
  // room for, so narrowing the window never loses what the message is.
  static_cast<ElidingToolButton*>(security_button_)->SetFullText(text);
  rebuild_security_menu();

  // What this message is has just been (re)decided, and with it what can be
  // done to it. The menu bar is driven from the same answer.
  emit SignalCryptoOperationsChanged();
}

auto EMailPageView::AvailableCryptoOperations() -> QStringList {
  // What the menu bar may offer for the message as it stands.
  //
  // Structure decides this, because structure is what the operations
  // themselves require: VerifyEMLData refuses anything that is not
  // multipart/signed and DecryptEMLData anything that is not
  // multipart/encrypted, both before any cryptography happens. Offering an
  // operation that is going to be refused on those grounds turns a menu into
  // a guessing game and answers a mistake with a security report.
  //
  // There is no inline-OpenPGP path here: an armored block sitting in a body
  // is not something this module's handlers accept, so a plain message really
  // does have nothing to decrypt or verify.
  QStringList reading;
  QStringList producing;

  switch (security_state_) {
    case EMailSecurityState::kPLAIN:
      producing << "sign" << "encrypt" << "encrypt_sign";
      break;

    case EMailSecurityState::kSIGNED:
      reading << "verify";
      // Signing again, or wrapping the signed message in encryption, are both
      // ordinary things to do to a signed message -- neither touches the
      // content the existing signature covers.
      producing << "sign" << "encrypt" << "encrypt_sign";
      break;

    case EMailSecurityState::kENCRYPTED:
    case EMailSecurityState::kSIGNED_ENCRYPTED:
      // Nothing else can be done with a message nobody has read yet. Signing
      // or re-encrypting ciphertext says nothing about what is inside it.
      reading << "decrypt" << "decrypt_verify";
      break;

    case EMailSecurityState::kMALFORMED_PGP:
      // It claims to be OpenPGP and is not quite. Both are offered because
      // the refusal is itself the diagnosis, and it names what is wrong.
      reading << "verify" << "decrypt";
      break;
  }

  // A document locked for inspection does not change, and every operation
  // that PRODUCES a message rewrites this tab. Reading one does not, by the
  // same rule that keeps Reply and Forward available on a forensic message.
  return forensic_ ? reading : reading + producing;
}

void EMailPageView::rebuild_security_menu() {
  security_menu_->clear();

  // Each of these items is a question -- what signed this, who is it
  // encrypted to -- and answering it by opening a pane and leaving the user to
  // find the row themselves answers a different, easier question.
  const auto show_details = [this](const QString& section) {
    if (security_view_ == nullptr) return;
    // Opened and brought up to date BEFORE the reveal: the refresh rebuilds
    // the tree, and a rebuild after the scroll throws the row away.
    open_details(section);
  };
  const auto request = [this](const QString& op) {
    // Verify goes through the one place that tracks a request in flight, so
    // that the badge's own menu, the Security tab's button and the toolbar are
    // all the same single request rather than three ways to start three.
    if (op == "verify") return request_verification();
    emit SignalCryptoOperationRequested(op);
  };

  // Producing a new protected message is a modification; forensic mode
  // forbids it, exactly as it forbids every other edit in place.
  const bool may_modify = !forensic_;

  const auto add_op = [&](const QString& label, const QString& op) {
    auto* action = security_menu_->addAction(label);
    action->setEnabled(may_modify);
    if (!may_modify) {
      action->setToolTip(tr("This message is locked for inspection."));
    }
    connect(action, &QAction::triggered, this,
            [request, op]() { request(op); });
    return action;
  };
  const auto add_read = [&](const QString& label, const QString& op) {
    auto* action = security_menu_->addAction(label);
    connect(action, &QAction::triggered, this,
            [request, op]() { request(op); });
    return action;
  };

  switch (security_state_) {
    case EMailSecurityState::kPLAIN:
      add_op(tr("Sign..."), "sign");
      add_op(tr("Encrypt..."), "encrypt");
      add_op(tr("Encrypt and Sign..."), "encrypt_sign");
      break;

    case EMailSecurityState::kSIGNED: {
      add_read(tr("Verify Signature"), "verify");
      auto* details = security_menu_->addAction(tr("Signature Details..."));
      connect(details, &QAction::triggered, this,
              [show_details]() { show_details(kSectionSignatures); });
      security_menu_->addSeparator();
      add_op(tr("Sign Again With Another Key..."), "sign");
      auto* remove = security_menu_->addAction(tr("Remove Signature..."));
      remove->setEnabled(may_modify);
      connect(remove, &QAction::triggered, this,
              &EMailPageView::slot_remove_protection_layer);
      break;
    }

    case EMailSecurityState::kENCRYPTED: {
      add_read(tr("Decrypt"), "decrypt");
      add_read(tr("Decrypt and Verify"), "decrypt_verify");
      security_menu_->addSeparator();
      auto* details = security_menu_->addAction(tr("Encryption Details..."));
      connect(details, &QAction::triggered, this,
              [show_details]() { show_details(kSectionRecipients); });
      break;
    }

    case EMailSecurityState::kSIGNED_ENCRYPTED: {
      add_read(tr("Decrypt and Verify"), "decrypt_verify");
      add_read(tr("Decrypt"), "decrypt");
      security_menu_->addSeparator();
      auto* details = security_menu_->addAction(tr("Details..."));
      connect(details, &QAction::triggered, this,
              [show_details]() { show_details(kSectionSignatures); });

      auto* remove = security_menu_->addAction(tr("Remove Signature..."));
      // Only a signature this view can actually see can be lifted off. When
      // the signing is outside the ciphertext there is one; when it is inside,
      // there is nothing here to remove until the message is decrypted.
      QByteArray probe;
      const bool removable =
          UnwrapProtectedLayer(tree_root_, last_source_, probe) ==
          EMailUnwrapResult::kOK;
      remove->setEnabled(may_modify && removable);
      if (!removable) {
        remove->setToolTip(tr("Decrypt this message first."));
      }
      connect(remove, &QAction::triggered, this,
              &EMailPageView::slot_remove_protection_layer);
      break;
    }

    case EMailSecurityState::kMALFORMED_PGP: {
      auto* details =
          security_menu_->addAction(tr("What Is Wrong With This Message?"));
      connect(details, &QAction::triggered, this,
              [show_details]() { show_details(kSectionFindings); });
      security_menu_->addSeparator();
      add_read(tr("Try to Verify Anyway"), "verify");
      add_read(tr("Try to Decrypt Anyway"), "decrypt");
      break;
    }
  }
}

void EMailPageView::slot_remove_protection_layer() {
  if (forensic_) return;

  QByteArray out;
  const auto result = UnwrapProtectedLayer(tree_root_, last_source_, out);

  switch (result) {
    case EMailUnwrapResult::kNOT_SUPPORTED:
      QMessageBox::information(
          this, tr("Signature Not Reachable"),
          tr("The signature is inside the encrypted part of this message. "
             "Decrypt it first."));
      return;
    case EMailUnwrapResult::kNOT_PROTECTED:
      QMessageBox::information(this, tr("Nothing to Remove"),
                               tr("This message carries no signature."));
      return;
    case EMailUnwrapResult::kMALFORMED:
      QMessageBox::warning(
          this, tr("Cannot Remove the Signature"),
          tr("This message does not follow RFC 3156 closely enough to take "
             "its signature off safely. Edit the raw source instead."));
      return;
    case EMailUnwrapResult::kOK:
      break;
  }

  if (QMessageBox::warning(
          this, tr("Remove Signature"),
          tr("Remove the signature from this message?\n\nThe signature is "
             "discarded and the message becomes an ordinary, unsigned one. "
             "The message itself is kept exactly as it is. This cannot be "
             "undone."),
          QMessageBox::Yes | QMessageBox::Cancel,
          QMessageBox::Cancel) != QMessageBox::Yes) {
    return;
  }

  // These bytes came out of the original document rather than being built from
  // the fields, and they have to reach the host exactly as they are.
  pending_replacement_ = out;

  LoadFromSource(out);

  // LoadFromSource() ends clean, but the host has not been given these bytes
  // yet. Set directly rather than through mark_dirty(), which would also mark
  // the inspection tabs stale -- they already describe exactly this document.
  dirty_ = true;
  inspection_stale_ = false;
  emit SignalContentModified();
}

void EMailPageView::refresh_fields() {
  from_edit_->setText(message_.from);
  to_edit_->setText(message_.to.join("; "));
  cc_edit_->setText(message_.cc.join("; "));
  // A received message almost never carries a Bcc header; when it does, that
  // is worth showing. Anything the user types here is compose state and is
  // deliberately not written back into the document.
  bcc_edit_->setText(!message_.bcc_header.isEmpty()
                         ? message_.bcc_header.join("; ")
                         : compose_.bcc.join("; "));
  subject_edit_->setText(message_.subject);
  body_edit_->setPlainText(QString::fromUtf8(message_.body));

  // Collapsing these rows is about the common case of not using them. A
  // message that actually has a Cc or Bcc must not have it hidden.
  if (!message_.cc.isEmpty() || !message_.bcc_header.isEmpty() ||
      !compose_.bcc.isEmpty()) {
    set_cc_bcc_visible(true);
  }
}

void EMailPageView::collect_fields() {
  const auto split = [](const QString& s) {
    QStringList out;
    for (const auto& part : s.split(';', Qt::SkipEmptyParts)) {
      const auto trimmed = part.trimmed();
      if (!trimmed.isEmpty()) out.append(trimmed);
    }
    return out;
  };

  message_.from = from_edit_->text().trimmed();
  message_.to = split(to_edit_->text());
  message_.cc = split(cc_edit_->text());
  // Straight into compose state, never into the message. The serializer has
  // no way to write a Bcc header, so this is the only place it can live.
  compose_.bcc = split(bcc_edit_->text());
  message_.subject = subject_edit_->text();
  message_.body = body_edit_->toPlainText().toUtf8();

  // The identity is decided HERE, the first time this document is collected
  // with a sender on it -- not at send time.
  //
  // Send time is too late for anything that gets protected first. Encrypting
  // or signing freezes the message into octets that must never be touched
  // again, so an identifier that is not already inside them can never be added
  // afterwards: that is exactly why an encrypted message used to go out with
  // no Message-ID and no copy that could be found in Sent.
  //
  // Minted once and kept, rather than per serialization: BuildMimeEML is
  // called for every draft save, and a fresh identifier each time would give
  // the message a different identity in every copy of it that exists.
  if (message_.message_id.trimmed().isEmpty()) {
    const auto sender = MailAddressOnly(message_.from);
    if (!sender.isEmpty()) message_.message_id = MailGenerateMessageId(sender);
  }
}

void EMailPageView::refresh_attachments() {
  attachment_list_->clear();

  bool any_unsigned = false;
  bool any_signed_info = false;

  for (const auto& att : message_.attachments) {
    auto* item = new QTreeWidgetItem(attachment_list_);
    item->setText(kColName,
                  SanitizeAttachmentFileName(att.filename, att.mime_type));

    // A key is not a document and the action it affords is a different one, so
    // telling them apart should not take a reading of the type column.
    item->setIcon(kColName, QIcon(att.is_openpgp_key ? ":/icons/key.png"
                                                     : ":/icons/misc_doc.png"));

    // Type and size describe the name; they are not values in their own right.
    item->setText(kColType, att.mime_type);
    EMailSetCellTone(item, kColType, EMailTone::kMUTED, this);
    item->setText(kColSize, HumanSize(att.data.size()));
    EMailSetCellTone(item, kColSize, EMailTone::kMUTED, this);
    item->setTextAlignment(kColSize, Qt::AlignRight | Qt::AlignVCenter);

    if (att.inside_signed_part) {
      // inside_signed_part is set while PARSING: it says the part sits within
      // a multipart/signed subtree, not that anything verified. Saying
      // "signed" in green on that basis is the attachment-row version of the
      // badge bug -- a part inside a subtree whose signature is forged, or
      // whose key is missing, was labelled exactly like a checked one.
      //
      // So the claim is only as strong as what has actually been checked --
      // and it is the SAME answer the badge and the status report quote,
      // because all three read the one verification rather than each working
      // something out for itself.
      const auto badge = security_badge();
      if (badge == EMailBadgeState::kSIGNED_GOOD) {
        item->setText(kColSigned, tr("signed"));
        EMailSetCellTone(item, kColSigned, EMailTone::kGOOD, this);
      } else if (badge == EMailBadgeState::kSIGNED_UNVERIFIED) {
        item->setText(kColSigned, tr("signed, not checked"));
        EMailSetCellTone(item, kColSigned, EMailTone::kMUTED, this);
      } else if (badge == EMailBadgeState::kSIGNED_ERROR) {
        // The check ran and could not answer. Saying "not good" here would
        // report an inconclusive check as evidence against the part.
        item->setText(kColSigned, tr("signed, could not be checked"));
        EMailSetCellTone(item, kColSigned, EMailTone::kWARN, this);
      } else {
        // The signature covering it came back bad, unknown or mismatched.
        // Whatever the Security tab says about that signature is what this
        // part is worth, and it is not a green tick.
        item->setText(kColSigned, tr("signature not good"));
        EMailSetCellTone(item, kColSigned,
                         badge == EMailBadgeState::kSIGNED_BAD
                             ? EMailTone::kDANGER
                             : EMailTone::kWARN,
                         this);
      }
      any_signed_info = true;
    } else {
      // Not painted red: nothing here is broken and nothing is irreversible.
      // The part simply arrived without the signature vouching for it, which
      // is a caveat rather than an alarm.
      item->setText(kColSigned, tr("unsigned"));
      EMailSetCellTone(item, kColSigned, EMailTone::kWARN, this);
      any_unsigned = true;
    }

    // The name as it arrived, in full, so a sanitized display name never hides
    // what the sender actually sent.
    item->setToolTip(kColName, att.filename);
  }

  // Only say it when it means something: before a verify has run nothing is
  // marked signed, and claiming everything is unsigned would be noise.
  const bool meaningful = any_unsigned && any_signed_info;
  unsigned_notice_->setVisible(meaningful);
  if (meaningful) {
    unsigned_notice_->setText(
        tr("Some parts are outside the signed section of this message. "
           "They are not covered by the signature and could have been added "
           "by anyone."));
  }

  const bool has_any = !message_.attachments.isEmpty();

  // An empty table is a large blank box that reads as a fault and takes the
  // room the body wants, so the list only exists once there is something in
  // it. "Attach File..." stays available either way.
  attachment_list_->setVisible(has_any);
  attachment_heading_->setVisible(has_any);
  if (has_any) {
    const auto count = static_cast<int>(message_.attachments.size());
    attachment_heading_->setText(count == 1 ? tr("1 attachment")
                                            : tr("%1 attachments").arg(count));
  }

  // "Signed" is only an answer on a message that was received and verified.
  // While composing, nothing has been signed yet and a column of "no" would
  // be alarming rather than informative.
  attachment_list_->setColumnHidden(kColSigned, !any_signed_info);

  // Grow with the content instead of always reserving the maximum: a single
  // attachment should not claim the same space as eight.
  if (has_any) {
    const auto row_height = attachment_list_->sizeHintForRow(0) > 0
                                ? attachment_list_->sizeHintForRow(0)
                                : 20;
    const auto header_height = attachment_list_->header()->height();
    const auto wanted =
        header_height +
        row_height * static_cast<int>(message_.attachments.size()) + 8;
    // Capped against the page rather than against a constant. A fixed 160px
    // meant a message with a dozen parts scrolled a box four rows tall no
    // matter how much room the window had; a third of the page is the same
    // restraint expressed as a share of what is actually available.
    const auto ceiling = qMax(kMinAttachmentHeight, height() / 3);
    attachment_list_->setFixedHeight(qMin(wanted, ceiling));
  }

  attachment_list_->resizeColumnToContents(kColType);
  attachment_list_->resizeColumnToContents(kColSize);
  if (any_signed_info) attachment_list_->resizeColumnToContents(kColSigned);
  slot_selection_changed();
}

void EMailPageView::report_attachment_status(const QString& note) {
  // Said where the attachments are, rather than in a dialog the user has to
  // dismiss: this is the outcome of something they just asked for and were
  // watching, not news that has to interrupt them.
  attachment_heading_->setVisible(true);
  attachment_heading_->setText(note);

  // Back to describing the list afterwards. The heading is the list's label
  // first and a status line only in passing, so it must not keep saying
  // "Saved 3 files." over a list the user has since changed.
  QTimer::singleShot(kStatusNoteMs, this, [this]() { refresh_attachments(); });
}

auto EMailPageView::SuggestedFileName() -> QString {
  // The subject as it is on screen, not as it was loaded: a draft being
  // written has a subject in the field and nothing in message_ until something
  // serializes it, and the name offered should follow what the user typed.
  const auto subject =
      subject_edit_ != nullptr ? subject_edit_->text() : message_.subject;
  return SuggestedEMailFileName(subject);
}

auto EMailPageView::FileTypeFilter() -> QString {
  return tr("E-Mail Message (*.eml);;All Files (*)");
}

auto EMailPageView::SaveToSource() -> QByteArray {
  // Refused outright rather than merely never reached: the host probes this by
  // name, and no future call site may be able to rewrite a forensic document.
  // Handing back the bytes as loaded is the only answer that preserves them.
  if (forensic_) return last_source_;

  // A message produced by lifting a protected layer off this one. These bytes
  // were taken OUT of the original rather than built from the fields, and they
  // must reach the document exactly as they are: a BuildMimeEML round trip
  // here would rewrite the very entity whose octets a nested signature still
  // covers. Deliberately before collect_fields(), which would start rebuilding
  // from the widgets.
  if (!pending_replacement_.isEmpty()) {
    last_source_ = pending_replacement_;
    pending_replacement_.clear();
    dirty_ = false;
    // Taken OUT of a message rather than built from the fields, so a nested
    // signature may still cover them.
    source_is_original_ = true;
    return last_source_;
  }

  collect_fields();

  QByteArray eml;
  if (BuildMimeEML(message_, message_.body, message_.attachments, eml) != 0) {
    MLogWarn("failed to serialize message: " + QString::fromUtf8(eml));
    // Hand back exactly what was loaded. Returning the body alone -- which an
    // earlier version did -- replaces the document with the message text and
    // silently drops every header the user typed and every attachment they
    // added. Staying dirty means the next flush tries again rather than
    // treating the failure as saved.
    return last_source_;
  }

  dirty_ = false;
  last_source_ = eml;
  // Built here, from the user's own fields. There is nothing in these bytes
  // to preserve, which is what lets the send path rebuild them and give the
  // message the Message-ID a draft has not got yet.
  source_is_original_ = false;
  return last_source_;
}

void EMailPageView::slot_send_message() {
  EMailOutgoingMessage message;
  if (!BuildOutgoing(message)) {
    QMessageBox::warning(
        this, tr("Cannot Send"),
        tr("This message needs a sender and at least one recipient before it "
           "can be sent."));
    return;
  }

  // Owns its own copy of the frozen bytes: the tab may be edited or closed
  // while the dialog is open, and what was approved must not change underneath
  // the submission.
  //
  // Parented to the window rather than to this view for the same reason: as a
  // child of the tab, closing the tab mid-send destroyed the dialog, and the
  // user never learned whether the message had gone out.
  auto* dialog = new EMailSendDialog(message, window());
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->show();
}

auto EMailPageView::IsDirty() -> bool { return dirty_; }

auto EMailPageView::BuildOutgoing(EMailOutgoingMessage& out) -> bool {
  collect_fields();

  // A clean document that came from OUTSIDE is handed over untouched. That is
  // not an optimization: those bytes may carry a signature computed over
  // exactly these octets, and rebuilding them would invalidate it. A forensic
  // document is never rebuilt at all, by the same rule that governs
  // SaveToSource().
  //
  // Bytes our own serializer wrote are a different thing entirely. A draft
  // saved by SaveToSource() is clean and non-empty like any loaded message,
  // and preserving it protected nothing while costing the message its
  // Message-ID -- which is the one handle a Sent-folder check has to search
  // on. Those are rebuilt, and FreezeOutgoing() mints the identifier.
  const auto reuse_source = MailShouldReuseSource(
      source_is_original_, dirty_, forensic_, last_source_.isEmpty());

  const auto result =
      FreezeOutgoing(message_, compose_, message_.body, message_.attachments,
                     reuse_source ? last_source_ : QByteArray(), out);

  if (result != EMailFreezeResult::kOK) {
    MLogWarn("cannot prepare message for sending");
    return false;
  }
  return true;
}

void EMailPageView::WipeContent() {
  // Overwrite before releasing: a decrypted body or attachment left in a freed
  // QByteArray would outlive the tab that was supposed to hold it.
  auto wipe = [](QByteArray& b) {
    if (!b.isEmpty()) b.fill('\0');
    b.clear();
  };

  wipe(message_.body);
  for (auto& att : message_.attachments) wipe(att.data);
  message_.attachments.clear();
  message_ = EMailMetaData{};

  // The tree holds a decoded copy of every leaf, so it is a second place the
  // plaintext of a decrypted message lives and has to be zeroed as well.
  std::function<void(EMailPart&)> wipe_tree = [&](EMailPart& part) {
    wipe(part.data);
    for (auto& child : part.children) wipe_tree(child);
  };
  wipe_tree(tree_root_);
  tree_root_ = EMailPart{};
  regions_.clear();
  discard_verification();
  recipient_rows_.clear();
  inspection_stale_ = false;
  security_state_ = EMailSecurityState::kPLAIN;
  view_state_ = EMailViewState{};
  compose_ = EMailComposeState{};

  if (!last_source_.isEmpty()) last_source_.fill('\0');
  last_source_.clear();
  source_is_original_ = false;
  document_is_received_ = false;

  // An unwrapped message the host has not collected yet is plaintext just as
  // much as the document is, so it is wiped with it.
  if (!pending_replacement_.isEmpty()) pending_replacement_.fill('\0');
  pending_replacement_.clear();

  loading_ = true;
  body_edit_->clear();
  from_edit_->clear();
  to_edit_->clear();
  cc_edit_->clear();
  bcc_edit_->clear();
  subject_edit_->clear();
  attachment_list_->clear();
  structure_view_->Clear();
  header_view_->Clear();
  security_view_->Clear();
  body_view_->Clear();
  body_stack_->setCurrentIndex(0);
  loading_ = false;

  // Back to what a new, empty draft looks like -- including the security
  // button, which is how signing and encrypting are reached.
  refresh_security_button();

  dirty_ = false;
}

void EMailPageView::slot_selection_changed() {
  const auto selected = attachment_list_->selectedItems().size();
  const auto total = attachment_list_->topLevelItemCount();

  // Selection alone is not enough to remove a part: a signed or encrypted
  // message does not change, and this runs AFTER apply_content_lock() on every
  // refresh, so without the lock here it handed the button straight back.
  // Saving a part out does not change the message, so those two only follow
  // the selection.
  const auto locked = content_lock() != EMailLockReason::kNONE;
  remove_button_->setEnabled(selected > 0 && !locked);
  save_button_->setEnabled(selected > 0);
  save_all_button_->setEnabled(total > 0);

  // Hidden rather than permanently greyed out: on a message with no
  // attachments these two can never apply, and a row of dead buttons is just
  // clutter.
  save_button_->setVisible(total > 0);
  save_all_button_->setVisible(total > 0);
  remove_button_->setVisible(total > 0);
}

void EMailPageView::slot_add_attachment() {
  auto default_dir = UnStrDup(GFUIDefaultUserFilePath());
  if (default_dir.isEmpty()) default_dir = QDir::homePath();

  attach_paths(
      QFileDialog::getOpenFileNames(this, tr("Attach Files"), default_dir));
}

void EMailPageView::attach_paths(const QStringList& paths) {
  if (paths.isEmpty()) return;

  // Attaching changes the message. A locked one -- signed, encrypted or open
  // for inspection -- does not change, so this is the last line of defence
  // behind the disabled button and the refused drag.
  if (content_lock() != EMailLockReason::kNONE) return;

  // Collected rather than reported as they happen: a folder dropped on the
  // message can easily be twenty unreadable paths, and twenty modal dialogs in
  // a row is a worse way to learn that than one list.
  QStringList refused;

  for (const auto& path : paths) {
    // The same guards the .eml open path uses, for the same reason. A drop
    // hands over whatever the user dragged: a folder, a FIFO, a device node.
    // The kind of file has to be settled BEFORE anything is read, because
    // those report a size of 0 and then block forever or grow without bound
    // inside the read -- here, on the GUI thread, with no way to cancel.
    QByteArray data;
    QString error;
    qint64 size = 0;
    const auto admission =
        ReadFileWithin(path, kMaxReadFileSize, data, error, size);

    if (admission == EMailFileAdmission::kNOT_REGULAR) {
      refused.append(QFileInfo(path).isDir()
                         ? tr("%1 is a folder").arg(path)
                         : tr("%1 is not an ordinary file").arg(path));
      continue;
    }
    if (admission == EMailFileAdmission::kTOO_LARGE) {
      refused.append(
          tr("%1 is too large (%2); the most that can be attached "
             "is %3")
              .arg(path, HumanSize(size), HumanSize(kMaxReadFileSize)));
      continue;
    }
    if (admission != EMailFileAdmission::kOK) {
      refused.append(tr("%1: %2").arg(path, error));
      continue;
    }

    EMailAttachment att;
    att.filename = QFileInfo(path).fileName();
    att.mime_type = GuessMimeType(path);
    att.data = data;
    att.disposition = "attachment";
    // Composed here, so it will be inside whatever this message gets signed
    // with. Nothing is claimed about a signature that does not exist yet.
    att.inside_signed_part = false;
    message_.attachments.append(att);
  }

  // Only when something actually arrived. A drop where every file was refused
  // left the document marked modified, so closing the tab then asked whether
  // to save a change that had never been made.
  if (refused.size() < paths.size()) {
    refresh_attachments();
    mark_dirty();
  }

  if (!refused.isEmpty()) {
    QMessageBox::warning(
        this, tr("Attach Files"),
        refused.size() == paths.size()
            ? tr("Nothing was attached:\n\n%1").arg(refused.join("\n"))
            : tr("These were not attached:\n\n%1").arg(refused.join("\n")));
  }
}

void EMailPageView::dragEnterEvent(QDragEnterEvent* event) {
  // Only files, and only when this document may change at all. A locked
  // message -- signed, encrypted or open for inspection -- refuses the drag
  // outright rather than accepting it and silently doing nothing.
  if (content_lock() == EMailLockReason::kNONE &&
      event->mimeData() != nullptr && event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
    return;
  }
  event->ignore();
}

void EMailPageView::dragMoveEvent(QDragMoveEvent* event) {
  if (content_lock() == EMailLockReason::kNONE &&
      event->mimeData() != nullptr && event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
    return;
  }
  event->ignore();
}

void EMailPageView::dropEvent(QDropEvent* event) {
  if (content_lock() != EMailLockReason::kNONE ||
      event->mimeData() == nullptr) {
    event->ignore();
    return;
  }

  QStringList paths;
  for (const auto& url : event->mimeData()->urls()) {
    // Local files only: a remote URL would mean fetching it, and nothing here
    // goes to the network.
    if (!url.isLocalFile()) continue;
    const auto path = url.toLocalFile();
    if (!path.isEmpty()) paths.append(path);
  }

  if (paths.isEmpty()) {
    event->ignore();
    return;
  }

  event->acceptProposedAction();
  attach_paths(paths);
}

void EMailPageView::slot_attachment_menu(const QPoint& pos) {
  auto* item = attachment_list_->itemAt(pos);
  if (item == nullptr) return;

  const auto row = attachment_list_->indexOfTopLevelItem(item);
  if (row < 0 || row >= message_.attachments.size()) return;
  const auto& att = message_.attachments.at(row);

  // The row under the cursor is what the menu is about, so it is what the menu
  // acts on. Clicking a row already selects it, which keeps this in step with
  // the buttons, which act on the selection.
  if (!item->isSelected()) {
    attachment_list_->setCurrentItem(item);
  }

  QMenu menu(this);
  auto* open = menu.addAction(tr("Open"));
  auto* save = menu.addAction(tr("Save..."));

  QAction* import = nullptr;
  if (att.is_openpgp_key) {
    menu.addSeparator();
    // Offered only where it means something. A message carrying a public key
    // is how most first contacts arrive, and until now the only way to act on
    // one was to save it and import it from the key manager.
    import = menu.addAction(tr("Import Key"));
  }

  menu.addSeparator();
  auto* copy_name = menu.addAction(tr("Copy Name"));
  auto* remove = menu.addAction(tr("Remove"));
  remove->setEnabled(content_lock() == EMailLockReason::kNONE);

  auto* chosen = menu.exec(attachment_list_->viewport()->mapToGlobal(pos));
  if (chosen == nullptr) return;

  if (chosen == open) {
    open_attachment(item);
  } else if (chosen == save) {
    slot_save_attachment();
  } else if (import != nullptr && chosen == import) {
    import_attachment_key(att);
  } else if (chosen == copy_name) {
    QApplication::clipboard()->setText(att.filename);
  } else if (chosen == remove) {
    slot_remove_attachment();
  }
}

auto EMailPageView::message_key_parts() const -> QList<const EMailPart*> {
  QList<const EMailPart*> keys;
  for (const auto* part : FlattenMimeTree(tree_root_)) {
    if (part != nullptr && part->is_openpgp_key && !part->data.isEmpty()) {
      keys.append(part);
    }
  }
  return keys;
}

void EMailPageView::import_message_keys() {
  const auto keys = message_key_parts();
  if (keys.isEmpty()) return;

  // Every key part in the message, in one go. A message that carries two of
  // them carries them for the same reason, and asking twice about one act the
  // user already asked for would be a dialog per part.
  for (const auto* part : keys) {
    GFGpgImportKeys(GFGpgCurrentGpgContextChannel(), this,
                    part->data.constData(),
                    static_cast<int>(part->data.size()));
  }

  // The keyring is what the Security tab was reporting against, so it has to
  // be asked again now rather than on the next visit -- the user is looking
  // straight at the row they just acted on.
  NotifyKeyringChanged();
  refresh_security();
}

void EMailPageView::import_attachment_key(const EMailAttachment& att) {
  // The host owns the import, including whatever it wants to say about what
  // came of it: this module has no business inventing a second report of the
  // same operation.
  GFGpgImportKeys(GFGpgCurrentGpgContextChannel(), this, att.data.constData(),
                  static_cast<int>(att.data.size()));
}

void EMailPageView::open_attachment(QTreeWidgetItem* item) {
  if (item == nullptr) return;

  const auto row = attachment_list_->indexOfTopLevelItem(item);
  if (row < 0 || row >= message_.attachments.size()) return;
  const auto& att = message_.attachments.at(row);

  const auto name = SanitizeAttachmentFileName(att.filename, att.mime_type);

  // Handing a file from a message straight to whatever the desktop has
  // registered for it is how mail clients have historically run attachments
  // for people. What can be opened here is limited to the types where that is
  // a viewing, not an execution -- everything else is offered as a save, which
  // puts the decision somewhere the user can see it.
  if (!IsSafeToOpenAttachment(att)) {
    QMessageBox::information(
        this, tr("Open Attachment"),
        tr("%1 is not a kind of file this program will open for you, because "
           "opening it would mean running it or handing it to something that "
           "might.\n\nSave it instead, and open it yourself if you are sure "
           "of where it came from.")
            .arg(name));
    slot_save_attachment();
    return;
  }

  // Written where the user cannot mistake it for a file of their own, and
  // named so the application that opens it sees the extension it expects.
  auto* temp = new QTemporaryDir();
  if (!temp->isValid()) {
    delete temp;
    QMessageBox::warning(this, tr("Open Attachment"),
                         tr("A temporary folder to open %1 from could not be "
                            "created.")
                             .arg(name));
    return;
  }

  // Kept alive for as long as this view is: the viewer is a separate process
  // and may still be reading the file long after this function has returned.
  // Removed when the tab closes, which is the last moment it is still ours to
  // remove.
  temp_dirs_.emplace_back(temp);

  if (!write_attachment(att, temp->path(), name)) {
    QMessageBox::warning(
        this, tr("Open Attachment"),
        tr("%1 could not be written out to be opened.").arg(name));
    return;
  }

  if (!QDesktopServices::openUrl(
          QUrl::fromLocalFile(QDir(temp->path()).filePath(name)))) {
    QMessageBox::warning(
        this, tr("Open Attachment"),
        tr("Nothing on this system is registered to open %1.").arg(name));
  }
}

void EMailPageView::slot_remove_attachment() {
  // Removing a part changes the message, so it follows the same lock as
  // adding one. The button is already disabled; this is the backstop.
  if (content_lock() != EMailLockReason::kNONE) return;

  const auto selected = attachment_list_->selectedItems();
  if (selected.isEmpty()) return;

  // Collect indices first: removing while iterating would shift the rest.
  QList<int> rows;
  for (auto* item : selected) {
    rows.append(attachment_list_->indexOfTopLevelItem(item));
  }
  std::sort(rows.begin(), rows.end(), std::greater<>());

  // On a message that arrived from somewhere else, these bytes may be the only
  // copy in existence -- an attachment out of a decrypted message has never
  // been on disk -- and the removal below wipes the buffer before dropping it.
  // Asked only here: warning every time a file is taken back out of one's own
  // draft would be noise, and there the original is still where it came from.
  if (document_is_received_) {
    QStringList names;
    for (const auto row : rows) {
      if (row >= 0 && row < message_.attachments.size()) {
        names.prepend(
            SanitizeAttachmentFileName(message_.attachments[row].filename,
                                       message_.attachments[row].mime_type));
      }
    }

    if (QMessageBox::question(
            this, tr("Remove Attachment"),
            tr("Take out of this message:\n\n%1\n\nThis message came from "
               "somewhere else, so unless these have been saved already this "
               "is the only copy. Removing them cannot be undone.")
                .arg(names.join("\n")),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel) != QMessageBox::Yes) {
      return;
    }
  }

  for (const auto row : rows) {
    if (row >= 0 && row < message_.attachments.size()) {
      auto& data = message_.attachments[row].data;
      if (!data.isEmpty()) data.fill('\0');
      message_.attachments.removeAt(row);
    }
  }

  refresh_attachments();
  mark_dirty();
}

auto EMailPageView::write_attachment(const EMailAttachment& att,
                                     const QString& dir, const QString& name)
    -> bool {
  const QDir target(dir);
  const auto path = target.filePath(name);

  // The name has already been sanitized, so this is the second line rather
  // than the first: if anything ever slips through, the write still cannot
  // land outside the directory the user picked.
  const auto canonical_dir = QFileInfo(dir).canonicalFilePath();
  const auto resolved = QFileInfo(path).absolutePath();
  if (canonical_dir.isEmpty() ||
      QFileInfo(resolved).canonicalFilePath() != canonical_dir) {
    MLogWarn("refusing to write an attachment outside the chosen directory");
    return false;
  }

  // Atomic: a failure part-way through leaves no truncated file behind.
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) return false;
  if (file.write(att.data) != att.data.size()) return false;
  return file.commit();
}

void EMailPageView::slot_save_attachment() {
  const auto selected = attachment_list_->selectedItems();
  if (selected.isEmpty()) return;

  QList<EMailAttachment> chosen;
  for (auto* item : selected) {
    const auto row = attachment_list_->indexOfTopLevelItem(item);
    if (row >= 0 && row < message_.attachments.size()) {
      chosen.append(message_.attachments[row]);
    }
  }
  save_attachments(chosen);
}

void EMailPageView::slot_save_all_attachments() {
  // Every attachment, passed explicitly. This used to call selectAll() and
  // reuse the selection path, which left everything selected afterwards -- so
  // a Save All followed by a Remove took out the whole message's attachments
  // rather than the one row the user thought was still selected.
  save_attachments(message_.attachments);
}

void EMailPageView::save_attachments(const QList<EMailAttachment>& chosen) {
  if (chosen.isEmpty()) return;

  auto default_dir = UnStrDup(GFUIDefaultUserFilePath());
  if (default_dir.isEmpty()) default_dir = QDir::homePath();

  if (chosen.size() == 1) {
    const auto suggested =
        QDir(default_dir)
            .filePath(SanitizeAttachmentFileName(chosen.front().filename,
                                                 chosen.front().mime_type));
    const auto path =
        QFileDialog::getSaveFileName(this, tr("Save Attachment"), suggested);
    if (path.isEmpty()) return;

    QSaveFile file(path);
    const bool ok =
        file.open(QIODevice::WriteOnly) &&
        file.write(chosen.front().data) == chosen.front().data.size() &&
        file.commit();
    if (!ok) {
      QMessageBox::warning(this, tr("Save Attachment"),
                           tr("Cannot write %1.").arg(path));
    }
    return;
  }

  const auto dir = QFileDialog::getExistingDirectory(
      this, tr("Save Attachments"), default_dir);
  if (dir.isEmpty()) return;

  const auto names =
      UniqueAttachmentFileNames(chosen, QDir(dir).entryList(QDir::Files));

  QStringList written;
  QStringList failed;
  for (int i = 0; i < chosen.size(); ++i) {
    if (write_attachment(chosen[i], dir, names[i])) {
      written.append(names[i]);
    } else {
      failed.append(names[i]);
    }
  }

  if (!failed.isEmpty()) {
    QMessageBox::warning(this, tr("Save Attachments"),
                         tr("Could not write: %1").arg(failed.join(", ")));
    return;
  }

  // A rename is still reported, because sanitizing or de-duplicating may have
  // changed a name and a silent rename is worse than a noisy one. A save that
  // wrote what it was asked to write is not: a modal confirming that a routine
  // operation did what it said is a click the user has to spend on nothing.
  QStringList renamed;
  for (int i = 0; i < chosen.size(); ++i) {
    const auto asked =
        SanitizeAttachmentFileName(chosen[i].filename, chosen[i].mime_type);
    if (names[i] != asked) renamed.append(tr("%1 -> %2").arg(asked, names[i]));
  }

  if (!renamed.isEmpty()) {
    QMessageBox::information(
        this, tr("Save Attachments"),
        tr("Saved. Some names were already taken in that folder, so these "
           "were written under a different name:\n\n%1")
            .arg(renamed.join("\n")));
    return;
  }

  report_attachment_status(written.size() == 1
                               ? tr("Saved 1 file.")
                               : tr("Saved %1 files.").arg(written.size()));
}
