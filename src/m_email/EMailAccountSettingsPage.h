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

#include <GFModule.h>

#include <QList>
#include <QMap>
#include <QWidget>

#include "EMailAccountModel.h"
#include "EMailAccountProbe.h"
#include "EMailAccountStore.h"
#include "EMailNetError.h"
#include "EMailSecret.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

/**
 * @brief The Mail Accounts page of the application's Settings dialog.
 *
 * Owned by this module, hosted by the application: the dialog finds
 * SetSettings() and ApplySettings() by name, so edits are staged here and only
 * written when the user accepts.
 *
 * New accounts come from the Add Account wizard; this page is for looking at
 * and changing the ones that exist. It shows what nearly everyone needs -- the
 * address, the password and the two servers -- and keeps the rest behind
 * Advanced. Protocol behaviour -- PEEK, read-only folders, certificate
 * verification, retry and Sent-copy policy -- is not configurable and must not
 * gain controls here.
 */
class EMailAccountSettingsPage : public QWidget, public gf::ui::SettingsWidget {
  Q_OBJECT

 public:
  explicit EMailAccountSettingsPage(QWidget* parent = nullptr);
  ~EMailAccountSettingsPage() override;

 public slots:
  /// Load the stored accounts, discarding anything staged.
  void LoadSettings() override;
  /// Persist the staged accounts.
  bool ApplySettings() override;  // NOLINT

 private slots:
  void slot_add_account();
  void slot_remove_account();
  void slot_selection_changed();
  void slot_field_edited();
  void slot_test();
  void slot_set_default();
  /// Takes the selected account out of use, or puts it back.
  void slot_toggle_enabled();
  /// The account list's context menu: make default, enable or disable,
  /// remove.
  void show_account_menu(const QPoint& pos);

 protected:
  void changeEvent(QEvent* event) override;

 private:
  /// One direction's controls in the editor.
  struct TransportControls {
    QCheckBox* enabled{};
    QLineEdit* host{};
    QComboBox* security{};
    QLabel* port_hint{};
    QLineEdit* user{};
    EMailProbeStatus* status{};
    QLabel* pin{};
    QWidget* fields{};
  };

  void build_ui();
  auto build_empty_panel() -> QWidget*;
  auto build_list_panel() -> QWidget*;
  auto build_editor() -> QWidget*;
  auto build_servers_card() -> QWidget*;
  auto build_advanced() -> QWidget*;
  void build_transport_rows(bool imap, QWidget* parent, QVBoxLayout* layout);

  /// Write the selected account into the widgets.
  void load_selected();
  /// Read the widgets back into the selected account.
  void store_selected();
  void refresh_list();
  /// Update only the selected row. Used while typing, because rebuilding the
  /// list mid-edit takes focus away from the field.
  void refresh_current_row();
  static auto label_for(const MailAccountConfig& account) -> QString;
  /// What one row of the account list says, marker included.
  [[nodiscard]] auto row_text_for(const MailAccountConfig& account) const
      -> QString;
  /// Enable, disable and relabel everything that depends on current state.
  void refresh_enabled_state();
  /// Updates the read-only port labels from the connection choices.
  void refresh_port_hints();
  /// Rebuilds the connection choices from the host each transport now names.
  void refresh_security_choices();
  /// Says what is missing or wrong about the selected account, if anything.
  void refresh_validation();
  /// The editor's heading: which account this is, and whether it is the
  /// default.
  void refresh_heading();
  /// The single place this page's colours are decided. Must not touch fonts.
  void apply_colors();

  /// Starts the next test in the queue, or ends testing.
  void run_next_test();
  void on_test_finished(bool imap, EMailAccountProbe::Outcome outcome,
                        const MailError& error);
  /// Locks down the controls a running test must not have changed under it.
  void set_testing(bool testing);
  /// Overwrites every staged password before releasing it.
  void wipe_pending_passwords();
  /// Shows, per transport, whether a certificate is pinned.
  void refresh_pin_state();
  /// Drops the pinned certificate for one transport.
  void forget_pin(bool imap);

  [[nodiscard]] auto selected_index() const -> int;
  [[nodiscard]] auto controls(bool imap) -> TransportControls&;

  /// Staged, not stored: the dialog's Cancel throws these away by calling
  /// LoadSettings() again.
  QList<MailAccountConfig> accounts_;
  QString default_id_;

  /// Passwords the user typed this session, by account id, as erasable
  /// secrets rather than QStrings. Only written to the credential store on
  /// Apply.
  ///
  /// This does not close the window, it narrows it: the text still passes
  /// through QLineEdit's own storage and through the QString that
  /// EMailSecret::CopyFrom() is handed. What it removes is this page holding
  /// its own un-erasable copy for as long as the dialog stays open.
  QMap<QString, EMailSecretPtr> pending_passwords_;
  /// Accounts removed from the list but whose stored password is only
  /// forgotten on Apply, so Cancel really does put everything back.
  QStringList pending_removals_;
  /// Whether anything has been edited since the page was loaded.
  bool dirty_{false};
  /// Whether the list that was read may be written back. False when the
  /// stored accounts were refused -- see EMailAccountStore::LoadResult.
  bool may_store_{true};
  EMailAccountStore::LoadOutcome load_outcome_{
      EMailAccountStore::LoadOutcome::kOK};
  bool loading_{false};

  EMailAccountProbe* probe_{};
  /// The transports still to test in the current run, IMAP first.
  QList<bool> test_queue_;
  /// The account being tested, as it was on screen when the test started.
  MailAccountConfig tested_;
  EMailSecretPtr tested_password_;

  QWidget* left_panel_{};
  QListWidget* account_list_{};
  QPushButton* add_button_{};

  QWidget* empty_panel_{};
  QLabel* empty_notice_{};
  QPushButton* empty_add_button_{};

  QScrollArea* editor_scroll_{};
  QWidget* editor_{};
  QLabel* heading_{};
  QLabel* heading_detail_{};
  QLabel* validation_{};

  QLineEdit* display_name_edit_{};
  QLineEdit* address_edit_{};
  QLineEdit* password_edit_{};
  QLabel* oauth_notice_{};

  TransportControls imap_;
  TransportControls smtp_;
  QPushButton* test_button_{};
  QPushButton* stop_button_{};

  QLineEdit* reply_to_edit_{};
  QLineEdit* sent_folder_{};
};
