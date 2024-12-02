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

#include "EMailBasicGpgOpera.h"

#include <GFSDKGpg.h>

//
#include <QCryptographicHash>

#include "EMailHelper.h"
#include "GFModuleCommonUtils.hpp"

auto EncryptPlainText(int channel, const QStringList& keys,
                      const EMailMetaData& meta_data,
                      const QByteArray& body_data, QString& eml_data,
                      QString& capsule_id) -> int {
  auto from = meta_data.from;
  auto recipient_list = meta_data.to;
  auto cc_list = meta_data.cc;
  auto bcc_list = meta_data.bcc;
  auto subject = meta_data.subject;

  QString name;
  QString email;

  try {
    GFGpgEncryptionResult* s = nullptr;
    auto ret = GFGpgEncryptData(channel, QStringListToCharArray(keys),
                                keys.size(), QDUP(body_data), 1, &s);

    auto encrypted_data = UDUP(s->encrypted_data);
    auto gpg_error_string = UDUP(s->error_string);
    capsule_id = UDUP(s->capsule_id);

    GFFreeMemory(s);

    if (ret != 0) {
      eml_data = "Encryption Failed: " + gpg_error_string;
      return -1;
    }

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

    auto root_body_part = vmime::make_shared<vmime::bodyPart>();
    auto control_info_part = vmime::make_shared<vmime::bodyPart>();
    auto encrypted_data_part = vmime::make_shared<vmime::bodyPart>();

    root_body_part->getBody()->appendPart(control_info_part);
    root_body_part->getBody()->appendPart(encrypted_data_part);
    root_body_part->getBody()->setPrologText(
        "This is an OpenPGP/MIME encrypted message (RFC 4880 and 3156)");
    msg->setBody(root_body_part->getBody());

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
        vmime::word(std::string{"encrypted.asc"}));

    auto encrypted_data_body = encrypted_data_part->getBody();
    auto encrypted_data_content =
        vmime::make_shared<vmime::stringContentHandler>(
            encrypted_data.toStdString());
    encrypted_data_body->setContents(encrypted_data_content);

    eml_data = Q_SC(msg->generate(vmime::lineLengthLimits::convenient));
    FLOG_DEBUG("EML Data: %1", eml_data);

    return 0;

  } catch (const vmime::exception& e) {
    eml_data = QString("VMIME Error: %1").arg(e.what());
    return -2;
  }

  eml_data = QString("Unknown Error: %1");
  return -1;
}

auto EncryptEMLData(int channel, const QStringList& keys,
                    const vmime::shared_ptr<vmime::message>& message,
                    const QByteArray& body_data, QString& eml_data,
                    QString& capsule_id) -> int {
  try {
    auto header = message->getHeader();
    auto body = message->getBody();

    auto body_offset = body->getParsedOffset();
    auto body_len = body->getParsedLength();

    auto plain_body_signed_raw_data = body_data.mid(
        static_cast<qsizetype>(body_offset), static_cast<qsizetype>(body_len));

    auto backup_content_type_header_field_component =
        header->getField<vmime::headerField>(vmime::fields::CONTENT_TYPE)
            ->clone();

    std::shared_ptr<vmime::headerField> backup_content_type_header_field =
        std::static_pointer_cast<vmime::headerField>(
            backup_content_type_header_field_component);

    auto backup_from_field_component =
        header->getField<vmime::headerField>(vmime::fields::FROM)->clone();

    std::shared_ptr<vmime::headerField> backup_from_field =
        std::static_pointer_cast<vmime::headerField>(
            backup_from_field_component);

    auto backup_to_field_component =
        header->getField<vmime::headerField>(vmime::fields::TO)->clone();

    std::shared_ptr<vmime::headerField> backup_to_field =
        std::static_pointer_cast<vmime::headerField>(backup_to_field_component);

    auto backup_message_id_field_component =
        header->hasField(vmime::fields::MESSAGE_ID)
            ? header->getField<vmime::headerField>(vmime::fields::MESSAGE_ID)
                  ->clone()
            : nullptr;

    std::shared_ptr<vmime::headerField> backup_message_id_field =
        std::static_pointer_cast<vmime::headerField>(
            backup_message_id_field_component);

    auto backup_subject_field_component =
        header->getField<vmime::headerField>(vmime::fields::SUBJECT)->clone();

    std::shared_ptr<vmime::headerField> backup_subject_field =
        std::static_pointer_cast<vmime::headerField>(
            backup_subject_field_component);

    auto plain_part = vmime::make_shared<vmime::bodyPart>();
    auto plain_part_header = plain_part->getHeader();
    plain_part_header->appendField(backup_content_type_header_field);
    plain_part_header->appendField(backup_subject_field);
    plain_part_header->appendField(backup_from_field);
    plain_part_header->appendField(backup_to_field);
    if (backup_message_id_field != nullptr) {
      plain_part_header->appendField(backup_message_id_field);
    }

    auto plain_header_raw_data =
        Q_SC(plain_part_header->generate(vmime::lineLengthLimits::convenient));

    auto plain_raw_data =
        plain_header_raw_data + "\r\n" + plain_body_signed_raw_data;

    plain_raw_data.replace("\r\n", "\n");
    plain_raw_data.replace("\n", "\r\n");

    GFGpgEncryptionResult* s = nullptr;
    auto ret = GFGpgEncryptData(channel, QStringListToCharArray(keys),
                                keys.size(), QDUP(plain_raw_data), 1, &s);

    auto encrypted_data = UDUP(s->encrypted_data);
    capsule_id = UDUP(s->capsule_id);
    auto gpg_error_string = UDUP(s->error_string);

    GFFreeMemory(s);

    if (ret != 0) {
      eml_data = "Encryption Failed: " + gpg_error_string;
      return -1;
    }

    // no Content-Transfer-Encoding
    header->removeField(
        header->getField(vmime::fields::CONTENT_TRANSFER_ENCODING));

    auto content_type_header_field =
        header->getField<vmime::contentTypeField>(vmime::fields::CONTENT_TYPE);
    content_type_header_field->setValue("multipart/encrypted");
    content_type_header_field->appendParameter(
        vmime::make_shared<vmime::parameter>("protocol",
                                             "application/pgp-encrypted"));

    // hide subject
    header->Subject()->setValue("...");

    auto root_part_boundary = vmime::body::generateRandomBoundaryString();
    content_type_header_field->setBoundary(root_part_boundary);

    auto root_body_part = vmime::make_shared<vmime::bodyPart>();
    auto control_info_part = vmime::make_shared<vmime::bodyPart>();
    auto encrypted_data_part = vmime::make_shared<vmime::bodyPart>();

    root_body_part->getBody()->appendPart(control_info_part);
    root_body_part->getBody()->appendPart(encrypted_data_part);
    root_body_part->getBody()->setPrologText(
        "This is an OpenPGP/MIME encrypted message (RFC 4880 and 3156)");
    message->setBody(root_body_part->getBody());

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
        vmime::word(std::string{"encrypted.asc"}));

    auto encrypted_data_body = encrypted_data_part->getBody();
    auto encrypted_data_content =
        vmime::make_shared<vmime::stringContentHandler>(
            encrypted_data.toStdString());
    encrypted_data_body->setContents(encrypted_data_content);

    eml_data = Q_SC(message->generate(vmime::lineLengthLimits::convenient));
    FLOG_DEBUG("EML Data: %1", eml_data);

    return 0;

  } catch (const vmime::exception& e) {
    eml_data = QString("VMIME Error: %1").arg(e.what());
    return -2;
  }

  eml_data = QString("Unknown Error: %1");
  return -1;
}

auto SignPlainText(int channel, const QString& key,
                   const EMailMetaData& meta_data, const QByteArray& body_data,
                   QString& eml_data, QString& capsule_id) -> int {
  auto from = meta_data.from;
  auto recipient_list = meta_data.to;
  auto cc_list = meta_data.cc;
  auto bcc_list = meta_data.bcc;
  auto subject = meta_data.subject;

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

    if (!subject.isEmpty()) {
      msg_builder.setSubject(vmime::text(subject.toStdString()));
    }

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

    auto root_body_part = vmime::make_shared<vmime::bodyPart>();
    auto container_part = vmime::make_shared<vmime::bodyPart>();
    auto mime_part = vmime::make_shared<vmime::bodyPart>();
    auto public_key_part = vmime::make_shared<vmime::bodyPart>();
    auto signature_part = vmime::make_shared<vmime::bodyPart>();

    root_body_part->getBody()->appendPart(container_part);
    root_body_part->getBody()->appendPart(signature_part);
    root_body_part->getBody()->setPrologText(
        "This is an OpenPGP/MIME signed message (RFC 4880 and 3156)");

    msg->setBody(root_body_part->getBody());

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

    auto public_key_name = QString("OpenPGP_0x%1.asc").arg(key.toUpper());
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
        vmime::word(std::string{"OpenPGP_signature.asc"}));

    auto public_key = UDUP(GFGpgPublicKey(channel, QDUP(key), 1));
    if (public_key.isEmpty()) {
      eml_data = "Get Public Key of Sign Key Failed";
      return -1;
    }

    public_key.replace("\r\n", "\n");
    public_key.replace("\n", "\r\n");

    auto public_key_part_part_body = public_key_part->getBody();
    auto public_key_part_body_content =
        vmime::make_shared<vmime::stringContentHandler>();
    public_key_part_body_content->setData(
        public_key.toLatin1().toStdString(),
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
    mime_part_body_content->setData(body_data.toStdString());
    mime_part_part_body->setContents(mime_part_body_content);

    auto container_raw_data =
        Q_SC(container_part->generate(vmime::lineLengthLimits::convenient));

    auto container_raw_data_hash = QCryptographicHash::hash(
        container_raw_data.toLatin1(), QCryptographicHash::Sha1);
    FLOG_DEBUG("raw content of signature hash: %1",
               container_raw_data_hash.toHex());

    FLOG_DEBUG("MIME Raw Data For Signature: %1", container_raw_data);
    FLOG_DEBUG("Signature Channel: %1, Sign Key: %2", channel, key);

    GFGpgSignResult* s;
    auto ret = GFGpgSignData(channel, QStringListToCharArray({key}), 1,
                             QDUP(container_raw_data), 1, 1, &s);

    auto signature = UDUP(s->signature);
    auto hash_algo = UDUP(s->hash_algo);
    capsule_id = UDUP(s->capsule_id);
    auto gpg_error_string = UDUP(s->error_string);

    GFFreeMemory(s);

    if (ret != 0) {
      eml_data = "Sign Failed: " + gpg_error_string;
      return -1;
    }

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

    eml_data = Q_SC(msg->generate(vmime::lineLengthLimits::convenient));

    FLOG_DEBUG("EML Data: %1", eml_data);

    return 0;

  } catch (const vmime::exception& e) {
    eml_data = QString("VMIME Error: %1").arg(e.what());
    return -2;
  }

  eml_data = QString("Unknown Error: %1");
  return -1;
}

auto SignEMLData(int channel, const QString& key,
                 const vmime::shared_ptr<vmime::message>& message,
                 QString& eml_data, QString& capsule_id) -> int {
  try {
    auto header = message->getHeader();

    auto backup_body_component = message->getBody()->clone();

    std::shared_ptr<vmime::body> backup_body =
        std::static_pointer_cast<vmime::body>(backup_body_component);

    auto backup_content_type_header_field_component =
        header->getField<vmime::headerField>(vmime::fields::CONTENT_TYPE)
            ->clone();

    std::shared_ptr<vmime::headerField> backup_content_type_header_field =
        std::static_pointer_cast<vmime::headerField>(
            backup_content_type_header_field_component);

    auto backup_content_trans_encode_field_component =
        header
            ->getField<vmime::headerField>(
                vmime::fields::CONTENT_TRANSFER_ENCODING)
            ->clone();

    std::shared_ptr<vmime::headerField>
        backup_content_trans_encode_header_field =
            std::static_pointer_cast<vmime::headerField>(
                backup_content_trans_encode_field_component);

    FLOG_DEBUG("Content-Transfer-Encoding Header Data: %1",
               backup_content_trans_encode_header_field->generate());

    // no Content-Transfer-Encoding
    header->removeField(
        header->getField(vmime::fields::CONTENT_TRANSFER_ENCODING));

    FLOG_DEBUG("Backup Content-Type Header Data: %1",
               backup_content_type_header_field->generate());
    FLOG_DEBUG("Backup Content-Transfer-Encoding Header Data: %1",
               backup_content_trans_encode_header_field->generate());

    auto content_type_header_field =
        header->getField<vmime::contentTypeField>(vmime::fields::CONTENT_TYPE);
    content_type_header_field->setValue("multipart/signed");
    auto body_boundary = vmime::body::generateRandomBoundaryString();
    content_type_header_field->appendParameter(
        vmime::make_shared<vmime::parameter>("protocol",
                                             "application/pgp-signature"));
    content_type_header_field->setBoundary(body_boundary);

    // update date field
    auto datetime_header_field = header->Date();
    datetime_header_field->setValue(vmime::datetime::now());

    auto root_body_part = vmime::make_shared<vmime::bodyPart>();
    auto container_part = vmime::make_shared<vmime::bodyPart>();
    auto mime_part = vmime::make_shared<vmime::bodyPart>();
    auto public_key_part = vmime::make_shared<vmime::bodyPart>();
    auto signature_part = vmime::make_shared<vmime::bodyPart>();

    root_body_part->getBody()->appendPart(container_part);
    root_body_part->getBody()->appendPart(signature_part);
    root_body_part->getBody()->setPrologText(
        "This is an OpenPGP/MIME signed message (RFC 4880 and 3156)");
    message->setBody(root_body_part->getBody());

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

    auto public_key_name = QString("OpenPGP_0x%1.asc").arg(key.toUpper());
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
        vmime::word(std::string{"OpenPGP_signature.asc"}));

    auto public_key = UDUP(GFGpgPublicKey(channel, QDUP(key), 1));
    if (public_key.isEmpty()) {
      eml_data = "Get Public Key of Sign Key Failed";
      return -1;
    }

    public_key.replace("\r\n", "\n");
    public_key.replace("\n", "\r\n");

    auto public_key_part_part_body = public_key_part->getBody();
    auto public_key_part_body_content =
        vmime::make_shared<vmime::stringContentHandler>();
    public_key_part_body_content->setData(
        public_key.toLatin1().toStdString(),
        vmime::encoding(vmime::encodingTypes::QUOTED_PRINTABLE));
    public_key_part_part_body->setContents(public_key_part_body_content);

    auto mime_part_header = mime_part->getHeader();

    auto mime_part_content_trans_encode_field =
        mime_part_header->getField<vmime::headerField>(
            vmime::fields::CONTENT_TRANSFER_ENCODING);
    mime_part_header->replaceField(mime_part_content_trans_encode_field,
                                   backup_content_trans_encode_header_field);

    auto mime_part_content_type_header_field =
        mime_part_header->getField<vmime::contentTypeField>(
            vmime::fields::CONTENT_TYPE);
    mime_part_header->replaceField(mime_part_content_type_header_field,
                                   backup_content_type_header_field);

    mime_part->setBody(backup_body);

    auto container_raw_data =
        Q_SC(container_part->generate(vmime::lineLengthLimits::convenient));

    container_raw_data.replace("\r\n", "\n");
    container_raw_data.replace("\n", "\r\n");

    auto container_raw_data_hash = QCryptographicHash::hash(
        container_raw_data.toLatin1(), QCryptographicHash::Sha1);
    FLOG_DEBUG("raw content of signature hash: %1",
               container_raw_data_hash.toHex());

    FLOG_DEBUG("MIME Raw Data For Signature: %1", container_raw_data);
    FLOG_DEBUG("Signature Channel: %1, Sign Key: %2", channel, key);

    GFGpgSignResult* s;
    auto ret = GFGpgSignData(channel, QStringListToCharArray({key}), 1,
                             QDUP(container_raw_data), 1, 1, &s);

    capsule_id = UDUP(s->capsule_id);
    auto signature = UDUP(s->signature);
    auto hash_algo = UDUP(s->hash_algo);
    auto gpg_error_string = UDUP(s->error_string);

    GFFreeMemory(s);

    if (ret != 0) {
      eml_data = "Sign Failed: " + gpg_error_string;
      return -1;
    }

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

    eml_data = Q_SC(message->generate(vmime::lineLengthLimits::convenient));

    FLOG_DEBUG("EML Data: %1", eml_data);

    return 0;

  } catch (const vmime::exception& e) {
    eml_data = QString("VMIME Error: %1").arg(e.what());
    return -2;
  }

  eml_data = QString("Unknown Error: %1");
  return -1;
}

auto VerifyEMLData(int channel, const QByteArray& data,
                   EMailMetaData& meta_data, QString& error_string,
                   QString& capsule_id) -> int {
  vmime::string vmime_data(data.constData(), data.size());

  auto message = vmime::make_shared<vmime::message>();
  try {
    message->parse(vmime_data);
  } catch (const vmime::exception& e) {
    FLOG_DEBUG("error when parsing vmime data: %1", e.what());
    error_string = "Error when parsing eml raw data";
    return -2;
  }

  auto header = message->getHeader();

  auto content_type_field =
      header->getField<vmime::contentTypeField>(vmime::fields::CONTENT_TYPE);
  if (!content_type_field) {
    error_string = "Cannot get 'Content-Type' Field from header";
    return -2;
  }

  auto content_type_value =
      Q_SC(content_type_field->getValue()->generate()).trimmed();

  auto prm_protocol = content_type_field->getParameter("protocol");
  if (!prm_protocol) {
    error_string = "Cannot get 'protocol' from 'Content-Type'";
    return -2;
  }

  /*
   * OpenPGP signed messages are denoted by the "multipart/signed" content
   * type.
   */
  if (content_type_value != "multipart/signed") {
    error_string =
        "OpenPGP signed messages are denoted by the 'multipart/signed' "
        "content type";
    return -2;
  }

  /*
   * with a "protocol" parameter which MUST have a value of
   * "application/pgp-signature"
   */
  auto prm_protocol_value = Q_SC(prm_protocol->getValue().generate());
  if (prm_protocol_value != "application/pgp-signature") {
    error_string =
        "The 'protocol' parameter which MUST have a value of "
        "'application/pgp-signature' (MUST be quoted)";
    return -2;
  }

  auto prm_micalg = content_type_field->getParameter("micalg");
  if (!prm_micalg) {
    error_string = "cannot get 'micalg' from 'Content-Type'";
    return -2;
  }

  /*
   * The "micalg" parameter for the "application/pgp-signature" protocol
   * MUST contain exactly one hash-symbol of the format "pgp-<hash-
   * identifier>", where <hash-identifier> identifies the Message
   * Integrity Check (MIC) algorithm used to generate the signature.
   */
  auto prm_micalg_value = Q_SC(prm_micalg->getValue().generate());
  FLOG_DEBUG("micalg value: %1", prm_micalg_value);
  if (!IsValidMicalgFormat(prm_micalg_value)) {
    error_string =
        "The 'micalg' parameter MUST contain exactly one hash-symbol of the "
        "format 'pgp-<hash-identifier>'";
    return -2;
  }

  auto from_field_value_text =
      ExtractFieldValueMailBox(header, vmime::fields::FROM);
  auto to_field_value_text =
      ExtractFieldValueAddressList(header, vmime::fields::TO);
  auto cc_field_value_text =
      ExtractFieldValueAddressList(header, vmime::fields::CC);
  auto bcc_field_value_text =
      ExtractFieldValueAddressList(header, vmime::fields::BCC);
  auto date_field_value =
      ExtractFieldValueDateTime(header, vmime::fields::DATE);
  auto subject_field_value_text =
      ExtractFieldValueText(header, vmime::fields::SUBJECT);
  auto reply_to_field_value_text =
      ExtractFieldValueMailBox(header, vmime::fields::REPLY_TO);
  auto organization_text =
      ExtractFieldValueText(header, vmime::fields::ORGANIZATION);

  auto body = message->getBody();
  auto content_type = body->getContentType();
  auto part_count = body->getPartCount();

  FLOG_DEBUG("body page count: %1", part_count);

  /*
   * The multipart/signed body MUST consist of exactly two parts.
   */
  if (part_count != 2) {
    error_string =
        "The multipart/signed body MUST consist of exactly two parts";
    return -2;
  }

  /*
    The first part contains the signed data in MIME canonical format,
    including a set of appropriate content headers describing the data.
  */
  auto part_mime = body->getPartAt(0);
  auto part_mime_parse_offset = part_mime->getParsedOffset();
  auto part_mime_parse_length = part_mime->getParsedLength();

  auto part_mime_content_text = QByteArray::fromStdString(
      vmime_data.substr(part_mime_parse_offset, part_mime_parse_length));

  FLOG_DEBUG("mime part info, raw offset: %1, length: %2",
             part_mime_parse_offset, part_mime_parse_length);

  auto part_mime_content_hash = QCryptographicHash::hash(
      part_mime_content_text, QCryptographicHash::Sha1);
  FLOG_DEBUG("mime part of raw content hash: %1",
             part_mime_content_hash.toHex());

  FLOG_DEBUG("mime part of raw content: %1", part_mime_content_text);
  qDebug().noquote() << "\n" << part_mime_content_text;

  if (part_mime_content_text.isEmpty()) {
    error_string = "Mime raw data part is empty";
    return -2;
  }

  auto attachments =
      vmime::attachmentHelper::findAttachmentsInBodyPart(part_mime);
  FLOG_DEBUG("mime part info, attachment count: %1", attachments.size());

  QStringList public_keys_buffer;

  for (const auto& att : attachments) {
    auto att_type = Q_SC(att->getType().generate()).trimmed();
    FLOG_DEBUG("mime part info, attachment type: %1", att_type);

    if (att_type != "application/pgp-keys") continue;

    std::ostringstream oss;
    vmime::utility::outputStreamAdapter osa(oss);
    att->getData()->extract(osa);

    public_keys_buffer.append(Q_SC(oss.str()));
  }

  FLOG_DEBUG("mime part info, attached public keys: ",
             public_keys_buffer.join("\n"));

  /*
   * The second body MUST contain the OpenPGP digital signature. It MUST
   * be labeled with a content type of "application/pgp-signature"
   */
  auto part_sign = body->getPartAt(1);
  auto part_sign_header = part_sign->getHeader();
  auto part_sign_content_type = part_sign_header->ContentType();
  auto part_sign_content_type_value =
      Q_SC(part_sign_content_type->getValue()->generate());

  if (part_sign_content_type_value != "application/pgp-signature") {
    error_string =
        "The second body MUST be labeled with a content type of "
        "'application/pgp-signature'";
    return -2;
  }

  auto part_sign_body_content =
      QByteArray::fromStdString(part_sign->getBody()->generate());
  if (part_sign_body_content.trimmed().isEmpty()) {
    error_string = "The signature part is empty";
    return -2;
  }

  FLOG_DEBUG("body part of signature content: %1", part_sign_body_content);

  GFGpgVerifyResult* s;
  auto ret = GFGpgVerifyData(channel, QDUP(part_mime_content_text),
                             QDUP(part_sign_body_content), &s);

  capsule_id = UDUP(s->capsule_id);
  auto gpg_error_string = UDUP(s->error_string);

  GFFreeMemory(s);

  if (ret != 0) {
    error_string = "Verify Failed: " + gpg_error_string;
    return -1;
  }

  meta_data.from = from_field_value_text;
  meta_data.to = to_field_value_text.split(',');
  meta_data.cc = cc_field_value_text.split(',');
  meta_data.bcc = bcc_field_value_text.split(',');
  meta_data.subject = subject_field_value_text;
  meta_data.datetime = date_field_value;
  meta_data.micalg = prm_micalg_value;
  meta_data.public_keys = public_keys_buffer.join("\n");
  meta_data.mime = {};
  meta_data.mime_hash = part_mime_content_hash.toHex();
  meta_data.signature = {};
  return 0;
}

auto DecryptEMLData(int channel, const QByteArray& data,
                    EMailMetaData& meta_data, QString& eml_data,
                    QString& capsule_id) -> int {
  vmime::string vmime_data(data.constData(), data.size());
  auto message = vmime::make_shared<vmime::message>();
  try {
    message->parse(vmime_data);
  } catch (const vmime::exception& e) {
    FLOG_DEBUG("error when parsing vmime data: %1", e.what());
    eml_data = "Error when parsing EML Data";
    return -2;
  }

  auto header = message->getHeader();

  auto content_type_field =
      header->getField<vmime::contentTypeField>(vmime::fields::CONTENT_TYPE);
  if (!content_type_field) {
    eml_data = "cannot get 'Content-Type' Field from header";
    return -2;
  }

  auto content_type_value =
      Q_SC(content_type_field->getValue()->generate()).trimmed();

  auto prm_protocol = content_type_field->getParameter("protocol");
  if (!prm_protocol) {
    eml_data = "cannot get 'protocol' from 'Content-Type'";
    return -2;
  }

  /*
   * OpenPGP encrypted data is denoted by the "multipart/encrypted"
   * content type
   */
  if (content_type_value != "multipart/encrypted") {
    eml_data =
        "OpenPGP encrypted data is denoted by the 'multipart/encrypted' "
        "content type";
    return -2;
  }

  /*
   * MUST have a "protocol" parameter value of "application/pgp-encrypted"
   */
  auto prm_protocol_value = Q_SC(prm_protocol->getValue().generate());
  if (prm_protocol_value != "application/pgp-encrypted") {
    eml_data =
        "'protocol' parameter which MUST have a value of "
        "'application/pgp-encrypted' (MUST be quoted)";
    return -2;
  }

  auto from_field_value_text =
      ExtractFieldValueMailBox(header, vmime::fields::FROM);
  auto to_field_value_text =
      ExtractFieldValueAddressList(header, vmime::fields::TO);
  auto cc_field_value_text =
      ExtractFieldValueAddressList(header, vmime::fields::CC);
  auto bcc_field_value_text =
      ExtractFieldValueAddressList(header, vmime::fields::BCC);
  auto date_field_value =
      ExtractFieldValueDateTime(header, vmime::fields::DATE);
  auto subject_field_value_text =
      ExtractFieldValueText(header, vmime::fields::SUBJECT);
  auto reply_to_field_value_text =
      ExtractFieldValueMailBox(header, vmime::fields::REPLY_TO);
  auto organization_text =
      ExtractFieldValueText(header, vmime::fields::ORGANIZATION);

  auto body = message->getBody();
  auto content_type = body->getContentType();
  auto part_count = body->getPartCount();

  FLOG_DEBUG("body page count: %1", part_count);

  /*
   * The multipart/encrypted body MUST consist of exactly two parts.
   */
  if (part_count != 2) {
    eml_data = "The multipart/signed body MUST consist of exactly two parts";
    return -2;
  }

  /*
   * The multipart/encrypted MIME body MUST consist of exactly two body
   * parts, the first with content type "application/pgp-encrypted".  This
   * body contains the control information.
   */
  auto part_mime = body->getPartAt(0);

  std::ostringstream oss;
  vmime::utility::outputStreamAdapter osa(oss);

  auto part_mime_body = part_mime->getBody();
  auto part_mime_body_content = part_mime_body->getContents();
  if (!part_mime_body_content) {
    eml_data = "Cannot get the content of the first part's body";
    return -2;
  }

  part_mime_body_content->extractRaw(osa);
  osa.flush();

  auto part_mime_body_content_text = Q_SC(oss.str());
  FLOG_DEBUG("body part of raw content text: %1", part_mime_body_content_text);

  /*
   * A message complying with this
   * standard MUST contain a "Version: 1" field in this body.
   */
  if (!part_mime_body_content_text.contains("Version: 1")) {
    eml_data =
        "The first part MUST contain a 'Version: 1' field in this "
        "body.";
    return -2;
  }

  /*
   * The second MIME body part MUST contain the actual encrypted data.  It
   * MUST be labeled with a content type of "application/octet-stream".
   */
  auto part_sign = body->getPartAt(1);
  auto part_sign_header = part_sign->getHeader();
  auto part_sign_content_type = part_sign_header->ContentType();
  auto part_sign_content_type_value =
      Q_SC(part_sign_content_type->getValue()->generate());

  if (part_sign_content_type_value != "application/octet-stream") {
    eml_data =
        "The second part MUST be labeled with a content type of "
        "'application/octet-stream'";
    return -2;
  }

  auto part_encr_body_content =
      QByteArray::fromStdString(part_sign->getBody()->generate());
  if (part_encr_body_content.trimmed().isEmpty()) {
    eml_data = "The second part is empty";
    return -2;
  }

  FLOG_DEBUG("body part of encrypt content: %1", part_encr_body_content);

  GFGpgDecryptResult* s;
  auto ret = GFGpgDecryptData(channel, QDUP(part_encr_body_content), &s);

  eml_data = UDUP(s->decrypted_data);
  capsule_id = UDUP(s->capsule_id);
  auto gpg_error_string = UDUP(s->error_string);

  GFFreeMemory(s);

  if (ret != 0) {
    eml_data = "Decrypt Failed: " + gpg_error_string;
    return -1;
  }

  // callback
  meta_data.from = from_field_value_text;
  meta_data.to = to_field_value_text.split(',');
  meta_data.cc = cc_field_value_text.split(',');
  meta_data.bcc = bcc_field_value_text.split(',');
  meta_data.subject = subject_field_value_text;
  meta_data.datetime = date_field_value;
  meta_data.encrypted_data = part_encr_body_content;
  return 0;
}