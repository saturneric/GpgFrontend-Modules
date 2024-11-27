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

#include "EMailHelper.h"
#include "ui_EMailMetaDataDialog.h"

static const QRegularExpression kNameEmailStringRegex{
    R"(^\s*(.*)\s*<\s*([^<>@\s]+@[^<>@\s]+)\s*>\s*$)"};

static const QRegularExpression kNameEmailStringValidateRegex(
    R"(^\s*(.*\s*)?<\s*([a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\.[a-zA-Z0-9-.]+)\s*>\s*$|(^[a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\.[a-zA-Z0-9-.]+$))");

auto ParseEmailString(const QString& input, QString& name,
                      QString& email) -> bool {
  QRegularExpressionMatch match = kNameEmailStringRegex.match(input);

  if (match.hasMatch()) {
    name = match.captured(1).trimmed();
    email = match.captured(2).trimmed();
    return true;
  }

  return false;
}

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

EMailMetaDataDialog::EMailMetaDataDialog(int mode, QWidget* parent)
    : QDialog(parent),
      ui_(QSharedPointer<Ui_EMailMetaDataDialog>::create()),
      mode_(mode) {
  ui_->setupUi(this);

  ui_->ccEdit->setHidden(true);
  ui_->ccLabel->setHidden(true);
  ui_->bccEdit->setHidden(true);
  ui_->bccLabel->setHidden(true);

  if (mode == 0) {
    connect(ui_->okButton, &QPushButton::clicked, this,
            &EMailMetaDataDialog::slot_sign_eml_data);
  } else {
    connect(ui_->okButton, &QPushButton::clicked, this,
            &EMailMetaDataDialog::slot_encrypt_eml_data);
  }

  connect(ui_->cancelButton, &QPushButton::clicked, this, &QDialog::close);

  connect(ui_->ccButton, &QPushButton::clicked, this, [this]() {
    ui_->ccEdit->setHidden(!ui_->ccEdit->isHidden());
    ui_->ccLabel->setHidden(!ui_->ccLabel->isHidden());
  });

  connect(ui_->bccButton, &QPushButton::clicked, this, [this]() {
    ui_->bccEdit->setHidden(!ui_->bccEdit->isHidden());
    ui_->bccLabel->setHidden(!ui_->bccLabel->isHidden());
  });

  connect(this, &EMailMetaDataDialog::SignalEMLDataGenerateSuccess, this,
          &QDialog::close);

  connect(this, &EMailMetaDataDialog::SignalEMLDataGenerateFailed, this,
          &QDialog::close);

  setModal(true);
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowFlags(Qt::Dialog | Qt::Window);
}

Q_VARIANT_Q_OBJECT_FACTORY_DEFINE(CreateEMailMetaDataDialog,
                                  [](QVariant data) -> void* {
                                    return new EMailMetaDataDialog(data.toInt(),
                                                                   nullptr);
                                  });

void EMailMetaDataDialog::slot_sign_eml_data() {
  // sign key is a must
  if (keys_.isEmpty()) emit SignalEMLDataGenerateFailed("No Sign Key");

  // only allow one sign key
  const auto sign_key = keys_.front();

  // validate
  slot_validate_inputs_and_show_errors();
  if (!ui_->errorLabel->text().trimmed().isEmpty()) return;

  auto from = ui_->fromEdit->text();
  auto to = ui_->toEdit->text();
  auto cc = ui_->ccEdit->text();
  auto bcc = ui_->bccEdit->text();
  auto subject = ui_->subjectEdit->text();

  auto recipient_list = to.split(';', Qt::SkipEmptyParts);
  auto cc_list = cc.split(';', Qt::SkipEmptyParts);
  auto bcc_list = bcc.split(';', Qt::SkipEmptyParts);

  QString name;
  QString email;

  try {
    vmime::messageBuilder msg_builder;

    if (ParseEmailString(from, name, email)) {
      msg_builder.setExpeditor(
          vmime::mailbox(vmime::text(name.toStdString()), email.toStdString()));
    } else {
      msg_builder.setExpeditor(vmime::mailbox(email.toStdString()));
    }

    for (const QString& recipient : recipient_list) {
      auto trimmed_recipient = recipient.trimmed();

      if (ParseEmailString(trimmed_recipient, name, email)) {
        msg_builder.getRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(vmime::text(name.toStdString()),
                                               email.toStdString()));
      } else {
        msg_builder.getRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(
                trimmed_recipient.toStdString()));
      }
    }

    for (const QString& recipient : cc_list) {
      auto trimmed_recipient = recipient.trimmed();
      if (ParseEmailString(trimmed_recipient, name, email)) {
        msg_builder.getCopyRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(vmime::text(name.toStdString()),
                                               email.toStdString()));
      } else {
        msg_builder.getCopyRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(
                trimmed_recipient.toStdString()));
      }
    }

    for (const QString& recipient : bcc_list) {
      auto trimmed_recipient = recipient.trimmed();
      if (ParseEmailString(trimmed_recipient, name, email)) {
        msg_builder.getBlindCopyRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(vmime::text(name.toStdString()),
                                               email.toStdString()));
      } else {
        msg_builder.getBlindCopyRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(
                trimmed_recipient.toStdString()));
      }
    }

    msg_builder.setSubject(vmime::text(subject.toStdString()));

    vmime::shared_ptr<vmime::message> msg = msg_builder.construct();

    auto header = msg->getHeader();

    // no Content-Transfer-Encoding
    header->removeField(
        header->getField(vmime::fields::CONTENT_TRANSFER_ENCODING));

    auto content_type_header_field =
        header->getField<vmime::contentTypeField>(vmime::fields::CONTENT_TYPE);
    content_type_header_field->setValue("multipart/signed");
    auto body_boundary = vmime::body::generateRandomBoundaryString();
    content_type_header_field->appendParameter(
        vmime::make_shared<vmime::parameter>("protocol",
                                             "application/pgp-signature"));
    content_type_header_field->setBoundary(body_boundary);

    auto root_body_part = vmime::make_shared<vmime::body>();
    auto container_part = vmime::make_shared<vmime::bodyPart>();
    auto mime_part = vmime::make_shared<vmime::bodyPart>();
    auto public_key_part = vmime::make_shared<vmime::bodyPart>();
    auto signature_part = vmime::make_shared<vmime::bodyPart>();

    root_body_part->appendPart(container_part);
    root_body_part->appendPart(signature_part);
    msg->setBody(root_body_part);

    root_body_part->setPrologText(
        "This is an OpenPGP/MIME signed message (RFC 4880 and 3156)");

    auto container_boundary = vmime::body::generateRandomBoundaryString();
    auto container_part_header = container_part->getHeader();
    auto container_part_content_ttype_header_field =
        container_part_header->getField<vmime::contentTypeField>(
            vmime::fields::CONTENT_TYPE);
    container_part_content_ttype_header_field->setValue("multipart/mixed");
    container_part_content_ttype_header_field->setBoundary(container_boundary);

    auto container_part_body = container_part->getBody();

    container_part_body->appendPart(mime_part);
    container_part_body->appendPart(public_key_part);

    auto public_key_part_header = public_key_part->getHeader();

    auto public_key_name = QString("OpenPGP_0x%1.asc").arg(sign_key.toUpper());
    auto public_key_part_content_type_header_field =
        public_key_part_header->getField<vmime::contentTypeField>(
            vmime::fields::CONTENT_TYPE);
    public_key_part_content_type_header_field->setValue("application/pgp-keys");
    public_key_part_content_type_header_field->appendParameter(
        vmime::make_shared<vmime::parameter>("name",
                                             public_key_name.toStdString()));

    auto public_key_part_content_desc_header_field =
        public_key_part_header->getField(vmime::fields::CONTENT_DESCRIPTION);
    public_key_part_content_desc_header_field->setValue("OpenPGP public key");

    auto public_key_part_content_trans_encode_field =
        public_key_part_header->getField(
            vmime::fields::CONTENT_TRANSFER_ENCODING);
    public_key_part_content_trans_encode_field->setValue("quoted-printable");

    auto public_key_part_content_disp_header_field =
        public_key_part_header->getField<vmime::contentDispositionField>(
            vmime::fields::CONTENT_DISPOSITION);
    public_key_part_content_disp_header_field->setValue("attachment");
    public_key_part_content_disp_header_field->setFilename(
        vmime::word(public_key_name.toStdString()));

    auto signature_part_header = signature_part->getHeader();

    auto signature_part_content_type_header_field =
        signature_part_header->getField<vmime::contentTypeField>(
            vmime::fields::CONTENT_TYPE);
    signature_part_content_type_header_field->setValue(
        "application/pgp-signature");
    signature_part_content_type_header_field->appendParameter(
        vmime::make_shared<vmime::parameter>("name", "OpenPGP_signature.asc"));

    auto signature_part_content_desc_header_field =
        signature_part_header->getField(vmime::fields::CONTENT_DESCRIPTION);
    signature_part_content_desc_header_field->setValue(
        "OpenPGP digital signature");

    auto signature_part_content_disp_header_field =
        signature_part_header->getField<vmime::contentDispositionField>(
            vmime::fields::CONTENT_DISPOSITION);
    signature_part_content_disp_header_field->setValue("attachment");
    signature_part_content_disp_header_field->setFilename(
        vmime::word({"OpenPGP_signature.asc"}));

    auto public_key = UDUP(GFGpgPublicKey(channel_, QDUP(sign_key), 1));
    if (public_key.isEmpty()) {
      emit SignalEMLDataGenerateFailed("Get Public Key of Sign Key Failed");
      return;
    }

    auto public_key_part_part_body = public_key_part->getBody();
    auto public_key_part_body_content =
        vmime::make_shared<vmime::stringContentHandler>();
    public_key_part_body_content->setData(
        public_key.toLatin1().replace('\n', "\r\n").toStdString(),
        vmime::encoding(vmime::encodingTypes::QUOTED_PRINTABLE));
    public_key_part_part_body->setContents(public_key_part_body_content);

    auto mime_part_header = mime_part->getHeader();

    auto mime_part_content_type_header_field =
        mime_part_header->getField<vmime::contentTypeField>(
            vmime::fields::CONTENT_TYPE);
    mime_part_content_type_header_field->setValue("text/plain");
    mime_part_content_type_header_field->appendParameter(
        vmime::make_shared<vmime::parameter>("charset", "UTF-8"));
    mime_part_content_type_header_field->appendParameter(
        vmime::make_shared<vmime::parameter>("format", "flowed"));
    auto mime_part_content_trans_encode_field =
        mime_part_header->getField(vmime::fields::CONTENT_TRANSFER_ENCODING);
    mime_part_content_trans_encode_field->setValue("base64");

    auto mime_part_part_body = mime_part->getBody();
    auto mime_part_body_content =
        vmime::make_shared<vmime::stringContentHandler>();
    mime_part_body_content->setData(
        body_data_.toBase64().toStdString(),
        vmime::encoding(vmime::encodingTypes::BASE64));
    mime_part_part_body->setContents(mime_part_body_content);

    auto container_raw_data =
        Q_SC(container_part->generate(vmime::lineLengthLimits::infinite));

    auto container_raw_data_hash = QCryptographicHash::hash(
        container_raw_data.toLatin1(), QCryptographicHash::Sha1);
    FLOG_DEBUG("raw content of signature hash: %1",
               container_raw_data_hash.toHex());

    FLOG_DEBUG("MIME Raw Data For Signature: %1", container_raw_data);
    FLOG_DEBUG("Signature Channel: %1, Sign Key: %2", channel_, sign_key);

    GFGpgSignResult* s;
    auto ret = GFGpgSignData(channel_, QListToCharArray({sign_key}), 1,
                             QDUP(container_raw_data), 1, 1, &s);

    if (ret != 0) {
      emit SignalEMLDataGenerateFailed("Sign Failed");
      return;
    }

    auto signature = UDUP(s->signature);
    auto hash_algo = UDUP(s->hash_algo);

    GFFreeMemory(s);

    FLOG_DEBUG("Hash Algo: %1 Signature Data: %2", hash_algo, signature);
    content_type_header_field->appendParameter(
        vmime::make_shared<vmime::parameter>(
            "micalg",
            QString("pgp-%1").arg(hash_algo.toLower()).toStdString()));

    auto signature_part_body = signature_part->getBody();
    auto signature_part_body_content =
        vmime::make_shared<vmime::stringContentHandler>(
            signature.toStdString());
    signature_part_body->setContents(signature_part_body_content);

    auto eml_data = Q_SC(msg->generate(vmime::lineLengthLimits::infinite));

    FLOG_DEBUG("EML Data: %1", eml_data);

    emit SignalEMLDataGenerateSuccess(eml_data);
    this->close();
    return;

  } catch (const vmime::exception& e) {
    QMessageBox::critical(this, tr("Error"),
                          tr("Failed to export EML Data: %1").arg(e.what()));
    emit SignalEMLDataGenerateFailed(e.what());
    this->close();
    return;
  }

  emit SignalEMLDataGenerateFailed("Unknown Error");
  this->close();
}

void EMailMetaDataDialog::slot_export_encrypted_data() {}

void EMailMetaDataDialog::SetKeys(QStringList k) {
  keys_ = std::move(k);

  if (mode_ == 0) {
    slot_set_from_field_by_sign_key();
  } else {
    slot_set_to_field_by_encrypt_keys();
  }
}

void EMailMetaDataDialog::SetChannel(int c) { channel_ = c; }

void EMailMetaDataDialog::slot_set_from_field_by_sign_key() {
  // only allow one sign key
  const auto sign_key = keys_.front();

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
  for (const QString& email : email_list) {
    if (!is_valid_email(email.trimmed())) {
      return false;
    }
  }
  return true;
}

auto EMailMetaDataDialog::is_valid_email(const QString& email) -> bool {
  return kNameEmailStringValidateRegex.match(email).hasMatch();
}

void EMailMetaDataDialog::SetBodyData(QByteArray b) {
  body_data_ = std::move(b);
}

void EMailMetaDataDialog::slot_encrypt_eml_data() {
  if (keys_.isEmpty()) {
    emit SignalEMLDataGenerateFailed("No Encryption Key Selected");
    return;
  }

  const auto encrypt_key = keys_.front();

  slot_validate_inputs_and_show_errors();
  if (!ui_->errorLabel->text().trimmed().isEmpty()) {
    return;
  }

  auto from = ui_->fromEdit->text();
  auto to = ui_->toEdit->text();
  auto cc = ui_->ccEdit->text();
  auto bcc = ui_->bccEdit->text();
  auto subject = ui_->subjectEdit->text();

  auto recipient_list = to.split(';', Qt::SkipEmptyParts);
  auto cc_list = cc.split(';', Qt::SkipEmptyParts);
  auto bcc_list = bcc.split(';', Qt::SkipEmptyParts);

  QString name;
  QString email;

  try {
    vmime::messageBuilder plaintext_msg_builder;

    if (ParseEmailString(from, name, email)) {
      plaintext_msg_builder.setExpeditor(
          vmime::mailbox(vmime::text(name.toStdString()), email.toStdString()));
    } else {
      plaintext_msg_builder.setExpeditor(vmime::mailbox(from.toStdString()));
    }

    for (const QString& recipient : recipient_list) {
      auto trimmed_recipient = recipient.trimmed();
      if (ParseEmailString(trimmed_recipient, name, email)) {
        plaintext_msg_builder.getRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(vmime::text(name.toStdString()),
                                               email.toStdString()));
      } else {
        plaintext_msg_builder.getRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(
                trimmed_recipient.toStdString()));
      }
    }

    for (const QString& recipient : cc_list) {
      auto trimmed_recipient = recipient.trimmed();
      if (ParseEmailString(trimmed_recipient, name, email)) {
        plaintext_msg_builder.getCopyRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(vmime::text(name.toStdString()),
                                               email.toStdString()));
      } else {
        plaintext_msg_builder.getCopyRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(
                trimmed_recipient.toStdString()));
      }
    }

    for (const QString& recipient : bcc_list) {
      auto trimmed_recipient = recipient.trimmed();
      if (ParseEmailString(trimmed_recipient, name, email)) {
        plaintext_msg_builder.getBlindCopyRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(vmime::text(name.toStdString()),
                                               email.toStdString()));
      } else {
        plaintext_msg_builder.getBlindCopyRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(
                trimmed_recipient.toStdString()));
      }
    }

    plaintext_msg_builder.setSubject(vmime::text(subject.toStdString()));

    vmime::shared_ptr<vmime::message> plaintext_msg =
        plaintext_msg_builder.construct();

    auto plaintext_msg_header = plaintext_msg->getHeader();

    auto plaintext_msg_content_type_header_field =
        plaintext_msg_header->getField<vmime::contentTypeField>(
            vmime::fields::CONTENT_TYPE);
    plaintext_msg_content_type_header_field->setValue("text/plain");
    plaintext_msg_content_type_header_field->appendParameter(
        vmime::make_shared<vmime::parameter>("charset", "UTF-8"));
    plaintext_msg_content_type_header_field->appendParameter(
        vmime::make_shared<vmime::parameter>("format", "flowed"));

    auto plaintext_msg_content_trans_encode_field =
        plaintext_msg_header->getField(
            vmime::fields::CONTENT_TRANSFER_ENCODING);
    plaintext_msg_content_trans_encode_field->setValue("base64");

    auto plaintext_msg_body = plaintext_msg->getBody();

    auto mime_part_body_content =
        vmime::make_shared<vmime::stringContentHandler>();
    mime_part_body_content->setData(
        body_data_.toBase64().toStdString(),
        vmime::encoding(vmime::encodingTypes::BASE64));
    plaintext_msg_body->setContents(mime_part_body_content);

    auto plaintext_eml_data =
        Q_SC(plaintext_msg->generate(vmime::lineLengthLimits::infinite));

    GFGpgEncryptionResult* enc_result = nullptr;
    auto ret = GFGpgEncryptData(channel_, QListToCharArray(keys_), keys_.size(),
                                QDUP(plaintext_eml_data), 1, &enc_result);

    if (ret != 0) {
      emit SignalEMLDataGenerateFailed("Encryption Failed");
      return;
    }

    auto encrypted_data = UDUP(enc_result->encrypted_data);
    GFFreeMemory(enc_result);

    vmime::messageBuilder msg_builder;

    if (ParseEmailString(from, name, email)) {
      msg_builder.setExpeditor(vmime::mailbox(email.toStdString()));
    } else {
      msg_builder.setExpeditor(vmime::mailbox(from.toStdString()));
    }

    for (const QString& recipient : recipient_list) {
      auto trimmed_recipient = recipient.trimmed();
      if (ParseEmailString(trimmed_recipient, name, email)) {
        msg_builder.getRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(email.toStdString()));
      } else {
        msg_builder.getRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(
                trimmed_recipient.toStdString()));
      }
    }

    for (const QString& recipient : cc_list) {
      auto trimmed_recipient = recipient.trimmed();
      if (ParseEmailString(trimmed_recipient, name, email)) {
        msg_builder.getCopyRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(email.toStdString()));
      } else {
        msg_builder.getCopyRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(
                trimmed_recipient.toStdString()));
      }
    }

    for (const QString& recipient : bcc_list) {
      auto trimmed_recipient = recipient.trimmed();
      if (ParseEmailString(trimmed_recipient, name, email)) {
        msg_builder.getBlindCopyRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(email.toStdString()));
      } else {
        msg_builder.getBlindCopyRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(
                trimmed_recipient.toStdString()));
      }
    }

    msg_builder.setSubject(vmime::text("..."));

    vmime::shared_ptr<vmime::message> msg = msg_builder.construct();

    auto header = msg->getHeader();

    // no Content-Transfer-Encoding
    header->removeField(
        header->getField(vmime::fields::CONTENT_TRANSFER_ENCODING));

    auto content_type_header_field =
        header->getField<vmime::contentTypeField>(vmime::fields::CONTENT_TYPE);
    content_type_header_field->setValue("multipart/encrypted");
    content_type_header_field->appendParameter(
        vmime::make_shared<vmime::parameter>("protocol",
                                             "application/pgp-encrypted"));

    auto root_part_boundary = vmime::body::generateRandomBoundaryString();
    content_type_header_field->setBoundary(root_part_boundary);

    auto root_body_part = vmime::make_shared<vmime::body>();
    auto control_info_part = vmime::make_shared<vmime::bodyPart>();
    auto encrypted_data_part = vmime::make_shared<vmime::bodyPart>();

    root_body_part->appendPart(control_info_part);
    root_body_part->appendPart(encrypted_data_part);
    msg->setBody(root_body_part);

    root_body_part->setPrologText(
        "This is an OpenPGP/MIME encrypted message (RFC 4880 and 3156)");

    auto control_info_part_header = control_info_part->getHeader();
    auto control_info_content_type_field =
        control_info_part_header->getField<vmime::contentTypeField>(
            vmime::fields::CONTENT_TYPE);
    control_info_content_type_field->setValue("application/pgp-encrypted");

    auto control_info_part_content_desc_header_field =
        control_info_part_header->getField(vmime::fields::CONTENT_DESCRIPTION);
    control_info_part_content_desc_header_field->setValue(
        "PGP/MIME version identification");

    auto control_info_body = control_info_part->getBody();
    auto control_info_content =
        vmime::make_shared<vmime::stringContentHandler>("Version: 1");
    control_info_body->setContents(control_info_content);

    auto encrypted_data_part_header = encrypted_data_part->getHeader();
    auto encrypted_data_content_type_field =
        encrypted_data_part_header->getField<vmime::contentTypeField>(
            vmime::fields::CONTENT_TYPE);
    encrypted_data_content_type_field->setValue("application/octet-stream");
    encrypted_data_content_type_field->appendParameter(
        vmime::make_shared<vmime::parameter>("name", "encrypted.asc"));

    auto encrypted_data_content_desc_header_field =
        encrypted_data_part_header->getField(
            vmime::fields::CONTENT_DESCRIPTION);
    encrypted_data_content_desc_header_field->setValue(
        "OpenPGP encrypted message");

    auto encrypted_data_content_disp_header_field =
        encrypted_data_part_header->getField<vmime::contentDispositionField>(
            vmime::fields::CONTENT_DISPOSITION);
    encrypted_data_content_disp_header_field->setValue("inline");
    encrypted_data_content_disp_header_field->setFilename(
        vmime::word({"encrypted.asc"}));

    auto encrypted_data_body = encrypted_data_part->getBody();
    auto encrypted_data_content =
        vmime::make_shared<vmime::stringContentHandler>(
            encrypted_data.toStdString());
    encrypted_data_body->setContents(encrypted_data_content);

    auto eml_data = Q_SC(msg->generate(vmime::lineLengthLimits::infinite));
    FLOG_DEBUG("EML Data: %1", eml_data);

    emit SignalEMLDataGenerateSuccess(eml_data);

    return;

  } catch (const vmime::exception& e) {
    QMessageBox::critical(this, tr("Error"),
                          tr("Failed to export EML Data: %1").arg(e.what()));
    emit SignalEMLDataGenerateFailed(e.what());
    return;
  }

  emit SignalEMLDataGenerateFailed("Unknown Error");
}

void EMailMetaDataDialog::slot_set_to_field_by_encrypt_keys() {
  QStringList to_list;

  for (const auto& key : keys_) {
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