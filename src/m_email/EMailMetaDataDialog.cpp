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

#include "EMailMetaDataDialog.h"

#include <GFSDKGpg.h>

#include <QCryptographicHash>
#include <QDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <algorithm>

#include "EMailHelper.h"
#include "ui_EMailMetaDataDialog.h"

static const QRegularExpression kNameEmailStringValidateRegex(
    R"(^\s*(.*\s*)?<\s*([a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\.[a-zA-Z0-9-.]+)\s*>\s*$|(^[a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\.[a-zA-Z0-9-.]+$))");

class ParameterizedHeaderField : public vmime::parameterizedHeaderField {
 public:
  ParameterizedHeaderField() = default;
};

class HeaderField : public vmime::headerField {
 public:
  HeaderField() = default;
};

class ContentTypeField : public vmime::contentTypeField {
 public:
  ContentTypeField() = default;
};

EMailMetaDataDialog::EMailMetaDataDialog(QWidget* parent)
    : QDialog(parent), ui_(QSharedPointer<Ui_EMailMetaDataDialog>::create()) {
  ui_->setupUi(this);

  ui_->ccEdit->setHidden(true);
  ui_->ccLabel->setHidden(true);
  ui_->bccEdit->setHidden(true);
  ui_->bccLabel->setHidden(true);

  connect(ui_->okButton, &QPushButton::clicked, this,
          &EMailMetaDataDialog::slot_parse_eml_meta_data);

  connect(ui_->cancelButton, &QPushButton::clicked, this, [=]() {
    emit SignalNoEMLMetaData("User canceled");
    close();
  });

  connect(ui_->ccButton, &QPushButton::clicked, this, [this]() {
    ui_->ccEdit->setHidden(!ui_->ccEdit->isHidden());
    ui_->ccLabel->setHidden(!ui_->ccLabel->isHidden());
    ui_->ccEdit->clear();
  });

  connect(ui_->bccButton, &QPushButton::clicked, this, [this]() {
    ui_->bccEdit->setHidden(!ui_->bccEdit->isHidden());
    ui_->bccLabel->setHidden(!ui_->bccLabel->isHidden());
    ui_->bccEdit->clear();
  });

  connect(this, &EMailMetaDataDialog::SignalEMLMetaData, this, &QDialog::close);
  connect(this, &EMailMetaDataDialog::SignalNoEMLMetaData, this,
          &QDialog::close);

  setModal(true);
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
}

Q_VARIANT_Q_OBJECT_FACTORY_DEFINE(CreateEMailMetaDataDialog,
                                  [](const QVariant& /*v*/) -> void* {
                                    return new EMailMetaDataDialog(nullptr);
                                  });

void EMailMetaDataDialog::slot_parse_eml_meta_data() {
  // validate
  slot_validate_inputs_and_show_errors();

  if (!ui_->errorLabel->text().trimmed().isEmpty()) return;

  auto from = ui_->fromEdit->text();
  auto raw_to = ui_->toEdit->text();
  auto raw_cc = ui_->ccEdit->text();
  auto raw_bcc = ui_->bccEdit->text();
  auto subject = ui_->subjectEdit->text();

  auto to = raw_to.split(';', Qt::SkipEmptyParts);
  auto cc = raw_cc.trimmed().isEmpty() ? QStringList()
                                       : raw_cc.split(';', Qt::SkipEmptyParts);
  auto bcc = raw_bcc.trimmed().isEmpty()
                 ? QStringList()
                 : raw_bcc.split(';', Qt::SkipEmptyParts);
  QString name;
  QString email;

  EMailMetaData meta_data;
  meta_data.from = from;
  meta_data.to = to;
  meta_data.cc = cc;
  meta_data.bcc = bcc;
  meta_data.subject = subject;

  emit SignalEMLMetaData(meta_data);
}

void EMailMetaDataDialog::slot_export_encrypted_data() {}

void EMailMetaDataDialog::SetFromKeys(QStringList k) {
  from_keys_ = std::move(k);
  slot_set_from_field_by_sign_key();
}

void EMailMetaDataDialog::SetToKeys(QStringList k) {
  to_keys_ = std::move(k);
  slot_set_to_field_by_encrypt_keys();
}

void EMailMetaDataDialog::SetChannel(int c) { channel_ = c; }

void EMailMetaDataDialog::slot_set_from_field_by_sign_key() {
  // only allow one sign key
  const auto sign_key = from_keys_.front();

  GFGpgKeyUID* s;
  auto ret = GFGpgKeyPrimaryUID(channel_, QDUP(sign_key), &s);
  if (ret != 0) {
    FLOG_WARN("cannot get primary uid from sign key %1, ret: %2", sign_key,
              ret);
    return;
  }

  from_name_ = UDUP(s->name);
  from_email_ = UDUP(s->email);
  auto comment = UDUP(s->comment);

  GFFreeMemory(s);

  ui_->fromEdit->setText(QString("%1 <%2>").arg(from_name_).arg(from_email_));
}

void EMailMetaDataDialog::slot_set_to_field_by_encrypt_keys() {
  QStringList to_list;

  for (const auto& key : to_keys_) {
    GFGpgKeyUID* s;
    auto ret = GFGpgKeyPrimaryUID(channel_, QDUP(key), &s);
    if (ret != 0) {
      FLOG_WARN("cannot get primary uid from sign key %1, ret: %2", key, ret);
      continue;
    }

    from_name_ = UDUP(s->name);
    from_email_ = UDUP(s->email);
    auto comment = UDUP(s->comment);

    GFFreeMemory(s);

    to_list.append(QString("%1 <%2>").arg(from_name_).arg(from_email_));
  }

  ui_->toEdit->setText(to_list.join("; "));
}

void EMailMetaDataDialog::slot_validate_inputs_and_show_errors() {
  QString error_message;

  if (ui_->fromEdit->text().trimmed().isEmpty()) {
    error_message = tr("The 'From' field cannot be empty.");
  } else if (!is_valid_email(ui_->fromEdit->text().trimmed())) {
    error_message = tr("The 'From' field must contain a valid email address.");
  }

  if (error_message.isEmpty() && ui_->toEdit->text().trimmed().isEmpty()) {
    error_message = tr("The 'To' field cannot be empty.");
  } else if (error_message.isEmpty() &&
             !are_valid_emails(ui_->toEdit->text())) {
    error_message =
        tr("One or more 'To' addresses are invalid. Please separate multiple "
           "addresses with \";\".");
  }

  if (error_message.isEmpty() && !ui_->ccEdit->text().trimmed().isEmpty()) {
    if (!are_valid_emails(ui_->ccEdit->text())) {
      error_message =
          tr("One or more 'CC' addresses are invalid. Please separate "
             "multiple addresses with \";\".");
    }
  }

  if (error_message.isEmpty() && !ui_->bccEdit->text().trimmed().isEmpty()) {
    if (!are_valid_emails(ui_->bccEdit->text())) {
      error_message =
          tr("One or more 'BCC' addresses are invalid. Please separate "
             "multiple addresses with \";\".");
    }
  }

  if (error_message.isEmpty() && ui_->subjectEdit->text().trimmed().isEmpty()) {
    error_message = tr("The 'Subject' field cannot be empty.");
  }

  if (!error_message.isEmpty()) {
    ui_->errorLabel->setText(error_message);
    ui_->errorLabel->setStyleSheet("QLabel { color : red; }");
  } else {
    ui_->errorLabel->clear();
  }
}

auto EMailMetaDataDialog::are_valid_emails(const QString& emails) -> bool {
  QStringList email_list = emails.split(';', Qt::SkipEmptyParts);
  return std::all_of(
      email_list.begin(), email_list.end(),
      [=](const QString& email) { return is_valid_email(email.trimmed()); });
}

auto EMailMetaDataDialog::is_valid_email(const QString& email) -> bool {
  return kNameEmailStringValidateRegex.match(email).hasMatch();
}

void EMailMetaDataDialog::SetBodyData(QByteArray b) {
  body_data_ = std::move(b);
}