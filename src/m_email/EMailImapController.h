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

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QThread;

/**
 * @brief A remote file picker for messages. Not an inbox.
 *
 * One window, short-lived, owning one IMAP session for as long as it is open
 * and nothing afterwards. It browses, it returns one message's bytes, and it
 * closes; no mailbox state outlives it, and none of what it learns reaches the
 * document model.
 *
 * It deliberately knows nothing about tabs. The chosen message leaves through
 * SignalMessageChosen() and the module decides what to do with it, which is
 * what keeps the picker from growing into a mail client.
 */
class EMailImapController : public QDialog {
  Q_OBJECT

 public:
  explicit EMailImapController(QWidget* parent = nullptr);
  ~EMailImapController() override;

  /// Whether there is any account this window could browse.
  static auto HasUsableAccount() -> bool;

 signals:
  /**
   * @brief The user picked a message.
   *
   * The whole boundary: raw RFC bytes, exactly as the server holds them, with
   * no folder, no UID and no account attached.
   */
  void SignalMessageChosen(const QByteArray& raw_eml);

 protected:
  void closeEvent(QCloseEvent* event) override;

 private slots:
  void slot_account_changed();
  void slot_folder_changed();
  void slot_search();
  void slot_load_more();
  void slot_open_selected();
  void slot_cancel_busy();

  void handle_connected(quint64 seq);
  void handle_folders(quint64 seq, const QList<EMailFolderInfo>& folders);
  void handle_messages(quint64 seq, const EMailMessagePage& page);
  void handle_fetched(quint64 seq, const QByteArray& raw_eml);
  void handle_failed(quint64 seq, const MailError& error);

 private:
  void build_ui();
  void start_worker();
  /// Tear the session and its thread down, on the worker's own thread.
  void stop_worker();

  void connect_to_selected_account();
  void request_page(bool reset);
  void refresh_table();
  void set_busy(bool busy, const QString& what);
  void show_error(const MailError& error);

  /// Whether a result belongs to the request we are still waiting for. Stale
  /// answers are dropped rather than applied: without this, picking message A
  /// and then B would show A's bytes if A's fetch finished last.
  auto is_current(quint64 seq) const -> bool { return seq == request_seq_; }
  auto next_seq() -> quint64 { return ++request_seq_; }

  QList<MailAccountConfig> accounts_;
  QList<EMailFolderInfo> folders_;
  QList<EMailMessageSummary> rows_;

  EMailImapWorker* worker_{};
  QThread* thread_{};

  quint64 request_seq_{0};
  QString current_folder_;
  /// Sequence number the next page continues from; 0 means "the newest".
  quint64 cursor_{0};
  bool searching_{false};
  bool closing_{false};

  QComboBox* account_combo_{};
  QComboBox* folder_combo_{};
  QLineEdit* search_edit_{};
  QPushButton* search_button_{};
  QTableWidget* table_{};
  QPushButton* more_button_{};
  QPushButton* open_button_{};
  QPushButton* cancel_button_{};
  QLabel* status_label_{};
};
