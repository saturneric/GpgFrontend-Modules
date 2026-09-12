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

#pragma once

#include <QDialog>
#include <QList>

#include "EMailAccountModel.h"
#include "EMailImapWorker.h"
#include "EMailOutgoing.h"
#include "EMailSmtpWorker.h"

class QComboBox;
class QLabel;
class QPushButton;
class QPlainTextEdit;
class QStackedWidget;
class QThread;

/**
 * @brief Confirms a message, submits it, and reports precisely what happened.
 *
 * Two pages: what is about to be sent, and what is known afterwards. The
 * second is the reason this dialog exists at all -- an SMTP 250 means the
 * outgoing server took responsibility for the message and nothing more, and
 * this is where that distinction is kept rather than flattened into "Sent".
 */
class EMailSendDialog : public QDialog {
  Q_OBJECT

 public:
  EMailSendDialog(EMailOutgoingMessage message, QWidget* parent = nullptr);
  ~EMailSendDialog() override;

  static auto HasUsableAccount() -> bool;

 private slots:
  void slot_send();
  void slot_stop_confirming();
  void handle_sent(quint64 seq, const EMailSendReceipt& receipt);
  void handle_sent_lookup(quint64 seq, bool resolved, bool found,
                          const QString& folder);
  void handle_confirm_failed(quint64 seq, const MailError& error);
  void slot_copy_evidence();

 private:
  void build_ui();
  auto build_confirm_page() -> QWidget*;
  auto build_result_page() -> QWidget*;

  void start_smtp_worker();
  void stop_workers();
  /// Begin the Sent-folder check, after the send is already reported as done.
  void begin_sent_confirmation();
  void refresh_result();
  [[nodiscard]] auto evidence_text() const -> QString;

  EMailOutgoingMessage message_;
  QList<MailAccountConfig> accounts_;
  EMailSendReceipt receipt_;

  /// Where the Sent check got to. Deliberately separate from the send itself:
  /// stopping this must never read as having cancelled the submission.
  enum class ConfirmState : uint8_t {
    kNOT_STARTED,
    kCHECKING,
    kCONFIRMED,
    kNOT_FOUND,
    kUNAVAILABLE,
    kSTOPPED,
  };
  ConfirmState confirm_state_{ConfirmState::kNOT_STARTED};
  QString sent_folder_;

  EMailSmtpWorker* smtp_worker_{};
  QThread* smtp_thread_{};
  EMailImapWorker* imap_worker_{};
  QThread* imap_thread_{};

  quint64 seq_{0};

  QStackedWidget* pages_{};
  QComboBox* account_combo_{};
  QLabel* summary_label_{};
  QLabel* bcc_label_{};
  QPushButton* send_button_{};
  QPushButton* close_button_{};

  QLabel* accepted_label_{};
  QLabel* sent_copy_label_{};
  QLabel* delivery_label_{};
  QPushButton* stop_confirm_button_{};
  QPlainTextEdit* evidence_view_{};
};
