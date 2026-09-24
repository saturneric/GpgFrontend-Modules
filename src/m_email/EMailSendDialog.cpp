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

#include "EMailSendDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "EMailAccountStore.h"
#include "EMailCredentialStore.h"
#include "EMailViewStyle.h"
#include "GFModule.h"

namespace {

/// Long enough for the bar not to flash on a fast server, short enough that a
/// slow one never looks like a frozen window. Same value as the mailbox
/// browser uses, for the same reason.
constexpr int kProgressDelayMs = 200;

/// How long a confirmed send stays on screen before the window closes itself.
constexpr int kAutoCloseSeconds = 3;

/// How far the blind-copy strip moves from the page towards the warning
/// colour: enough to read as a caution, not enough to shout.
constexpr double kBannerTint = 0.14;

/// A value in a summary row: server- or user-chosen text, never markup.
auto MakeValueLabel(QWidget* parent) -> QLabel* {
  auto* label = new QLabel(parent);
  label->setWordWrap(true);
  label->setTextFormat(Qt::PlainText);
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  return label;
}

}  // namespace

EMailSendDialog::EMailSendDialog(EMailOutgoingMessage message, QWidget* parent)
    : QDialog(parent), message_(std::move(message)) {
  setWindowTitle(tr("Send message"));
  resize(640, 520);
  build_ui();

  accounts_ = EMailAccountStore::SmtpAccounts();
  for (const auto& account : accounts_) {
    account_combo_->addItem(account.Label().isEmpty() ? account.smtp.host
                                                      : account.Label());
  }

  const auto preferred = EMailAccountStore::DefaultAccount();
  for (int i = 0; i < accounts_.size(); ++i) {
    if (accounts_.at(i).id != preferred.id) continue;
    account_combo_->setCurrentIndex(i);
    break;
  }

  refresh_account_state();
  // Paints the three result lines in their "nothing has happened yet" state,
  // so the block is legible the moment the dialog opens rather than blank
  // until the first signal arrives.
  refresh_result();
}

EMailSendDialog::~EMailSendDialog() { stop_workers(); }

auto EMailSendDialog::HasUsableAccount() -> bool {
  return !EMailAccountStore::SmtpAccounts().isEmpty();
}

/**
 * @brief What is about to be sent, said once at the top.
 *
 * The subject, because that is what the user calls this message; not the
 * window title, which is what the program calls this window.
 */
auto EMailSendDialog::build_header() -> QWidget* {
  auto* header = new QWidget(this);
  auto* row = new QHBoxLayout(header);
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(10);

  auto* icon = new QLabel(header);
  icon->setPixmap(
      QIcon::fromTheme("mail-send", QIcon(":/icons/email.png")).pixmap(32, 32));
  icon->setAlignment(Qt::AlignTop);
  row->addWidget(icon);

  auto* text = new QVBoxLayout;
  text->setContentsMargins(0, 0, 0, 0);
  text->setSpacing(2);

  auto* title = MakeValueLabel(header);
  title->setText(message_.subject.isEmpty() ? tr("(no subject)")
                                            : message_.subject);
  EMailMakeTitle(title);
  text->addWidget(title);

  subtitle_label_ = new QLabel(tr("Ready to send. Nothing has left this "
                                  "computer yet."),
                               header);
  subtitle_label_->setWordWrap(true);
  EMailMakeSecondary(subtitle_label_);
  text->addWidget(subtitle_label_);

  row->addLayout(text, 1);
  return header;
}

/**
 * @brief Who it goes to, and what it weighs.
 *
 * Stays on screen through the send and after it. A result without the thing
 * it is a result about is half an answer, and the old two-page arrangement
 * took the message away at exactly the moment it started to matter.
 */
auto EMailSendDialog::build_summary_card() -> QWidget* {
  auto* card = EMailCard(this, tr("Message"));
  auto* form = new QFormLayout;
  form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
  form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
  form->setHorizontalSpacing(12);

  const auto add_row = [this, card, form](const QString& name) -> QLabel* {
    auto* caption = new QLabel(name, card);
    EMailMakeMuted(caption);
    auto* value = MakeValueLabel(card);
    form->addRow(caption, value);
    return value;
  };

  auto* from = add_row(tr("From"));
  from->setText(message_.envelope_from);

  auto visible = message_.envelope_rcpt;
  for (const auto& blind : message_.blind_rcpt) visible.removeAll(blind);

  auto* recipients = add_row(tr("To"));
  recipients->setText(visible.isEmpty() ? tr("No visible recipients")
                                        : visible.join(", "));

  // Added only when there are any. An always-present empty row taught the eye
  // to skip the place where a blind copy would have been announced.
  if (!message_.blind_rcpt.isEmpty()) {
    add_row(tr("Blind copies"))->setText(message_.blind_rcpt.join(", "));
  }

  const auto attachments = message_.attachment_count;
  auto contents = EMailHumanSize(message_.eml.size());
  if (attachments > 0) {
    contents = tr("%1, %n attachment(s)", nullptr, attachments).arg(contents);
  }
  add_row(tr("Contains"))->setText(contents);

  qobject_cast<QVBoxLayout*>(card->layout())->addLayout(form);
  return card;
}

/**
 * @brief Which account sends it, and whether it can.
 *
 * The password check happens here rather than after the button is pressed.
 * A dialog that accepts a click and then refuses is a dialog that wasted the
 * user's decision; the same fact, said before, costs nothing.
 */
auto EMailSendDialog::build_account_block() -> QWidget* {
  auto* block = new QWidget(this);
  auto* layout = new QVBoxLayout(block);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  auto* row = new QHBoxLayout;
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(8);

  auto* caption = new QLabel(tr("Send using"), block);
  EMailMakeMuted(caption);
  account_combo_ = new QComboBox(block);
  row->addWidget(caption);
  row->addWidget(account_combo_, 1);
  layout->addLayout(row);

  account_detail_label_ = new QLabel(block);
  account_detail_label_->setWordWrap(true);
  account_detail_label_->setTextFormat(Qt::PlainText);
  EMailMakeSecondary(account_detail_label_);
  layout->addWidget(account_detail_label_);

  connect(account_combo_, &QComboBox::currentIndexChanged, this,
          &EMailSendDialog::slot_account_changed);
  return block;
}

/**
 * @brief What is known afterwards, one line per question.
 *
 * Two questions that are genuinely different, kept apart on purpose: an SMTP
 * 250 means the outgoing server took responsibility for the message, and a
 * copy in Sent means the account's own server has it.
 *
 * There is deliberately no third line for delivery. It is not observable from
 * a sending program, and a row that could only ever say "unknown" was read as
 * a step that had failed rather than as a question nobody can answer. The
 * wording above carries the distinction instead: "accepted by outgoing mail
 * server" is a claim about the handover and says nothing about arrival.
 */
auto EMailSendDialog::build_result_card() -> QFrame* {
  auto* card = EMailCard(this, tr("Result"));
  auto* layout = qobject_cast<QVBoxLayout*>(card->layout());

  auto* grid = new QFormLayout;
  grid->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
  grid->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
  grid->setHorizontalSpacing(12);

  const auto add_status_row = [this, card, grid](const QString& name,
                                                 EMailStatusDot** dot,
                                                 QLabel** value) {
    auto* caption = new QLabel(name, card);
    EMailMakeMuted(caption);

    auto* holder = new QWidget(card);
    auto* row = new QHBoxLayout(holder);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);

    *dot = new EMailStatusDot(holder);
    // Aligned to the first line of text rather than to the block, so a
    // three-line explanation does not float its own marker into the middle.
    row->addWidget(*dot, 0, Qt::AlignTop);

    *value = MakeValueLabel(holder);
    row->addWidget(*value, 1);

    grid->addRow(caption, holder);
    return holder;
  };

  auto* submission_row =
      add_status_row(tr("Submission"), &accepted_dot_, &accepted_label_);
  // Stopping a send was previously only expressible as destroying the window,
  // which threw the verdict away with it.
  stop_send_button_ = new QToolButton(submission_row);
  stop_send_button_->setText(tr("Stop"));
  stop_send_button_->setAutoRaise(true);
  stop_send_button_->setVisible(false);
  qobject_cast<QHBoxLayout*>(submission_row->layout())
      ->addWidget(stop_send_button_, 0, Qt::AlignTop);

  auto* sent_row =
      add_status_row(tr("Copy in Sent"), &sent_copy_dot_, &sent_copy_label_);
  stop_confirm_button_ = new QToolButton(sent_row);
  stop_confirm_button_->setText(tr("Stop"));
  stop_confirm_button_->setAutoRaise(true);
  stop_confirm_button_->setVisible(false);
  qobject_cast<QHBoxLayout*>(sent_row->layout())
      ->addWidget(stop_confirm_button_, 0, Qt::AlignTop);

  layout->addLayout(grid);

  // Collapsed, because the protocol transcript is not what a successful send
  // is about. It opens itself exactly when it becomes the thing worth reading.
  auto* details_row = new QHBoxLayout;
  details_row->setContentsMargins(0, 0, 0, 0);
  details_button_ = new QToolButton(card);
  details_button_->setText(tr("Details"));
  details_button_->setCheckable(true);
  details_button_->setAutoRaise(true);
  details_button_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  details_button_->setArrowType(Qt::RightArrow);
  details_row->addWidget(details_button_);
  details_row->addStretch();

  copy_evidence_button_ = new QToolButton(card);
  copy_evidence_button_->setText(tr("Copy details"));
  copy_evidence_button_->setAutoRaise(true);
  details_row->addWidget(copy_evidence_button_);
  layout->addLayout(details_row);

  evidence_view_ = new QPlainTextEdit(card);
  evidence_view_->setReadOnly(true);
  evidence_view_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  evidence_view_->setVisible(false);
  evidence_view_->setMaximumHeight(120);
  layout->addWidget(evidence_view_);

  connect(copy_evidence_button_, &QToolButton::clicked, this,
          &EMailSendDialog::slot_copy_evidence);
  connect(details_button_, &QToolButton::clicked, this,
          &EMailSendDialog::slot_toggle_details);
  connect(stop_confirm_button_, &QToolButton::clicked, this,
          &EMailSendDialog::slot_stop_confirming);
  connect(stop_send_button_, &QToolButton::clicked, this,
          &EMailSendDialog::slot_stop_sending);

  // A click that lands on a button never reaches the dialog's own
  // mousePressEvent, so each of these says for itself that someone is here.
  for (auto* button : {details_button_, copy_evidence_button_,
                       stop_confirm_button_, stop_send_button_}) {
    connect(button, &QToolButton::clicked, this,
            &EMailSendDialog::cancel_auto_close);
  }
  return card;
}

void EMailSendDialog::build_ui() {
  auto* outer = new QVBoxLayout(this);
  outer->setSpacing(10);

  outer->addWidget(build_header());
  outer->addWidget(build_summary_card());

  // Said next to the addresses it is about, and in its own tinted strip: this
  // is the one thing about a blind copy a user must be able to check before
  // sending, and a footnote is not where that belongs.
  if (!message_.blind_rcpt.isEmpty()) {
    auto* blind_banner =
        EMailTintedBanner(this, EMailWarningColor(this), kBannerTint);
    auto* banner_row = new QHBoxLayout(blind_banner);
    banner_row->setContentsMargins(10, 8, 10, 8);

    auto* note = new QLabel(
        tr("These addresses are given to the outgoing server only. They do "
           "not appear in the message, so no other recipient can see them."),
        blind_banner);
    note->setWordWrap(true);
    EMailMakeSecondary(note);
    banner_row->addWidget(note, 1);
    outer->addWidget(blind_banner);
  }

  outer->addWidget(build_account_block());

  // Indeterminate: SMTP says when it is done, never how far along it is.
  // Shown only after a short delay, so a fast server never flashes a bar.
  progress_ = new QProgressBar(this);
  progress_->setRange(0, 0);
  progress_->setTextVisible(false);
  progress_->setFixedHeight(4);
  progress_->setVisible(false);
  outer->addWidget(progress_);

  progress_timer_ = new QTimer(this);
  progress_timer_->setSingleShot(true);
  progress_timer_->setInterval(kProgressDelayMs);
  connect(progress_timer_, &QTimer::timeout, this,
          [this]() { progress_->setVisible(true); });

  // Shown from the start, not conjured up by pressing Send. These are the
  // three questions this dialog exists to answer, and saying what they are
  // before anything happens is what lets the user watch them being answered
  // rather than meet them all at once afterwards.
  result_card_ = build_result_card();
  outer->addWidget(result_card_);

  outer->addStretch();

  auto* buttons = new QHBoxLayout;
  send_button_ = new QPushButton(tr("Send"), this);
  send_button_->setDefault(true);
  close_button_ = new QPushButton(tr("Cancel"), this);
  buttons->addStretch();
  buttons->addWidget(send_button_);
  buttons->addWidget(close_button_);
  outer->addLayout(buttons);

  connect(send_button_, &QPushButton::clicked, this,
          &EMailSendDialog::slot_send);
  connect(close_button_, &QPushButton::clicked, this, &QDialog::reject);
}

void EMailSendDialog::start_smtp_worker() {
  smtp_thread_ = new QThread(this);
  smtp_worker_ = new EMailSmtpWorker;
  smtp_worker_->moveToThread(smtp_thread_);
  connect(smtp_thread_, &QThread::finished, smtp_worker_,
          &QObject::deleteLater);
  connect(smtp_worker_, &EMailSmtpWorker::SignalFinished, this,
          &EMailSendDialog::handle_sent);
  smtp_thread_->start();
}

void EMailSendDialog::stop_workers() {
  // Unblocks whatever socket call a worker is sitting inside. quit() alone
  // only asks an event loop to exit, and a thread blocked in vmime is not in
  // its event loop -- so without this, closing during a submit waited out the
  // full timeout on the GUI thread and then fell into the detach path below.
  // CancelAll, not Cancel(seq): this window is going away, so anything still
  // queued behind the current request is unwanted too.
  if (smtp_worker_ != nullptr) smtp_worker_->Token()->CancelAll();
  if (imap_worker_ != nullptr) imap_worker_->Token()->CancelAll();

  StopMailWorkerThread(smtp_thread_, "SMTP worker thread");
  StopMailWorkerThread(imap_thread_, "IMAP worker thread");
  smtp_thread_ = nullptr;
  imap_thread_ = nullptr;
  smtp_worker_ = nullptr;
  imap_worker_ = nullptr;
}

void EMailSendDialog::slot_account_changed() { refresh_account_state(); }

void EMailSendDialog::refresh_account_state() {
  // Only before sending. Once a submission is under way the account is fixed,
  // and rewriting this line then would describe a choice nobody can still make.
  if (seq_ != 0) return;

  const auto index = account_combo_->currentIndex();
  if (index < 0 || index >= accounts_.size()) {
    account_detail_label_->setText(tr("No outgoing account is configured."));
    EMailSetLabelColor(account_detail_label_,
                       EMailWarningColor(account_detail_label_));
    send_button_->setEnabled(false);
    return;
  }

  const auto account = accounts_.at(index);

  // Only whether there is one; the password itself is not this function's
  // business, and the secret is erased as the shared_ptr goes out of scope.
  const bool has_password = EMailCredentialStore::Has(account.id);

  if (!has_password) {
    // No prompt here: the password is set in Settings and nowhere else.
    // Refusing plainly is better than a dialog that would make sending depend
    // on a credential the application never actually keeps.
    account_detail_label_->setText(
        tr("No password is stored for this account, so nothing can be sent "
           "through it. Set the password in Settings, under Mail Accounts, "
           "and turn on the option to remember it."));
    EMailSetLabelColor(account_detail_label_,
                       EMailWarningColor(account_detail_label_));
    send_button_->setEnabled(false);
    return;
  }

  account_detail_label_->setText(
      tr("%1 port %2 · %3")
          .arg(account.smtp.host)
          .arg(account.smtp.EffectivePort(false))
          .arg(MailTlsModeToString(account.smtp.tls)));
  EMailMakeMuted(account_detail_label_);
  send_button_->setEnabled(true);
}

void EMailSendDialog::slot_send() {
  const auto index = account_combo_->currentIndex();
  if (index < 0 || index >= accounts_.size()) return;

  const auto account = accounts_.at(index);

  auto password = EMailCredentialStore::Load(account.id);
  if (password->IsEmpty()) {
    // Already reported inline by refresh_account_state(), which also disables
    // the button; reaching here means it changed underneath us.
    refresh_account_state();
    return;
  }

  send_button_->setEnabled(false);
  account_combo_->setEnabled(false);
  close_button_->setText(tr("Close"));
  subtitle_label_->setText(tr("Sending. This window will say what happened."));

  start_smtp_worker();

  ++seq_;
  QMetaObject::invokeMethod(
      smtp_worker_, "Submit", Qt::QueuedConnection, Q_ARG(quint64, seq_),
      Q_ARG(MailAccountConfig, account), Q_ARG(EMailSecretPtr, password),
      Q_ARG(EMailOutgoingMessage, message_));
  // Released here; the worker holds the only remaining reference and the bytes
  // are erased when it drops it. Nothing is copied along the way.
  password.reset();

  // The summary above stays exactly where it is, and so does the result block:
  // its first line simply stops saying "not sent yet".
  submit_state_ = SubmitState::kSUBMITTING;
  progress_timer_->start();
  refresh_result();
}

void EMailSendDialog::handle_sent(quint64 seq,
                                  const EMailSendReceipt& receipt) {
  if (seq != seq_) return;

  submit_state_ = SubmitState::kFINISHED;
  receipt_ = receipt;
  refresh_result();

  // The submission is finished and reported before anything else happens. The
  // user never waits on an IMAP round trip to learn whether their message went
  // out.
  if (receipt_.accepted) begin_sent_copy();
}

/**
 * @brief File a copy of the message in the account's Sent folder.
 *
 * Runs only after the submission has already been reported. Most servers do
 * not file a copy of what you send them -- that is the client's job, and not
 * doing it is why sent mail used to vanish from this program's point of view.
 *
 * Nothing here can change whether the message was sent. Every way this can
 * fail is reported against this line and never against the one above it.
 */
void EMailSendDialog::begin_sent_copy() {
  const auto index = account_combo_->currentIndex();
  if (index < 0 || index >= accounts_.size()) return;

  const auto account = accounts_.at(index);

  // Filing a copy needs IMAP, and an account may legitimately have only SMTP.
  // That is not a failure; there is simply nowhere to put it.
  if (!account.imap.enabled) {
    confirm_note_ =
        tr("This account is set up for sending only, so there is no mailbox to "
           "keep a copy in.");
    confirm_state_ = ConfirmState::kUNAVAILABLE;
    refresh_result();
    return;
  }

  auto password = EMailCredentialStore::Load(account.id);
  if (password->IsEmpty()) {
    confirm_note_ = tr(
        "No password is stored for this account, so its mailbox could not be "
        "opened.");
    confirm_state_ = ConfirmState::kUNAVAILABLE;
    refresh_result();
    return;
  }

  confirm_state_ = ConfirmState::kCHECKING;
  refresh_result();

  imap_thread_ = new QThread(this);
  imap_worker_ = new EMailImapWorker;
  imap_worker_->moveToThread(imap_thread_);
  connect(imap_thread_, &QThread::finished, imap_worker_,
          &QObject::deleteLater);
  connect(imap_worker_, &EMailImapWorker::SignalSentSaved, this,
          &EMailSendDialog::handle_sent_saved);
  connect(imap_worker_, &EMailImapWorker::SignalFailed, this,
          &EMailSendDialog::handle_confirm_failed);
  connect(imap_worker_, &EMailImapWorker::SignalConnected, this,
          [this](quint64 seq) {
            // Guarded like every other handler. Harmless today only because
            // seq_ is frozen once a send starts and there is no retry -- but
            // an unguarded append is a duplicate in Sent the moment either of
            // those changes, and a connection that lands after the user
            // pressed Stop must not file anything at all.
            if (seq != seq_ || confirm_state_ == ConfirmState::kSTOPPED) {
              return;
            }
            QMetaObject::invokeMethod(
                imap_worker_, "SaveToSentFolder", Qt::QueuedConnection,
                Q_ARG(quint64, seq_), Q_ARG(QString, message_.message_id),
                Q_ARG(QByteArray, message_.eml));
          });
  imap_thread_->start();

  QMetaObject::invokeMethod(
      imap_worker_, "Connect", Qt::QueuedConnection, Q_ARG(quint64, seq_),
      Q_ARG(MailAccountConfig, account), Q_ARG(EMailSecretPtr, password));
  password.reset();
}

void EMailSendDialog::handle_sent_saved(quint64 seq,
                                        MailSentSaveOutcome outcome,
                                        const QString& folder,
                                        const MailError& error) {
  if (seq != seq_) return;
  if (confirm_state_ == ConfirmState::kSTOPPED) return;

  sent_folder_ = folder;

  switch (outcome) {
    case MailSentSaveOutcome::kALREADY_THERE:
    case MailSentSaveOutcome::kSAVED:
      confirm_state_ = ConfirmState::kCONFIRMED;
      confirm_note_.clear();
      // Which of the two it was still matters to anyone reading closely, so
      // the wording keeps them apart even though the state does not.
      already_filed_by_server_ = outcome == MailSentSaveOutcome::kALREADY_THERE;
      break;

    case MailSentSaveOutcome::kSAVED_UNVERIFIED:
      // Written, and not provable. Claiming a confirmed copy here would be
      // asserting something we did not check.
      confirm_state_ = ConfirmState::kUNAVAILABLE;
      confirm_note_ =
          tr("a copy was put in %1, but it cannot be looked up afterwards: "
             "this message carries no Message-ID. A message sent exactly as it "
             "arrived is never given one, because adding it would change bytes "
             "a signature may cover.")
              .arg(folder);
      break;

    case MailSentSaveOutcome::kUNRESOLVED:
      confirm_note_ =
          tr("no Sent folder could be identified for this account.");
      confirm_state_ = ConfirmState::kUNAVAILABLE;
      break;

    case MailSentSaveOutcome::kFAILED:
      confirm_note_ = error.title.isEmpty()
                          ? tr("the copy could not be written to the mailbox.")
                          : tr("the copy could not be written to the mailbox: "
                               "%1")
                                .arg(error.title);
      confirm_state_ = ConfirmState::kUNAVAILABLE;
      break;
  }

  refresh_result();
}

void EMailSendDialog::handle_confirm_failed(quint64 seq,
                                            const MailError& error) {
  if (seq != seq_) return;
  if (confirm_state_ == ConfirmState::kSTOPPED) return;

  // A failure here says nothing about the submission, which has already
  // succeeded, so it is reported only as "could not check" -- but it is said
  // as the mailbox problem it is, not as a missing folder.
  confirm_note_ =
      error.title.isEmpty()
          ? tr("The mailbox could not be reached.")
          : tr("The mailbox could not be reached: %1").arg(error.title);
  confirm_state_ = ConfirmState::kUNAVAILABLE;
  refresh_result();
}

void EMailSendDialog::slot_stop_sending() {
  // Asks the worker to stop; it does NOT decide what happened. If the body was
  // already on the wire the classifier reports the outcome as unknown, and
  // that verdict is shown here like any other. Stopping is a request, not a
  // statement that nothing was delivered.
  if (smtp_worker_ != nullptr) smtp_worker_->Token()->Cancel(seq_);
  stop_send_button_->setEnabled(false);
  stop_send_button_->setText(tr("Stopping..."));
}

void EMailSendDialog::closeEvent(QCloseEvent* event) {
  if (submit_state_ != SubmitState::kSUBMITTING) {
    QDialog::closeEvent(event);
    return;
  }

  // The bytes may already be at the server. This window is the only place the
  // answer will ever appear, so closing it now is a decision to never find
  // out -- and after an ambiguous outcome, resending delivers twice.
  QMessageBox box(this);
  box.setIcon(QMessageBox::Warning);
  box.setWindowTitle(tr("Still sending"));
  box.setText(tr("This message is still being sent."));
  box.setInformativeText(
      tr("The server may already have accepted it. If you close this window "
         "now, you will not find out whether it was delivered, and sending "
         "it again could deliver it twice."));
  auto* wait = box.addButton(tr("Keep Waiting"), QMessageBox::RejectRole);
  auto* close = box.addButton(tr("Close Anyway"), QMessageBox::DestructiveRole);
  box.setDefaultButton(wait);
  box.exec();

  if (box.clickedButton() != close) {
    event->ignore();
    return;
  }

  // Recorded rather than merely dropped: an abandoned submission is exactly
  // the thing someone will later need to explain a duplicate.
  abandoned_mid_submit_ = true;
  LOG_WARN(
      "send dialog closed while a submission was in flight; the outcome "
      "will not be reported");
  QDialog::closeEvent(event);
}

void EMailSendDialog::slot_stop_confirming() {
  if (imap_worker_ != nullptr) imap_worker_->Token()->Cancel(seq_);
  confirm_state_ = ConfirmState::kSTOPPED;
  refresh_result();
}

void EMailSendDialog::slot_toggle_details() {
  const auto open = details_button_->isChecked();
  details_button_->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
  evidence_view_->setVisible(open);
}

void EMailSendDialog::refresh_result() {
  const bool sending = seq_ != 0 && !receipt_.accepted && !receipt_.ambiguous &&
                       !receipt_.error.IsError();

  if (seq_ == 0) {
    // Nothing has been attempted. Said plainly, because an empty line beside a
    // grey dot reads as a result that has gone missing.
    accepted_dot_->SetState(EMailStatusState::kPENDING);
    accepted_label_->setText(tr("Not sent yet"));
  } else if (sending) {
    accepted_dot_->SetState(EMailStatusState::kPENDING);
    accepted_label_->setText(tr("Sending..."));
  } else if (receipt_.ambiguous) {
    accepted_dot_->SetState(EMailStatusState::kUNKNOWN);
    accepted_label_->setText(
        tr("Unknown. The connection was lost after the message had been sent, "
           "so the server may or may not have accepted it. Check the Sent "
           "folder or the recipient before sending again: resending may "
           "deliver it twice."));
  } else if (receipt_.accepted) {
    accepted_dot_->SetState(EMailStatusState::kGOOD);
    accepted_label_->setText(tr("Yes, accepted by the outgoing mail server"));
  } else if (receipt_.error.IsError()) {
    accepted_dot_->SetState(EMailStatusState::kBAD);
    auto text = receipt_.error.title;
    if (!receipt_.error.detail.isEmpty()) {
      text += "\n" + receipt_.error.detail;
    }
    accepted_label_->setText(text);
  }

  switch (confirm_state_) {
    case ConfirmState::kNOT_STARTED:
      sent_copy_dot_->SetState(EMailStatusState::kPENDING);
      sent_copy_label_->setText(receipt_.accepted
                                    ? tr("Not saved")
                                    : tr("Waits until the message is sent"));
      break;
    case ConfirmState::kCHECKING:
      sent_copy_dot_->SetState(EMailStatusState::kPENDING);
      sent_copy_label_->setText(tr("Saving a copy to Sent..."));
      break;
    case ConfirmState::kCONFIRMED:
      sent_copy_dot_->SetState(EMailStatusState::kGOOD);
      // The distinction is kept because it answers a question a user does ask:
      // whether their server keeps its own copies, or this program does it.
      sent_copy_label_->setText(
          already_filed_by_server_
              ? tr("Yes, your mail server had already filed it in %1")
                    .arg(sent_folder_)
              : tr("Yes, saved to %1").arg(sent_folder_));
      break;
    case ConfirmState::kUNAVAILABLE:
      sent_copy_dot_->SetState(EMailStatusState::kUNKNOWN);
      // The reason, not a guess at it. Several different things end up here
      // and naming the wrong one sends the user to fix something that was
      // never broken. The message still went out, and that is said first.
      sent_copy_label_->setText(
          confirm_note_.isEmpty()
              ? tr("No copy was kept.")
              : tr("The message was sent, but %1").arg(confirm_note_));
      break;
    case ConfirmState::kSTOPPED:
      sent_copy_dot_->SetState(EMailStatusState::kUNKNOWN);
      sent_copy_label_->setText(
          tr("Stopped: no copy was kept. The message was still sent."));
      break;
  }

  stop_confirm_button_->setVisible(confirm_state_ == ConfirmState::kCHECKING);
  stop_send_button_->setVisible(submit_state_ == SubmitState::kSUBMITTING);

  const auto evidence = evidence_text();
  evidence_view_->setPlainText(evidence);

  // An empty transcript is not worth a control that opens onto nothing.
  details_button_->setEnabled(!evidence.isEmpty());
  copy_evidence_button_->setEnabled(!evidence.isEmpty());
  if (evidence.isEmpty() && details_button_->isChecked()) {
    details_button_->setChecked(false);
    slot_toggle_details();
  }

  // The bar belongs to whatever is still happening. Nothing is, once the
  // submission has an answer and the Sent check has stopped asking.
  if (!sending && confirm_state_ != ConfirmState::kCHECKING) {
    progress_timer_->stop();
    progress_->setVisible(false);
  }

  // Opened for the user exactly once, and only when the transcript is the
  // thing worth reading. If they close it again it stays closed.
  if ((receipt_.error.IsError() || receipt_.ambiguous) &&
      !details_auto_opened_ && !evidence_text().isEmpty()) {
    details_auto_opened_ = true;
    details_button_->setChecked(true);
    slot_toggle_details();
  }

  if (receipt_.ambiguous) {
    // Neither of the other two. Saying "sent" or "not sent" here would be a
    // guess presented as a fact, and this is the one case where the user has
    // to decide what to do next.
    subtitle_label_->setText(tr("It is not known whether this was sent."));
  } else if (receipt_.accepted) {
    subtitle_label_->setText(tr("Handed to the outgoing mail server."));
  } else if (receipt_.error.IsError()) {
    subtitle_label_->setText(tr("Not sent."));
  }

  maybe_auto_close();
}

/**
 * @brief Closes the window only when there is evidence the message arrived
 * somewhere, and only if nobody is using the window.
 *
 * A copy found in the Sent folder is the one outcome that needs no reading:
 * the account's own server has the message. Everything else -- an ambiguous
 * connection, a failure, a Sent folder that could not be identified, a check
 * the user stopped -- is something they have to see, so it stays on screen.
 */
void EMailSendDialog::maybe_auto_close() {
  if (user_interacted_ || auto_close_timer_ != nullptr) return;
  if (!receipt_.accepted || receipt_.ambiguous) return;
  if (confirm_state_ != ConfirmState::kCONFIRMED) return;

  auto_close_left_ = kAutoCloseSeconds;
  close_button_->setText(tr("Close (%1)").arg(auto_close_left_));

  auto_close_timer_ = new QTimer(this);
  auto_close_timer_->setInterval(1000);
  connect(auto_close_timer_, &QTimer::timeout, this, [this]() {
    if (--auto_close_left_ <= 0) {
      accept();
      return;
    }
    close_button_->setText(tr("Close (%1)").arg(auto_close_left_));
  });
  auto_close_timer_->start();
}

void EMailSendDialog::cancel_auto_close() {
  // Permanent. Someone who has reached for this window is reading it, and a
  // countdown that restarts would take it away from them anyway.
  user_interacted_ = true;
  if (auto_close_timer_ == nullptr) return;

  auto_close_timer_->stop();
  auto_close_timer_->deleteLater();
  auto_close_timer_ = nullptr;
  close_button_->setText(tr("Close"));
}

void EMailSendDialog::mousePressEvent(QMouseEvent* event) {
  cancel_auto_close();
  QDialog::mousePressEvent(event);
}

void EMailSendDialog::keyPressEvent(QKeyEvent* event) {
  cancel_auto_close();
  QDialog::keyPressEvent(event);
}

auto EMailSendDialog::evidence_text() const -> QString {
  QStringList lines;
  if (!receipt_.message_id.isEmpty()) {
    lines << QString("Message-ID: <%1>").arg(receipt_.message_id);
  }
  if (receipt_.submitted_at.isValid()) {
    lines << QString("Submitted: %1")
                 .arg(receipt_.submitted_at.toString(Qt::ISODate));
  }
  if (!receipt_.host.isEmpty()) {
    lines << QString("Server: %1 (%2)").arg(receipt_.host, receipt_.security);
  }
  if (receipt_.reply_code != 0) {
    lines << QString("Reply: %1 %2")
                 .arg(receipt_.reply_code)
                 .arg(receipt_.reply_text);
  }
  if (!receipt_.error.protocol_detail.isEmpty()) {
    lines << QString("Detail: %1").arg(receipt_.error.protocol_detail);
  }
  return lines.join("\n");
}

void EMailSendDialog::slot_copy_evidence() {
  QApplication::clipboard()->setText(evidence_text());
}
