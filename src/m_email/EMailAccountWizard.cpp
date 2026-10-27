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

#include "EMailAccountWizard.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "EMailAccountForm.h"
#include "EMailOutgoing.h"
#include "EMailServerLookup.h"
#include "EMailViewStyle.h"

namespace {

auto Step(MailSetupStep step) -> int { return static_cast<int>(step); }

auto ProblemLabel(QWidget* parent) -> QLabel* {
  auto* label = new QLabel(parent);
  label->setWordWrap(true);
  EMailMakeSecondary(label);
  EMailSetLabelColor(label, EMailWarningColor(label));
  return label;
}

auto Form(QWidget* parent) -> QFormLayout* {
  auto* form = new QFormLayout(parent);
  form->setContentsMargins(0, 0, 0, 0);
  // Every field, labels included, so a long value wraps instead of clipping.
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  form->setRowWrapPolicy(QFormLayout::WrapLongRows);
  return form;
}

auto BoldLabel(const QString& text, QWidget* parent) -> QLabel* {
  auto* label = new QLabel(text, parent);
  label->setWordWrap(true);
  auto font = label->font();
  font.setBold(true);
  label->setFont(font);
  return label;
}

/// A small muted caption above a field.
auto Caption(const QString& text, QWidget* buddy, QWidget* parent) -> QLabel* {
  auto* label = new QLabel(text, parent);
  label->setBuddy(buddy);
  EMailMakeSecondary(label);
  return label;
}

/**
 * @brief The text of a checkbox, wrapping where the checkbox's own cannot.
 *
 * A QCheckBox draws its text on one line, so a long translation of it sets the
 * minimum width of the whole step. This label wraps instead, and clicking it
 * does what clicking the checkbox's own text would.
 */
class CheckText : public QLabel {
 public:
  CheckText(const QString& text, QCheckBox* box, QWidget* parent)
      : QLabel(text, parent), box_(box) {
    setWordWrap(true);
    setBuddy(box);
    box->setAccessibleName(text);
  }

 protected:
  void mouseReleaseEvent(QMouseEvent* event) override {
    if (box_->isEnabled() && rect().contains(event->position().toPoint())) {
      box_->toggle();
      box_->setFocus(Qt::MouseFocusReason);
    }
    QLabel::mouseReleaseEvent(event);
  }

 private:
  QCheckBox* box_;
};

/// A heading with its one action at the right, for a region of a step.
auto RegionHeading(const QString& title, QPushButton* action, QWidget* parent)
    -> QHBoxLayout* {
  auto* row = new QHBoxLayout;
  row->setContentsMargins(0, 0, 0, 0);
  row->addWidget(BoldLabel(title, parent), 1);
  row->addWidget(action);
  return row;
}

}  // namespace

EMailAccountWizard::EMailAccountWizard(QString id,
                                       QList<MailAccountConfig> existing,
                                       QWidget* parent)
    : EMailWizard("account_wizard", tr("Add Account"),
                  {tr("Account"), tr("Servers"), tr("Check"), tr("Review")},
                  parent),
      id_(std::move(id)),
      existing_(std::move(existing)),
      lookup_(new EMailServerLookup(this)),
      probe_(new EMailAccountProbe(this)) {
  setWindowTitle(tr("Add Mail Account"));

  build_account_page();
  build_servers_page();
  build_check_page();
  build_review_page();

  // Typing can start at once, and a screen reader starts at the first field.
  // Queued, because showing the wizard moves focus to its buttons.
  QTimer::singleShot(0, name_edit_, [this]() { name_edit_->setFocus(); });

  connect(lookup_, &EMailServerLookup::SignalFinished, this,
          &EMailAccountWizard::on_lookup_finished);
  connect(probe_, &EMailAccountProbe::SignalFinished, this,
          &EMailAccountWizard::on_check_finished);

  connect(this, &QWizard::currentIdChanged, this, [this](int id) {
    // A test left running behind the user keeps a socket and a password busy
    // for an answer nobody will see.
    if (id != Step(MailSetupStep::kCHECK) && probe_->IsRunning()) {
      abandon_checks();
    }

    if (id == Step(MailSetupStep::kSERVERS)) fill_servers();
    if (id == Step(MailSetupStep::kCHECK)) start_checks(false);
    if (id == Step(MailSetupStep::kREVIEW)) fill_review();
  });
}

EMailAccountWizard::~EMailAccountWizard() = default;

auto EMailAccountWizard::fields(bool imap) -> ServerFields& {
  return imap ? imap_fields_ : smtp_fields_;
}

auto EMailAccountWizard::result(bool imap) -> CheckResult& {
  return imap ? imap_result_ : smtp_result_;
}

auto EMailAccountWizard::check_row(bool imap) -> CheckRow& {
  return imap ? imap_row_ : smtp_row_;
}

void EMailAccountWizard::build_account_page() {
  account_page_ =
      AddStep(Step(MailSetupStep::kACCOUNT), tr("Your Account"),
              tr("GpgFrontend finds the servers from your address."));
  auto* body = account_page_->body;

  auto* form_holder = new QWidget(account_page_);
  account_form_ = Form(form_holder);

  name_edit_ = new QLineEdit(form_holder);
  name_edit_->setClearButtonEnabled(true);
  name_edit_->setPlaceholderText(tr("for example: Alice Liddell"));

  address_edit_ = new QLineEdit(form_holder);
  address_edit_->setClearButtonEnabled(true);
  address_edit_->setPlaceholderText(tr("for example: alice@example.org"));
  address_problem_ = ProblemLabel(form_holder);

  password_edit_ = new QLineEdit(form_holder);
  password_edit_->setEchoMode(QLineEdit::Password);
  password_note_ = ProblemLabel(form_holder);

  // The sentences about a field are rows of their own under it, hidden until
  // there is something to say. Nested in a cell with the field, a wrapped
  // sentence was given the height of one line and clipped.
  account_form_->addRow(tr("Your name"), name_edit_);
  account_form_->addRow(tr("Email address"), address_edit_);
  account_form_->addRow(QString(), address_problem_);
  account_form_->addRow(tr("Password"), password_edit_);
  account_form_->addRow(QString(), password_note_);
  account_form_->setRowVisible(address_problem_, false);
  account_form_->setRowVisible(password_note_, false);
  body->addWidget(form_holder);

  // Directly under the form, where the password is typed: it is about what
  // goes into that field.
  auto* hint = new QLabel(
      tr("Signing in through a provider's own web page (OAuth2) is not "
         "supported yet. If your provider requires it, create an "
         "app-specific password and use that here."),
      account_page_);
  hint->setWordWrap(true);
  EMailMakeSecondary(hint);
  body->addWidget(hint);

  // Said before it happens: the lookup tells the DNS server which domain is
  // being set up, and a known provider needs none.
  dns_notice_ = new QLabel(account_page_);
  dns_notice_->setWordWrap(true);
  EMailMakeSecondary(dns_notice_);
  dns_notice_->hide();
  body->addWidget(dns_notice_);

  lookup_status_ = new QLabel(account_page_);
  lookup_status_->setWordWrap(true);
  EMailMakeSecondary(lookup_status_);
  lookup_status_->hide();
  body->addWidget(lookup_status_);
  body->addStretch(1);

  connect(address_edit_, &QLineEdit::textChanged, this, [this]() {
    refresh_address_problem(false);
    refresh_provider_note();
    refresh_dns_notice();
    account_page_->Recheck();
  });
  connect(address_edit_, &QLineEdit::editingFinished, this,
          [this]() { refresh_address_problem(true); });
  connect(password_edit_, &QLineEdit::textChanged, this, [this]() {
    // A check made with another password says nothing about this one.
    abandon_checks();
    account_page_->Recheck();
  });

  account_page_->complete = [this]() {
    return !lookup_->IsRunning() &&
           MailSetupCanAdvance(MailSetupStep::kACCOUNT, state());
  };

  // Continue looks the servers up first, then moves on by itself once they
  // are known. Asked only here, and only once per address: never while the
  // address is being typed.
  account_page_->validate = [this]() {
    const auto address = address_edit_->text().trimmed();
    if (looked_up_for_.compare(address, Qt::CaseInsensitive) == 0) return true;
    if (lookup_->IsRunning()) return false;

    lookup_status_->setText(
        tr("Looking up the servers for %1...").arg(MailDomainOf(address)));
    lookup_status_->show();
    for (auto* edit : {name_edit_, address_edit_, password_edit_}) {
      edit->setEnabled(false);
    }
    lookup_->Start(address);
    account_page_->Recheck();
    return false;
  };
}

auto EMailAccountWizard::build_server_fields(bool imap, QWidget* parent)
    -> ServerFields {
  ServerFields f;
  f.section = new QWidget(parent);
  auto* layout = new QVBoxLayout(f.section);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);
  layout->addWidget(
      BoldLabel(imap ? tr("Incoming mail (IMAP)") : tr("Outgoing mail (SMTP)"),
                f.section));

  f.host = new QLineEdit(f.section);
  f.host->setClearButtonEnabled(true);
  f.security = new QComboBox(f.section);
  EMailFillSecurityCombo(f.security, {});

  // Read-only, as in the editor: the port follows from the security choice,
  // and a port must never be able to imply a security level.
  f.port_hint = new QLabel(f.section);
  EMailMakeSecondary(f.port_hint);

  // Labels above the fields, the address taking most of the width: host
  // names are long, the security choice is one of three short words.
  auto* grid = new QGridLayout;
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(10);
  grid->setVerticalSpacing(3);
  grid->addWidget(Caption(tr("Server address"), f.host, f.section), 0, 0);
  grid->addWidget(Caption(tr("Security"), f.security, f.section), 0, 1);
  grid->addWidget(f.host, 1, 0);
  grid->addWidget(f.security, 1, 1);
  grid->addWidget(f.port_hint, 2, 1);
  // The address takes all the spare width; the security choice keeps the
  // width of its longest entry, so none of them is ever cut off.
  f.security->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  grid->setColumnStretch(0, 1);
  layout->addLayout(grid);

  f.user = new QLineEdit(parent);
  if (!imap) f.user->setPlaceholderText(tr("Empty: send without signing in"));
  return f;
}

void EMailAccountWizard::build_servers_page() {
  servers_page_ = AddStep(Step(MailSetupStep::kSERVERS), tr("Servers"),
                          tr("Check the servers this account will use."));
  auto* body = servers_page_->body;

  // What the account is for, as one row: it changes which servers are
  // needed, and everything under it follows.
  auto* usage_row = new EMailWrapRow(servers_page_);
  usage_row->setAccessibleName(tr("Use this account to"));
  usage_ = new QButtonGroup(this);
  const auto add_choice = [this, usage_row](MailAccountUsage usage,
                                            const QString& text) {
    auto* radio = new QRadioButton(text, usage_row);
    usage_->addButton(radio, static_cast<int>(usage));
    usage_row->AddWidget(radio);
  };
  add_choice(MailAccountUsage::kBOTH, tr("Send and receive"));
  add_choice(MailAccountUsage::kRECEIVE_ONLY, tr("Receive only"));
  add_choice(MailAccountUsage::kSEND_ONLY, tr("Send only"));
  usage_->button(static_cast<int>(MailAccountUsage::kBOTH))->setChecked(true);
  body->addWidget(usage_row);

  // The summary and the fields that change it are one region: Edit puts the
  // fields where the summary was, Done puts the summary back.
  servers_region_ = new QStackedWidget(servers_page_);

  summary_view_ = new QWidget(servers_region_);
  auto* summary_layout = new QVBoxLayout(summary_view_);
  summary_layout->setContentsMargins(0, 0, 0, 0);
  summary_layout->setSpacing(10);
  auto* edit_button = new QPushButton(tr("Edit"), summary_view_);
  edit_button->setAccessibleName(tr("Edit servers"));
  summary_layout->addLayout(
      RegionHeading(tr("Server settings"), edit_button, summary_view_));
  summary_rows_ = new EMailSummaryRows(summary_view_);
  summary_layout->addWidget(summary_rows_);
  summary_layout->addStretch(1);

  edit_view_ = new QWidget(servers_region_);
  auto* edit_layout = new QVBoxLayout(edit_view_);
  edit_layout->setContentsMargins(0, 0, 0, 0);
  edit_layout->setSpacing(16);
  auto* done_button = new QPushButton(tr("Done"), edit_view_);
  done_button->setAccessibleName(tr("Done editing servers"));
  edit_layout->addLayout(
      RegionHeading(tr("Server settings"), done_button, edit_view_));

  imap_fields_ = build_server_fields(true, edit_view_);
  smtp_fields_ = build_server_fields(false, edit_view_);
  edit_layout->addWidget(imap_fields_.section);
  edit_layout->addWidget(smtp_fields_.section);

  // Rarely different from the address, so out of the way until wanted.
  auto* usernames = new QWidget(edit_view_);
  usernames_form_ = Form(usernames);
  usernames_form_->addRow(tr("Incoming (IMAP)"), imap_fields_.user);
  usernames_form_->addRow(tr("Outgoing (SMTP)"), smtp_fields_.user);
  edit_layout->addWidget(
      EMailDisclosure(tr("Login usernames"), usernames, edit_view_));
  edit_layout->addStretch(1);

  servers_region_->addWidget(summary_view_);
  servers_region_->addWidget(edit_view_);
  body->addWidget(servers_region_);
  body->addStretch(1);
  show_server_fields(false);

  connect(edit_button, &QPushButton::clicked, this, [this]() {
    show_server_fields(true);
    imap_fields_.section->isVisible() ? imap_fields_.host->setFocus()
                                      : smtp_fields_.host->setFocus();
  });
  connect(done_button, &QPushButton::clicked, this,
          [this]() { show_server_fields(false); });

  const auto edited = [this]() {
    // An answer about the old servers says nothing about the new ones.
    abandon_checks();
    for (const bool imap : {true, false}) {
      auto& f = fields(imap);
      EMailRefreshSecurityCombo(f.security, f.host->text());
    }
    refresh_servers();
    servers_page_->Recheck();
  };
  for (const bool imap : {true, false}) {
    auto& f = fields(imap);
    connect(f.host, &QLineEdit::textEdited, this, edited);
    connect(f.security, &QComboBox::currentIndexChanged, this, edited);
    connect(f.user, &QLineEdit::textEdited, this, [this, imap, edited]() {
      // Typed by the user, so theirs from now on -- unless it is simply the
      // address again, which is what it would have followed anyway.
      auto& f = fields(imap);
      f.user_overridden =
          f.user->text().trimmed().compare(address_edit_->text().trimmed(),
                                           Qt::CaseInsensitive) != 0;
      edited();
    });
  }
  connect(usage_, &QButtonGroup::idToggled, this, [edited](int, bool on) {
    if (on) edited();
  });

  servers_page_->complete = [this]() {
    return MailSetupCanAdvance(MailSetupStep::kSERVERS, state());
  };
}

void EMailAccountWizard::show_server_fields(bool edit) {
  servers_region_->setCurrentWidget(edit ? edit_view_ : summary_view_);

  // A stack is as tall as its tallest page unless the hidden one stops
  // asking for room; otherwise the summary would sit above a gap the size of
  // the fields.
  summary_view_->setSizePolicy(
      QSizePolicy::Preferred,
      edit ? QSizePolicy::Ignored : QSizePolicy::Preferred);
  edit_view_->setSizePolicy(QSizePolicy::Preferred, edit
                                                        ? QSizePolicy::Preferred
                                                        : QSizePolicy::Ignored);
  servers_region_->adjustSize();
  if (!edit) refresh_servers();
}

auto EMailAccountWizard::build_check_row(bool imap, QWidget* parent)
    -> CheckRow {
  CheckRow r;
  r.row = new QWidget(parent);
  auto* grid = new QGridLayout(r.row);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(16);
  grid->setVerticalSpacing(2);

  // Left: which server. Right: what became of it.
  grid->addWidget(
      BoldLabel(imap ? tr("Incoming mail (IMAP)") : tr("Outgoing mail (SMTP)"),
                r.row),
      0, 0);
  r.details = new QLabel(r.row);
  r.details->setWordWrap(true);
  EMailMakeSecondary(r.details);
  grid->addWidget(r.details, 1, 0);

  auto* status_box = new QHBoxLayout;
  status_box->setSpacing(6);
  r.icon = new EMailStatusIcon(r.row);
  r.status = new QLabel(r.row);
  status_box->addWidget(r.icon, 0, Qt::AlignVCenter);
  status_box->addWidget(r.status, 0, Qt::AlignVCenter);
  grid->addLayout(status_box, 0, 1, Qt::AlignRight | Qt::AlignTop);

  r.explanation = new QLabel(r.row);
  r.explanation->setWordWrap(true);
  r.explanation->setTextInteractionFlags(Qt::TextSelectableByMouse);
  r.explanation->hide();
  grid->addWidget(r.explanation, 2, 0, 1, 2);
  grid->setColumnStretch(0, 1);
  return r;
}

void EMailAccountWizard::build_check_page() {
  check_page_ =
      AddStep(Step(MailSetupStep::kCHECK), tr("Connection Check"),
              tr("GpgFrontend signs in to each server without sending or "
                 "changing anything."));
  auto* body = check_page_->body;

  auto* results = new QVBoxLayout;
  results->setSpacing(12);
  imap_row_ = build_check_row(true, check_page_);
  smtp_row_ = build_check_row(false, check_page_);
  results->addWidget(imap_row_.row);
  results->addWidget(smtp_row_.row);
  body->addLayout(results);

  // The check as a whole, then what can be done about it, directly under the
  // rows it is about.
  overall_ = new QLabel(check_page_);
  overall_->setWordWrap(true);
  body->addWidget(overall_);

  retry_button_ = new QPushButton(tr("Retry"), check_page_);
  stop_button_ = new QPushButton(tr("Stop"), check_page_);
  stop_button_->hide();
  auto* buttons = new QHBoxLayout;
  buttons->setSpacing(8);
  buttons->addWidget(retry_button_);
  buttons->addWidget(stop_button_);
  buttons->addStretch(1);

  // The explicit way past a failure, and only ever shown after one.
  auto* accept_row = new QWidget(check_page_);
  auto* accept_layout = new QHBoxLayout(accept_row);
  accept_layout->setContentsMargins(0, 0, 0, 0);
  accept_layout->setSpacing(6);
  accept_unverified_ = new QCheckBox(accept_row);
  accept_layout->addWidget(accept_unverified_, 0, Qt::AlignTop);
  accept_layout->addWidget(
      new CheckText(tr("Add this account anyway and fix the connection later"),
                    accept_unverified_, accept_row),
      1);
  accept_row->hide();

  auto* actions = new QVBoxLayout;
  actions->setSpacing(10);
  actions->addLayout(buttons);
  actions->addWidget(accept_row);
  body->addLayout(actions);
  body->addStretch(1);

  connect(retry_button_, &QPushButton::clicked, this,
          [this]() { start_checks(true); });
  connect(stop_button_, &QPushButton::clicked, probe_,
          &EMailAccountProbe::Stop);
  connect(accept_unverified_, &QCheckBox::toggled, check_page_,
          &EMailWizardPage::Recheck);

  check_page_->complete = [this]() {
    return MailSetupCanAdvance(MailSetupStep::kCHECK, state());
  };
}

void EMailAccountWizard::build_review_page() {
  review_page_ = AddStep(
      Step(MailSetupStep::kREVIEW), tr("Review"),
      tr("Nothing is saved until you press OK in the Settings window."));
  auto* body = review_page_->body;

  review_rows_ = new EMailSummaryRows(review_page_);
  body->addWidget(review_rows_);

  default_switch_ =
      new EMailToggleSwitch(tr("Use as the default account"), review_page_);
  default_switch_->setToolTip(
      tr("The account offered first when sending, and the one used when "
         "nothing else is chosen."));
  default_switch_->setChecked(existing_.isEmpty());

  auto* switch_row = new QHBoxLayout;
  switch_row->addWidget(default_switch_);
  switch_row->addStretch(1);
  body->addLayout(switch_row);

  // The first account is the default whatever a switch would say, so it is
  // told rather than asked.
  auto* first_note = new QLabel(
      tr("As your only account, this becomes the default one."), review_page_);
  first_note->setWordWrap(true);
  EMailMakeSecondary(first_note);
  body->addWidget(first_note);
  default_switch_->setVisible(!existing_.isEmpty());
  first_note->setVisible(existing_.isEmpty());
  body->addStretch(1);

  review_page_->complete = [this]() {
    return MailSetupCanAdvance(MailSetupStep::kREVIEW, state());
  };
}

auto EMailAccountWizard::usage() const -> MailAccountUsage {
  const auto id = usage_->checkedId();
  return id < 0 ? MailAccountUsage::kBOTH : static_cast<MailAccountUsage>(id);
}

auto EMailAccountWizard::build_account() const -> MailAccountConfig {
  MailServerSuggestion servers;
  servers.imap = {imap_fields_.host->text().trimmed(),
                  EMailSecurityOf(imap_fields_.security),
                  MailServerSource::kNONE};
  servers.smtp = {smtp_fields_.host->text().trimmed(),
                  EMailSecurityOf(smtp_fields_.security),
                  MailServerSource::kNONE};

  auto account = MailNewAccount(id_, name_edit_->text(), address_edit_->text(),
                                servers, usage());

  // The address, unless the user typed another user name for that server.
  if (imap_fields_.user_overridden) {
    account.imap.username = imap_fields_.user->text().trimmed();
  }
  if (smtp_fields_.user_overridden) {
    account.smtp.username = smtp_fields_.user->text().trimmed();
  }

  // A trust decision belongs to the server it was made about; one made
  // before the server was changed is dropped rather than carried over.
  if (!imap_pin_.isEmpty() && imap_pin_host_ == account.imap.host) {
    account.imap.pinned_cert_sha256 = imap_pin_;
  }
  if (!smtp_pin_.isEmpty() && smtp_pin_host_ == account.smtp.host) {
    account.smtp.pinned_cert_sha256 = smtp_pin_;
  }
  return account;
}

auto EMailAccountWizard::state() const -> MailSetupState {
  MailSetupState s;
  s.account = build_account();
  s.others = existing_;
  s.has_password = !password_edit_->text().isEmpty();
  s.imap_check = imap_result_.state;
  s.smtp_check = smtp_result_.state;
  s.accept_unverified =
      accept_unverified_ != nullptr && accept_unverified_->isChecked();
  return s;
}

void EMailAccountWizard::refresh_address_problem(bool strict) {
  const auto problems = MailAccountProblems(build_account(), existing_);
  const auto bad_text = tr("This does not look like an email address.");

  // A duplicate is said at once. An address that does not parse yet is
  // mostly an address still being typed, so that is only said once the user
  // moves on -- and then kept until it is fixed.
  const auto showing_bad = address_problem_->text() == bad_text;

  QString text;
  if (problems.contains(MailAccountProblem::kDUPLICATE_ADDRESS)) {
    text = tr("Another account already uses this address.");
  } else if (problems.contains(MailAccountProblem::kBAD_ADDRESS) &&
             (strict || showing_bad)) {
    text = bad_text;
  }

  address_problem_->setText(text);
  account_form_->setRowVisible(address_problem_, !text.isEmpty());
}

void EMailAccountWizard::refresh_dns_notice() {
  const auto address = address_edit_->text();
  const auto needed =
      MailNeedsDnsLookup(address) && MailIsPlausibleAddress(address.trimmed());
  if (needed) {
    dns_notice_->setText(
        tr("%1 is not in GpgFrontend's list of providers. To find its mail "
           "servers, pressing Continue asks your DNS server about %1.")
            .arg(MailDomainOf(address)));
  }
  dns_notice_->setVisible(needed);
}

void EMailAccountWizard::refresh_provider_note() {
  const auto provider = MailProviderInfoFor(address_edit_->text());

  QString text;
  if (provider.has_value()) {
    switch (provider->note) {
      case MailProviderNote::kAPP_PASSWORD:
        text = tr("%1 usually refuses your normal password here. Create an "
                  "app password in your %1 account settings and use that.")
                   .arg(provider->name);
        break;
      case MailProviderNote::kWEB_SIGN_IN:
        text = tr("%1 asks mail programs to sign in through its own web "
                  "page, which GpgFrontend cannot do yet. Signing in works "
                  "only if your account allows an app password.")
                   .arg(provider->name);
        break;
      case MailProviderNote::kNONE:
        break;
    }
  }
  password_note_->setText(text);
  account_form_->setRowVisible(password_note_, !text.isEmpty());
  EMailMakeMuted(password_note_);
}

void EMailAccountWizard::on_lookup_finished(
    const MailServerSuggestion& suggestion) {
  suggestion_ = suggestion;
  looked_up_for_ = address_edit_->text().trimmed();

  lookup_status_->hide();
  for (auto* edit : {name_edit_, address_edit_, password_edit_}) {
    edit->setEnabled(true);
  }
  account_page_->Recheck();

  if (currentPage() == account_page_) next();
}

void EMailAccountWizard::fill_servers() {
  if (filled_for_.compare(looked_up_for_, Qt::CaseInsensitive) != 0) {
    filled_for_ = looked_up_for_;
    abandon_checks();

    for (const bool imap : {true, false}) {
      auto& f = fields(imap);
      const auto& choice = imap ? suggestion_.imap : suggestion_.smtp;
      f.host->setText(choice.host);
      EMailFillSecurityCombo(f.security, choice.host);
      EMailSelectSecurity(f.security, choice.tls);
    }
    // New servers start from the summary; the fields are for changing them.
    show_server_fields(false);
  }

  // A user name nobody typed follows the address, wherever it was changed.
  for (const bool imap : {true, false}) {
    auto& f = fields(imap);
    if (!f.user_overridden) f.user->setText(address_edit_->text().trimmed());
  }

  refresh_servers();
  servers_page_->Recheck();
}

auto EMailAccountWizard::source_text() const -> QString {
  const auto account = build_account();
  const auto untouched = account.imap.host == suggestion_.imap.host &&
                         account.smtp.host == suggestion_.smtp.host &&
                         account.imap.tls == suggestion_.imap.tls &&
                         account.smtp.tls == suggestion_.smtp.tls;
  if (!untouched) return tr("Entered by you");

  const auto domain = MailDomainOf(looked_up_for_);
  const auto imap = suggestion_.imap.source;
  const auto smtp = suggestion_.smtp.source;
  const auto both = [imap, smtp](MailServerSource s) {
    return imap == s && smtp == s;
  };
  const auto either = [imap, smtp](MailServerSource s) {
    return imap == s || smtp == s;
  };

  if (both(MailServerSource::kPRESET)) {
    return tr("Built-in settings for %1").arg(suggestion_.provider);
  }
  if (both(MailServerSource::kDNS)) {
    return tr("Published by %1 in DNS").arg(domain);
  }
  if (both(MailServerSource::kGUESS)) return tr("Guessed from %1").arg(domain);
  if (either(MailServerSource::kGUESS)) {
    return tr("Partly found in DNS for %1, partly guessed").arg(domain);
  }
  // Published, or a conventional name that exists, or one of each.
  return tr("Found in DNS for %1").arg(domain);
}

void EMailAccountWizard::refresh_servers() {
  const auto account = build_account();

  // Only the directions this account is for, in the summary and in the
  // fields alike.
  summary_rows_->Clear();
  if (account.imap.enabled) {
    summary_rows_->AddRow(
        tr("Incoming"),
        EMailServerSummary(account.imap.host, account.imap.tls, true));
  }
  if (account.smtp.enabled) {
    summary_rows_->AddRow(
        tr("Outgoing"),
        EMailServerSummary(account.smtp.host, account.smtp.tls, false));
  }
  summary_rows_->AddRow(tr("Source"), source_text());

  for (const bool imap : {true, false}) {
    auto& f = fields(imap);
    const auto enabled = imap ? account.imap.enabled : account.smtp.enabled;
    f.section->setVisible(enabled);
    usernames_form_->setRowVisible(f.user, enabled);
    f.port_hint->setText(
        tr("Port %1").arg(MailDefaultPort(imap, EMailSecurityOf(f.security))));
  }
}

void EMailAccountWizard::abandon_checks() {
  // The run is retired before it is stopped, so whatever it still reports is
  // recognised as belonging to no one.
  ++check_run_;
  checked_ = {};
  probe_->Stop();
  imap_result_ = {};
  smtp_result_ = {};
  if (accept_unverified_ != nullptr) accept_unverified_->setChecked(false);
}

void EMailAccountWizard::start_checks(bool force) {
  const auto account = build_account();
  const auto json = account.ToJson();

  // Stepping back onto the check with nothing changed keeps its answer.
  if (!force && json == checked_) {
    refresh_check_controls();
    return;
  }

  abandon_checks();
  checked_ = json;

  for (const bool imap : {true, false}) {
    const auto& transport = imap ? account.imap : account.smtp;
    auto& row = check_row(imap);
    row.row->setVisible(transport.enabled);
    row.details->setText(
        EMailServerSummary(transport.host, transport.tls, imap));
    show_check_row(imap);
  }

  run_next_check();
}

void EMailAccountWizard::run_next_check() {
  if (probe_->IsRunning()) {
    refresh_check_controls();
    return;
  }

  const auto account = build_account();
  for (const bool imap : {true, false}) {
    const auto& transport = imap ? account.imap : account.smtp;
    auto& r = result(imap);
    if (!transport.enabled || r.state != MailCheckState::kNOT_RUN) continue;

    if (probe_->Test(account, EMailSecret::CopyFrom(password_edit_->text()),
                     imap)) {
      r.state = MailCheckState::kRUNNING;
      probe_run_ = check_run_;
    }
    show_check_row(imap);
    break;
  }

  refresh_check_controls();
}

void EMailAccountWizard::show_check_row(bool imap) {
  auto& row = check_row(imap);
  const auto& r = result(imap);

  auto state = EMailStatusState::kUNKNOWN;
  QString status;
  QString explanation;
  QString tooltip;
  switch (r.state) {
    case MailCheckState::kNOT_RUN:
      state = EMailStatusState::kPENDING;
      status = tr("Waiting");
      break;
    case MailCheckState::kRUNNING:
      state = EMailStatusState::kPENDING;
      status = tr("Connecting...");
      break;
    case MailCheckState::kPASSED:
      state = EMailStatusState::kGOOD;
      status = tr("Connected");
      break;
    case MailCheckState::kFAILED:
      state = EMailStatusState::kBAD;
      status = tr("Failed");
      explanation = EMailAccountProbe::Describe(r.error);
      tooltip = QStringList{r.error.detail, r.error.protocol_detail}
                    .filter(QRegularExpression("\\S"))
                    .join("\n\n");
      break;
    case MailCheckState::kSTOPPED:
      state = EMailStatusState::kUNKNOWN;
      status = tr("Not verified");
      break;
  }

  row.icon->SetState(static_cast<int>(state), status);
  row.status->setText(status);
  switch (state) {
    case EMailStatusState::kGOOD:
      EMailSetLabelColor(row.status, EMailAccentColor(row.status, true));
      break;
    case EMailStatusState::kBAD:
      EMailSetLabelColor(row.status, EMailWarningColor(row.status));
      break;
    default:
      EMailMakeMuted(row.status);
      break;
  }

  row.explanation->setText(explanation);
  row.explanation->setToolTip(tooltip);
  row.explanation->setVisible(!explanation.isEmpty());
}

void EMailAccountWizard::refresh_overall() {
  const auto account = build_account();
  const auto running = imap_result_.state == MailCheckState::kRUNNING ||
                       smtp_result_.state == MailCheckState::kRUNNING;

  bool failed = false;
  bool stopped = false;
  for (const bool imap : {true, false}) {
    const auto enabled = imap ? account.imap.enabled : account.smtp.enabled;
    if (!enabled) continue;
    failed |= result(imap).state == MailCheckState::kFAILED;
    stopped |= result(imap).state == MailCheckState::kSTOPPED;
  }

  QString text;
  auto color = EMailMutedColor(overall_);
  if (running) {
    text = tr("Checking the servers...");
  } else if (MailSetupChecksPassed(state())) {
    text = tr("Everything works. Continue to review the account.");
    color = EMailAccentColor(overall_, true);
  } else if (failed) {
    text =
        tr("The account cannot connect yet. Correct the servers and "
           "retry, or add it anyway and fix it later in Settings.");
    color = EMailWarningColor(overall_);
  } else if (stopped) {
    text = tr("The check was stopped, so nothing was verified.");
  }
  overall_->setText(text);
  overall_->setVisible(!text.isEmpty());
  EMailSetLabelColor(overall_, color);
}

void EMailAccountWizard::on_check_finished(bool imap,
                                           EMailAccountProbe::Outcome outcome,
                                           const MailError& error) {
  // An answer from a run that was abandoned -- something changed, or the user
  // left -- is about an account no longer on screen. The run that replaced it
  // may be waiting for the probe to come free.
  if (probe_run_ != check_run_) {
    run_next_check();
    return;
  }

  auto& r = result(imap);
  switch (outcome) {
    case EMailAccountProbe::Outcome::kCONNECTED:
      r.state = MailCheckState::kPASSED;
      break;
    case EMailAccountProbe::Outcome::kSTOPPED:
      r.state = MailCheckState::kSTOPPED;
      break;
    case EMailAccountProbe::Outcome::kFAILED:
      r.state = MailCheckState::kFAILED;
      break;
  }
  r.error = error;
  show_check_row(imap);

  if (outcome == EMailAccountProbe::Outcome::kFAILED && error.IsPinnable()) {
    const auto pin = EMailAccountProbe::AskToPin(this, error);
    if (!pin.isEmpty()) {
      const auto host = fields(imap).host->text().trimmed();
      (imap ? imap_pin_host_ : smtp_pin_host_) = host;
      (imap ? imap_pin_ : smtp_pin_) = pin;
      checked_ = build_account().ToJson();

      // Asked again at once, now with the certificate trusted.
      r = {};
      show_check_row(imap);
    }
  }

  // Stop means stop: the other server is not tried either.
  if (outcome == EMailAccountProbe::Outcome::kSTOPPED) {
    auto& other = result(!imap);
    if (other.state == MailCheckState::kNOT_RUN) {
      other.state = MailCheckState::kSTOPPED;
      show_check_row(!imap);
    }
  }

  // Otherwise the other server is checked even when this one failed: knowing
  // that sending works while receiving does not is half the diagnosis.
  run_next_check();
}

void EMailAccountWizard::refresh_check_controls() {
  const auto running = imap_result_.state == MailCheckState::kRUNNING ||
                       smtp_result_.state == MailCheckState::kRUNNING;
  const auto s = state();
  const auto unverified = !running && !MailSetupChecksPassed(s) &&
                          (imap_result_.state != MailCheckState::kNOT_RUN ||
                           smtp_result_.state != MailCheckState::kNOT_RUN);

  stop_button_->setVisible(running);
  retry_button_->setVisible(!running && !MailSetupChecksPassed(s));
  accept_unverified_->parentWidget()->setVisible(unverified);
  if (!unverified) accept_unverified_->setChecked(false);
  refresh_overall();
  check_page_->Recheck();
}

void EMailAccountWizard::show_result(EMailProbeStatus* status,
                                     const CheckResult& result) const {
  switch (result.state) {
    case MailCheckState::kPASSED:
      status->Set(EMailStatusState::kGOOD, tr("Connected"));
      break;
    case MailCheckState::kFAILED: {
      // The title is a sentence of its own; it is only given the full stop
      // it lacks, never a second one.
      auto title = result.error.title.trimmed();
      if (!title.isEmpty() && !title.endsWith('.') && !title.endsWith('!') &&
          !title.endsWith('?')) {
        title += '.';
      }
      status->Set(EMailStatusState::kBAD,
                  title.isEmpty()
                      ? tr("Could not connect. Added without a working "
                           "connection.")
                      : tr("%1 Added without a working connection.").arg(title),
                  result.error.protocol_detail);
      break;
    }
    case MailCheckState::kSTOPPED:
      status->Set(EMailStatusState::kUNKNOWN,
                  tr("Not verified: the check was stopped."));
      break;
    case MailCheckState::kNOT_RUN:
    case MailCheckState::kRUNNING:
      status->Set(EMailStatusState::kUNKNOWN, tr("Not checked."));
      break;
  }
}

void EMailAccountWizard::fill_review() {
  const auto account = build_account();
  review_rows_->Clear();

  if (!account.display_name.isEmpty()) {
    review_rows_->AddRow(tr("Name"), account.display_name);
  }
  review_rows_->AddRow(tr("Address"), account.address);

  // Whatever the check found -- including that it was skipped -- is still in
  // front of the user when they add the account.
  if (account.imap.enabled) {
    show_result(review_rows_->AddRow(
                    tr("Incoming"), EMailServerSummary(account.imap.host,
                                                       account.imap.tls, true)),
                imap_result_);
  }
  if (account.smtp.enabled) {
    show_result(
        review_rows_->AddRow(
            tr("Outgoing"),
            EMailServerSummary(account.smtp.host, account.smtp.tls, false)),
        smtp_result_);
  }
}

void EMailAccountWizard::done(int result) {
  probe_->Stop();
  lookup_->Cancel();

  // Into an erasable secret before the field is cleared: the only copy this
  // wizard hands on.
  if (result == QDialog::Accepted) {
    password_ = EMailSecret::CopyFrom(password_edit_->text());
  }
  password_edit_->clear();

  EMailWizard::done(result);
}

auto EMailAccountWizard::Account() const -> MailAccountConfig {
  return build_account();
}

auto EMailAccountWizard::Password() const -> EMailSecretPtr {
  return password_;
}

auto EMailAccountWizard::MakeDefault() const -> bool {
  return default_switch_->isChecked();
}
