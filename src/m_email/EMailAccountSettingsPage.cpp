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
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QThread>
#include <QVBoxLayout>

#include "EMailAccountStore.h"
#include "EMailCredentialStore.h"
#include "EMailImapWorker.h"
#include "EMailOutgoing.h"
#include "EMailSmtpWorker.h"
#include "EMailTlsSetup.h"
#include "EMailViewStyle.h"
#include "GFModuleCommonUtils.hpp"

namespace {

/// The security choices offered for @p host.
///
/// "None" appears only for a loopback host, which is the only place the model
/// permits it (MailHostAllowsCleartext) and the only place the worker will go
/// through with it. It used to be absent everywhere, which meant a stored
/// cleartext account had to be DISPLAYED as something else -- and the first
/// keystroke in any field then wrote that something else back, silently
/// converting a working localhost account. A choice the program honours has
/// to be a choice the program can show.
void FillSecurityCombo(QComboBox* combo, const QString& host) {
  const auto previous = combo->currentData();

  const QSignalBlocker blocker(combo);
  combo->clear();
  combo->addItem(QCoreApplication::translate("EMailAccountSettingsPage",
                                             "TLS (recommended)"),
                 static_cast<int>(MailTlsMode::kIMPLICIT));
  combo->addItem(
      QCoreApplication::translate("EMailAccountSettingsPage", "STARTTLS"),
      static_cast<int>(MailTlsMode::kSTARTTLS));

  if (MailHostAllowsCleartext(host)) {
    combo->addItem(QCoreApplication::translate("EMailAccountSettingsPage",
                                               "None (this machine only)"),
                   static_cast<int>(MailTlsMode::kNONE));
  }

  const auto index = combo->findData(previous);
  combo->setCurrentIndex(index < 0 ? 0 : index);
}

auto SecurityOf(const QComboBox* combo) -> MailTlsMode {
  return static_cast<MailTlsMode>(combo->currentData().toInt());
}

void SelectSecurity(QComboBox* combo, MailTlsMode mode) {
  const auto index = combo->findData(static_cast<int>(mode));

  // A cleartext account whose host is no longer loopback has no row to select:
  // the safest offered choice is the honest answer, and the mode only changes
  // if the user goes on to edit something.
  combo->setCurrentIndex(index < 0 ? 0 : index);
}

}  // namespace

EMailAccountSettingsPage::EMailAccountSettingsPage(QWidget* parent)
    : QWidget(parent) {
  build_ui();
  SetSettings();
}

namespace {

/// Lets a form give up its two-column shape when the page is too narrow for
/// it, rather than keeping the columns and clipping the right-hand one.
///
/// This page lives inside the host's settings dialog, whose width is not ours
/// to choose, and the alternative is a horizontal scrollbar that hides the end
/// of every field behind it.
void MakeFormNarrowable(QFormLayout* form) {
  form->setRowWrapPolicy(QFormLayout::WrapLongRows);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
}

}  // namespace

auto EMailAccountSettingsPage::build_identity_group() -> QGroupBox* {
  auto* group = new QGroupBox(tr("Identity"), this);
  auto* form = new QFormLayout(group);
  MakeFormNarrowable(form);

  display_name_edit_ = new QLineEdit(group);
  address_edit_ = new QLineEdit(group);
  reply_to_edit_ = new QLineEdit(group);
  reply_to_edit_->setPlaceholderText(tr("Optional"));

  form->addRow(tr("Display name"), display_name_edit_);
  form->addRow(tr("Email address"), address_edit_);
  form->addRow(tr("Reply-To"), reply_to_edit_);

  for (auto* edit : {display_name_edit_, address_edit_, reply_to_edit_}) {
    connect(edit, &QLineEdit::textEdited, this,
            &EMailAccountSettingsPage::slot_field_edited);
  }
  return group;
}

auto EMailAccountSettingsPage::build_transport_group(bool imap) -> QGroupBox* {
  auto* group =
      new QGroupBox(imap ? tr("Receiving (IMAP)") : tr("Sending (SMTP)"), this);
  auto* layout = new QVBoxLayout(group);

  // Short, because a QCheckBox cannot wrap and a long one sets the minimum
  // width of the whole page. The group box above it already says which
  // direction this is.
  auto* enabled = new QCheckBox(
      imap ? tr("Use this account to receive") : tr("Use this account to send"),
      group);
  auto* form = new QFormLayout;
  MakeFormNarrowable(form);

  auto* host = new QLineEdit(group);
  auto* security = new QComboBox(group);
  FillSecurityCombo(security, {});
  auto* user = new QLineEdit(group);

  // The port is policy, not configuration: it follows from the connection
  // choice and there is no field for it. Showing which port that means keeps
  // the policy visible without turning it back into a decision -- and a port
  // number must never be able to imply a security level, which is exactly
  // what an editable port next to a security combo invites.
  auto* port_hint = new QLabel(group);
  port_hint->setEnabled(false);

  auto* security_row = new QHBoxLayout;
  security_row->setContentsMargins(0, 0, 0, 0);
  security_row->addWidget(security, 1);
  security_row->addWidget(port_hint);

  form->addRow(tr("Server"), host);
  form->addRow(tr("Connection"), security_row);
  form->addRow(tr("Username"), user);

  auto* test = new QPushButton(tr("Test Connection"), group);
  auto* status = new QLabel(group);
  status->setWordWrap(true);

  layout->addWidget(enabled);
  layout->addLayout(form);

  if (imap) {
    imap_enabled_ = enabled;
    imap_host_ = host;
    imap_security_ = security;
    imap_user_ = user;
    imap_port_hint_ = port_hint;
    imap_test_ = test;
    imap_status_ = status;

    // The one remaining override, and it earns its place: a server that does
    // not advertise RFC 6154 SPECIAL-USE cannot have its Sent folder
    // discovered, and guessing the name would be worse than asking.
    sent_folder_ = new QLineEdit(group);
    sent_folder_->setPlaceholderText(tr("Discovered automatically"));
    form->addRow(tr("Sent folder"), sent_folder_);

    connect(sent_folder_, &QLineEdit::textEdited, this,
            &EMailAccountSettingsPage::slot_field_edited);
    connect(test, &QPushButton::clicked, this,
            &EMailAccountSettingsPage::slot_test_imap);
  } else {
    smtp_enabled_ = enabled;
    smtp_host_ = host;
    smtp_security_ = security;
    smtp_user_ = user;
    smtp_port_hint_ = port_hint;
    smtp_test_ = test;
    smtp_status_ = status;
    connect(test, &QPushButton::clicked, this,
            &EMailAccountSettingsPage::slot_test_smtp);
  }

  // Shown only while a test is running, and only next to the test it stops.
  // Without it a probe against a black-holed host holds the page for the full
  // timeout with nothing to press.
  auto* stop = new QPushButton(tr("Stop"), group);
  stop->setVisible(false);
  connect(stop, &QPushButton::clicked, this,
          &EMailAccountSettingsPage::slot_stop_test);

  // Says a certificate is pinned for this transport, and offers the only way
  // to take it back. A pin the user cannot see is a trust decision they cannot
  // revisit.
  auto* pin = new QLabel(group);
  pin->setWordWrap(true);
  pin->setVisible(false);
  pin->setTextFormat(Qt::RichText);
  pin->setTextInteractionFlags(Qt::TextBrowserInteraction);
  connect(pin, &QLabel::linkActivated, this,
          [this, imap](const QString&) { forget_pin(imap); });

  if (imap) {
    imap_stop_ = stop;
    imap_pin_ = pin;
  } else {
    smtp_stop_ = stop;
    smtp_pin_ = pin;
  }

  auto* row = new QHBoxLayout;
  row->addWidget(test);
  row->addWidget(stop);
  row->addStretch();
  layout->addLayout(row);
  layout->addWidget(status);
  layout->addWidget(pin);

  connect(enabled, &QCheckBox::toggled, this,
          &EMailAccountSettingsPage::slot_field_edited);
  connect(security, &QComboBox::currentIndexChanged, this,
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
  account_list_->setMaximumWidth(200);
  add_button_ = new QPushButton(tr("Add"), this);
  remove_button_ = new QPushButton(tr("Remove"), this);

  default_button_ = new QPushButton(tr("Set as Default"), this);
  default_button_->setToolTip(
      tr("The account offered first when sending, and the one used when "
         "nothing else is chosen."));
  connect(default_button_, &QPushButton::clicked, this,
          &EMailAccountSettingsPage::slot_set_default);

  auto* buttons = new QHBoxLayout;
  buttons->setContentsMargins(0, 0, 0, 0);
  buttons->addWidget(add_button_);
  buttons->addWidget(remove_button_);
  left->addWidget(account_list_);
  left->addLayout(buttons);
  left->addWidget(default_button_);

  // The whole left column goes away when there is nothing to list, so the
  // empty state has the page to itself rather than sitting in a narrow
  // column beside an empty form.
  left_panel_ = new QWidget(this);
  left_panel_->setLayout(left);

  // A page with no accounts used to be a blank disabled form, which reads as
  // something that failed to load rather than as somewhere to start. One
  // sentence and one button, centred: at this point there is exactly one
  // thing to do.
  empty_panel_ = new QWidget(this);
  auto* empty_layout = new QVBoxLayout(empty_panel_);
  empty_layout->setContentsMargins(0, 0, 0, 0);
  empty_layout->setSpacing(0);
  empty_layout->addStretch();

  auto* empty_title = new QLabel(tr("No mail accounts yet"), empty_panel_);
  empty_title->setAlignment(Qt::AlignCenter);
  EMailMakeTitle(empty_title);
  empty_layout->addWidget(empty_title);

  empty_notice_ = EMailEmptyNotice(
      empty_panel_,
      tr("An account is needed only to fetch and send messages. Everything "
         "else in GpgFrontend works without one."));
  empty_notice_->setMargin(8);
  empty_layout->addWidget(empty_notice_);

  empty_add_button_ = new QPushButton(tr("Add an Account"), empty_panel_);
  empty_add_button_->setMinimumWidth(180);
  empty_add_button_->setDefault(true);
  connect(empty_add_button_, &QPushButton::clicked, this,
          &EMailAccountSettingsPage::slot_add_account);

  auto* empty_row = new QHBoxLayout;
  empty_row->setContentsMargins(0, 16, 0, 0);
  empty_row->addStretch();
  empty_row->addWidget(empty_add_button_);
  empty_row->addStretch();
  empty_layout->addLayout(empty_row);

  empty_layout->addStretch();

  editor_ = new QWidget(this);
  auto* right = new QVBoxLayout(editor_);

  // Says what is missing before the user finds out by the account quietly not
  // being offered anywhere.
  validation_ = new QLabel(editor_);
  validation_->setWordWrap(true);
  validation_->setVisible(false);
  EMailMakeSecondary(validation_);
  right->addWidget(validation_);

  right->addWidget(build_identity_group());
  right->addWidget(build_transport_group(true));
  right->addWidget(build_transport_group(false));

  auto* credentials = new QGroupBox(tr("Password"), editor_);
  auto* credential_form = new QFormLayout(credentials);
  MakeFormNarrowable(credential_form);
  password_edit_ = new QLineEdit(credentials);
  password_edit_->setEchoMode(QLineEdit::Password);
  password_edit_->setPlaceholderText(tr("Leave blank to keep the stored one"));

  oauth_notice_ = new QLabel(
      tr("Signing in through a provider's own web page (OAuth2) is not "
         "supported yet. If your provider requires it, create an "
         "app-specific password and use that here."),
      credentials);
  oauth_notice_->setWordWrap(true);
  EMailMakeSecondary(oauth_notice_);

  credential_form->addRow(tr("Password"), password_edit_);
  credential_form->addRow(QString(), oauth_notice_);
  right->addWidget(credentials);
  right->addStretch();

  // Four group boxes and a paragraph do not fit a small settings dialog, and
  // without this the bottom of the form is simply unreachable.
  editor_scroll_ = new QScrollArea(this);
  editor_scroll_->setWidget(editor_);
  editor_scroll_->setWidgetResizable(true);
  editor_scroll_->setFrameShape(QFrame::NoFrame);

  // Vertically only. A form that scrolls sideways hides the right-hand end of
  // every field it contains, which is worse than the overflow it solves; the
  // fields shrink to the width available instead.
  editor_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  outer->addWidget(left_panel_);
  outer->addWidget(editor_scroll_, 1);
  outer->addWidget(empty_panel_, 1);

  connect(account_list_, &QListWidget::currentRowChanged, this,
          &EMailAccountSettingsPage::slot_selection_changed);
  connect(add_button_, &QPushButton::clicked, this,
          &EMailAccountSettingsPage::slot_add_account);
  connect(remove_button_, &QPushButton::clicked, this,
          &EMailAccountSettingsPage::slot_remove_account);
  connect(password_edit_, &QLineEdit::textEdited, this,
          &EMailAccountSettingsPage::slot_field_edited);

  apply_colors();
}

void EMailAccountSettingsPage::SetSettings() {
  loading_ = true;

  // Checked rather than plain: this page WRITES what it reads, and an empty
  // list that came back because the stored one could not be read must never be
  // stored over it.
  const auto loaded = EMailAccountStore::LoadChecked();
  accounts_ = loaded.accounts;
  may_store_ = loaded.MayStore();
  load_outcome_ = loaded.outcome;
  wipe_pending_passwords();
  pending_removals_.clear();

  const auto preferred = EMailAccountStore::DefaultAccount();
  default_id_ = preferred.id;

  refresh_list();
  loading_ = false;

  if (!accounts_.isEmpty()) account_list_->setCurrentRow(0);
  load_selected();
  refresh_enabled_state();
}

void EMailAccountSettingsPage::ApplySettings() {
  // Refused outright when the stored list could not be read. Writing here
  // would replace a newer build's accounts -- or a list this build simply
  // could not parse -- with an empty one, which is exactly what refusing to
  // read it was meant to prevent. Pressing OK in the settings dialog for some
  // unrelated reason must not erase the user's mail accounts.
  if (!may_store_) {
    QMessageBox::warning(
        this, tr("Mail Accounts Not Saved"),
        load_outcome_ == EMailAccountStore::LoadOutcome::kNEWER
            ? tr("These accounts were set up by a newer version of "
                 "GpgFrontend, so this version did not read them and has not "
                 "changed them.\n\nYour accounts are still there. Use the "
                 "newer version to edit them.")
            : tr("The stored mail accounts could not be read, so this version "
                 "has not changed them.\n\nNothing has been lost. If this "
                 "keeps happening, the stored list may need to be removed by "
                 "hand."));
    return;
  }

  store_selected();

  // Nothing was touched, so nothing is rewritten. This used to rewrite the
  // whole account list and re-save every stored password on every OK press,
  // whichever settings page the user had actually come for.
  if (!dirty_ && pending_passwords_.isEmpty() && pending_removals_.isEmpty()) {
    return;
  }

  EMailAccountStore::Store(accounts_, default_id_);

  // Deferred to here rather than done on the click: until OK is pressed the
  // removal has not happened, and a credential destroyed earlier could not be
  // brought back by Cancel.
  for (const auto& id : pending_removals_) EMailCredentialStore::Remove(id);
  pending_removals_.clear();

  // A password typed here is stored, full stop. There is no prompt anywhere
  // else and no per-account opt-out, so storing is the only way an account can
  // ever be used -- a choice about it would be a choice between working and
  // not working.
  for (const auto& account : accounts_) {
    const auto password = pending_passwords_.value(account.id);
    if (!password || password->IsEmpty()) continue;
    EMailCredentialStore::Save(account.id, *password);
  }

  wipe_pending_passwords();
}

auto EMailAccountSettingsPage::selected_index() const -> int {
  const auto row = account_list_->currentRow();
  return row >= 0 && row < accounts_.size() ? row : -1;
}

auto EMailAccountSettingsPage::label_for(const MailAccountConfig& account)
    -> QString {
  const auto label = account.Label();
  return label.isEmpty() ? tr("Untitled account") : label;
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
    // Which account is the default was decided somewhere and shown nowhere.
    const auto is_default = !default_id_.isEmpty() && account.id == default_id_;
    account_list_->addItem(is_default
                               ? tr("%1 (default)").arg(label_for(account))
                               : label_for(account));
  }
  if (row >= 0 && row < accounts_.size()) account_list_->setCurrentRow(row);

  // With nothing to list, the list, its buttons and the form are all
  // pointless, and the empty state takes the whole page.
  const bool any = !accounts_.isEmpty();
  if (left_panel_ != nullptr) left_panel_->setVisible(any);
  if (editor_scroll_ != nullptr) editor_scroll_->setVisible(any);
  if (empty_panel_ != nullptr) empty_panel_->setVisible(!any);

  loading_ = restoring;
}

auto EMailAccountSettingsPage::row_text_for(
    const MailAccountConfig& account) const -> QString {
  // One definition of what a row says. The per-keystroke update used to build
  // it separately from the full rebuild, so typing an address silently dropped
  // the "(default)" marker the rebuild had put there.
  const auto label = label_for(account);
  return !default_id_.isEmpty() && account.id == default_id_
             ? tr("%1 (default)").arg(label)
             : label;
}

void EMailAccountSettingsPage::refresh_current_label() {
  const auto index = selected_index();
  if (index < 0) return;

  auto* item = account_list_->item(index);
  if (item == nullptr) return;

  const auto label = row_text_for(accounts_.at(index));
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
  accounts_.append(account);
  dirty_ = true;

  if (default_id_.isEmpty()) default_id_ = account.id;

  refresh_list();
  account_list_->setCurrentRow(accounts_.size() - 1);
  load_selected();
  refresh_enabled_state();
}

void EMailAccountSettingsPage::slot_remove_account() {
  const auto index = selected_index();
  if (index < 0) return;

  const auto& account = accounts_.at(index);

  // Asked, because there is no undo and the account carries a password that
  // cannot be recovered once it is gone. A single click is not enough for
  // that.
  if (QMessageBox::question(
          this, tr("Remove Account"),
          tr("Remove %1?\n\nIts stored password is forgotten as well. This "
             "cannot be undone once you press OK in this dialog.")
              .arg(label_for(account)),
          QMessageBox::Yes | QMessageBox::Cancel,
          QMessageBox::Cancel) != QMessageBox::Yes) {
    return;
  }

  const auto id = account.id;
  accounts_.removeAt(index);
  pending_passwords_.remove(id);
  dirty_ = true;

  // Staged, not done. Forgetting the secret here would survive a Cancel: the
  // account list comes back from settings, but the password would already be
  // gone, and nothing on screen would say so -- it would surface later as an
  // authentication failure with no explanation.
  if (!pending_removals_.contains(id)) pending_removals_.append(id);

  refresh_list();

  // Removing the last row used to leave nothing selected, because the restored
  // row index was one past the end. Fall back to the new last row.
  if (!accounts_.isEmpty()) {
    account_list_->setCurrentRow(qMin(index, accounts_.size() - 1));
  }

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

  // Before store_selected(): the choices on offer follow the host, and storing
  // a selection that the new host no longer permits is how a cleartext account
  // used to be converted behind the user's back.
  refresh_security_choices();

  store_selected();
  dirty_ = true;
  refresh_current_label();
  refresh_port_hints();
  refresh_validation();
  refresh_enabled_state();
}

/// Keeps the "Port 993" hints beside the connection combos truthful. They are
/// labels, never inputs: the number is derived from the choice, so it can only
/// ever be read.
void EMailAccountSettingsPage::slot_set_default() {
  const auto index = selected_index();
  if (index < 0) return;

  default_id_ = accounts_.at(index).id;
  dirty_ = true;

  // The list is what shows which one it is, so it is what has to be redrawn.
  refresh_list();
  account_list_->setCurrentRow(index);
  refresh_enabled_state();
}

void EMailAccountSettingsPage::apply_colors() {
  // The single place this page's colours are decided, so a theme change has
  // one thing to call. This page was the only substantial view in the module
  // with no colour conventions at all; now that it has them, it also has to
  // follow a theme change like the rest.
  if (empty_notice_ != nullptr) {
    EMailSetLabelColor(empty_notice_, EMailMutedColor(this));
  }
  if (oauth_notice_ != nullptr) {
    EMailSetLabelColor(oauth_notice_, EMailMutedColor(this));
  }

  // These decide their colour from what they are currently saying, so they are
  // asked again rather than recoloured from here.
  refresh_validation();
  refresh_pin_state();
}

void EMailAccountSettingsPage::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);
  if (EMailIsRestyle(event)) apply_colors();
}

void EMailAccountSettingsPage::refresh_validation() {
  if (validation_ == nullptr) return;

  const auto index = selected_index();
  if (index < 0) {
    validation_->setVisible(false);
    return;
  }

  const auto& account = accounts_.at(index);

  // Said as soon as it is true, and only about things the user can act on.
  // None of this refuses to save: a half-finished account is a normal state to
  // leave a settings page in, and an account that cannot be used is simply not
  // offered elsewhere. The point is that it stops being SILENT about why.
  QStringList problems;

  if (account.address.trimmed().isEmpty()) {
    problems.append(tr("This account needs an e-mail address."));
  } else if (!MailIsPlausibleAddress(account.address)) {
    problems.append(tr("\"%1\" does not look like an e-mail address.")
                        .arg(account.address));
  }

  if (account.imap.enabled && account.imap.host.trimmed().isEmpty()) {
    problems.append(tr("Receiving is turned on but no server is set."));
  }
  if (account.smtp.enabled && account.smtp.host.trimmed().isEmpty()) {
    problems.append(tr("Sending is turned on but no server is set."));
  }

  if (!account.imap.enabled && !account.smtp.enabled) {
    problems.append(
        tr("Turn on receiving, sending, or both. An account that does "
           "neither cannot be used."));
  }

  // Two accounts with the same address are indistinguishable in every list
  // that offers them, and the one that gets picked is whichever comes first.
  for (int i = 0; i < accounts_.size(); ++i) {
    if (i == index) continue;
    const auto& other = accounts_.at(i);
    if (other.address.trimmed().isEmpty()) continue;
    if (other.address.compare(account.address.trimmed(), Qt::CaseInsensitive) !=
        0) {
      continue;
    }
    problems.append(
        tr("Another account already uses this address. They cannot be told "
           "apart where accounts are offered."));
    break;
  }

  validation_->setVisible(!problems.isEmpty());
  if (problems.isEmpty()) return;

  validation_->setText(problems.join("\n"));
  EMailSetLabelColor(validation_, EMailWarningColor(validation_));
}

void EMailAccountSettingsPage::refresh_security_choices() {
  // The host decides which choices exist: typing a remote address into an
  // account that was cleartext takes the option away, and the combo falls back
  // to the safest one rather than keeping a selection the worker would refuse.
  const auto rebuild = [](QComboBox* combo, QLineEdit* host) {
    const auto current = SecurityOf(combo);
    FillSecurityCombo(combo, host->text().trimmed());
    SelectSecurity(combo, current);
  };

  rebuild(imap_security_, imap_host_);
  rebuild(smtp_security_, smtp_host_);
}

void EMailAccountSettingsPage::refresh_port_hints() {
  const auto describe = [](QComboBox* combo, bool imap) {
    return tr("Port %1").arg(MailDefaultPort(imap, SecurityOf(combo)));
  };
  if (imap_port_hint_ != nullptr) {
    imap_port_hint_->setText(describe(imap_security_, true));
  }
  if (smtp_port_hint_ != nullptr) {
    smtp_port_hint_->setText(describe(smtp_security_, false));
  }
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
  FillSecurityCombo(imap_security_, account.imap.host);
  SelectSecurity(imap_security_, account.imap.tls);
  imap_user_->setText(account.imap.username);
  sent_folder_->setText(account.sent_folder_override);

  smtp_enabled_->setChecked(account.smtp.enabled);
  smtp_host_->setText(account.smtp.host);
  FillSecurityCombo(smtp_security_, account.smtp.host);
  SelectSecurity(smtp_security_, account.smtp.tls);
  smtp_user_->setText(account.smtp.username);

  // Deliberately NOT refilled from the staged secret. Rendering it back into
  // the field would create another QString copy that cannot be erased, to show
  // the user a row of dots they cannot read anyway. An empty field means
  // "unchanged"; what was staged is still applied on Save.
  password_edit_->clear();

  refresh_port_hints();
  refresh_pin_state();
  refresh_validation();

  // Cleared with their tooltips: a protocol detail left behind would describe
  // the previous account's failure.
  for (auto* label : {imap_status_, smtp_status_}) {
    label->clear();
    label->setToolTip({});
  }

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

  account.smtp.enabled = smtp_enabled_->isChecked();
  account.smtp.host = smtp_host_->text().trimmed();
  account.smtp.tls = SecurityOf(smtp_security_);
  account.smtp.username = smtp_user_->text().trimmed();

  account.sent_folder_override = sent_folder_->text().trimmed();

  // Out of the widget and into an erasable secret at once. The QString handed
  // over here is still beyond anyone's reach -- see EMailSecret -- but it goes
  // out of scope at the end of this function instead of living in a map for as
  // long as the settings page is open.
  const auto typed = password_edit_->text();
  if (!typed.isEmpty()) {
    pending_passwords_[account.id] = EMailSecret::CopyFrom(typed);
  }
}

void EMailAccountSettingsPage::refresh_enabled_state() {
  const auto index = selected_index();
  const auto has_selection = index >= 0;
  editor_->setEnabled(has_selection);
  remove_button_->setEnabled(has_selection);

  // Off for the account that already is the default: pressing it would be a
  // no-op, and a button that does nothing is a question about whether it
  // worked.
  default_button_->setEnabled(has_selection &&
                              accounts_.at(index).id != default_id_);

  for (auto* widget :
       {static_cast<QWidget*>(imap_host_),
        static_cast<QWidget*>(imap_security_),
        static_cast<QWidget*>(imap_user_), static_cast<QWidget*>(sent_folder_),
        static_cast<QWidget*>(imap_test_)}) {
    widget->setEnabled(imap_enabled_->isChecked());
  }

  for (auto* widget :
       {static_cast<QWidget*>(smtp_host_),
        static_cast<QWidget*>(smtp_security_),
        static_cast<QWidget*>(smtp_user_), static_cast<QWidget*>(smtp_test_)}) {
    widget->setEnabled(smtp_enabled_->isChecked());
  }
}

void EMailAccountSettingsPage::slot_test_imap() {
  test_transport(true, imap_status_);
}

void EMailAccountSettingsPage::slot_test_smtp() {
  test_transport(false, smtp_status_);
}

void EMailAccountSettingsPage::refresh_pin_state() {
  const auto index = selected_index();

  const auto show = [this, index](QLabel* label, bool imap) {
    if (label == nullptr) return;
    if (index < 0) {
      label->setVisible(false);
      return;
    }

    const auto& account = accounts_.at(index);
    const auto& pin = (imap ? account.imap : account.smtp).pinned_cert_sha256;
    if (pin.isEmpty()) {
      label->setVisible(false);
      return;
    }

    // The fingerprint is shown in full: it is the whole content of the
    // decision, and an abbreviated one cannot be compared against anything.
    label->setText(tr("Trusting one certificate: %1 &nbsp; <a "
                      "href=\"forget\">Stop trusting it</a>")
                       .arg(pin.toUpper()));
    EMailSetLabelColor(label, EMailMutedColor(label));
    label->setVisible(true);
  };

  show(imap_pin_, true);
  show(smtp_pin_, false);
}

void EMailAccountSettingsPage::forget_pin(bool imap) {
  const auto index = selected_index();
  if (index < 0) return;

  auto& account = accounts_[index];
  (imap ? account.imap : account.smtp).pinned_cert_sha256.clear();
  refresh_pin_state();
}

void EMailAccountSettingsPage::set_status(QLabel* status, const QString& text,
                                          StatusTone tone) {
  if (status == nullptr) return;
  status->setText(text);

  // A failure and a success used to look identical apart from their wording.
  switch (tone) {
    case StatusTone::kGOOD:
      EMailSetLabelColor(status, EMailAccentColor(status, true));
      break;
    case StatusTone::kBAD:
      EMailSetLabelColor(status, EMailWarningColor(status));
      break;
    case StatusTone::kPLAIN:
      EMailSetLabelColor(status, EMailMutedColor(status));
      break;
  }
}

void EMailAccountSettingsPage::set_testing(bool testing) {
  // Everything a running test reads must stay still while it runs. Only the
  // two test buttons used to be disabled, so the account could be switched,
  // edited or deleted underneath a probe that had already copied it.
  account_list_->setEnabled(!testing);
  add_button_->setEnabled(!testing);
  remove_button_->setEnabled(!testing);
  editor_->setEnabled(!testing && selected_index() >= 0);

  if (testing) {
    imap_test_->setEnabled(false);
    smtp_test_->setEnabled(false);
  } else {
    refresh_enabled_state();
  }

  // Only beside the test that is actually running.
  const auto running_imap = testing && probe_ != nullptr && probe_->imap;
  imap_stop_->setVisible(testing && running_imap);
  smtp_stop_->setVisible(testing && !running_imap);
}

void EMailAccountSettingsPage::slot_stop_test() {
  auto probe = probe_;
  if (probe == nullptr) return;

  probe->cancelled = true;

  // The worker publishes its token as soon as it has one. A Stop pressed
  // before that lands still counts -- `cancelled` is already set, so the
  // outcome is reported as stopped whatever the connection went on to do.
  const std::lock_guard<std::mutex> guard(probe->mutex);
  // A probe is a one-shot with no requests behind it, so stopping it means
  // stopping everything it might still do.
  if (probe->token != nullptr) probe->token->CancelAll();
}

void EMailAccountSettingsPage::wipe_pending_passwords() {
  // Really overwritten now. These were QStrings, and the fill() that stood
  // here claimed to reach their buffers -- it did not: the value came from
  // QLineEdit::text(), which is implicitly shared with the widget, so fill()
  // detached and zeroed a fresh copy while the original went on living inside
  // the QLineEdit and was later freed with the password still in it.
  //
  // What matters as much is that this happens on EVERY path out: clear() alone
  // freed the plaintext, and two of the three exits took that route.
  for (auto& password : pending_passwords_) {
    if (password) password->Wipe();
  }
  pending_passwords_.clear();
}

void EMailAccountSettingsPage::report_probe_error(const MailError& error,
                                                  bool imap, QLabel* status) {
  // The category is the part worth reading. This used to concatenate three
  // strings and throw the classification away, so a wrong password and an
  // unreachable host produced the same shape of answer and neither said what
  // to do next.
  QString advice;
  switch (error.category) {
    case MailErrorCategory::kAUTH:
      advice = tr("Check the user name and password.");
      break;
    case MailErrorCategory::kDNS:
      advice = tr("Check the server address for a typo.");
      break;
    case MailErrorCategory::kCONNECT:
      advice =
          tr("The address resolves but nothing answered. Check the "
             "connection type, and whether the server is reachable from "
             "here.");
      break;
    case MailErrorCategory::kTLS_UNTRUSTED:
      advice =
          tr("The server's certificate is not signed by an authority this "
             "computer trusts. If you run this server yourself, you can "
             "choose to trust its certificate.");
      break;
    case MailErrorCategory::kTLS_EXPIRED:
      advice =
          tr("The server's certificate is expired or not yet valid. Check "
             "the clock on both machines before anything else.");
      break;
    case MailErrorCategory::kTLS_HOSTNAME:
      advice =
          tr("The server's certificate is for a different host. Check the "
             "server address.");
      break;
    case MailErrorCategory::kTLS_REQUIRED:
      advice =
          tr("The server would not start an encrypted session. Nothing was "
             "sent to it.");
      break;
    case MailErrorCategory::kTLS_HANDSHAKE:
      advice = tr("The encrypted connection could not be established.");
      break;
    case MailErrorCategory::kTIMEOUT:
      advice = tr("The server stopped answering. Try again.");
      break;
    case MailErrorCategory::kINTERNAL:
      advice = tr("This is a fault in GpgFrontend rather than in the server.");
      break;
    default:
      break;
  }

  // Only where it is not already implied by the advice: "try again" under a
  // wrong password would be wrong, and under a timeout it is redundant.
  if (advice.isEmpty() && error.transient) {
    advice = tr("This may be temporary. Try again.");
  }

  auto text = error.title;
  if (!error.detail.isEmpty()) text += "\n" + error.detail;
  if (!advice.isEmpty()) text += "\n" + advice;
  set_status(status, text, StatusTone::kBAD);

  // The raw protocol line is kept out of the status label and put where it can
  // be copied: it is written for a bug report, not for a person deciding what
  // to do next.
  status->setToolTip(error.protocol_detail);

  if (error.IsPinnable()) offer_certificate_pin(imap, status);
}

void EMailAccountSettingsPage::offer_certificate_pin(bool imap,
                                                     QLabel* status) {
  const auto index = selected_index();
  if (index < 0) return;

  const auto fingerprint = EMailTlsSetup::LastSeenFingerprint();
  if (fingerprint.isEmpty()) return;

  // The fingerprint is put in front of the user and the decision is theirs.
  // Trusting a certificate because a dialog asked is not a decision, so the
  // default button is Cancel and the question names exactly what is being
  // trusted and how far that trust goes.
  const auto summary = EMailTlsSetup::LastSeenCertificateSummary();
  const auto question =
      tr("This server presented a certificate that no authority vouches for. "
         "That is normal for a server you run yourself, and it is also what "
         "an interception looks like.\n\n%1\nSHA-256: %2\n\nTrust this "
         "exact certificate for this account? Any other certificate, "
         "including a later replacement of this one, will still be "
         "refused.")
          .arg(summary.isEmpty() ? tr("(no certificate details)") : summary,
               fingerprint);

  if (QMessageBox::question(this, tr("Trust This Certificate?"), question,
                            QMessageBox::Yes | QMessageBox::Cancel,
                            QMessageBox::Cancel) != QMessageBox::Yes) {
    return;
  }

  // Pinned on the transport the test was actually for. The two are configured
  // separately and may well be different machines.
  auto& account = accounts_[index];
  (imap ? account.imap : account.smtp).pinned_cert_sha256 =
      fingerprint.toLower();

  set_status(status,
             tr("Certificate trusted for this account. Test again to confirm."),
             StatusTone::kPLAIN);
  refresh_pin_state();
}

void EMailAccountSettingsPage::test_transport(bool imap, QLabel* status) {
  // Reads the form; does not commit it. Testing used to call store_selected(),
  // so a test was also an edit -- which, before the connection choices
  // followed the host, was one of the ways a cleartext account got converted.
  const auto index = selected_index();
  if (index < 0) return;

  // Built from what is on screen, so the test is about what the user is
  // looking at rather than about what was last stored.
  auto account = accounts_.at(index);
  account.address = address_edit_->text().trimmed();
  account.imap.host = imap_host_->text().trimmed();
  account.imap.tls = SecurityOf(imap_security_);
  account.imap.username = imap_user_->text().trimmed();
  account.smtp.host = smtp_host_->text().trimmed();
  account.smtp.tls = SecurityOf(smtp_security_);
  account.smtp.username = smtp_user_->text().trimmed();

  const auto& config = imap ? account.imap : account.smtp;

  if (config.host.isEmpty()) {
    set_status(status, tr("Enter a server address first."), StatusTone::kPLAIN);
    return;
  }

  // Typed field first, then anything staged but not yet applied, then what is
  // already stored. Whichever it is, it ends up in an EMailSecret so the copy
  // this function holds can actually be erased.
  auto password = EMailSecret::CopyFrom(password_edit_->text());
  if (password->IsEmpty()) {
    const auto staged = pending_passwords_.value(account.id);
    if (staged && !staged->IsEmpty()) password = staged;
  }
  if (password->IsEmpty()) password = EMailCredentialStore::Load(account.id);
  if (password->IsEmpty()) {
    set_status(status, tr("Enter a password first."), StatusTone::kPLAIN);
    return;
  }

  // One test at a time. Two probes would fight over the same busy state and
  // the same status labels.
  if (probe_ != nullptr) return;

  set_status(status, tr("Testing..."), StatusTone::kPLAIN);

  // Everything the probe needs to report back, on the heap and owned by a
  // shared_ptr. The stack frame this was written on used to be alive for the
  // whole test because the GUI thread spun waiting for it; now it returns
  // immediately, so capturing its locals by reference would be a dangling
  // write from the other thread.
  auto probe = std::make_shared<Probe>();
  probe->seq = ++probe_seq_;
  probe->imap = imap;
  probe_ = probe;

  const auto seq = probe->seq;
  probe_thread_ = QThread::create([probe, account, password, imap]() {
    const auto publish = [&probe](const EMailCancelTokenPtr& token) {
      // Published so Stop can reach it. Under the mutex because the GUI thread
      // reads it the moment this function returns.
      const std::lock_guard<std::mutex> guard(probe->mutex);
      probe->token = token;
    };

    if (imap) {
      // Called directly rather than through the event loop: the worker's
      // slots are synchronous, so the signal has already been delivered by
      // the time Connect() returns.
      EMailImapWorker worker;
      publish(worker.Token());
      QObject::connect(
          &worker, &EMailImapWorker::SignalFailed, &worker,
          [probe](quint64, const MailError& e) { probe->error = e; });
      QObject::connect(&worker, &EMailImapWorker::SignalConnected, &worker,
                       [probe](quint64) { probe->connected = true; });
      worker.Connect(1, account, password);
      worker.Disconnect();
    } else {
      EMailSmtpWorker worker;
      publish(worker.Token());
      QObject::connect(&worker, &EMailSmtpWorker::SignalFinished, &worker,
                       [probe](quint64, const EMailSendReceipt& receipt) {
                         probe->error = receipt.error;
                         probe->connected = receipt.accepted;
                       });
      worker.TestConnection(1, account, password);
    }
  });

  // `this` as the context object, so a page closed while the probe is running
  // never sees this run at all -- the settings dialog is modeless and deletes
  // itself on close, and the old code dereferenced its widgets after spinning
  // the event loop, which is exactly how that page got closed underneath it.
  // `this` as the context object, so a page closed while the probe is running
  // never sees this run at all -- the settings dialog is modeless and deletes
  // itself on close, and the old code dereferenced its widgets after spinning
  // the event loop, which is exactly how that page got closed underneath it.
  connect(probe_thread_, &QThread::finished, this,
          [this, probe, seq, status]() { finish_probe(probe, seq, status); });
  connect(probe_thread_, &QThread::finished, probe_thread_,
          &QObject::deleteLater);
  connect(probe_thread_, &QThread::finished, this,
          [this]() { probe_thread_ = nullptr; });

  set_testing(true);
  probe_thread_->start();

  // Released rather than wiped, and that is now a real release: the probe
  // holds the same EMailSecret, and the bytes are erased when the last holder
  // drops them. This used to be a QString, where filling it would have
  // detached and zeroed a fresh copy while leaving the probe's buffer intact.
  password.reset();
}

EMailAccountSettingsPage::~EMailAccountSettingsPage() {
  // A probe outliving this page is not a use-after-free -- everything it
  // touches is held by value or by shared_ptr, and the result is delivered
  // with `this` as context -- but it is a live socket and a live credential
  // with nothing left that can stop it: slot_stop_test() goes away with the
  // page, and the token is only reachable through probe_.
  if (probe_ != nullptr) {
    const std::lock_guard<std::mutex> guard(probe_->mutex);
    probe_->cancelled = true;
    if (probe_->token != nullptr) probe_->token->CancelAll();
  }

  if (probe_thread_ != nullptr) {
    // Bounded, and deliberately not followed by terminate(): killing a thread
    // inside OpenSSL or getaddrinfo corrupts process state. A thread that will
    // not come back is left to finish and delete itself.
    if (probe_thread_->wait(5000)) {
      delete probe_thread_;
    } else {
      LOG_ERROR("mail account test thread did not stop; detaching it");
      connect(probe_thread_, &QThread::finished, probe_thread_,
              &QObject::deleteLater);
    }
    probe_thread_ = nullptr;
  }
}

void EMailAccountSettingsPage::finish_probe(const std::shared_ptr<Probe>& probe,
                                            quint64 seq, QLabel* status) {
  // A superseded probe reports nothing: its answer is about an account or a
  // form that has since moved on.
  if (seq != probe_seq_) return;

  probe_ = nullptr;
  set_testing(false);

  if (probe->cancelled) {
    set_status(status, tr("Stopped."), StatusTone::kPLAIN);
    return;
  }

  // Judged on having actually connected, not on the absence of an error: an
  // internal failure is not a successful login, and reporting one as success
  // is worse than reporting nothing.
  if (probe->connected) {
    set_status(status, tr("Connected successfully."), StatusTone::kGOOD);
    return;
  }

  if (!probe->error.IsError()) {
    set_status(status, tr("Could not connect, and the reason is not known."),
               StatusTone::kBAD);
    return;
  }

  report_probe_error(probe->error, probe->imap, status);
}

auto EMailAccountSettingsPageFactory(void* /*data*/) -> void* {
  return new EMailAccountSettingsPage(nullptr);
}
