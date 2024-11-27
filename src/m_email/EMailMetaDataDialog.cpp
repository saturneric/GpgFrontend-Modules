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

EMailMetaDataDialog::EMailMetaDataDialog(QByteArray body_data, QWidget* parent)
    : QDialog(parent),
      ui_(QSharedPointer<Ui_EMailMetaDataDialog>::create()),
      body_data_(std::move(body_data)) {
  ui_->setupUi(this);

  connect(ui_->exportMailButton, &QPushButton::clicked, this,
          &EMailMetaDataDialog::slot_export_eml_data);

  setModal(true);
  setAttribute(Qt::WA_DeleteOnClose);
  setWindowFlags(Qt::Dialog | Qt::Window);
}

Q_VARIANT_Q_OBJECT_FACTORY_DEFINE(CreateEMailMetaDataDialog,
                                  [](QVariant data) -> void* {
                                    return new EMailMetaDataDialog(
                                        data.toByteArray(), nullptr);
                                  });

void EMailMetaDataDialog::slot_export_eml_data() {
  // sign key is a must
  if (sign_key_.isEmpty()) emit SignalEMLDataGenerateFailed("No Sign Key");

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

    auto public_key_name = QString("OpenPGP_0x%1.asc").arg(sign_key_.toUpper());
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

    auto public_key = UDUP(GFGpgPublicKey(channel_, QDUP(sign_key_), 1));
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
        container_part->generate(vmime::lineLengthLimits::infinite);
    qDebug().noquote() << "\n" << container_raw_data;

    container_raw_data =
        container_part->generate(vmime::lineLengthLimits::infinite);
    qDebug().noquote() << "\n" << container_raw_data;

    auto container_raw_data_hash =
        QCryptographicHash::hash(container_raw_data, QCryptographicHash::Sha1);
    FLOG_DEBUG("raw content of signature hash: %1",
               container_raw_data_hash.toHex());

    FLOG_DEBUG("MIME Raw Data For Signature: %1", container_raw_data);
    FLOG_DEBUG("Signature Channel: %1, Sign Key: %2", channel_, sign_key_);

    GFGpgSignResult* s;
    auto ret = GFGpgSignData(channel_, QListToCharArray({sign_key_}), 1,
                             QDUP(Q_SC(container_raw_data)), 1, 1, &s);

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

void EMailMetaDataDialog::SetSignKey(QString k) {
  sign_key_ = std::move(k);
  slot_set_from_field_by_sign_key();
}

void EMailMetaDataDialog::SetChannel(int c) { channel_ = c; }

void EMailMetaDataDialog::slot_set_from_field_by_sign_key() {
  GFGpgKeyUID* s;
  auto ret = GFGpgKeyPrimaryUID(channel_, QDUP(sign_key_), &s);
  if (ret != 0) {
    FLOG_WARN("cannot get primary uid from sign key %1, ret: %2", sign_key_,
              ret);
    return;
  }

  from_name_ = UDUP(s->name);
  from_email_ = UDUP(s->email);
  auto comment = UDUP(s->comment);

  GFFreeMemory(s);

  ui_->fromEdit->setText(QString("%1 <%2>").arg(from_name_).arg(from_email_));
}
