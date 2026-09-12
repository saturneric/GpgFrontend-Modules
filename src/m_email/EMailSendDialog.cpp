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
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QThread>
#include <QVBoxLayout>

#include "EMailAccountStore.h"
#include "EMailCredentialStore.h"
#include "GFModuleCommonUtils.hpp"

namespace {

auto Tr(const char* text) -> QString {
  return QCoreApplication::translate("EMailSendDialog", text);
}

}  // namespace

EMailSendDialog::EMailSendDialog(EMailOutgoingMessage message, QWidget* parent)
    : QDialog(parent), message_(std::move(message)) {
  setWindowTitle(Tr("Send message"));
  resize(620, 460);
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
}

EMailSendDialog::~EMailSendDialog() { stop_workers(); }

auto EMailSendDialog::HasUsableAccount() -> bool {
  return !EMailAccountStore::SmtpAccounts().isEmpty();
}

auto EMailSendDialog::build_confirm_page() -> QWidget* {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  auto* form = new QFormLayout;
  account_combo_ = new QComboBox(page);
  form->addRow(Tr("Send using"), account_combo_);
  layout->addLayout(form);

  summary_label_ = new QLabel(page);
  summary_label_->setWordWrap(true);
  summary_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);

  auto visible = message_.envelope_rcpt;
  for (const auto& blind : message_.blind_rcpt) visible.removeAll(blind);

  summary_label_->setText(
      Tr("From: %1\nTo: %2").arg(message_.envelope_from, visible.join(", ")));
  layout->addWidget(summary_label_);

  bcc_label_ = new QLabel(page);
  bcc_label_->setWordWrap(true);
  if (!message_.blind_rcpt.isEmpty()) {
    // Said explicitly, because this is the one thing about a BCC that a user
    // must be able to verify before sending: the addresses go to the server as
    // envelope recipients and appear nowhere in the message the others read.
    bcc_label_->setText(
        Tr("Blind copies to: %1\n"
           "These addresses are given to the outgoing server only. They do not "
           "appear in the message, so no other recipient can see them.")
            .arg(message_.blind_rcpt.join(", ")));
  }
  layout->addWidget(bcc_label_);
  layout->addStretch();
  return page;
}

auto EMailSendDialog::build_result_page() -> QWidget* {
  auto* page = new QWidget(this);
  auto* layout = new QVBoxLayout(page);

  auto* group = new QGroupBox(Tr("Result"), page);
  auto* form = new QFormLayout(group);

  accepted_label_ = new QLabel(group);
  sent_copy_label_ = new QLabel(group);
  delivery_label_ = new QLabel(group);

  for (auto* label : {accepted_label_, sent_copy_label_, delivery_label_}) {
    label->setWordWrap(true);
  }

  form->addRow(Tr("SMTP accepted"), accepted_label_);
  form->addRow(Tr("Sent copy confirmed"), sent_copy_label_);
  form->addRow(Tr("Delivery status"), delivery_label_);

  stop_confirm_button_ = new QPushButton(Tr("Stop checking"), group);
  stop_confirm_button_->setVisible(false);
  form->addRow(QString(), stop_confirm_button_);

  layout->addWidget(group);

  evidence_view_ = new QPlainTextEdit(page);
  evidence_view_->setReadOnly(true);
  layout->addWidget(evidence_view_, 1);

  auto* copy = new QPushButton(Tr("Copy details"), page);
  auto* row = new QHBoxLayout;
  row->addWidget(copy);
  row->addStretch();
  layout->addLayout(row);

  connect(copy, &QPushButton::clicked, this,
          &EMailSendDialog::slot_copy_evidence);
  connect(stop_confirm_button_, &QPushButton::clicked, this,
          &EMailSendDialog::slot_stop_confirming);
  return page;
}

void EMailSendDialog::build_ui() {
  auto* outer = new QVBoxLayout(this);

  pages_ = new QStackedWidget(this);
  pages_->addWidget(build_confirm_page());
  pages_->addWidget(build_result_page());
  outer->addWidget(pages_, 1);

  auto* buttons = new QHBoxLayout;
  send_button_ = new QPushButton(Tr("Send"), this);
  send_button_->setDefault(true);
  close_button_ = new QPushButton(Tr("Cancel"), this);
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
  for (auto* pair : {&smtp_thread_, &imap_thread_}) {
    auto*& thread = *pair;
    if (thread == nullptr) continue;
    thread->quit();
    if (!thread->wait(5000)) {
      LOG_ERROR("mail worker thread did not stop; leaving it to finish");
      thread->deleteLater();
    }
    thread = nullptr;
  }
  smtp_worker_ = nullptr;
  imap_worker_ = nullptr;
}

void EMailSendDialog::slot_send() {
  const auto index = account_combo_->currentIndex();
  if (index < 0 || index >= accounts_.size()) return;

  const auto account = accounts_.at(index);

  auto password = EMailCredentialStore::Load(account.id);
  if (password.isEmpty()) {
    // No prompt here either: the password is set in Settings and nowhere else.
    // Refusing plainly is better than a dialog that would make sending depend
    // on a credential the application never actually keeps.
    QMessageBox::information(
        this, Tr("No password stored"),
        Tr("This account has no stored password, so nothing can be sent "
           "through it. Set the password in Settings, under Mail Accounts, "
           "and turn on the option to remember it."));
    return;
  }

  send_button_->setEnabled(false);
  account_combo_->setEnabled(false);
  close_button_->setText(Tr("Close"));

  start_smtp_worker();

  ++seq_;
  QMetaObject::invokeMethod(
      smtp_worker_, "Submit", Qt::QueuedConnection, Q_ARG(quint64, seq_),
      Q_ARG(MailAccountConfig, account), Q_ARG(QString, password),
      Q_ARG(EMailOutgoingMessage, message_));
  password.fill(QChar('\0'));

  pages_->setCurrentIndex(1);
  accepted_label_->setText(Tr("Sending..."));
  refresh_result();
}

void EMailSendDialog::handle_sent(quint64 seq,
                                  const EMailSendReceipt& receipt) {
  if (seq != seq_) return;

  receipt_ = receipt;
  refresh_result();

  // The submission is finished and reported before anything else happens. The
  // user never waits on an IMAP round trip to learn whether their message went
  // out.
  if (receipt_.accepted) begin_sent_confirmation();
}

void EMailSendDialog::begin_sent_confirmation() {
  const auto index = account_combo_->currentIndex();
  if (index < 0 || index >= accounts_.size()) return;

  const auto account = accounts_.at(index);

  // Confirmation needs IMAP, and an account may legitimately have only SMTP.
  // That is not a failure; there is simply nothing to check against.
  if (!account.imap.enabled || message_.message_id.isEmpty()) {
    confirm_state_ = ConfirmState::kUNAVAILABLE;
    refresh_result();
    return;
  }

  auto password = EMailCredentialStore::Load(account.id);
  if (password.isEmpty()) {
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
  connect(imap_worker_, &EMailImapWorker::SignalSentLookup, this,
          &EMailSendDialog::handle_sent_lookup);
  connect(imap_worker_, &EMailImapWorker::SignalFailed, this,
          &EMailSendDialog::handle_confirm_failed);
  connect(
      imap_worker_, &EMailImapWorker::SignalConnected, this, [this](quint64) {
        QMetaObject::invokeMethod(imap_worker_, "FindInSentFolder",
                                  Qt::QueuedConnection, Q_ARG(quint64, seq_),
                                  Q_ARG(QString, message_.message_id));
      });
  imap_thread_->start();

  QMetaObject::invokeMethod(
      imap_worker_, "Connect", Qt::QueuedConnection, Q_ARG(quint64, seq_),
      Q_ARG(MailAccountConfig, account), Q_ARG(QString, password));
  password.fill(QChar('\0'));
}

void EMailSendDialog::handle_sent_lookup(quint64 seq, bool resolved, bool found,
                                         const QString& folder) {
  if (seq != seq_) return;
  if (confirm_state_ == ConfirmState::kSTOPPED) return;

  sent_folder_ = folder;

  // "We could not work out where sent mail goes" is a different answer from
  // "it is not there", and reporting the first as the second would claim
  // knowledge we do not have.
  if (!resolved) {
    confirm_state_ = ConfirmState::kUNAVAILABLE;
  } else {
    confirm_state_ =
        found ? ConfirmState::kCONFIRMED : ConfirmState::kNOT_FOUND;
  }
  refresh_result();
}

void EMailSendDialog::handle_confirm_failed(quint64 seq,
                                            const MailError& /*error*/) {
  if (seq != seq_) return;
  if (confirm_state_ == ConfirmState::kSTOPPED) return;

  // A failure here says nothing about the submission, which has already
  // succeeded, so it is reported only as "could not check".
  confirm_state_ = ConfirmState::kUNAVAILABLE;
  refresh_result();
}

void EMailSendDialog::slot_stop_confirming() {
  if (imap_worker_ != nullptr) imap_worker_->Token()->Cancel();
  confirm_state_ = ConfirmState::kSTOPPED;
  refresh_result();
}

void EMailSendDialog::refresh_result() {
  if (receipt_.ambiguous) {
    accepted_label_->setText(
        Tr("Unknown. The connection was lost after the message had been sent, "
           "so the server may or may not have accepted it. Check the Sent "
           "folder or the recipient before sending again -- resending may "
           "deliver it twice."));
  } else if (receipt_.accepted) {
    accepted_label_->setText(Tr("Yes -- accepted by outgoing mail server"));
  } else if (receipt_.error.IsError()) {
    auto text = receipt_.error.title;
    if (!receipt_.error.detail.isEmpty()) {
      text += "\n" + receipt_.error.detail;
    }
    accepted_label_->setText(text);
  }

  switch (confirm_state_) {
    case ConfirmState::kNOT_STARTED:
      sent_copy_label_->setText(receipt_.accepted ? Tr("Not checked")
                                                  : QString("-"));
      break;
    case ConfirmState::kCHECKING:
      sent_copy_label_->setText(Tr("Checking Sent copy..."));
      break;
    case ConfirmState::kCONFIRMED:
      sent_copy_label_->setText(Tr("Yes -- found in %1").arg(sent_folder_));
      break;
    case ConfirmState::kNOT_FOUND:
      sent_copy_label_->setText(
          Tr("Not found in %1. Many servers file a copy only after a delay, "
             "and some do not file one at all.")
              .arg(sent_folder_));
      break;
    case ConfirmState::kUNAVAILABLE:
      sent_copy_label_->setText(
          Tr("Unavailable -- no Sent folder could be identified for this "
             "account."));
      break;
    case ConfirmState::kSTOPPED:
      sent_copy_label_->setText(
          Tr("Not checked -- you stopped the check. The "
             "message was still sent."));
      break;
  }

  stop_confirm_button_->setVisible(confirm_state_ == ConfirmState::kCHECKING);

  // Never anything else. Delivery is not observable from here, and a field
  // that sometimes said otherwise would teach the user to read acceptance as
  // delivery.
  delivery_label_->setText(
      Tr("Unknown -- delivery cannot be confirmed by the "
         "sending program."));

  evidence_view_->setPlainText(evidence_text());
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
