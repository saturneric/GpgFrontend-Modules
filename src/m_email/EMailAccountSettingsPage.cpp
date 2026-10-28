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
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "EMailAccountForm.h"
#include "EMailAccountSetup.h"
#include "EMailAccountStore.h"
#include "EMailAccountWizard.h"
#include "EMailCredentialStore.h"
#include "EMailViewStyle.h"
#include "EMailWizardWidgets.h"

namespace {

/// Lets a form give up its two-column shape when the page is too narrow for
/// it, rather than keeping the columns and clipping the right-hand one.
///
/// This page lives inside the host's settings dialog, whose width is not ours
/// to choose.
auto NarrowableForm(QWidget* parent) -> QFormLayout* {
  auto* form = new QFormLayout(parent);
  form->setContentsMargins(0, 0, 0, 0);
  form->setRowWrapPolicy(QFormLayout::WrapLongRows);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  return form;
}

}  // namespace

EMailAccountSettingsPage::EMailAccountSettingsPage(QWidget* parent)
    : QWidget(parent), probe_(new EMailAccountProbe(this)) {
  connect(probe_, &EMailAccountProbe::SignalFinished, this,
          &EMailAccountSettingsPage::on_test_finished);
  build_ui();
  LoadSettings();
}

EMailAccountSettingsPage::~EMailAccountSettingsPage() {
  // The probe cancels its own test as it goes; the staged passwords are
  // erased here because no Apply is coming for them.
  wipe_pending_passwords();
  if (tested_password_) tested_password_.reset();
}

auto EMailAccountSettingsPage::controls(bool imap) -> TransportControls& {
  return imap ? imap_ : smtp_;
}

auto EMailAccountSettingsPage::build_empty_panel() -> QWidget* {
  // A page with no accounts used to be a blank disabled form, which reads as
  // something that failed to load rather than as somewhere to start. One
  // sentence and one button, centred: at this point there is exactly one
  // thing to do.
  auto* panel = new QWidget(this);
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addStretch();

  auto* title = new QLabel(tr("No mail accounts yet"), panel);
  title->setAlignment(Qt::AlignCenter);
  EMailMakeTitle(title);
  layout->addWidget(title);

  empty_notice_ = EMailEmptyNotice(
      panel, tr("An account is needed only to fetch and send messages. "
                "Everything else in GpgFrontend works without one."));
  empty_notice_->setMargin(8);
  layout->addWidget(empty_notice_);

  empty_add_button_ = new QPushButton(tr("Add an Account"), panel);
  empty_add_button_->setMinimumWidth(180);
  empty_add_button_->setDefault(true);
  connect(empty_add_button_, &QPushButton::clicked, this,
          &EMailAccountSettingsPage::slot_add_account);

  auto* row = new QHBoxLayout;
  row->setContentsMargins(0, 16, 0, 0);
  row->addStretch();
  row->addWidget(empty_add_button_);
  row->addStretch();
  layout->addLayout(row);

  layout->addStretch();
  return panel;
}

auto EMailAccountSettingsPage::build_list_panel() -> QWidget* {
  auto* panel = new QWidget(this);
  auto* layout = new QVBoxLayout(panel);
  layout->setContentsMargins(0, 0, 0, 0);

  account_list_ = new QListWidget(panel);
  account_list_->setMaximumWidth(200);
  account_list_->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(account_list_, &QWidget::customContextMenuRequested, this,
          &EMailAccountSettingsPage::show_account_menu);

  add_button_ = new QPushButton(tr("Add Account..."), panel);
  connect(add_button_, &QPushButton::clicked, this,
          &EMailAccountSettingsPage::slot_add_account);

  layout->addWidget(account_list_, 1);
  layout->addWidget(add_button_);

  connect(account_list_, &QListWidget::currentRowChanged, this,
          &EMailAccountSettingsPage::slot_selection_changed);
  return panel;
}

void EMailAccountSettingsPage::build_transport_rows(bool imap, QWidget* parent,
                                                    QVBoxLayout* layout) {
  auto& c = controls(imap);

  // Short, because the switch cannot wrap and a long label sets the minimum
  // width of the whole page.
  c.enabled = new QCheckBox(
      imap ? tr("Use this account to receive") : tr("Use this account to send"),
      parent);

  c.fields = new QWidget(parent);
  auto* form = NarrowableForm(c.fields);

  c.host = new QLineEdit(c.fields);
  c.host->setClearButtonEnabled(true);
  c.security = new QComboBox(c.fields);
  EMailFillSecurityCombo(c.security, {});

  // The port is policy, not configuration: it follows from the connection
  // choice and there is no field for it. Showing which port that means keeps
  // the policy visible without turning it back into a decision.
  c.port_hint = new QLabel(c.fields);
  EMailMakeSecondary(c.port_hint);

  auto* connection_row = new QHBoxLayout;
  connection_row->setContentsMargins(0, 0, 0, 0);
  connection_row->addWidget(c.security, 1);
  connection_row->addWidget(c.port_hint);

  form->addRow(tr("Server"), c.host);
  form->addRow(tr("Connection"), connection_row);

  // One line that is always there, so a result arriving -- or a long error --
  // never pushes the rest of the form down.
  c.status = new EMailProbeStatus(parent);
  c.status->SetFixedLine(tr("Not tested"));

  auto* heading_row = new QHBoxLayout;
  heading_row->setContentsMargins(0, 0, 0, 0);
  heading_row->addWidget(c.enabled);
  heading_row->addStretch(1);

  layout->addLayout(heading_row);
  layout->addWidget(c.fields);
  layout->addWidget(c.status);

  connect(c.enabled, &QAbstractButton::toggled, this,
          &EMailAccountSettingsPage::slot_field_edited);
  connect(c.security, &QComboBox::currentIndexChanged, this,
          &EMailAccountSettingsPage::slot_field_edited);
  connect(c.host, &QLineEdit::textEdited, this,
          &EMailAccountSettingsPage::slot_field_edited);
}

auto EMailAccountSettingsPage::build_servers_card() -> QWidget* {
  auto* card = new QGroupBox(tr("Servers"), editor_);
  auto* layout = new QVBoxLayout(card);

  build_transport_rows(true, card, layout);
  layout->addSpacing(4);
  layout->addWidget(EMailRule(card));
  layout->addSpacing(4);
  build_transport_rows(false, card, layout);

  test_button_ = new QPushButton(tr("Test Connection"), card);
  stop_button_ = new QPushButton(tr("Stop"), card);
  // Shown only while a test is running. Without it a probe against a
  // black-holed host holds the page for the full timeout with nothing to
  // press.
  stop_button_->setVisible(false);
  // Swapped for each other while a test runs, so they are one width.
  const auto width = std::max(test_button_->sizeHint().width(),
                              stop_button_->sizeHint().width());
  test_button_->setMinimumWidth(width);
  stop_button_->setMinimumWidth(width);
  connect(test_button_, &QPushButton::clicked, this,
          &EMailAccountSettingsPage::slot_test);
  connect(stop_button_, &QPushButton::clicked, probe_,
          &EMailAccountProbe::Stop);

  auto* buttons = new QHBoxLayout;
  buttons->setContentsMargins(0, 4, 0, 0);
  buttons->addWidget(test_button_);
  buttons->addWidget(stop_button_);
  buttons->addStretch(1);
  layout->addLayout(buttons);
  return card;
}

auto EMailAccountSettingsPage::build_advanced() -> QWidget* {
  auto* content = new QWidget(editor_);
  auto* layout = new QVBoxLayout(content);
  layout->setContentsMargins(0, 0, 0, 0);

  auto* fields = new QWidget(content);
  auto* form = NarrowableForm(fields);

  reply_to_edit_ = new QLineEdit(fields);
  reply_to_edit_->setPlaceholderText(tr("Optional"));

  imap_.user = new QLineEdit(fields);
  smtp_.user = new QLineEdit(fields);
  smtp_.user->setPlaceholderText(tr("Empty: send without signing in"));

  // The one remaining override, and it earns its place: a server that does
  // not advertise RFC 6154 SPECIAL-USE cannot have its Sent folder
  // discovered, and guessing the name would be worse than asking.
  sent_folder_ = new QLineEdit(fields);
  sent_folder_->setPlaceholderText(tr("Discovered automatically"));

  form->addRow(tr("Reply-To"), reply_to_edit_);
  form->addRow(tr("Incoming user name"), imap_.user);
  form->addRow(tr("Outgoing user name"), smtp_.user);
  form->addRow(tr("Sent folder"), sent_folder_);
  layout->addWidget(fields);

  // Says a certificate is pinned for a transport, and offers the only way to
  // take it back. A pin the user cannot see is a trust decision they cannot
  // revisit.
  for (const bool imap : {true, false}) {
    auto* pin = new QLabel(content);
    pin->setWordWrap(true);
    pin->setVisible(false);
    pin->setTextFormat(Qt::RichText);
    pin->setTextInteractionFlags(Qt::TextBrowserInteraction);
    connect(pin, &QLabel::linkActivated, this,
            [this, imap](const QString&) { forget_pin(imap); });
    controls(imap).pin = pin;
    layout->addWidget(pin);
  }

  for (auto* edit : {reply_to_edit_, imap_.user, smtp_.user, sent_folder_}) {
    connect(edit, &QLineEdit::textEdited, this,
            &EMailAccountSettingsPage::slot_field_edited);
  }
  return EMailDisclosure(tr("Advanced"), content, editor_);
}

auto EMailAccountSettingsPage::build_editor() -> QWidget* {
  editor_ = new QWidget(this);
  auto* layout = new QVBoxLayout(editor_);
  layout->setSpacing(10);

  // Which account this is. What is done to an account as a whole -- making it
  // the default, removing it -- is on the list's context menu.
  heading_ = new QLabel(editor_);
  EMailMakeTitle(heading_);
  heading_->setTextFormat(Qt::PlainText);
  heading_detail_ = new QLabel(editor_);
  heading_detail_->setTextFormat(Qt::PlainText);
  EMailMakeSecondary(heading_detail_);

  auto* titles = new QVBoxLayout;
  titles->setSpacing(2);
  titles->addWidget(heading_);
  titles->addWidget(heading_detail_);

  layout->addLayout(titles);

  // Says what is missing before the user finds out by the account quietly not
  // being offered anywhere.
  validation_ = new QLabel(editor_);
  validation_->setWordWrap(true);
  validation_->setVisible(false);
  gf::ui::SetLabelColor(validation_, gf::ui::WarningColor(validation_));
  layout->addWidget(validation_);

  auto* account_card = new QGroupBox(tr("Account"), editor_);
  auto* account_layout = new QVBoxLayout(account_card);
  auto* account_fields = new QWidget(account_card);
  auto* form = NarrowableForm(account_fields);

  display_name_edit_ = new QLineEdit(account_fields);
  address_edit_ = new QLineEdit(account_fields);
  password_edit_ = new QLineEdit(account_fields);
  password_edit_->setEchoMode(QLineEdit::Password);
  password_edit_->setPlaceholderText(tr("Leave blank to keep the stored one"));

  form->addRow(tr("Display name"), display_name_edit_);
  form->addRow(tr("Email address"), address_edit_);
  form->addRow(tr("Password"), password_edit_);
  account_layout->addWidget(account_fields);

  oauth_notice_ = new QLabel(
      tr("Signing in through a provider's own web page (OAuth2) is not "
         "supported yet. If your provider requires it, create an "
         "app-specific password and use that here."),
      account_card);
  oauth_notice_->setWordWrap(true);
  EMailMakeSecondary(oauth_notice_);
  account_layout->addWidget(oauth_notice_);
  layout->addWidget(account_card);

  for (auto* edit : {display_name_edit_, address_edit_, password_edit_}) {
    connect(edit, &QLineEdit::textEdited, this,
            &EMailAccountSettingsPage::slot_field_edited);
  }

  layout->addWidget(build_servers_card());
  layout->addWidget(build_advanced());
  layout->addStretch();
  return editor_;
}

void EMailAccountSettingsPage::build_ui() {
  auto* outer = new QHBoxLayout(this);

  left_panel_ = build_list_panel();
  empty_panel_ = build_empty_panel();

  // The editor scrolls vertically only. A form that scrolls sideways hides the
  // right-hand end of every field it contains; the fields shrink to the width
  // available instead.
  editor_scroll_ = new QScrollArea(this);
  editor_scroll_->setWidget(build_editor());
  editor_scroll_->setWidgetResizable(true);
  editor_scroll_->setFrameShape(QFrame::NoFrame);
  editor_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  outer->addWidget(left_panel_);
  outer->addWidget(editor_scroll_, 1);
  outer->addWidget(empty_panel_, 1);

  apply_colors();
}

void EMailAccountSettingsPage::LoadSettings() {
  probe_->Stop();
  test_queue_.clear();
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
  dirty_ = false;

  // The stored choice, so a disabled default is shown as what it is. Only a
  // default naming no listed account falls back to the one offered first.
  default_id_ = EMailAccountStore::StoredDefaultId();
  const auto listed = std::any_of(
      accounts_.cbegin(), accounts_.cend(),
      [this](const MailAccountConfig& a) { return a.id == default_id_; });
  if (!listed) default_id_ = EMailAccountStore::DefaultAccount().id;

  refresh_list();
  loading_ = false;

  if (!accounts_.isEmpty()) account_list_->setCurrentRow(0);
  load_selected();
  refresh_enabled_state();
}

auto EMailAccountSettingsPage::ApplySettings() -> bool {
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
    return false;
  }

  store_selected();

  // Nothing was touched, so nothing is rewritten.
  if (!dirty_ && pending_passwords_.isEmpty() && pending_removals_.isEmpty()) {
    return false;
  }

  EMailAccountStore::Store(accounts_, default_id_);

  // Deferred to here rather than done on the click: until OK is pressed the
  // removal has not happened, and a credential destroyed earlier could not be
  // brought back by Cancel.
  for (const auto& id : pending_removals_) EMailCredentialStore::Remove(id);
  pending_removals_.clear();

  // A password typed here is stored, full stop. There is no prompt anywhere
  // else and no per-account opt-out, so storing is the only way an account can
  // ever be used.
  for (const auto& account : accounts_) {
    const auto password = pending_passwords_.value(account.id);
    if (!password || password->IsEmpty()) continue;
    EMailCredentialStore::Save(account.id, *password);
  }

  wipe_pending_passwords();
  dirty_ = false;
  return true;
}

auto EMailAccountSettingsPage::selected_index() const -> int {
  const auto row = account_list_->currentRow();
  return row >= 0 && row < accounts_.size() ? row : -1;
}

auto EMailAccountSettingsPage::label_for(const MailAccountConfig& account)
    -> QString {
  if (!account.display_name.trimmed().isEmpty()) {
    return account.display_name.trimmed();
  }
  if (!account.address.trimmed().isEmpty()) return account.address.trimmed();
  return tr("Untitled account");
}

auto EMailAccountSettingsPage::row_text_for(
    const MailAccountConfig& account) const -> QString {
  // One definition of what a row says, so the per-keystroke update and the
  // full rebuild cannot disagree about the "(default)" marker.
  const auto label =
      account.Label().isEmpty() ? tr("Untitled account") : account.Label();
  return !default_id_.isEmpty() && account.id == default_id_
             ? tr("%1 (default)").arg(label)
             : label;
}

void EMailAccountSettingsPage::refresh_list() {
  // Rebuilding the list re-enters this class through currentRowChanged --
  // clear() alone reports "nothing is selected" -- and acting on that would
  // blank the editor mid-edit. The guard makes the rebuild invisible to the
  // selection handler.
  const auto restoring = loading_;
  loading_ = true;

  const auto row = account_list_->currentRow();
  account_list_->clear();
  for (const auto& account : accounts_)
    account_list_->addItem(row_text_for(account));
  if (row >= 0 && row < accounts_.size()) account_list_->setCurrentRow(row);

  // With nothing to list, the list and the form are pointless, and the empty
  // state takes the whole page.
  const bool any = !accounts_.isEmpty();
  left_panel_->setVisible(any);
  editor_scroll_->setVisible(any);
  empty_panel_->setVisible(!any);

  loading_ = restoring;
}

void EMailAccountSettingsPage::refresh_current_row() {
  const auto index = selected_index();
  if (index < 0) return;

  auto* item = account_list_->item(index);
  if (item == nullptr) return;

  // Only the one row, and only when it changed: rebuilding the whole list on
  // every keystroke is what stole focus from the field being typed into.
  const auto text = row_text_for(accounts_.at(index));
  if (item->text() != text) item->setText(text);
}

void EMailAccountSettingsPage::refresh_heading() {
  const auto index = selected_index();
  if (index < 0) {
    heading_->clear();
    heading_detail_->clear();
    return;
  }

  const auto& account = accounts_.at(index);
  heading_->setText(label_for(account));

  // Disabled first: it is the one thing about this account that changes what
  // the rest of the program does with it.
  const auto is_default = !default_id_.isEmpty() && account.id == default_id_;
  const auto address = account.address.trimmed();
  QString detail;
  if (!account.enabled) {
    detail =
        tr("Disabled: not offered for sending or fetching until it is "
           "enabled again.");
  } else if (is_default) {
    detail = tr("Default account");
  } else if (address != label_for(account)) {
    detail = address;
  }
  heading_detail_->setText(detail);
}

void EMailAccountSettingsPage::slot_add_account() {
  if (probe_->IsRunning()) return;
  store_selected();

  EMailAccountWizard wizard(EMailAccountStore::NewAccountId(), accounts_, this);
  if (wizard.exec() != QDialog::Accepted) return;

  const auto account = wizard.Account();
  accounts_.append(account);
  dirty_ = true;

  if (const auto password = wizard.Password();
      password && !password->IsEmpty()) {
    pending_passwords_[account.id] = password;
  }
  if (default_id_.isEmpty() || wizard.MakeDefault()) default_id_ = account.id;

  refresh_list();
  account_list_->setCurrentRow(static_cast<int>(accounts_.size()) - 1);
  load_selected();
  refresh_enabled_state();
}

void EMailAccountSettingsPage::slot_remove_account() {
  const auto index = selected_index();
  if (index < 0) return;

  const auto& account = accounts_.at(index);

  // Asked, because there is no undo and the account carries a password that
  // cannot be recovered once it is gone.
  if (QMessageBox::question(
          this, tr("Remove Account"),
          tr("Remove %1?\n\nIts stored password is forgotten as well. This "
             "cannot be undone once you press OK in this dialog.")
              .arg(account.Label().isEmpty() ? label_for(account)
                                             : account.Label()),
          QMessageBox::Yes | QMessageBox::Cancel,
          QMessageBox::Cancel) != QMessageBox::Yes) {
    return;
  }

  const auto id = account.id;
  accounts_.removeAt(index);
  if (auto password = pending_passwords_.take(id)) password->Wipe();
  dirty_ = true;

  // The default goes to whichever account is now first, rather than to none:
  // an unset default is one more thing the user has to discover.
  if (id == default_id_) {
    default_id_ = accounts_.isEmpty() ? QString() : accounts_.first().id;
  }

  // Staged, not done. Forgetting the secret here would survive a Cancel.
  if (!pending_removals_.contains(id)) pending_removals_.append(id);

  refresh_list();
  if (!accounts_.isEmpty()) {
    account_list_->setCurrentRow(
        qMin(index, static_cast<int>(accounts_.size()) - 1));
  }

  load_selected();
  refresh_enabled_state();
}

void EMailAccountSettingsPage::show_account_menu(const QPoint& pos) {
  // The row under the pointer, not whichever was selected before: the menu is
  // about the account that was clicked.
  auto* item = account_list_->itemAt(pos);
  if (item == nullptr || probe_->IsRunning()) return;
  account_list_->setCurrentItem(item);

  const auto index = selected_index();
  if (index < 0) return;

  QMenu menu(account_list_);
  auto* make_default = menu.addAction(
      tr("Set as Default"), this, &EMailAccountSettingsPage::slot_set_default);
  make_default->setToolTip(
      tr("The account offered first when sending, and the one used when "
         "nothing else is chosen."));
  // Off for the account that already is the default: choosing it would be a
  // no-op, and a command that does nothing is a question about whether it
  // worked.
  make_default->setEnabled(accounts_.at(index).id != default_id_);

  make_default->setEnabled(make_default->isEnabled() &&
                           accounts_.at(index).enabled);

  menu.addAction(accounts_.at(index).enabled ? tr("Disable") : tr("Enable"),
                 this, &EMailAccountSettingsPage::slot_toggle_enabled);

  menu.addSeparator();
  menu.addAction(tr("Remove"), this,
                 &EMailAccountSettingsPage::slot_remove_account);

  menu.exec(account_list_->viewport()->mapToGlobal(pos));
}

void EMailAccountSettingsPage::slot_toggle_enabled() {
  const auto index = selected_index();
  if (index < 0) return;

  store_selected();
  auto& account = accounts_[index];
  account.enabled = !account.enabled;
  dirty_ = true;

  // A disabled account is never offered first, so it does not stay the
  // default: the choice moves to the first account still in use, the same as
  // on removal. Enabling it again does not take the choice back.
  if (!account.enabled && account.id == default_id_) {
    for (const auto& other : accounts_) {
      if (!other.enabled) continue;
      default_id_ = other.id;
      break;
    }
  }

  refresh_list();
  account_list_->setCurrentRow(index);
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

  const auto index = selected_index();
  const auto old_address = index < 0 ? QString() : accounts_.at(index).address;

  store_selected();
  dirty_ = true;

  // A user name that was the address follows it.
  if (index >= 0 && accounts_.at(index).address != old_address) {
    auto& account = accounts_[index];
    MailFollowAddress(account, old_address);
    const QSignalBlocker imap_blocker(imap_.user);
    const QSignalBlocker smtp_blocker(smtp_.user);
    imap_.user->setText(account.imap.username);
    smtp_.user->setText(account.smtp.username);
  }

  refresh_current_row();
  refresh_heading();
  refresh_port_hints();
  refresh_validation();
  refresh_enabled_state();
}

void EMailAccountSettingsPage::slot_set_default() {
  const auto index = selected_index();
  if (index < 0) return;

  default_id_ = accounts_.at(index).id;
  dirty_ = true;

  refresh_list();
  account_list_->setCurrentRow(index);
  refresh_heading();
  refresh_enabled_state();
}

void EMailAccountSettingsPage::apply_colors() {
  // The single place this page's colours are decided, so a theme change has
  // one thing to call.
  if (empty_notice_ != nullptr) EMailMakeMuted(empty_notice_);
  if (oauth_notice_ != nullptr) EMailMakeMuted(oauth_notice_);
  if (heading_detail_ != nullptr) EMailMakeMuted(heading_detail_);
  for (auto* hint : {imap_.port_hint, smtp_.port_hint}) {
    if (hint != nullptr) EMailMakeMuted(hint);
  }
  if (validation_ != nullptr) {
    gf::ui::SetLabelColor(validation_, gf::ui::WarningColor(validation_));
  }

  refresh_pin_state();
}

void EMailAccountSettingsPage::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);
  if (gf::ui::IsRestyle(event)) apply_colors();
}

void EMailAccountSettingsPage::refresh_validation() {
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
  QStringList lines;
  for (const auto problem : MailAccountProblems(account, accounts_)) {
    switch (problem) {
      case MailAccountProblem::kNO_ADDRESS:
        lines.append(tr("This account needs an e-mail address."));
        break;
      case MailAccountProblem::kBAD_ADDRESS:
        lines.append(tr("\"%1\" does not look like an e-mail address.")
                         .arg(account.address));
        break;
      case MailAccountProblem::kRECEIVE_WITHOUT_SERVER:
        lines.append(tr("Receiving is turned on but no server is set."));
        break;
      case MailAccountProblem::kSEND_WITHOUT_SERVER:
        lines.append(tr("Sending is turned on but no server is set."));
        break;
      case MailAccountProblem::kNOTHING_ENABLED:
        lines.append(
            tr("Turn on receiving, sending, or both. An account that does "
               "neither cannot be used."));
        break;
      case MailAccountProblem::kDUPLICATE_ADDRESS:
        lines.append(
            tr("Another account already uses this address. They cannot be "
               "told apart wherever accounts are offered."));
        break;
    }
  }

  validation_->setText(lines.join("\n"));
  validation_->setVisible(!lines.isEmpty());
}

void EMailAccountSettingsPage::refresh_security_choices() {
  // The host decides which choices exist: typing a remote address into an
  // account that was cleartext takes the option away, and the combo falls back
  // to the safest one rather than keeping a selection the worker would refuse.
  EMailRefreshSecurityCombo(imap_.security, imap_.host->text());
  EMailRefreshSecurityCombo(smtp_.security, smtp_.host->text());
}

void EMailAccountSettingsPage::refresh_port_hints() {
  for (const bool imap : {true, false}) {
    auto& c = controls(imap);
    c.port_hint->setText(
        tr("Port %1").arg(MailDefaultPort(imap, EMailSecurityOf(c.security))));
  }
}

void EMailAccountSettingsPage::load_selected() {
  loading_ = true;

  const auto index = selected_index();
  const auto account = index < 0 ? MailAccountConfig() : accounts_.at(index);

  display_name_edit_->setText(account.display_name);
  address_edit_->setText(account.address);
  reply_to_edit_->setText(account.reply_to);
  sent_folder_->setText(account.sent_folder_override);

  for (const bool imap : {true, false}) {
    auto& c = controls(imap);
    const auto& transport = imap ? account.imap : account.smtp;
    c.enabled->setChecked(transport.enabled);
    c.host->setText(transport.host);
    EMailFillSecurityCombo(c.security, transport.host);
    EMailSelectSecurity(c.security, transport.tls);
    c.user->setText(transport.username);

    // Cleared: an outcome left behind would describe the previous account.
    c.status->Clear();
  }

  // Deliberately NOT refilled from the staged secret. Rendering it back into
  // the field would create another QString copy that cannot be erased, to show
  // the user a row of dots they cannot read anyway. An empty field means
  // "unchanged"; what was staged is still applied on Save.
  password_edit_->clear();

  refresh_heading();
  refresh_port_hints();
  refresh_pin_state();
  refresh_validation();

  loading_ = false;
}

void EMailAccountSettingsPage::store_selected() {
  const auto index = selected_index();
  if (index < 0) return;

  auto& account = accounts_[index];
  account.display_name = display_name_edit_->text().trimmed();
  account.address = address_edit_->text().trimmed();
  account.reply_to = reply_to_edit_->text().trimmed();
  account.sent_folder_override = sent_folder_->text().trimmed();

  for (const bool imap : {true, false}) {
    auto& c = controls(imap);
    auto& transport = imap ? account.imap : account.smtp;
    transport.enabled = c.enabled->isChecked();
    transport.host = c.host->text().trimmed();
    transport.tls = EMailSecurityOf(c.security);
    transport.username = c.user->text().trimmed();
  }

  // Out of the widget and into an erasable secret at once.
  const auto typed = password_edit_->text();
  if (!typed.isEmpty()) {
    pending_passwords_[account.id] = EMailSecret::CopyFrom(typed);
  }
}

void EMailAccountSettingsPage::refresh_enabled_state() {
  const auto index = selected_index();
  const auto has_selection = index >= 0;
  const auto testing = probe_->IsRunning() || !test_queue_.isEmpty();

  account_list_->setEnabled(!testing);
  add_button_->setEnabled(!testing);
  editor_->setEnabled(has_selection && !testing);

  for (const bool imap : {true, false}) {
    auto& c = controls(imap);
    c.fields->setEnabled(c.enabled->isChecked());
    c.user->setEnabled(c.enabled->isChecked());
  }
  test_button_->setEnabled(imap_.enabled->isChecked() ||
                           smtp_.enabled->isChecked());
}

void EMailAccountSettingsPage::slot_test() {
  // Reads the form; does not commit it. The test is about what the user is
  // looking at rather than about what was last stored, and a test is not an
  // edit.
  const auto index = selected_index();
  if (index < 0 || probe_->IsRunning()) return;

  auto account = accounts_.at(index);
  account.address = address_edit_->text().trimmed();
  for (const bool imap : {true, false}) {
    auto& c = controls(imap);
    auto& transport = imap ? account.imap : account.smtp;
    transport.enabled = c.enabled->isChecked();
    transport.host = c.host->text().trimmed();
    transport.tls = EMailSecurityOf(c.security);
    transport.username = c.user->text().trimmed();
  }

  // Typed field first, then anything staged but not yet applied, then what is
  // already stored. Whichever it is ends up in an EMailSecret, so the copy
  // held for the test can actually be erased.
  auto password = EMailSecret::CopyFrom(password_edit_->text());
  if (password->IsEmpty()) {
    const auto staged = pending_passwords_.value(account.id);
    if (staged && !staged->IsEmpty()) password = staged;
  }
  if (password->IsEmpty()) password = EMailCredentialStore::Load(account.id);

  test_queue_.clear();
  for (const bool imap : {true, false}) {
    auto& c = controls(imap);
    const auto& transport = imap ? account.imap : account.smtp;
    if (!transport.enabled) {
      c.status->Clear();
      continue;
    }
    if (transport.host.isEmpty()) {
      c.status->Set(EMailStatusState::kUNKNOWN,
                    tr("Enter a server address first."));
      continue;
    }
    if (password->IsEmpty()) {
      c.status->Set(EMailStatusState::kUNKNOWN, tr("Enter a password first."));
      continue;
    }
    c.status->Set(EMailStatusState::kPENDING, tr("Testing..."));
    test_queue_.append(imap);
  }

  tested_ = account;
  tested_password_ = password;
  set_testing(true);
  run_next_test();
}

void EMailAccountSettingsPage::run_next_test() {
  while (!test_queue_.isEmpty()) {
    const auto imap = test_queue_.takeFirst();
    if (probe_->Test(tested_, tested_password_, imap)) return;
  }

  // Released rather than wiped: the probe may still share the same secret,
  // and the bytes are erased when the last holder drops them.
  tested_password_.reset();
  set_testing(false);
}

void EMailAccountSettingsPage::on_test_finished(
    bool imap, EMailAccountProbe::Outcome outcome, const MailError& error) {
  auto& c = controls(imap);
  c.status->SetOutcome(outcome, error);

  if (outcome == EMailAccountProbe::Outcome::kSTOPPED) {
    // Stop means stop: the other server is not tried either.
    for (const auto other : test_queue_) controls(other).status->Clear();
    test_queue_.clear();
  }

  if (outcome == EMailAccountProbe::Outcome::kFAILED && error.IsPinnable()) {
    const auto index = selected_index();
    const auto pin = EMailAccountProbe::AskToPin(this, error);
    if (index >= 0 && !pin.isEmpty()) {
      // Pinned on the transport the test was actually for. The two are
      // configured separately and may well be different machines.
      auto& account = accounts_[index];
      (imap ? account.imap : account.smtp).pinned_cert_sha256 = pin;
      (imap ? tested_.imap : tested_.smtp).pinned_cert_sha256 = pin;
      dirty_ = true;

      c.status->Set(
          EMailStatusState::kUNKNOWN,
          tr("Certificate trusted for this account. Test again to confirm."));
      refresh_pin_state();
    }
  }

  run_next_test();
}

void EMailAccountSettingsPage::set_testing(bool testing) {
  // Everything a running test reads must stay still while it runs, so the
  // account cannot be switched, edited or deleted underneath it.
  test_button_->setVisible(!testing);
  stop_button_->setVisible(testing);
  refresh_enabled_state();
}

void EMailAccountSettingsPage::refresh_pin_state() {
  const auto index = selected_index();

  for (const bool imap : {true, false}) {
    auto* label = controls(imap).pin;
    if (label == nullptr) continue;
    if (index < 0) {
      label->setVisible(false);
      continue;
    }

    const auto& account = accounts_.at(index);
    const auto& pin = (imap ? account.imap : account.smtp).pinned_cert_sha256;
    if (pin.isEmpty()) {
      label->setVisible(false);
      continue;
    }

    // The fingerprint is shown in full: it is the whole content of the
    // decision, and an abbreviated one cannot be compared against anything.
    label->setText((imap ? tr("Receiving server") : tr("Sending server")) +
                   ": " +
                   tr("Trusting one certificate: %1 &nbsp; <a "
                      "href=\"forget\">Stop trusting it</a>")
                       .arg(pin.toUpper()));
    EMailMakeMuted(label);
    label->setVisible(true);
  }
}

void EMailAccountSettingsPage::forget_pin(bool imap) {
  const auto index = selected_index();
  if (index < 0) return;

  auto& account = accounts_[index];
  (imap ? account.imap : account.smtp).pinned_cert_sha256.clear();
  dirty_ = true;
  refresh_pin_state();
}

void EMailAccountSettingsPage::wipe_pending_passwords() {
  // Really overwritten, and on EVERY path out: clear() alone frees the
  // plaintext without erasing it.
  for (auto& password : pending_passwords_) {
    if (password) password->Wipe();
  }
  pending_passwords_.clear();
}
