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

auto EncryptEMLData(int channel, const QStringList& keys,
                    const EMailMetaData& meta_data, const QByteArray& body_data,
                    QString& eml_data) -> int {
  auto from = meta_data.from;
  auto recipient_list = meta_data.to;
  auto cc_list = meta_data.cc;
  auto bcc_list = meta_data.bcc;
  auto subject = meta_data.subject;

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
        body_data.toBase64().toStdString(),
        vmime::encoding(vmime::encodingTypes::BASE64));
    plaintext_msg_body->setContents(mime_part_body_content);

    auto plaintext_eml_data =
        Q_SC(plaintext_msg->generate(vmime::lineLengthLimits::infinite));

    GFGpgEncryptionResult* enc_result = nullptr;
    auto ret = GFGpgEncryptData(channel, QListToCharArray(keys), keys.size(),
                                QDUP(plaintext_eml_data), 1, &enc_result);

    if (ret != 0) {
      eml_data = "Encryption Failed";
      return -1;
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

    eml_data = Q_SC(msg->generate(vmime::lineLengthLimits::infinite));
    FLOG_DEBUG("EML Data: %1", eml_data);

    return 0;

  } catch (const vmime::exception& e) {
    eml_data = QString("VMIME Error: %1").arg(e.what());
    return -1;
  }

  eml_data = QString("Unknown Error: %1");
  return -1;
}

auto SignEMLData(int channel, const QString& key,
                 const EMailMetaData& meta_data, const QByteArray& body_data,
                 QString& eml_data) -> int {
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
        vmime::word({"OpenPGP_signature.asc"}));

    auto public_key = UDUP(GFGpgPublicKey(channel, QDUP(key), 1));
    if (public_key.isEmpty()) {
      eml_data = "Get Public Key of Sign Key Failed";
      return -1;
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
        body_data.toBase64().toStdString(),
        vmime::encoding(vmime::encodingTypes::BASE64));
    mime_part_part_body->setContents(mime_part_body_content);

    auto container_raw_data =
        Q_SC(container_part->generate(vmime::lineLengthLimits::infinite));

    auto container_raw_data_hash = QCryptographicHash::hash(
        container_raw_data.toLatin1(), QCryptographicHash::Sha1);
    FLOG_DEBUG("raw content of signature hash: %1",
               container_raw_data_hash.toHex());

    FLOG_DEBUG("MIME Raw Data For Signature: %1", container_raw_data);
    FLOG_DEBUG("Signature Channel: %1, Sign Key: %2", channel, key);

    GFGpgSignResult* s;
    auto ret = GFGpgSignData(channel, QListToCharArray({key}), 1,
                             QDUP(container_raw_data), 1, 1, &s);

    if (ret != 0) {
      eml_data = "Sign Failed";
      return -1;
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

    eml_data = Q_SC(msg->generate(vmime::lineLengthLimits::infinite));

    FLOG_DEBUG("EML Data: %1", eml_data);

    return 0;

  } catch (const vmime::exception& e) {
    eml_data = QString("VMIME Error: %1").arg(e.what());
    return -1;
  }

  eml_data = QString("Unknown Error: %1");
  return -1;
}