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

#include <QApplication>
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
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <functional>

#include "EMailBasicGpgOpera.h"
#include "EMailBodyView.h"
#include "EMailHeaderView.h"
#include "EMailHelper.h"
#include "EMailSecurityView.h"
#include "EMailSendDialog.h"
#include "EMailStructureView.h"
#include "EMailViewStyle.h"
#include "GFModuleCommonUtils.hpp"

namespace {

// Shared with the other views in this module; see EMailViewStyle.h.
const auto& ThemeColor = EMailThemeColor;
const auto& MutedColor = EMailMutedColor;
const auto& AccentColor = EMailAccentColor;
const auto& HumanSize = EMailHumanSize;

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

  tabs_ = new QTabWidget(this);
  tabs_->setDocumentMode(true);
  outer->addWidget(tabs_, 1);

  tabs_->addTab(build_message_tab(), tr("Message"));

  structure_view_ = new EMailStructureView(this);
  tabs_->addTab(structure_view_, tr("Structure"));

  security_view_ = new EMailSecurityView(this);
  tabs_->addTab(security_view_, tr("Security"));

  header_view_ = new EMailHeaderView(this);
  tabs_->addTab(header_view_, tr("Headers"));

  connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) {
    auto* page = tabs_->widget(index);

    // The raw document is the host's, and it may be behind this view's edits.
    // Asking for it to be brought up to date is the whole reason the host
    // wants to hear about this switch.
    if (page != nullptr && page == raw_tab_) {
      emit SignalSourceViewRequested();
      return;
    }

    if (page != structure_view_ && page != security_view_ &&
        page != header_view_) {
      return;
    }

    // Whatever was typed in the Message tab is part of the message now, so
    // the tab being opened has to describe that rather than what was loaded.
    sync_inspection();

    if (page != security_view_) return;
    ensure_regions_verified();
    refresh_security();
  });
}

auto EMailPageView::build_message_tab() -> QWidget* {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 6, 0, 0);
  layout->setSpacing(6);

  auto* form = new QFormLayout();
  form->setLabelAlignment(Qt::AlignRight);
  form->setHorizontalSpacing(8);
  form->setVerticalSpacing(3);
  form->setContentsMargins(0, 0, 0, 0);

  from_edit_ = new QLineEdit(this);
  to_edit_ = new QLineEdit(this);
  cc_edit_ = new QLineEdit(this);
  bcc_edit_ = new QLineEdit(this);
  subject_edit_ = new QLineEdit(this);

  // Separator is shown rather than assumed: the old dialog explained it in a
  // tips label the user had to find first.
  const auto multi = tr("separate several addresses with \";\"");
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
    auto palette = label->palette();
    palette.setColor(QPalette::WindowText, MutedColor(this));
    label->setPalette(palette);
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
  cc_bcc_toggle_->setToolTip(tr("Show carbon copy and blind carbon copy"));
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

  layout->addLayout(form);

  // A hairline between the envelope and the message, so the two stop running
  // together into one undifferentiated column of boxes.
  auto* rule = new QFrame(this);
  rule->setFrameShape(QFrame::HLine);
  rule->setFrameShadow(QFrame::Plain);
  auto rule_palette = rule->palette();
  rule_palette.setColor(QPalette::WindowText,
                        ThemeColor(this, &GFUIBorderColor));
  rule->setPalette(rule_palette);
  rule->setFixedHeight(1);
  layout->addWidget(rule);

  auto* body_row = new QHBoxLayout();
  body_row->setContentsMargins(0, 0, 0, 0);
  body_row->setSpacing(2);

  // Message-level actions. Each produces a NEW document in a new tab; none of
  // them writes to this one.
  //
  // Icons come from the desktop theme where there is one, because these three
  // actions have long-settled, instantly recognisable icons that no bundled
  // substitute would improve on. The bundled files are the fallback for the
  // platforms that have no icon theme at all.
  const auto make_action =
      [this, body_row](const QString& theme_icon, const QString& fallback_icon,
                       const QString& text, const QString& tip) {
        auto* button = new QToolButton(this);
        button->setIcon(QIcon::fromTheme(theme_icon, QIcon(fallback_icon)));
        button->setText(text);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setAutoRaise(true);
        button->setToolTip(tip);
        button->setVisible(false);
        body_row->addWidget(button);
        return button;
      };

  reply_button_ =
      make_action(QStringLiteral("mail-reply-sender"), ":/icons/reply.png",
                  tr("Reply"), tr("Write a reply to the sender."));
  reply_all_button_ = make_action(
      QStringLiteral("mail-reply-all"), ":/icons/reply-all.png",
      tr("Reply All"),
      tr("Write a reply to the sender and everyone else who was addressed. "
         "Blind recipients are not included."));
  forward_button_ = make_action(
      QStringLiteral("mail-forward"), ":/icons/redo.png", tr("Forward"),
      tr("Pass this message on, with its attachments."));

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
                                "mail account."));
  connect(send_button_, &QToolButton::clicked, this,
          &EMailPageView::slot_send_message);

  // Locking the document is a different kind of act from deriving a new
  // message out of it, so it is set apart rather than lined up with them.
  auto* action_separator = new QFrame(this);
  action_separator->setFrameShape(QFrame::VLine);
  action_separator->setFrameShadow(QFrame::Plain);
  action_separator->setFixedWidth(1);
  {
    auto separator_palette = action_separator->palette();
    separator_palette.setColor(QPalette::WindowText,
                               ThemeColor(this, &GFUIBorderColor));
    action_separator->setPalette(separator_palette);
  }
  action_separator->setVisible(false);
  body_row->addSpacing(4);
  body_row->addWidget(action_separator);
  body_row->addSpacing(4);
  action_separator_ = action_separator;

  forensic_toggle_ = make_action(
      QStringLiteral("object-locked"), ":/icons/read-only.png", tr("Read-only"),
      tr("Lock this message so it cannot be edited or rewritten. Reply and "
         "Forward still work and produce new messages."));
  forensic_toggle_->setCheckable(true);
  connect(forensic_toggle_, &QToolButton::toggled, this,
          [this](bool on) { SetForensicMode(on); });

  body_row->addStretch();

  // What the message IS, and which action applies, on the same row as the
  // actions themselves rather than in a band of its own above the tabs. It is
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

  body_mode_toggle_ = new QToolButton(this);
  body_mode_toggle_->setText(tr("Plain text"));
  body_mode_toggle_->setCheckable(true);
  body_mode_toggle_->setAutoRaise(true);
  body_mode_toggle_->setToolTip(
      tr("Switch between the formatted message and its source text."));
  body_mode_toggle_->setVisible(false);
  body_row->addSpacing(4);
  body_row->addWidget(body_mode_toggle_);
  layout->addLayout(body_row);

  body_edit_ = new QPlainTextEdit(this);
  body_edit_->setPlaceholderText(tr("Write your message here."));

  body_view_ = new EMailBodyView(this);

  locked_notice_ = new QLabel(this);
  locked_notice_->setWordWrap(true);
  locked_notice_->setVisible(false);
  {
    auto palette = locked_notice_->palette();
    palette.setColor(QPalette::WindowText, MutedColor(this));
    locked_notice_->setPalette(palette);
    auto font = locked_notice_->font();
    font.setPointSizeF(font.pointSizeF() * 0.92);
    locked_notice_->setFont(font);
  }
  layout->addWidget(locked_notice_);

  locked_panel_ = build_locked_panel();

  body_stack_ = new QStackedWidget(this);
  body_stack_->addWidget(body_edit_);
  body_stack_->addWidget(body_view_);
  body_stack_->addWidget(locked_panel_);
  layout->addWidget(body_stack_, 1);

  remote_content_notice_ = new QLabel(this);
  remote_content_notice_->setWordWrap(true);
  remote_content_notice_->setVisible(false);
  {
    auto palette = remote_content_notice_->palette();
    palette.setColor(QPalette::WindowText, MutedColor(this));
    remote_content_notice_->setPalette(palette);
    auto font = remote_content_notice_->font();
    font.setPointSizeF(font.pointSizeF() * 0.92);
    remote_content_notice_->setFont(font);
  }
  layout->addWidget(remote_content_notice_);

  connect(body_mode_toggle_, &QToolButton::toggled, this,
          [this](bool plain) { body_stack_->setCurrentIndex(plain ? 0 : 1); });
  connect(body_view_, &EMailBodyView::SignalRemoteContentBlocked, this,
          [this]() {
            remote_content_notice_->setVisible(true);
            remote_content_notice_->setText(
                tr("This message asked to load images or other content from "
                   "the internet. That was not done: fetching it would tell "
                   "the sender that you opened the message."));
          });

  attachment_heading_ = new QLabel(this);
  {
    auto palette = attachment_heading_->palette();
    palette.setColor(QPalette::WindowText, MutedColor(this));
    attachment_heading_->setPalette(palette);
    auto font = attachment_heading_->font();
    font.setPointSizeF(font.pointSizeF() * 0.92);
    attachment_heading_->setFont(font);
  }
  attachment_heading_->setVisible(false);
  layout->addWidget(attachment_heading_);

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
  attachment_list_->setMaximumHeight(160);
  attachment_list_->setVisible(false);
  layout->addWidget(attachment_list_);

  unsigned_notice_ = new QLabel(this);
  unsigned_notice_->setWordWrap(true);
  unsigned_notice_->setVisible(false);
  {
    auto palette = unsigned_notice_->palette();
    palette.setColor(QPalette::WindowText, ThemeColor(this, &GFUIWarningColor));
    unsigned_notice_->setPalette(palette);
    auto font = unsigned_notice_->font();
    font.setPointSizeF(font.pointSizeF() * 0.92);
    unsigned_notice_->setFont(font);
  }
  layout->addWidget(unsigned_notice_);

  // The same shape as the actions above: flat, icon beside text. These used
  // to be raised push buttons, which gave the attachment list a heavier
  // footer than the message itself.
  auto* buttons = new QHBoxLayout();
  buttons->setContentsMargins(0, 0, 0, 0);
  buttons->setSpacing(2);

  const auto make_attachment_action =
      [this](const QString& theme_icon, const QString& fallback_icon,
             const QString& text, const QString& tip) {
        auto* button = new QToolButton(this);
        button->setIcon(QIcon::fromTheme(theme_icon, QIcon(fallback_icon)));
        button->setText(text);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setAutoRaise(true);
        button->setToolTip(tip);
        return button;
      };

  add_button_ = make_attachment_action(
      QStringLiteral("mail-attachment"), ":/icons/attachment.png",
      tr("Attach File…"),
      tr("Add one or more files to this message. Files can also be dropped "
         "onto the message."));
  // Removing takes a part out of the message being composed; it deletes
  // nothing on disk, and the wording and icon both stay away from suggesting
  // it does.
  remove_button_ = make_attachment_action(
      QStringLiteral("list-remove"), ":/icons/remove.png", tr("Remove"),
      tr("Take the selected attachments out of this message."));
  save_button_ = make_attachment_action(
      QStringLiteral("document-save"), ":/icons/filesave.png", tr("Save…"),
      tr("Write the selected attachments to a folder."));
  save_all_button_ = make_attachment_action(
      QStringLiteral("document-save-all"), ":/icons/save-all.png",
      tr("Save All…"), tr("Write every attachment to a folder."));

  buttons->addWidget(add_button_);
  buttons->addWidget(remove_button_);
  buttons->addStretch();
  buttons->addWidget(save_button_);
  buttons->addWidget(save_all_button_);
  layout->addLayout(buttons);

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

void EMailPageView::set_cc_bcc_visible(bool visible) {
  if (header_form_ == nullptr) return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  header_form_->setRowVisible(cc_edit_, visible);
  header_form_->setRowVisible(bcc_edit_, visible);
#else
  // Older Qt has no setRowVisible; hiding both cells collapses the row.
  const auto hide_row = [this](QWidget* field) {
    if (auto* label = header_form_->labelForField(field)) {
      label->setVisible(visible);
    }
    field->setVisible(visible);
  };
  hide_row(cc_edit_);
  hide_row(bcc_edit_);
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

  QString notice;
  switch (reason) {
    case EMailLockReason::kNONE:
      break;
    case EMailLockReason::kFORENSIC:
      notice =
          tr("This message is open for inspection only. It cannot be edited or "
             "rewritten; Reply and Forward still work, and produce new "
             "messages.");
      break;
    case EMailLockReason::kPROTECTED:
      switch (security_state_) {
        case EMailSecurityState::kENCRYPTED:
          notice =
              tr("This message is encrypted. Decrypt it before editing it.");
          break;
        case EMailSecurityState::kSIGNED_ENCRYPTED:
          notice = tr(
              "This message is encrypted and signed. Decrypt it, then remove "
              "the signature, before editing it.");
          break;
        default:
          notice =
              tr("This message is signed. Remove the signature before editing "
                 "it -- an edit under a signature reads as a forgery.");
          break;
      }
      break;
  }

  setToolTip(notice);
  if (locked_notice_ != nullptr) {
    locked_notice_->setText(notice);
    locked_notice_->setVisible(locked && !notice.isEmpty());
  }

  refresh_raw_lock_ui();
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

  vmime::shared_ptr<vmime::message> parsed;
  if (CheckIfEMLMessage(source, parsed)) {
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
}

void EMailPageView::refresh_structure() {
  tree_root_ = EMailPart{};
  regions_.clear();
  security_state_ = EMailSecurityState::kPLAIN;
  // Results belong to the document that produced them. A new load has not
  // verified anything yet, and carrying the previous message's verdicts over
  // would be worse than showing none.
  signature_results_.clear();
  recipient_rows_.clear();
  regions_verified_ = false;

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

  // The editor does not go into the tab bare: it gets a row above it saying
  // whether it may be written to, and the control that changes that.
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

  // Last, after Headers: the tabs run from the most interpreted view of the
  // message to the least, ending at the bytes themselves.
  tabs_->addTab(raw_tab_, QIcon(":/icons/code.png"), tr("Raw Source"));

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
        notice = tr(
            "Read-only. These bytes are protected -- remove the signature, or "
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
  if (regions_.isEmpty()) return;

  // The previous answers were about a keyring that no longer exists.
  regions_verified_ = false;
  signature_results_.clear();

  // Only redo the work now if it is being looked at; otherwise the next visit
  // to the Security tab picks it up, which is where the verification is
  // normally triggered anyway.
  if (tabs_ != nullptr && tabs_->currentWidget() == security_view_) {
    ensure_regions_verified();
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

  QString eml;
  if (BuildMimeEML(message_, message_.body, message_.attachments, eml) != 0) {
    MLogWarn("failed to serialize the edited message for inspection: " + eml);
    // Show nothing rather than the previous document's structure: a stale tree
    // presented as the current one is worse than an empty tab. The flag stays
    // set, so the next visit tries again.
    structure_view_->Clear();
    header_view_->Clear();
    security_view_->Clear();
    return;
  }

  inspection_stale_ = false;
  // The inspection views read raw byte ranges out of this, so it has to be the
  // document they are describing. Still dirty: the host has yet to be given
  // these bytes, and only SaveToSource() settles that.
  last_source_ = eml.toUtf8();

  refresh_structure();
  refresh_security_button();
  refresh_security();
  apply_content_lock();
}

void EMailPageView::ensure_regions_verified() {
  if (regions_verified_ || regions_.isEmpty() || last_source_.isEmpty()) return;

  // Once per load. A failed or partial verification is still an answer, and
  // retrying it every time the tab is focused would re-run the engine for no
  // new information.
  regions_verified_ = true;

  signature_results_.clear();
  VerifyEMLRegions(GFGpgCurrentGpgContextChannel(), last_source_, tree_root_,
                   regions_, signature_results_);
}

void EMailPageView::slot_derive_message(int mode) {
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

  QString eml;
  if (BuildMimeEML(derived, quoted, derived.attachments, eml) != 0) {
    MLogWarn("failed to build the derived message");
    return;
  }

  QWidget* page = nullptr;
  QMetaObject::invokeMethod(
      edit, "SlotNewCustomTab", Qt::DirectConnection,
      Q_RETURN_ARG(QWidget*, page), Q_ARG(QString, "email"),
      Q_ARG(QString, "untitled.eml"), Q_ARG(QIcon, QIcon(":/icons/email.png")),
      Q_ARG(QString, ":/icons/email.png"));

  if (page == nullptr) {
    MLogWarn("the host did not create a tab for the derived message");
    return;
  }

  // The new tab mounts a view of this same type, so it can be addressed
  // directly rather than through the host's document.
  auto* view = page->findChild<EMailPageView*>();
  if (view == nullptr) {
    MLogWarn("the new tab has no message view to fill");
    return;
  }

  view->LoadFromSource(eml.toUtf8());
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
  {
    auto palette = locked_recipients_->palette();
    palette.setColor(QPalette::WindowText, MutedColor(this));
    locked_recipients_->setPalette(palette);
  }
  layout->addWidget(locked_recipients_);

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

  locked_decrypt_button_->setEnabled(true);
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
          tr("This message is signed, so it cannot be changed -- an edit "
             "under a signature reads as a forgery.\n\nRemove the signature "
             "and make it an ordinary message?"),
          QMessageBox::Yes | QMessageBox::Cancel,
          QMessageBox::Cancel) == QMessageBox::Yes) {
    slot_remove_protection_layer();
  }
  return false;
}

void EMailPageView::refresh_body_view() {
  remote_content_notice_->setVisible(false);

  // Only a message that actually parsed has a formatted form to offer; a draft
  // being typed is plain text and nothing else.
  const auto* html_body = SelectBodyPart(tree_root_, true);
  const bool has_html =
      html_body != nullptr && html_body->content_type == "text/html";

  body_mode_toggle_->setVisible(has_html);

  // Only offered for a message that parsed: there is nothing to reply to in a
  // draft the user is still typing.
  const bool is_message = !tree_root_.content_type.isEmpty();
  reply_button_->setVisible(is_message);
  // Offered whenever there is a message and an account that could send it.
  // Hidden rather than disabled when no account exists: an always-dead button
  // is just clutter on a workspace that may never send anything.
  send_button_->setVisible(is_message && EMailSendDialog::HasUsableAccount());
  reply_all_button_->setVisible(is_message);
  forward_button_->setVisible(is_message);
  forensic_toggle_->setVisible(is_message);
  if (action_separator_ != nullptr) action_separator_->setVisible(is_message);

  // Ciphertext is not text the user can read or edit, so it is not offered as
  // either. Deliberately after the block above: Reply and Forward derive a new
  // message and never touch this one, so they stay available on a message that
  // cannot be read.
  const bool locked = security_state_ == EMailSecurityState::kENCRYPTED ||
                      security_state_ == EMailSecurityState::kSIGNED_ENCRYPTED;
  if (locked) {
    refresh_locked_panel();
    body_mode_toggle_->setVisible(false);
    body_view_->Clear();
    body_stack_->setCurrentIndex(2);
    return;
  }

  if (!has_html) {
    body_view_->Clear();
    body_stack_->setCurrentIndex(0);
    return;
  }

  body_view_->SetBody(html_body, tree_root_);

  // Default to the formatted form, which is how the sender meant it to read.
  // The source stays one click away.
  QSignalBlocker blocker(body_mode_toggle_);
  body_mode_toggle_->setChecked(false);
  body_stack_->setCurrentIndex(1);
}

void EMailPageView::refresh_security() {
  QStringList addresses;
  addresses += message_.to;
  addresses += message_.cc;
  addresses += message_.bcc_header;
  addresses += compose_.bcc;
  if (!message_.from.isEmpty()) addresses.append(message_.from);

  const auto findings =
      tree_root_.content_type.isEmpty()
          ? QList<EMailFinding>{}
          : InspectMessage(message_, tree_root_, regions_, last_source_);

  security_view_->SetMessage(security_state_, regions_, signature_results_,
                             recipient_rows_, addresses,
                             GFGpgCurrentGpgContextChannel(), findings);
}

void EMailPageView::refresh_security_button() {
  QString text;
  QString icon;
  QColor colour = MutedColor(this);

  switch (security_state_) {
    case EMailSecurityState::kENCRYPTED:
      text = tr("Encrypted");
      icon = ":/icons/lock.png";
      colour = AccentColor(this, true);
      break;
    case EMailSecurityState::kSIGNED:
      text = tr("Signed");
      icon = ":/icons/signature.png";
      colour = AccentColor(this, true);
      break;
    case EMailSecurityState::kSIGNED_ENCRYPTED:
      text = tr("Encrypted and signed");
      icon = ":/icons/lock.png";
      colour = AccentColor(this, true);
      break;
    case EMailSecurityState::kMALFORMED_PGP:
      text = tr("Malformed OpenPGP structure");
      icon = ":/icons/warning.png";
      colour = EMailWarningColor(this);
      break;
    case EMailSecurityState::kPLAIN:
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

  auto palette = security_button_->palette();
  palette.setColor(QPalette::ButtonText, colour);
  security_button_->setPalette(palette);

  // The button keeps the whole wording and draws as much of it as it was given
  // room for, so narrowing the window never loses what the message is.
  static_cast<ElidingToolButton*>(security_button_)->SetFullText(text);
  rebuild_security_menu();

  // What this message is has just been (re)decided, and with it what can be
  // done to it. The menu bar is driven from the same answer.
  emit SignalCryptoOperationsChanged();
}

auto EMailPageView::AvailableCryptoOperations() -> QStringList {
  // Deliberately permissive. This decides what the menu bar ALLOWS, which is
  // a different question from what the security button's menu SUGGESTS: the
  // menu names the obvious next step, this rules out only what cannot work.
  //
  // In particular a message whose MIME structure is plain is not necessarily
  // free of OpenPGP -- an inline armored block in the body is ordinary, and
  // the module's decrypt and verify handle it. Structure alone cannot see
  // that, so "plain" must not withdraw those operations.
  static const QStringList kAll = {"sign",   "encrypt", "encrypt_sign",
                                   "verify", "decrypt", "decrypt_verify"};
  static const QStringList kReading = {"verify", "decrypt", "decrypt_verify"};

  QStringList operations = kAll;

  if (security_state_ == EMailSecurityState::kENCRYPTED ||
      security_state_ == EMailSecurityState::kSIGNED_ENCRYPTED) {
    // The one case where the rest genuinely cannot apply: nobody has read this
    // yet. Signing or re-encrypting ciphertext says nothing about what is
    // inside it, so only opening it is on offer.
    operations = {"decrypt", "decrypt_verify"};
  }

  if (!forensic_) return operations;

  // A document locked for inspection does not change, and every operation
  // that PRODUCES a message rewrites this tab. Reading one does not, by the
  // same rule that keeps Reply and Forward available on a forensic message.
  QStringList reading;
  for (const auto& operation : operations) {
    if (kReading.contains(operation)) reading.append(operation);
  }
  return reading;
}

void EMailPageView::rebuild_security_menu() {
  security_menu_->clear();

  const auto show_details = [this]() {
    if (tabs_ != nullptr && security_view_ != nullptr) {
      tabs_->setCurrentWidget(security_view_);
    }
  };
  const auto request = [this](const QString& op) {
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
      connect(details, &QAction::triggered, this, show_details);
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
      connect(details, &QAction::triggered, this, show_details);
      break;
    }

    case EMailSecurityState::kSIGNED_ENCRYPTED: {
      add_read(tr("Decrypt and Verify"), "decrypt_verify");
      add_read(tr("Decrypt"), "decrypt");
      security_menu_->addSeparator();
      auto* details = security_menu_->addAction(tr("Details..."));
      connect(details, &QAction::triggered, this, show_details);

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
      connect(details, &QAction::triggered, this, show_details);
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
}

void EMailPageView::refresh_attachments() {
  attachment_list_->clear();

  bool any_unsigned = false;
  bool any_signed_info = false;

  const auto muted = MutedColor(this);

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
    item->setForeground(kColType, muted);
    item->setText(kColSize, HumanSize(att.data.size()));
    item->setForeground(kColSize, muted);
    item->setTextAlignment(kColSize, Qt::AlignRight | Qt::AlignVCenter);

    if (att.inside_signed_part) {
      item->setText(kColSigned, tr("signed"));
      item->setForeground(kColSigned, AccentColor(this, true));
      any_signed_info = true;
    } else {
      // Not painted red: nothing here is broken and nothing is irreversible.
      // The part simply arrived without the signature vouching for it, which
      // is a caveat rather than an alarm.
      item->setText(kColSigned, tr("unsigned"));
      item->setForeground(kColSigned, ThemeColor(this, &GFUIWarningColor));
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
    attachment_list_->setFixedHeight(qMin(wanted, 160));
  }

  attachment_list_->resizeColumnToContents(kColType);
  attachment_list_->resizeColumnToContents(kColSize);
  if (any_signed_info) attachment_list_->resizeColumnToContents(kColSigned);
  slot_selection_changed();
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
    return last_source_;
  }

  collect_fields();

  QString eml;
  if (BuildMimeEML(message_, message_.body, message_.attachments, eml) != 0) {
    MLogWarn("failed to serialize message: " + eml);
    // Hand back exactly what was loaded. Returning the body alone -- which an
    // earlier version did -- replaces the document with the message text and
    // silently drops every header the user typed and every attachment they
    // added. Staying dirty means the next flush tries again rather than
    // treating the failure as saved.
    return last_source_;
  }

  dirty_ = false;
  last_source_ = eml.toUtf8();
  return last_source_;
}

void EMailPageView::slot_send_message() {
  EMailOutgoingMessage message;
  if (!BuildOutgoing(message)) {
    QMessageBox::warning(
        this, tr("Cannot send"),
        tr("This message needs a sender and at least one recipient before it "
           "can be sent."));
    return;
  }

  // Owns its own copy of the frozen bytes: the tab may be edited or closed
  // while the dialog is open, and what was approved must not change underneath
  // the submission.
  auto* dialog = new EMailSendDialog(message, this);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->show();
}

auto EMailPageView::IsDirty() -> bool { return dirty_; }

auto EMailPageView::BuildOutgoing(EMailOutgoingMessage& out) -> bool {
  collect_fields();

  // A clean document is handed over untouched. That is not an optimization:
  // these bytes may carry a signature computed over exactly these octets, and
  // rebuilding them would invalidate it. A forensic document is never
  // rebuilt at all, by the same rule that governs SaveToSource().
  const auto reuse_source = (!dirty_ || forensic_) && !last_source_.isEmpty();

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
  signature_results_.clear();
  recipient_rows_.clear();
  regions_verified_ = false;
  inspection_stale_ = false;
  security_state_ = EMailSecurityState::kPLAIN;
  view_state_ = EMailViewState{};
  compose_ = EMailComposeState{};

  if (!last_source_.isEmpty()) last_source_.fill('\0');
  last_source_.clear();

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
  body_mode_toggle_->setVisible(false);
  remote_content_notice_->setVisible(false);
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

  remove_button_->setEnabled(selected > 0);
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

  for (const auto& path : paths) {
    // Directories arrive from a drop as readily as files do, and reading one
    // yields nothing useful.
    if (QFileInfo(path).isDir()) {
      QMessageBox::warning(
          this, tr("Attach File"),
          tr("%1 is a folder and was not attached.").arg(path));
      continue;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      QMessageBox::warning(
          this, tr("Attach File"),
          tr("Cannot read %1:\n%2.").arg(path, file.errorString()));
      continue;
    }

    EMailAttachment att;
    att.filename = QFileInfo(path).fileName();
    att.mime_type = GuessMimeType(path);
    att.data = file.readAll();
    att.disposition = "attachment";
    // Composed here, so it will be inside whatever this message gets signed
    // with. Nothing is claimed about a signature that does not exist yet.
    att.inside_signed_part = false;
    message_.attachments.append(att);
  }

  refresh_attachments();
  mark_dirty();
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

  // The names are reported back because sanitizing or de-duplicating may have
  // changed them, and a silent rename is worse than a noisy one.
  const auto saved_count = written.size();
  const auto saved_line = saved_count == 1
                              ? tr("Saved 1 file:")
                              : tr("Saved %1 files:").arg(saved_count);
  QMessageBox::information(this, tr("Save Attachments"),
                           saved_line + "\n" + written.join("\n"));
}

void EMailPageView::slot_save_all_attachments() {
  if (message_.attachments.isEmpty()) return;

  attachment_list_->selectAll();
  slot_save_attachment();
}
