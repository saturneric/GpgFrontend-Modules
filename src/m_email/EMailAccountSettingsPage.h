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
#include <memory>
#include <mutex>

#include "EMailAccountModel.h"
#include "EMailAccountStore.h"
#include "EMailCancelToken.h"
#include "EMailNetError.h"
#include "EMailSecret.h"

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QScrollArea;
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
  ~EMailAccountSettingsPage() override;

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

 protected:
  void changeEvent(QEvent* event) override;

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
  /// What one row of the account list says, marker included.
  [[nodiscard]] auto row_text_for(const MailAccountConfig& account) const
      -> QString;
  static auto label_for(const MailAccountConfig& account) -> QString;
  /// Enable, disable and relabel everything that depends on current state.
  void refresh_enabled_state();
  /// Updates the read-only port labels from the connection choices.
  void refresh_port_hints();
  /// Rebuilds the connection choices from the host each transport now names.
  void refresh_security_choices();
  /// Says what is missing or wrong about the selected account, if anything.
  void refresh_validation();
  /// The single place this page's colours are decided. Must not touch fonts.
  void apply_colors();
  /// Makes the selected account the one used when nothing else is chosen.
  void slot_set_default();

  /// Run one connection test and report it in @p status.
  void test_transport(bool imap, QLabel* status);

  [[nodiscard]] auto selected_index() const -> int;

  /// Staged, not stored: the dialog's Cancel throws these away by calling
  /// SetSettings() again.
  QList<MailAccountConfig> accounts_;
  QString default_id_;
  /// Passwords the user typed this session, by account id. Only written to the
  /// credential store on Apply, and only when remembering is on.
  /// How a status line reads: neutral, an outcome to be glad of, or a problem.
  enum class StatusTone : uint8_t { kPLAIN, kGOOD, kBAD };

  /**
   * @brief One connection test in flight.
   *
   * Heap-allocated and shared with the thread doing the work, because the
   * function that starts a test now returns immediately -- its stack is gone
   * long before the answer arrives.
   */
  struct Probe {
    quint64 seq{0};
    /// Which transport this test was for; the two are configured separately
    /// and may be different machines, so an answer belongs to one of them.
    bool imap{false};
    bool connected{false};
    bool cancelled{false};
    MailError error;
    /// Published by the worker thread once its worker exists, so Stop can
    /// reach it. Guarded because the GUI thread reads it concurrently.
    std::mutex mutex;
    EMailCancelTokenPtr token;
  };

  /// Reports @p probe's outcome, unless it has been superseded.
  void finish_probe(const std::shared_ptr<Probe>& probe, quint64 seq,
                    QLabel* status);
  /// Cancels the running test, if there is one.
  void slot_stop_test();
  /// Locks down the controls a running test must not have changed under it.
  void set_testing(bool testing);
  /// Writes @p text into @p status in the colour @p tone calls for.
  void set_status(QLabel* status, const QString& text, StatusTone tone);
  /// Says what @p error means and what can be done about it.
  void report_probe_error(const MailError& error, bool imap, QLabel* status);
  /// Overwrites every staged password before releasing it.
  void wipe_pending_passwords();
  /// Offers to trust the certificate the last test was refused over.
  /// @param error the failure that produced the certificate, which carries it
  void offer_certificate_pin(bool imap, QLabel* status, const MailError& error);
  /// Shows, per transport, whether a certificate is pinned.
  void refresh_pin_state();
  /// Drops the pinned certificate for one transport.
  void forget_pin(bool imap);

  std::shared_ptr<Probe> probe_;
  /// The running test-connection thread, so the destructor can stop it.
  QThread* probe_thread_{};
  quint64 probe_seq_{0};

  /// Staged passwords, as erasable secrets rather than QStrings.
  ///
  /// They used to be QStrings taken straight from the line edit, and the wipe
  /// that "cleared" them called QString::fill() on a buffer the QLineEdit
  /// still shared -- which detaches and zeroes a fresh copy while the original
  /// lives on. See the note on EMailSecret.
  ///
  /// This does not close the window, it narrows it: the text still passes
  /// through QLineEdit's own storage and through the QString that
  /// EMailSecret::CopyFrom() is handed, and neither of those can be erased
  /// from here. What it removes is this page holding its own un-erasable copy
  /// for as long as the dialog stays open.
  QMap<QString, EMailSecretPtr> pending_passwords_;
  /// Accounts removed from the list but whose stored password is only
  /// forgotten on Apply, so Cancel really does put everything back.
  QStringList pending_removals_;
  /// Whether the list that was read may be written back. False when the stored
  /// accounts were refused -- see EMailAccountStore::LoadResult.
  /// Whether anything has been edited since the page was loaded.
  bool dirty_{false};
  bool may_store_{true};
  EMailAccountStore::LoadOutcome load_outcome_{
      EMailAccountStore::LoadOutcome::kOK};
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
  /// Says what is wrong with the selected account, above the transports.
  QLabel* validation_{};
  /// Shown instead of the form when there are no accounts at all.
  QLabel* empty_notice_{};
  QWidget* empty_panel_{};
  QWidget* left_panel_{};
  QPushButton* empty_add_button_{};
  QScrollArea* editor_scroll_{};
  QPushButton* default_button_{};
  QPushButton* imap_test_{};
  QPushButton* imap_stop_{};
  QLabel* imap_pin_{};
  QLabel* imap_status_{};

  QCheckBox* smtp_enabled_{};
  QLineEdit* smtp_host_{};
  QComboBox* smtp_security_{};
  QLineEdit* smtp_user_{};
  QLabel* smtp_port_hint_{};
  QPushButton* smtp_test_{};
  QPushButton* smtp_stop_{};
  QLabel* smtp_pin_{};
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
