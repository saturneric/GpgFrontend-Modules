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
class QFrame;
class QLabel;
class QProgressBar;
class QPushButton;
class QPlainTextEdit;
class QThread;
class QTimer;
class QToolButton;

class EMailStatusDot;

/**
 * @brief Confirms a message, submits it, and reports precisely what happened.
 *
 * One surface. What is about to be sent stays on screen while it is sent and
 * after it has been, because that is exactly the context a result needs: the
 * result block appears beneath the summary rather than replacing it.
 *
 * Reporting the result honestly is the reason this dialog exists at all -- an
 * SMTP 250 means the outgoing server took responsibility for the message and
 * nothing more, and this is where that distinction is kept rather than
 * flattened into "Sent".
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
  void slot_account_changed();
  void slot_toggle_details();

 private:
  void build_ui();
  auto build_header() -> QWidget*;
  auto build_summary_card() -> QWidget*;
  auto build_account_block() -> QWidget*;
  auto build_result_card() -> QFrame*;

  void start_smtp_worker();
  void stop_workers();
  /// Begin the Sent-folder check, after the send is already reported as done.
  void begin_sent_confirmation();
  void refresh_result();
  /// Re-reads the selected account and says, before anything is sent, whether
  /// it can send at all.
  void refresh_account_state();
  /// Closes the dialog only on positive evidence, and only if the user has
  /// not touched it. See the definition for why nothing else qualifies.
  void maybe_auto_close();
  void cancel_auto_close();
  [[nodiscard]] auto evidence_text() const -> QString;

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
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
  /// Why the check could not be made, when it could not. Five different
  /// things reach kUNAVAILABLE and only one of them is "no Sent folder", so
  /// the state alone cannot say what happened.
  QString confirm_note_;

  EMailSmtpWorker* smtp_worker_{};
  QThread* smtp_thread_{};
  EMailImapWorker* imap_worker_{};
  QThread* imap_thread_{};

  quint64 seq_{0};

  QLabel* subtitle_label_{};
  QComboBox* account_combo_{};
  QLabel* account_detail_label_{};
  QProgressBar* progress_{};
  QTimer* progress_timer_{};
  QPushButton* send_button_{};
  QPushButton* close_button_{};

  QFrame* result_card_{};
  EMailStatusDot* accepted_dot_{};
  EMailStatusDot* sent_copy_dot_{};
  EMailStatusDot* delivery_dot_{};
  QLabel* accepted_label_{};
  QLabel* sent_copy_label_{};
  QLabel* delivery_label_{};
  QToolButton* stop_confirm_button_{};
  QToolButton* details_button_{};
  QToolButton* copy_evidence_button_{};
  QPlainTextEdit* evidence_view_{};

  /// True once the details pane has been opened for the user rather than by
  /// them, so a later refresh does not keep re-opening what they closed.
  bool details_auto_opened_{false};

  QTimer* auto_close_timer_{};
  int auto_close_left_{0};
  /// Set the moment the user touches anything. An automatic close is a
  /// convenience for someone who has walked away, never something that takes
  /// the window out from under a hand that is using it.
  bool user_interacted_{false};
};
