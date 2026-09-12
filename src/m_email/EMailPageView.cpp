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
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QStackedWidget>
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
#include "EMailStructureView.h"
#include "EMailViewStyle.h"
#include "GFModuleCommonUtils.hpp"

namespace {

// Shared with the other views in this module; see EMailViewStyle.h.
const auto& ThemeColor = EMailThemeColor;
const auto& MutedColor = EMailMutedColor;
const auto& AccentColor = EMailAccentColor;
const auto& HumanSize = EMailHumanSize;

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
    if (page != nullptr && page == source_view_) {
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
      make_action(QStringLiteral("mail-reply-sender"), ":/icons/quote.png",
                  tr("Reply"), tr("Write a reply to the sender."));
  reply_all_button_ = make_action(
      QStringLiteral("mail-reply-all"), ":/icons/quote.png", tr("Reply All"),
      tr("Write a reply to the sender and everyone else who was addressed. "
         "Blind recipients are not included."));
  forward_button_ = make_action(
      QStringLiteral("mail-forward"), ":/icons/export-email.png", tr("Forward"),
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
      QStringLiteral("object-locked"), ":/icons/lock.png", tr("Read-only"),
      tr("Lock this message so it cannot be edited or rewritten. Reply and "
         "Forward still work and produce new messages."));
  forensic_toggle_->setCheckable(true);
  connect(forensic_toggle_, &QToolButton::toggled, this,
          [this](bool on) { SetForensicMode(on); });

  body_row->addStretch();

  // What the message IS, and which action applies, on the same row as the
  // actions themselves rather than in a band of its own above the tabs. It is
  // the answer to "what do I do with this", so it belongs where the doing is.
  status_banner_ = new QLabel(this);
  status_banner_->setVisible(false);
  status_banner_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  {
    auto font = status_banner_->font();
    font.setPointSizeF(font.pointSizeF() * 0.92);
    status_banner_->setFont(font);
  }
  // Never the reason the row cannot fit: the buttons keep their size and the
  // text gives way, with the full wording always in the tooltip.
  status_banner_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  status_banner_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  status_banner_->installEventFilter(this);
  body_row->addWidget(status_banner_, 1);

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

  body_stack_ = new QStackedWidget(this);
  body_stack_->addWidget(body_edit_);
  body_stack_->addWidget(body_view_);
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
      QStringLiteral("mail-attachment"), ":/icons/add.png", tr("Attach File"),
      tr("Add one or more files to this message. Files can also be dropped "
         "onto the message."));
  // Removing takes a part out of the message being composed; it deletes
  // nothing on disk, and the wording and icon both stay away from suggesting
  // it does.
  remove_button_ = make_attachment_action(
      QStringLiteral("list-remove"), ":/icons/minus.png", tr("Remove"),
      tr("Take the selected attachments out of this message."));
  save_button_ = make_attachment_action(
      QStringLiteral("document-save"), ":/icons/filesave.png", tr("Save"),
      tr("Write the selected attachments to a folder."));
  save_all_button_ = make_attachment_action(
      QStringLiteral("document-save-all"), ":/icons/filesaveas.png",
      tr("Save All"), tr("Write every attachment to a folder."));

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

void EMailPageView::SetForensicMode(bool on) {
  forensic_ = on;

  // Editing is disabled at the widget level as well as at the dirty flag:
  // a read-only field cannot produce an edit to refuse in the first place,
  // and the user can see the document is locked rather than discovering it.
  for (auto* edit :
       {from_edit_, to_edit_, cc_edit_, bcc_edit_, subject_edit_}) {
    edit->setReadOnly(on);
  }
  body_edit_->setReadOnly(on);

  // The raw editor writes straight into the document, so leaving it writable
  // would be a hole right through the lock.
  if (source_view_ != nullptr) source_view_->setProperty("readOnly", on);

  // Attaching and removing change the message; saving a part out does not.
  add_button_->setEnabled(!on);
  remove_button_->setEnabled(!on);

  if (on) {
    dirty_ = false;
    setToolTip(
        tr("This message is open for inspection only. It cannot be edited or "
           "rewritten; Reply and Forward still work, and produce new "
           "messages."));
  } else {
    setToolTip({});
  }
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
  refresh_status_banner();
  refresh_security();

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
  // Last, after Headers: the tabs run from the most interpreted view of the
  // message to the least, ending at the bytes themselves.
  tabs_->addTab(source, QIcon(":/icons/editor.png"), tr("Raw Source"));

  // A locked document cannot be edited here either. Set through the property
  // system because the page hands over a plain QWidget; an editor that does
  // not carry the property simply does not gain a way to be written to.
  if (forensic_) source_view_->setProperty("readOnly", true);
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
  refresh_status_banner();
  refresh_security();
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
  reply_all_button_->setVisible(is_message);
  forward_button_->setVisible(is_message);
  forensic_toggle_->setVisible(is_message);
  if (action_separator_ != nullptr) action_separator_->setVisible(is_message);

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

void EMailPageView::refresh_status_banner() {
  QString text;
  QColor colour = MutedColor(this);

  switch (security_state_) {
    case EMailSecurityState::kENCRYPTED:
      text = tr("Encrypted message. Decrypt it to read the contents.");
      colour = AccentColor(this, true);
      break;
    case EMailSecurityState::kSIGNED:
      text = tr("Signed message. Verify it to check the signature.");
      colour = AccentColor(this, true);
      break;
    case EMailSecurityState::kSIGNED_ENCRYPTED:
      text = tr("Encrypted and signed. Decrypt it, then verify the signature.");
      colour = AccentColor(this, true);
      break;
    case EMailSecurityState::kMALFORMED_PGP:
      text =
          tr("This message claims to use OpenPGP but its structure does not "
             "follow RFC 3156. It may not decrypt or verify.");
      colour = EMailWarningColor(this);
      break;
    case EMailSecurityState::kPLAIN:
      // Stated plainly and quietly. An unprotected message is the ordinary
      // case, not a fault, and painting it as a warning would train the user
      // to ignore the banner that matters.
      text = tr("Not signed or encrypted.");
      break;
  }

  // Nothing to say about a tab the user is still composing.
  const bool has_message =
      !last_source_.isEmpty() && !tree_root_.content_type.isEmpty();
  status_banner_->setVisible(has_message);
  if (!has_message) return;

  auto palette = status_banner_->palette();
  palette.setColor(QPalette::WindowText, colour);
  status_banner_->setPalette(palette);

  // Kept whole in the tooltip and on the property, so narrowing the window
  // shortens what is drawn without ever losing the wording itself.
  status_banner_->setProperty("gf_full_text", text);
  status_banner_->setToolTip(text);
  fit_status_banner();
}

void EMailPageView::fit_status_banner() {
  if (status_banner_ == nullptr) return;

  const auto full = status_banner_->property("gf_full_text").toString();
  if (full.isEmpty()) return;

  const QFontMetrics metrics(status_banner_->font());
  status_banner_->setText(
      metrics.elidedText(full, Qt::ElideRight, status_banner_->width()));
}

auto EMailPageView::eventFilter(QObject* watched, QEvent* event) -> bool {
  if (watched == status_banner_ && event->type() == QEvent::Resize) {
    fit_status_banner();
  }
  return QWidget::eventFilter(watched, event);
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

auto EMailPageView::IsDirty() -> bool { return dirty_; }

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
  status_banner_->clear();
  status_banner_->setVisible(false);
  loading_ = false;

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

  // Attaching changes the message, which a forensic document does not do.
  if (forensic_) return;

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
  // Only files, and only when this document may change at all. A forensic
  // message refuses the drag outright rather than accepting it and silently
  // doing nothing.
  if (!forensic_ && event->mimeData() != nullptr &&
      event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
    return;
  }
  event->ignore();
}

void EMailPageView::dragMoveEvent(QDragMoveEvent* event) {
  if (!forensic_ && event->mimeData() != nullptr &&
      event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
    return;
  }
  event->ignore();
}

void EMailPageView::dropEvent(QDropEvent* event) {
  if (forensic_ || event->mimeData() == nullptr) {
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
