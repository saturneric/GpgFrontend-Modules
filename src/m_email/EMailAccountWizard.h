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

#include <QJsonObject>
#include <QList>

#include "EMailAccountProbe.h"
#include "EMailAccountSetup.h"
#include "EMailSecret.h"
#include "EMailWizardWidgets.h"

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class EMailServerLookup;
class EMailSummaryRows;
class EMailToggleSwitch;

/**
 * @brief Adds a mail account in four steps: the account, its servers, a
 * connection check and a review.
 *
 * Writes nothing. The settings page takes the account and password from it
 * and stages them like any other edit, so the Settings dialog's Cancel still
 * throws a new account away.
 */
class EMailAccountWizard : public EMailWizard {
  Q_OBJECT

 public:
  /**
   * @param id the id the new account is to have
   * @param existing the accounts already configured, for the duplicate check
   * @param parent parent widget
   */
  EMailAccountWizard(QString id, QList<MailAccountConfig> existing,
                     QWidget* parent);
  ~EMailAccountWizard() override;

  /// The account as configured. Meaningful once the wizard is accepted.
  [[nodiscard]] auto Account() const -> MailAccountConfig;

  /// The password typed, as an erasable secret.
  [[nodiscard]] auto Password() const -> EMailSecretPtr;

  /// Whether the account is to become the default one.
  [[nodiscard]] auto MakeDefault() const -> bool;

  void done(int result) override;

 private:
  /// One direction's fields in the manual section.
  struct ServerFields {
    QWidget* section{};
    QLineEdit* host{};
    QComboBox* security{};
    QLabel* port_hint{};
    /// Its user name, in the Login usernames section, and whether the user
    /// typed one of their own. One that was not typed follows the address.
    QLineEdit* user{};
    bool user_overridden{false};
  };

  /// One direction's row of the connection check.
  struct CheckRow {
    QWidget* row{};
    QLabel* details{};
    EMailStatusIcon* icon{};
    QLabel* status{};
    /// What went wrong and what to do; empty and hidden otherwise.
    QLabel* explanation{};
  };

  /// What the check found for one direction, kept for the review.
  struct CheckResult {
    MailCheckState state{MailCheckState::kNOT_RUN};
    MailError error;
  };

  QString id_;
  QList<MailAccountConfig> existing_;
  EMailSecretPtr password_;

  EMailServerLookup* lookup_;
  EMailAccountProbe* probe_;

  /// The suggestion last looked up, and the address it was for. Going Back
  /// and Continue again with the same address keeps what the user edited.
  MailServerSuggestion suggestion_;
  QString looked_up_for_;
  QString filled_for_;

  CheckResult imap_result_;
  CheckResult smtp_result_;
  /// The account the current results are about. Anything that changes it
  /// makes them stale.
  QJsonObject checked_;
  /// Counts runs, so a stopped test's late answer is not taken for the
  /// current one's.
  quint64 check_run_{0};
  quint64 probe_run_{0};
  /// A certificate the user chose to trust during the check, with the server
  /// it was for.
  QString imap_pin_host_, imap_pin_;
  QString smtp_pin_host_, smtp_pin_;

  EMailWizardPage* account_page_{};
  EMailWizardPage* servers_page_{};
  EMailWizardPage* check_page_{};
  EMailWizardPage* review_page_{};

  QFormLayout* account_form_{};
  QLineEdit* name_edit_{};
  QLineEdit* address_edit_{};
  QLabel* address_problem_{};
  QLineEdit* password_edit_{};
  QLabel* password_note_{};
  QLabel* lookup_status_{};
  QLabel* dns_notice_{};

  QButtonGroup* usage_{};
  /// The summary and the fields that change it share one place: one or the
  /// other, never both.
  QStackedWidget* servers_region_{};
  QWidget* summary_view_{};
  QWidget* edit_view_{};
  EMailSummaryRows* summary_rows_{};
  ServerFields imap_fields_;
  ServerFields smtp_fields_;
  QFormLayout* usernames_form_{};

  CheckRow imap_row_;
  CheckRow smtp_row_;
  QLabel* overall_{};
  QPushButton* retry_button_{};
  QPushButton* stop_button_{};
  QCheckBox* accept_unverified_{};

  EMailSummaryRows* review_rows_{};
  EMailToggleSwitch* default_switch_{};

  void build_account_page();
  void build_servers_page();
  auto build_server_fields(bool imap, QWidget* parent) -> ServerFields;
  auto build_check_row(bool imap, QWidget* parent) -> CheckRow;
  /// Shows the summary, or the fields in its place.
  void show_server_fields(bool edit);
  void build_check_page();
  void build_review_page();

  [[nodiscard]] auto fields(bool imap) -> ServerFields&;
  [[nodiscard]] auto result(bool imap) -> CheckResult&;
  [[nodiscard]] auto check_row(bool imap) -> CheckRow&;

  /// Everything the steps' rules need, as they now stand.
  [[nodiscard]] auto state() const -> MailSetupState;
  [[nodiscard]] auto usage() const -> MailAccountUsage;
  /// The account the fields now describe.
  [[nodiscard]] auto build_account() const -> MailAccountConfig;

  /// @param strict also report an address that is still being typed
  void refresh_address_problem(bool strict);
  /// Says, before Continue is pressed, that it will ask DNS about the domain.
  void refresh_dns_notice();
  /// What the provider asks of the password, said under the password field.
  void refresh_provider_note();

  void on_lookup_finished(const MailServerSuggestion& suggestion);
  /// Puts the suggestion into the server fields, unless they were already
  /// filled for this address.
  void fill_servers();
  /// The summary, the port hints and which directions are shown.
  void refresh_servers();
  /// Where the servers on screen came from, in a few words.
  [[nodiscard]] auto source_text() const -> QString;

  /// Checks the account as the fields now describe it. Without @p force, an
  /// answer already given for exactly this account is kept.
  void start_checks(bool force);
  /// Makes every answer so far stale and stops the test running, if any.
  void abandon_checks();
  /// Runs the next check that has not run yet, if any.
  void run_next_check();
  void on_check_finished(bool imap, EMailAccountProbe::Outcome outcome,
                         const MailError& error);
  void refresh_check_controls();
  /// Puts one direction's state into its check row.
  void show_check_row(bool imap);
  /// The sentence under the rows about the check as a whole.
  void refresh_overall();

  /// A direction's check result, as the review says it.
  void show_result(EMailProbeStatus* status, const CheckResult& result) const;
  void fill_review();
};
