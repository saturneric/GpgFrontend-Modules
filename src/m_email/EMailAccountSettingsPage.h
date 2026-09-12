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

#include <QList>
#include <QWidget>

#include "EMailAccountModel.h"

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;

/**
 * @brief The Mail Accounts page of the application's Settings dialog.
 *
 * Owned by this module, hosted by the application: the dialog finds
 * SetSettings() and ApplySettings() by name, so edits are staged here and only
 * written when the user accepts.
 *
 * The page is deliberately small. Protocol behaviour -- PEEK, read-only
 * folders, certificate verification, retry and Sent-copy policy -- is not
 * configurable and must not gain controls here; what a user can legitimately
 * want to vary is which server they use and how much of a mailbox to show.
 */
class EMailAccountSettingsPage : public QWidget {
  Q_OBJECT

 public:
  explicit EMailAccountSettingsPage(QWidget* parent = nullptr);

 public slots:
  /// Load the stored accounts, discarding anything staged.
  void SetSettings();
  /// Persist the staged accounts.
  void ApplySettings();

 private slots:
  void slot_add_account();
  void slot_remove_account();
  void slot_selection_changed();
  void slot_field_edited();
  void slot_test_imap();
  void slot_test_smtp();

 private:
  void build_ui();
  auto build_identity_group() -> QGroupBox*;
  auto build_transport_group(bool imap) -> QGroupBox*;

  /// Write the selected account into the widgets.
  void load_selected();
  /// Read the widgets back into the selected account.
  void store_selected();
  void refresh_list();
  /// Update only the selected row's text. Used while typing, because
  /// rebuilding the list mid-edit takes focus away from the field.
  void refresh_current_label();
  static auto label_for(const MailAccountConfig& account) -> QString;
  /// Enable, disable and relabel everything that depends on current state.
  void refresh_enabled_state();
  /// Updates the read-only port labels from the connection choices.
  void refresh_port_hints();

  /// Run one connection test and report it in @p status.
  void test_transport(bool imap, QLabel* status);

  [[nodiscard]] auto selected_index() const -> int;

  /// Staged, not stored: the dialog's Cancel throws these away by calling
  /// SetSettings() again.
  QList<MailAccountConfig> accounts_;
  QString default_id_;
  /// Passwords the user typed this session, by account id. Only written to the
  /// credential store on Apply, and only when remembering is on.
  QMap<QString, QString> pending_passwords_;
  bool loading_{false};

  QListWidget* account_list_{};
  QPushButton* add_button_{};
  QPushButton* remove_button_{};

  QLineEdit* display_name_edit_{};
  QLineEdit* address_edit_{};
  QLineEdit* reply_to_edit_{};

  QCheckBox* imap_enabled_{};
  QLineEdit* imap_host_{};
  QComboBox* imap_security_{};
  QLineEdit* imap_user_{};
  QLabel* imap_port_hint_{};
  QLineEdit* sent_folder_{};
  QPushButton* imap_test_{};
  QLabel* imap_status_{};

  QCheckBox* smtp_enabled_{};
  QLineEdit* smtp_host_{};
  QComboBox* smtp_security_{};
  QLineEdit* smtp_user_{};
  QLabel* smtp_port_hint_{};
  QPushButton* smtp_test_{};
  QLabel* smtp_status_{};

  QLineEdit* password_edit_{};
  QLabel* oauth_notice_{};

  QWidget* editor_{};
};

/**
 * @brief Build a page for the host's Settings dialog.
 *
 * Matches QObjectFactory. A fresh page every call: the dialog is rebuilt each
 * time it opens and takes ownership of what it is given.
 */
auto EMailAccountSettingsPageFactory(void* data) -> void*;
