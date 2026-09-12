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

#include "EMailAccountSettingsPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QThread>
#include <QVBoxLayout>

#include "EMailAccountStore.h"
#include "EMailCredentialStore.h"
#include "EMailImapWorker.h"
#include "EMailSmtpWorker.h"
#include "GFModuleCommonUtils.hpp"

namespace {

auto Tr(const char* text) -> QString {
  return QCoreApplication::translate("EMailAccountSettingsPage", text);
}

/// The security choices offered. "None" is absent on purpose: it is not a
/// thing a user should be picking for a mail server, and it is reached only
/// for a loopback host, where it is inferred rather than chosen.
void FillSecurityCombo(QComboBox* combo) {
  combo->addItem(Tr("TLS (recommended)"),
                 static_cast<int>(MailTlsMode::kIMPLICIT));
  combo->addItem(Tr("STARTTLS"), static_cast<int>(MailTlsMode::kSTARTTLS));
}

auto SecurityOf(const QComboBox* combo) -> MailTlsMode {
  return static_cast<MailTlsMode>(combo->currentData().toInt());
}

void SelectSecurity(QComboBox* combo, MailTlsMode mode) {
  // A stored kNONE (only possible for loopback) shows as STARTTLS rather than
  // adding a "None" row that other accounts could then be switched to.
  const auto wanted =
      mode == MailTlsMode::kNONE ? MailTlsMode::kSTARTTLS : mode;
  const auto index = combo->findData(static_cast<int>(wanted));
  combo->setCurrentIndex(index < 0 ? 0 : index);
}

}  // namespace

EMailAccountSettingsPage::EMailAccountSettingsPage(QWidget* parent)
    : QWidget(parent) {
  build_ui();
  SetSettings();
}

auto EMailAccountSettingsPage::build_identity_group() -> QGroupBox* {
  auto* group = new QGroupBox(Tr("Identity"), this);
  auto* form = new QFormLayout(group);

  display_name_edit_ = new QLineEdit(group);
  address_edit_ = new QLineEdit(group);
  reply_to_edit_ = new QLineEdit(group);
  reply_to_edit_->setPlaceholderText(Tr("Optional"));

  form->addRow(Tr("Display name"), display_name_edit_);
  form->addRow(Tr("Email address"), address_edit_);
  form->addRow(Tr("Reply-To"), reply_to_edit_);

  for (auto* edit : {display_name_edit_, address_edit_, reply_to_edit_}) {
    connect(edit, &QLineEdit::textEdited, this,
            &EMailAccountSettingsPage::slot_field_edited);
  }
  return group;
}

auto EMailAccountSettingsPage::build_transport_group(bool imap) -> QGroupBox* {
  auto* group =
      new QGroupBox(imap ? Tr("Receiving (IMAP)") : Tr("Sending (SMTP)"), this);
  auto* layout = new QVBoxLayout(group);

  auto* enabled = new QCheckBox(imap ? Tr("Import messages from this account")
                                     : Tr("Send messages through this account"),
                                group);
  auto* form = new QFormLayout;

  auto* host = new QLineEdit(group);
  auto* security = new QComboBox(group);
  FillSecurityCombo(security);
  auto* user = new QLineEdit(group);

  form->addRow(Tr("Server"), host);
  form->addRow(Tr("Security"), security);
  form->addRow(Tr("Username"), user);

  // Advanced: a port that is not the well-known one for the chosen security,
  // and -- for IMAP -- the two listing knobs. 0 means "use the default", which
  // is what the placeholder says.
  auto* port = new QSpinBox(group);
  port->setRange(0, 65535);
  port->setSpecialValueText(Tr("Automatic"));
  form->addRow(Tr("Port (advanced)"), port);

  auto* test = new QPushButton(Tr("Test Connection"), group);
  auto* status = new QLabel(group);
  status->setWordWrap(true);

  layout->addWidget(enabled);
  layout->addLayout(form);

  if (imap) {
    imap_enabled_ = enabled;
    imap_host_ = host;
    imap_security_ = security;
    imap_user_ = user;
    imap_port_ = port;
    imap_test_ = test;
    imap_status_ = status;

    sent_folder_ = new QLineEdit(group);
    sent_folder_->setPlaceholderText(Tr("Discovered automatically"));
    form->addRow(Tr("Sent folder (advanced)"), sent_folder_);

    page_size_ = new QSpinBox(group);
    page_size_->setRange(25, 200);
    page_size_->setSingleStep(25);
    form->addRow(Tr("Messages per page (advanced)"), page_size_);

    connect(sent_folder_, &QLineEdit::textEdited, this,
            &EMailAccountSettingsPage::slot_field_edited);
    connect(page_size_, &QSpinBox::valueChanged, this,
            &EMailAccountSettingsPage::slot_field_edited);
    connect(test, &QPushButton::clicked, this,
            &EMailAccountSettingsPage::slot_test_imap);
  } else {
    smtp_enabled_ = enabled;
    smtp_host_ = host;
    smtp_security_ = security;
    smtp_user_ = user;
    smtp_port_ = port;
    smtp_test_ = test;
    smtp_status_ = status;
    connect(test, &QPushButton::clicked, this,
            &EMailAccountSettingsPage::slot_test_smtp);
  }

  auto* row = new QHBoxLayout;
  row->addWidget(test);
  row->addStretch();
  layout->addLayout(row);
  layout->addWidget(status);

  connect(enabled, &QCheckBox::toggled, this,
          &EMailAccountSettingsPage::slot_field_edited);
  connect(security, &QComboBox::currentIndexChanged, this,
          &EMailAccountSettingsPage::slot_field_edited);
  connect(port, &QSpinBox::valueChanged, this,
          &EMailAccountSettingsPage::slot_field_edited);
  for (auto* edit : {host, user}) {
    connect(edit, &QLineEdit::textEdited, this,
            &EMailAccountSettingsPage::slot_field_edited);
  }
  return group;
}

void EMailAccountSettingsPage::build_ui() {
  auto* outer = new QHBoxLayout(this);

  auto* left = new QVBoxLayout;
  account_list_ = new QListWidget(this);
  account_list_->setMaximumWidth(220);
  add_button_ = new QPushButton(Tr("Add"), this);
  remove_button_ = new QPushButton(Tr("Remove"), this);

  auto* buttons = new QHBoxLayout;
  buttons->addWidget(add_button_);
  buttons->addWidget(remove_button_);
  left->addWidget(account_list_);
  left->addLayout(buttons);

  editor_ = new QWidget(this);
  auto* right = new QVBoxLayout(editor_);
  right->addWidget(build_identity_group());
  right->addWidget(build_transport_group(true));
  right->addWidget(build_transport_group(false));

  auto* credentials = new QGroupBox(Tr("Password"), editor_);
  auto* credential_form = new QFormLayout(credentials);
  password_edit_ = new QLineEdit(credentials);
  password_edit_->setEchoMode(QLineEdit::Password);
  password_edit_->setPlaceholderText(Tr("Leave blank to keep the stored one"));
  remember_password_ = new QCheckBox(Tr("Remember this password"), credentials);
  password_notice_ = new QLabel(credentials);
  password_notice_->setWordWrap(true);

  oauth_notice_ = new QLabel(
      Tr("Signing in through a provider's own web page (OAuth2) is not "
         "supported yet. If your provider requires it, create an "
         "app-specific password and use that here."),
      credentials);
  oauth_notice_->setWordWrap(true);

  credential_form->addRow(Tr("Password"), password_edit_);
  credential_form->addRow(QString(), remember_password_);
  credential_form->addRow(QString(), password_notice_);
  credential_form->addRow(QString(), oauth_notice_);
  right->addWidget(credentials);
  right->addStretch();

  outer->addLayout(left);
  outer->addWidget(editor_, 1);

  connect(account_list_, &QListWidget::currentRowChanged, this,
          &EMailAccountSettingsPage::slot_selection_changed);
  connect(add_button_, &QPushButton::clicked, this,
          &EMailAccountSettingsPage::slot_add_account);
  connect(remove_button_, &QPushButton::clicked, this,
          &EMailAccountSettingsPage::slot_remove_account);
  connect(password_edit_, &QLineEdit::textEdited, this,
          &EMailAccountSettingsPage::slot_field_edited);
  connect(remember_password_, &QCheckBox::toggled, this,
          &EMailAccountSettingsPage::slot_field_edited);
}

void EMailAccountSettingsPage::SetSettings() {
  loading_ = true;
  accounts_ = EMailAccountStore::Load();
  pending_passwords_.clear();

  const auto preferred = EMailAccountStore::DefaultAccount();
  default_id_ = preferred.id;

  refresh_list();
  loading_ = false;

  if (!accounts_.isEmpty()) account_list_->setCurrentRow(0);
  load_selected();
  refresh_enabled_state();
}

void EMailAccountSettingsPage::ApplySettings() {
  store_selected();
  EMailAccountStore::Store(accounts_, default_id_);

  for (const auto& account : accounts_) {
    if (!account.remember_password) {
      // Turning remembering off is a request to forget, not merely to stop
      // saving: leaving the old secret behind would outlive the reason it was
      // kept.
      EMailCredentialStore::Remove(account.id);
      continue;
    }

    const auto password = pending_passwords_.value(account.id);
    if (password.isEmpty()) continue;
    EMailCredentialStore::Save(account.id, password);
  }

  for (auto& password : pending_passwords_) password.fill(QChar('\0'));
  pending_passwords_.clear();
}

auto EMailAccountSettingsPage::selected_index() const -> int {
  const auto row = account_list_->currentRow();
  return row >= 0 && row < accounts_.size() ? row : -1;
}

auto EMailAccountSettingsPage::label_for(const MailAccountConfig& account)
    -> QString {
  const auto label = account.Label();
  return label.isEmpty() ? Tr("Untitled account") : label;
}

void EMailAccountSettingsPage::refresh_list() {
  // Rebuilding the list re-enters this class through currentRowChanged --
  // clear() alone reports "nothing is selected" -- and acting on that would
  // blank the editor and disable it mid-edit. The guard makes the rebuild
  // invisible to the selection handler; it is the same flag load_selected()
  // and store_selected() already cooperate through.
  const auto restoring = loading_;
  loading_ = true;

  const auto row = account_list_->currentRow();
  account_list_->clear();
  for (const auto& account : accounts_) {
    account_list_->addItem(label_for(account));
  }
  if (row >= 0 && row < accounts_.size()) account_list_->setCurrentRow(row);

  loading_ = restoring;
}

void EMailAccountSettingsPage::refresh_current_label() {
  const auto index = selected_index();
  if (index < 0) return;

  auto* item = account_list_->item(index);
  if (item == nullptr) return;

  const auto label = label_for(accounts_.at(index));
  if (item->text() == label) return;

  // Only the one row's text, and only when it actually changed. Rebuilding the
  // whole list on every keystroke was what stole focus from the field being
  // typed into.
  item->setText(label);
}

void EMailAccountSettingsPage::slot_add_account() {
  store_selected();

  MailAccountConfig account;
  account.id = EMailAccountStore::NewAccountId();
  account.imap.enabled = true;
  account.smtp.enabled = true;
  account.page_size = kMailDefaultPageSize;
  accounts_.append(account);

  if (default_id_.isEmpty()) default_id_ = account.id;

  refresh_list();
  account_list_->setCurrentRow(accounts_.size() - 1);
  load_selected();
  refresh_enabled_state();
}

void EMailAccountSettingsPage::slot_remove_account() {
  const auto index = selected_index();
  if (index < 0) return;

  const auto id = accounts_.at(index).id;
  accounts_.removeAt(index);
  pending_passwords_.remove(id);

  // Forgotten immediately rather than on Apply: the account is gone from the
  // list either way, and a secret with nothing left to point at it is exactly
  // the kind of thing that lingers unnoticed.
  EMailCredentialStore::Remove(id);

  refresh_list();
  load_selected();
  refresh_enabled_state();
}

void EMailAccountSettingsPage::slot_selection_changed() {
  if (loading_) return;
  load_selected();
  refresh_enabled_state();
}

void EMailAccountSettingsPage::slot_field_edited() {
  if (loading_) return;
  store_selected();
  refresh_current_label();
  refresh_enabled_state();
}

void EMailAccountSettingsPage::load_selected() {
  loading_ = true;

  const auto index = selected_index();
  const auto account = index < 0 ? MailAccountConfig() : accounts_.at(index);

  display_name_edit_->setText(account.display_name);
  address_edit_->setText(account.address);
  reply_to_edit_->setText(account.reply_to);

  imap_enabled_->setChecked(account.imap.enabled);
  imap_host_->setText(account.imap.host);
  SelectSecurity(imap_security_, account.imap.tls);
  imap_user_->setText(account.imap.username);
  imap_port_->setValue(account.imap.port);
  sent_folder_->setText(account.sent_folder_override);
  page_size_->setValue(MailClampPageSize(account.page_size));

  smtp_enabled_->setChecked(account.smtp.enabled);
  smtp_host_->setText(account.smtp.host);
  SelectSecurity(smtp_security_, account.smtp.tls);
  smtp_user_->setText(account.smtp.username);
  smtp_port_->setValue(account.smtp.port);

  remember_password_->setChecked(account.remember_password);
  password_edit_->setText(pending_passwords_.value(account.id));

  imap_status_->clear();
  smtp_status_->clear();

  loading_ = false;
}

void EMailAccountSettingsPage::store_selected() {
  const auto index = selected_index();
  if (index < 0) return;

  auto& account = accounts_[index];
  account.display_name = display_name_edit_->text().trimmed();
  account.address = address_edit_->text().trimmed();
  account.reply_to = reply_to_edit_->text().trimmed();

  account.imap.enabled = imap_enabled_->isChecked();
  account.imap.host = imap_host_->text().trimmed();
  account.imap.tls = SecurityOf(imap_security_);
  account.imap.username = imap_user_->text().trimmed();
  account.imap.port = static_cast<quint16>(imap_port_->value());

  account.smtp.enabled = smtp_enabled_->isChecked();
  account.smtp.host = smtp_host_->text().trimmed();
  account.smtp.tls = SecurityOf(smtp_security_);
  account.smtp.username = smtp_user_->text().trimmed();
  account.smtp.port = static_cast<quint16>(smtp_port_->value());

  account.sent_folder_override = sent_folder_->text().trimmed();
  account.page_size = MailClampPageSize(page_size_->value());
  account.remember_password = remember_password_->isChecked();

  const auto password = password_edit_->text();
  if (!password.isEmpty()) pending_passwords_[account.id] = password;
}

void EMailAccountSettingsPage::refresh_enabled_state() {
  const auto has_selection = selected_index() >= 0;
  editor_->setEnabled(has_selection);
  remove_button_->setEnabled(has_selection);

  for (auto* widget :
       {static_cast<QWidget*>(imap_host_),
        static_cast<QWidget*>(imap_security_),
        static_cast<QWidget*>(imap_user_), static_cast<QWidget*>(imap_port_),
        static_cast<QWidget*>(sent_folder_), static_cast<QWidget*>(page_size_),
        static_cast<QWidget*>(imap_test_)}) {
    widget->setEnabled(imap_enabled_->isChecked());
  }

  for (auto* widget :
       {static_cast<QWidget*>(smtp_host_),
        static_cast<QWidget*>(smtp_security_),
        static_cast<QWidget*>(smtp_user_), static_cast<QWidget*>(smtp_port_),
        static_cast<QWidget*>(smtp_test_)}) {
    widget->setEnabled(smtp_enabled_->isChecked());
  }

  // The credential policy, made visible. Everything the durable store holds is
  // encrypted under the application key, so when that key is itself
  // unprotected, storing a password there protects it in name only. The user
  // may still insist -- but not by default, and not without being told.
  if (EMailCredentialStore::MayPersistSilently()) {
    password_notice_->clear();
    remember_password_->setEnabled(true);
  } else {
    password_notice_->setText(
        Tr("This profile has no protection set, so a saved password would be "
           "stored with no real protection. Leave this off to be asked each "
           "time, or turn on profile protection in Advanced settings first."));
    remember_password_->setEnabled(true);
  }
}

void EMailAccountSettingsPage::slot_test_imap() {
  test_transport(true, imap_status_);
}

void EMailAccountSettingsPage::slot_test_smtp() {
  test_transport(false, smtp_status_);
}

void EMailAccountSettingsPage::test_transport(bool imap, QLabel* status) {
  store_selected();

  const auto index = selected_index();
  if (index < 0) return;

  const auto account = accounts_.at(index);
  const auto& config = imap ? account.imap : account.smtp;

  if (config.host.isEmpty()) {
    status->setText(Tr("Enter a server address first."));
    return;
  }

  auto password = pending_passwords_.value(account.id);
  if (password.isEmpty()) password = EMailCredentialStore::Load(account.id);
  if (password.isEmpty()) {
    status->setText(Tr("Enter a password first."));
    return;
  }

  status->setText(Tr("Testing..."));
  imap_test_->setEnabled(false);
  smtp_test_->setEnabled(false);
  QCoreApplication::processEvents();

  // A blocking test on a throwaway thread. The picker and the send path use a
  // long-lived worker; a one-shot probe does not need one, and joining here
  // keeps the settings page from having to manage a session lifetime.
  MailError error;
  bool connected = false;
  auto* thread = QThread::create([&]() {
    if (imap) {
      // Called directly rather than through the event loop: the worker's
      // slots are synchronous, so the signal has already been delivered by
      // the time Connect() returns.
      EMailImapWorker worker;
      QObject::connect(&worker, &EMailImapWorker::SignalFailed, &worker,
                       [&error](quint64, const MailError& e) { error = e; });
      QObject::connect(&worker, &EMailImapWorker::SignalConnected, &worker,
                       [&connected](quint64) { connected = true; });
      worker.Connect(1, account, password);
      worker.Disconnect();
    } else {
      EMailSmtpWorker worker;
      QObject::connect(
          &worker, &EMailSmtpWorker::SignalFinished, &worker,
          [&error, &connected](quint64, const EMailSendReceipt& receipt) {
            error = receipt.error;
            connected = receipt.accepted;
          });
      worker.TestConnection(1, account, password);
    }
  });

  thread->start();
  while (!thread->isFinished()) QCoreApplication::processEvents();
  thread->deleteLater();

  password.fill(QChar('\0'));

  imap_test_->setEnabled(imap_enabled_->isChecked());
  smtp_test_->setEnabled(smtp_enabled_->isChecked());

  // Judged on having actually connected, not on the absence of an error: an
  // internal failure is not a successful login, and reporting one as success
  // is worse than reporting nothing.
  if (connected) {
    status->setText(Tr("Connected successfully."));
    return;
  }

  if (!error.IsError()) {
    status->setText(Tr("Could not connect, and the reason is not known."));
    return;
  }

  auto text = error.title;
  if (!error.detail.isEmpty()) text += "\n" + error.detail;
  if (!error.protocol_detail.isEmpty()) text += "\n" + error.protocol_detail;
  status->setText(text);
}

auto EMailAccountSettingsPageFactory(void* /*data*/) -> void* {
  return new EMailAccountSettingsPage(nullptr);
}
