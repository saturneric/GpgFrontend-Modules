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
#include <GFSDKBuffer.hpp>
#include <GFSDKGpgResult.hpp>

//
#include <QCryptographicHash>

#include "EMailHelper.h"
#include "GFModuleCommonUtils.hpp"

namespace {

/// Describes a blob without disclosing it.
///
/// This module used to log whole messages at debug level -- the decrypted MIME
/// body among them -- which put plaintext into the log file and, in one case,
/// straight onto the console. A size and a short digest is enough to correlate
/// a blob across a session, and discloses nothing.
auto Elide(const QByteArray& data) -> QString {
  const auto digest =
      QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex().left(12);
  return QString("%1 bytes, sha1:%2").arg(data.size()).arg(QString(digest));
}

auto Elide(const QString& data) -> QString { return Elide(data.toUtf8()); }

/// Borrowed argv for the SDK's key-id parameters.
///
/// SDK arguments are borrowed, so these only have to outlive the call -- but
/// they DO have to outlive it, which a chain of temporaries would not. This
/// owns the UTF-8 bytes and the pointer array for as long as it is in scope.
/// The pointers are taken only after the byte arrays are all appended, so a
/// reallocation cannot leave them dangling.
class KeyIdArgs {
 public:
  explicit KeyIdArgs(const QStringList& ids) {
    utf8_.reserve(ids.size());
    for (const auto& id : ids) utf8_.append(id.toUtf8());
    ptrs_.reserve(utf8_.size());
    for (const auto& bytes : utf8_) ptrs_.append(bytes.constData());
  }

  [[nodiscard]] auto Data() const -> const char* const* {
    return ptrs_.constData();
  }
  [[nodiscard]] auto Size() const -> size_t {
    return static_cast<size_t>(ptrs_.size());
  }

 private:
  QList<QByteArray> utf8_;
  QList<const char*> ptrs_;
};

/// The failure text to show: the engine's own words wherever it gave any.
auto SdkFailureText(const QString& prefix, const QString& reason) -> QString {
  return reason.isEmpty() ? QString("%1 Failed.").arg(prefix)
                          : QString("%1 Failed: %2").arg(prefix, reason);
}

}  // namespace

auto EncryptPlainText(int channel, const QStringList& keys,
                      const EMailMetaData& meta_data,
                      const QByteArray& body_data, QByteArray& eml_data,
                      gpgme_error_t& err, QString& capsule_id) -> int {
  auto from = meta_data.from;
  auto recipient_list = meta_data.to;
  auto cc_list = meta_data.cc;
  auto subject = meta_data.subject;

  QString name;
  QString email;

  try {
    // The SDK sets *ps = nullptr and returns non-zero when it cannot
    // allocate the result, so both must be checked before the first
    // dereference below -- not after it, as this used to.
    // One owned result, reclaimed by its own destructor on every path --
    // including the early return below, which is where the struct-based API
    // leaked because a FAILED call still allocated a result to explain
    // itself.
    auto in = GFBuf::Copy(body_data);
    auto key_ids = KeyIdArgs(keys);
    GFGpgResult r;
    if (GFGpgEncrypt(channel, key_ids.Data(), key_ids.Size(), in.View(), 1,
                     r.Out()) != GF_GPG_OK) {
      eml_data = SdkFailureText("Encryption", r.ErrorString()).toUtf8();
      return kFAILED;
    }

    auto encrypted_data = r.DataCopy();
    err = r.Error();
    capsule_id = r.CapsuleId();
    auto gpg_error_string = r.ErrorString();

    if (err != GPG_ERR_NO_ERROR) {
      eml_data = "Gpg Encryption Failed: " + gpg_error_string.toUtf8();
      return kGPG_FAILED;
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

    // No blind recipients are given to the builder: it would write them into
    // a Bcc: header, and these are the bytes that get sent. A blind recipient
    // named in the message is not a blind recipient. They belong to
    // EMailComposeState, where they select encryption recipients without ever
    // becoming part of the message.

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

    eml_data = QByteArray::fromStdString(
        msg->generate(vmime::lineLengthLimits::convenient));
    FLOG_DEBUG("eml data: %1", Elide(eml_data));

    return kSUCCESS;

  } catch (const vmime::exception& e) {
    eml_data = QByteArray("VMIME Error: ") + e.what();
    return kEML_FAILED;
  }

  eml_data = QByteArray("Unknown Error");
  return kFAILED;
}

auto EncryptEMLData(int channel, const QStringList& keys,
                    const vmime::shared_ptr<vmime::message>& message,
                    const QByteArray& body_data, QByteArray& eml_data,
                    gpgme_error_t& err, QString& capsule_id) -> int {
  try {
    auto header = message->getHeader();
    auto body = message->getBody();

    auto body_offset = body->getParsedOffset();
    auto body_len = body->getParsedLength();

    auto plain_body_signed_raw_data = body_data.mid(
        static_cast<qsizetype>(body_offset), static_cast<qsizetype>(body_len));

    // Every Content-* field travels with the body, which is carried over
    // below as a raw, still-encoded slice. Hand-picking a few headers here
    // used to drop Content-Transfer-Encoding, so a base64 body -- which is
    // every message carrying an attachment -- reached the recipient with
    // nothing saying how to decode it.
    auto plain_header_raw_data = BuildInnerPartHeader(header);

    auto plain_raw_data =
        plain_header_raw_data + "\r\n" + plain_body_signed_raw_data;

    plain_raw_data.replace("\r\n", "\n");
    plain_raw_data.replace("\n", "\r\n");

    // The SDK sets *ps = nullptr and returns non-zero when it cannot
    // allocate the result, so both must be checked before the first
    // dereference below -- not after it, as this used to.
    auto in = GFBuf::Copy(plain_raw_data);
    auto key_ids = KeyIdArgs(keys);
    GFGpgResult r;
    if (GFGpgEncrypt(channel, key_ids.Data(), key_ids.Size(), in.View(), 1,
                     r.Out()) != GF_GPG_OK) {
      eml_data = SdkFailureText("Encryption", r.ErrorString()).toUtf8();
      return kFAILED;
    }

    auto encrypted_data = r.DataCopy();
    err = r.Error();
    capsule_id = r.CapsuleId();
    auto gpg_error_string = r.ErrorString();

    if (err != GPG_ERR_NO_ERROR) {
      eml_data = "Encryption Failed: " + gpg_error_string.toUtf8();
      return kGPG_FAILED;
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

    eml_data = QByteArray::fromStdString(
        message->generate(vmime::lineLengthLimits::convenient));
    FLOG_DEBUG("eml data: %1", Elide(eml_data));

    return kSUCCESS;

  } catch (const vmime::exception& e) {
    eml_data = QByteArray("VMIME Error: ") + e.what();
    return kEML_FAILED;
  }

  eml_data = QByteArray("Unknown Error");
  return kFAILED;
}

auto SignPlainText(int channel, const QString& key,
                   const EMailMetaData& meta_data, const QByteArray& body_data,
                   QByteArray& eml_data, gpgme_error_t& err,
                   QString& capsule_id) -> int {
  auto from = meta_data.from;
  auto recipient_list = meta_data.to;
  auto cc_list = meta_data.cc;
  auto subject = meta_data.subject;

  QString name;
  QString email;

  try {
    vmime::messageBuilder msg_builder;

    if (ParseEmailString(from, name, email)) {
      msg_builder.setExpeditor(
          vmime::mailbox(Q_TEXT(name), email.toStdString()));
    } else {
      msg_builder.setExpeditor(vmime::mailbox(from.toStdString()));
    }

    for (const QString& recipient : recipient_list) {
      auto trimmed_recipient = recipient.trimmed();

      if (ParseEmailString(trimmed_recipient, name, email)) {
        msg_builder.getRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(Q_TEXT(name),
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
            vmime::make_shared<vmime::mailbox>(Q_TEXT(name),
                                               email.toStdString()));
      } else {
        msg_builder.getCopyRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(
                trimmed_recipient.toStdString()));
      }
    }

    // No blind recipients are given to the builder: it would write them into
    // a Bcc: header, and these are the bytes that get sent. A blind recipient
    // named in the message is not a blind recipient. They belong to
    // EMailComposeState, where they select encryption recipients without ever
    // becoming part of the message.

    if (!subject.isEmpty()) {
      msg_builder.setSubject(Q_TEXT(subject));
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

    auto public_key = UDUP(GFGpgPublicKey(channel, (key).toUtf8().constData(), 1));
    if (public_key.isEmpty()) {
      eml_data = "Get Public Key of Sign Key Failed";
      return kFAILED;
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

    auto container_raw_data = QByteArray::fromStdString(
        container_part->generate(vmime::lineLengthLimits::convenient));

    auto container_raw_data_hash =
        QCryptographicHash::hash(container_raw_data, QCryptographicHash::Sha1);
    FLOG_DEBUG("raw content of signature hash: %1",
               container_raw_data_hash.toHex());

    FLOG_DEBUG("mime raw data for signature: %1", Elide(container_raw_data));
    FLOG_DEBUG("Signature Channel: %1, Sign Key: %2", channel, key);

    // The SDK sets *ps = nullptr and returns non-zero when it cannot
    // allocate the result, so both must be checked before the first
    // dereference below -- not after it, as this used to.
    auto in = GFBuf::Copy(container_raw_data);
    auto key_ids = KeyIdArgs({key});
    GFGpgResult r;
    if (GFGpgSign(channel, key_ids.Data(), key_ids.Size(), in.View(), 1, 1,
                  r.Out()) != GF_GPG_OK) {
      eml_data = SdkFailureText("Sign", r.ErrorString()).toUtf8();
      return kFAILED;
    }

    auto signature = r.DataCopy();
    auto hash_algo = r.HashAlgo();
    err = r.Error();
    capsule_id = r.CapsuleId();
    auto gpg_error_string = r.ErrorString();

    if (err != GPG_ERR_NO_ERROR) {
      eml_data = "Sign Failed: " + gpg_error_string.toUtf8();
      return kGPG_FAILED;
    }

    FLOG_DEBUG("hash algo: %1, signature: %2", hash_algo, Elide(signature));
    content_type_header_field->appendParameter(
        vmime::make_shared<vmime::parameter>(
            "micalg",
            QString("pgp-%1").arg(hash_algo.toLower()).toStdString()));

    auto signature_part_body = signature_part->getBody();
    auto signature_part_body_content =
        vmime::make_shared<vmime::stringContentHandler>(
            signature.toStdString());
    signature_part_body->setContents(signature_part_body_content);

    eml_data = QByteArray::fromStdString(
        msg->generate(vmime::lineLengthLimits::convenient));

    FLOG_DEBUG("eml data: %1", Elide(eml_data));

    return kSUCCESS;

  } catch (const vmime::exception& e) {
    eml_data = QByteArray("VMIME Error: ") + e.what();
    return kEML_FAILED;
  }

  eml_data = QByteArray("Unknown Error");
  return kFAILED;
}

auto SignEMLData(int channel, const QString& key,
                 const vmime::shared_ptr<vmime::message>& message,
                 QByteArray& eml_data, gpgme_error_t& err, QString& capsule_id)
    -> int {
  try {
    // Re-signing replaces the previous signature; it does not pile a new one on
    // top of it. Without this the old wrapper and the old signer's key both
    // survive into the new message, and every re-sign adds another pair.
    StripPreviousSignature(message);

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

    auto public_key = UDUP(GFGpgPublicKey(channel, (key).toUtf8().constData(), 1));
    if (public_key.isEmpty()) {
      eml_data = "Get Public Key of Sign Key Failed";
      return kFAILED;
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

    auto container_raw_data = QByteArray::fromStdString(
        container_part->generate(vmime::lineLengthLimits::convenient));

    container_raw_data.replace("\r\n", "\n");
    container_raw_data.replace("\n", "\r\n");

    auto container_raw_data_hash =
        QCryptographicHash::hash(container_raw_data, QCryptographicHash::Sha1);
    FLOG_DEBUG("raw content of signature hash: %1",
               container_raw_data_hash.toHex());

    FLOG_DEBUG("mime raw data for signature: %1", Elide(container_raw_data));
    FLOG_DEBUG("Signature Channel: %1, Sign Key: %2", channel, key);

    // The SDK sets *ps = nullptr and returns non-zero when it cannot
    // allocate the result, so both must be checked before the first
    // dereference below -- not after it, as this used to.
    auto in = GFBuf::Copy(container_raw_data);
    auto key_ids = KeyIdArgs({key});
    GFGpgResult r;
    if (GFGpgSign(channel, key_ids.Data(), key_ids.Size(), in.View(), 1, 1,
                  r.Out()) != GF_GPG_OK) {
      eml_data = SdkFailureText("Sign", r.ErrorString()).toUtf8();
      return kFAILED;
    }

    auto signature = r.DataCopy();
    auto hash_algo = r.HashAlgo();
    auto gpg_error_string = r.ErrorString();
    err = r.Error();
    capsule_id = r.CapsuleId();

    if (err != GPG_ERR_NO_ERROR) {
      eml_data = "Sign Failed: " + gpg_error_string.toUtf8();
      return kGPG_FAILED;
    }

    FLOG_DEBUG("hash algo: %1, signature: %2", hash_algo, Elide(signature));
    content_type_header_field->appendParameter(
        vmime::make_shared<vmime::parameter>(
            "micalg",
            QString("pgp-%1").arg(hash_algo.toLower()).toStdString()));

    auto signature_part_body = signature_part->getBody();
    auto signature_part_body_content =
        vmime::make_shared<vmime::stringContentHandler>(
            signature.toStdString());
    signature_part_body->setContents(signature_part_body_content);

    eml_data = QByteArray::fromStdString(
        message->generate(vmime::lineLengthLimits::convenient));

    FLOG_DEBUG("eml data: %1", Elide(eml_data));

    return kSUCCESS;

  } catch (const vmime::exception& e) {
    eml_data = QByteArray("VMIME Error: ") + e.what();
    return kEML_FAILED;
  }

  eml_data = QByteArray("Unknown Error");
  return kFAILED;
}

namespace {

/// The headers and parts of a message, whatever shape it is.
///
/// Shared by the strict RFC 3156 path below and by the messages that carry
/// signed parts without being one themselves -- a signed attachment inside an
/// ordinary multipart/mixed, say. Those have headers worth reporting too, and
/// the walk verifies them either way.
auto FillMessageMeta(const vmime::shared_ptr<vmime::message>& message,
                     EMailMetaData& meta_data, QString& error_string) -> int {
  auto header = message->getHeader();

  // Walk the message for everything it carries. This used to enumerate the
  // attachments and then skip every one that was not an OpenPGP key, so a
  // signed message with a document attached showed the user nothing at all.
  if (ExtractParts(message, meta_data) != 0) {
    error_string = "Message structure exceeds the supported parsing limits";
    return kEML_FAILED;
  }

  QStringList public_keys_buffer;
  for (const auto& att : meta_data.attachments) {
    if (att.is_openpgp_key) public_keys_buffer.append(QString(att.data));
  }

  meta_data.from = ExtractFieldValueMailBox(header, vmime::fields::FROM);
  meta_data.to =
      ExtractFieldValueAddressListItems(header, vmime::fields::TO);
  meta_data.cc =
      ExtractFieldValueAddressListItems(header, vmime::fields::CC);
  meta_data.bcc_header =
      ExtractFieldValueAddressListItems(header, vmime::fields::BCC);
  meta_data.reply_to =
      ExtractFieldValueMailBox(header, vmime::fields::REPLY_TO);
  meta_data.organization =
      ExtractFieldValueText(header, vmime::fields::ORGANIZATION);
  meta_data.subject = ExtractFieldValueText(header, vmime::fields::SUBJECT);
  meta_data.datetime = ExtractFieldValueDateTime(header, vmime::fields::DATE);
  meta_data.public_keys = public_keys_buffer.join("\n");
  meta_data.mime = {};
  meta_data.signature = {};
  return kSUCCESS;
}

/// The RFC 3156 half of a verify: does this message's outermost structure hold
/// up, and what do its headers say. Split out of VerifyEMLMessage() so the
/// region walk below reads as the one thing it does.
///
/// No cryptography happens here. The refusals it returns are what the status
/// report shows when a message cannot be verified at all -- naming what is
/// wrong is worth more than silently reporting "unsigned".
auto ValidateSignedStructure(const QByteArray& data,
                             const vmime::shared_ptr<vmime::message>& message,
                             EMailMetaData& meta_data, QString& error_string)
    -> int {
  vmime::string vmime_data(data.constData(), data.size());

  auto header = message->getHeader();

  auto content_type_field =
      header->getField<vmime::contentTypeField>(vmime::fields::CONTENT_TYPE);
  if (!content_type_field) {
    error_string = "Cannot get 'Content-Type' Field from header";
    return kEML_FAILED;
  }

  auto content_type_value =
      Q_SC(content_type_field->getValue()->generate()).trimmed();

  auto prm_protocol = content_type_field->getParameter("protocol");
  if (!prm_protocol) {
    error_string = "Cannot get 'protocol' from 'Content-Type'";
    return kEML_FAILED;
  }

  /*
   * OpenPGP signed messages are denoted by the "multipart/signed" content
   * type.
   */
  if (content_type_value != "multipart/signed") {
    error_string =
        "OpenPGP signed messages are denoted by the 'multipart/signed' "
        "content type";
    return kEML_FAILED;
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
    return kEML_FAILED;
  }

  auto prm_micalg = content_type_field->getParameter("micalg");
  if (!prm_micalg) {
    error_string = "cannot get 'micalg' from 'Content-Type'";
    return kEML_FAILED;
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
    return kEML_FAILED;
  }

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
    return kEML_FAILED;
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

  // A digest WE compute over the exact bytes the signature covers, so the user
  // can confirm what was signed. It is not the signature's own hash algorithm:
  // that is what micalg declares and what the verify result actually reports.
  auto part_mime_content_hash = QCryptographicHash::hash(
      part_mime_content_text, QCryptographicHash::Sha256);
  FLOG_DEBUG("mime part of raw content hash: %1",
             part_mime_content_hash.toHex());

  FLOG_DEBUG("mime part of raw content: %1", Elide(part_mime_content_text));

  if (part_mime_content_text.isEmpty()) {
    error_string = "Mime raw data part is empty";
    return kEML_FAILED;
  }

  FLOG_DEBUG("mime part info, attachment count: %1",
             meta_data.attachments.size());

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
    return kEML_FAILED;
  }

  // DECODED, not generated. body::generate() emits the WIRE form -- for a
  // part carrying Content-Transfer-Encoding: base64, that is the base64 text
  // rather than the signature. Handing that to the engine makes a perfectly
  // good signature fail as bad data, and the user is shown a verification
  // failure that looks exactly like a forgery.
  auto part_sign_body_content = DecodePartContent(part_sign);
  if (part_sign_body_content.trimmed().isEmpty()) {
    error_string = "The signature part is empty";
    return kEML_FAILED;
  }

  FLOG_DEBUG("body part of signature content: %1",
             Elide(part_sign_body_content));

  // What only the RFC 3156 shape can say. The rest of the headers were filled
  // in before this ran, for every message shape alike.
  meta_data.micalg = prm_micalg_value;
  meta_data.signed_entity_digest = part_mime_content_hash.toHex();
  meta_data.signed_entity_digest_algo = "SHA-256";
  return kSUCCESS;
}

/// The engine's own analysis of one region, kept for the status report.
struct RegionReport {
  int region_id{-1};
  int status{0};
  QString detail;
  QString cards;
  QByteArray info_json;
};

/// Verifies one region on its own bytes and says what came back.
///
/// The bytes are handed over AS THEY STAND. Nothing here canonicalizes, pads
/// or repairs them: a signature covers exact octets, and a verifier that
/// quietly repaired them would report a good signature over a document that
/// does not have one.
auto VerifyOneRegion(int channel, const QByteArray& raw,
                     const QList<const EMailPart*>& flat,
                     const EMailSignatureRegion& region, const QString& from,
                     QList<EMailSignatureResult>& signatures,
                     RegionReport& report) -> EMailRegionVerdict {
  EMailRegionVerdict verdict;
  verdict.region_id = region.region_id;
  verdict.nesting_depth = region.nesting_depth;
  verdict.covers_ciphertext_only = region.covers_ciphertext_only;

  const auto unusable = [&verdict](const char* why) {
    // Recorded, not skipped. A region the walk could not hand to the engine
    // used to vanish from the results entirely, which reads to every consumer
    // as a message with one signature fewer -- an engine failure presented as
    // an absence.
    MimeLog(QString("signature region %1 cannot be verified: %2")
                .arg(verdict.region_id)
                .arg(QString::fromLatin1(why)));
    verdict.exec = EMailVerifyExec::kNO_RESULT;
    verdict.verdict = EMailBadgeState::kSIGNED_ERROR;
    return verdict;
  };

  if (region.raw_offset < 0 || region.raw_length <= 0 ||
      region.raw_offset + region.raw_length > raw.size()) {
    return unusable("its byte range is not inside the document");
  }
  if (region.signature_part_index < 0 ||
      region.signature_part_index >= flat.size()) {
    return unusable("it has no signature part beside it");
  }

  // A view, not a copy: nothing below mutates it, and the SDK copies what it
  // is given. Nested regions overlap, so copying each one made the work a
  // multiple of the message size for no benefit.
  const auto signed_bytes =
      QByteArray::fromRawData(raw.constData() + region.raw_offset,
                              static_cast<qsizetype>(region.raw_length));
  const auto signature_bytes = flat[region.signature_part_index]->data;
  if (signature_bytes.trimmed().isEmpty()) {
    return unusable("its signature part is empty");
  }

  // Decided here, on the exact slice the engine is about to be given, so that
  // no consumer has to scan the document again and reach a different answer.
  // It EXPLAINS a failure below; it never rescues one.
  verdict.signed_bytes_non_canonical = HasBareLineFeeds(signed_bytes);

  auto in = GFBuf::Copy(signed_bytes);
  auto sig = GFBuf::Copy(signature_bytes);
  GFGpgResult r;
  if (GFGpgVerify(channel, in.View(), sig.View(), r.Out()) != GF_GPG_OK) {
    // One region failing is not the walk failing: the others are still worth
    // verifying. Nothing to reclaim by hand -- r releases itself.
    verdict.exec = EMailVerifyExec::kENGINE_ERROR;
    verdict.verdict = EMailBadgeState::kSIGNED_ERROR;
    return verdict;
  }

  const auto err = r.Error();
  const auto capsule_id = r.CapsuleId();
  // No per-field reclaim here at all. This is the site where a successful
  // verification used to leak error_string once per signed region -- found by
  // ASan, not by inspection -- because correctness depended on remembering
  // every char* member. There is no longer a member to forget.

  // The structured form AND the report text, from the one call: a per-
  // signature view cannot be rebuilt by re-reading prose, and the capsule is
  // consumed here, so everything anyone needs has to come out of it now.
  const char* analyse = nullptr;
  const char* cards = nullptr;
  const char* info_json = nullptr;
  report.region_id = region.region_id;
  report.status = GFAnalyseVerifyResultInfoByCapsule(
      channel, err, (capsule_id).toUtf8().constData(), &analyse, &cards, &info_json);
  report.detail = UnStrDup(analyse);
  report.cards = UnStrDup(cards);
  report.info_json = UnStrDup(info_json).toUtf8();

  auto region_results = ParseSignatureResults(report.info_json, region.region_id);
  CheckMicalgAgreement(region_results, region);

  if (region_results.isEmpty()) {
    // The call came back and reported no signature at all. Not "unsigned":
    // this region carries a signature part, and something about it could not
    // be read.
    verdict.exec = EMailVerifyExec::kNO_RESULT;
    verdict.verdict = EMailBadgeState::kSIGNED_ERROR;
    return verdict;
  }

  // The worst signature decides the region. A region carrying one good
  // signature and one bad one is not a good region, and reporting the best of
  // them is how an attacker gets a second attempt.
  auto worst = EMailBadgeState::kSIGNED_GOOD;
  for (const auto& result : region_results) {
    worst = std::max(worst, BadgeForSignature(result, from));
  }

  verdict.exec = EMailVerifyExec::kOK;
  verdict.verdict = worst;
  signatures.append(region_results);
  return verdict;
}

}  // namespace

auto VerifyEMLMessage(int channel, const QByteArray& raw,
                      EMailVerificationResult& out, QString& error_string)
    -> int {
  out = {};

  // Taken over the bytes as given, before anything looks at them: this is what
  // ties the answer to the document it is an answer about.
  out.source_length = static_cast<qint64>(raw.size());
  out.source_sha256 = QCryptographicHash::hash(raw, QCryptographicHash::Sha256);

  auto message = vmime::make_shared<vmime::message>();
  try {
    vmime::string vmime_data(raw.constData(), raw.size());
    message->parse(vmime_data);
  } catch (const vmime::exception& e) {
    FLOG_DEBUG("error when parsing vmime data: %1", e.what());
    error_string = "Error when parsing eml raw data";
    return kEML_FAILED;
  }

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  if (ParseMimeTree(message, raw, root, regions) != 0) {
    error_string = "Message structure exceeds the supported parsing limits";
    return kEML_FAILED;
  }

  if (FillMessageMeta(message, out.meta, error_string) != kSUCCESS) {
    return kEML_FAILED;
  }

  // The strict reading applies to a message that CLAIMS to be one: it is what
  // produces the micalg, the digest of the signed entity, and the refusals
  // that name what is wrong with a malformed one.
  //
  // A message that makes no such claim is not refused out of hand, as long as
  // it carries signed regions somewhere inside it -- a signed part forwarded
  // inside an ordinary multipart/mixed is a real message, and verifying its
  // regions is exactly what this walk is for. Refusing those was the old
  // outermost-only verify's limitation, and unifying on it would have lost
  // every verdict the security surface used to show for them.
  const auto outermost_claims_signed = root.content_type == "multipart/signed";
  if (outermost_claims_signed || regions.isEmpty()) {
    auto ret = ValidateSignedStructure(raw, message, out.meta, error_string);
    if (ret != kSUCCESS) return ret;
  }

  const auto structure = ClassifyOpenPGPStructure(root, regions);
  out.regions = regions;

  const auto flat = FlattenMimeTree(root);
  QList<RegionReport> reports;
  int verified = 0;

  for (const auto& region : regions) {
    // Each region below costs a blocking GPG call, so the number of them is a
    // multiplier on how long this can be made to take. Stopping here leaves
    // the rest unreported rather than unverified-but-claimed.
    if (verified >= kMaxVerifiedRegions) {
      MimeLog(QString("stopping after %1 signature regions; %2 were offered")
                  .arg(kMaxVerifiedRegions)
                  .arg(regions.size()));
      break;
    }

    RegionReport report;
    out.verdicts.append(VerifyOneRegion(channel, raw, flat, region,
                                        out.meta.from, out.signatures, report));
    if (report.region_id >= 0) reports.append(report);
    ++verified;
  }

  out.overall = AggregateVerification(structure, regions, out.verdicts);
  out.state = out.verdicts.isEmpty() ? EMailVerifyState::kATTEMPTED_EMPTY
                                     : EMailVerifyState::kVERIFIED;

  // The message-level flag follows the regions that DECIDED the verdict. A
  // rewrite inside quoted content is worth reporting on that content, and is
  // not a reason to tell the user this message's own signed bytes were
  // touched.
  out.meta.signed_entity_non_canonical = false;
  for (const auto& verdict : out.verdicts) {
    if (verdict.covers_ciphertext_only) continue;
    if (!verdict.signed_bytes_non_canonical) continue;
    out.meta.signed_entity_non_canonical = true;
    break;
  }

  // The analysis the status report quotes comes from the region that decided
  // the verdict, so the report and the badge are describing the same
  // signature rather than two different ones.
  for (const auto& verdict : out.verdicts) {
    if (verdict.verdict != out.overall) continue;
    for (const auto& report : reports) {
      if (report.region_id != verdict.region_id) continue;
      out.report_status = report.status;
      out.report_detail = report.detail;
      out.report_cards = report.cards;
      out.report_info_json = report.info_json;
      break;
    }
    break;
  }
  if (out.report_detail.isEmpty() && !reports.isEmpty()) {
    out.report_status = reports.front().status;
    out.report_detail = reports.front().detail;
    out.report_cards = reports.front().cards;
    out.report_info_json = reports.front().info_json;
  }

  return kSUCCESS;
}

auto DecryptEMLData(int channel, const QByteArray& data,
                    EMailMetaData& meta_data, QByteArray& eml_data,
                    gpgme_error_t& err, QString& capsule_id) -> int {
  vmime::string vmime_data(data.constData(), data.size());
  auto message = vmime::make_shared<vmime::message>();
  try {
    message->parse(vmime_data);
  } catch (const vmime::exception& e) {
    FLOG_DEBUG("error when parsing vmime data: %1", e.what());
    eml_data = "Error when parsing EML Data";
    return kEML_FAILED;
  }

  auto header = message->getHeader();

  auto content_type_field =
      header->getField<vmime::contentTypeField>(vmime::fields::CONTENT_TYPE);
  if (!content_type_field) {
    eml_data = "cannot get 'Content-Type' Field from header";
    return kEML_FAILED;
  }

  auto content_type_value =
      Q_SC(content_type_field->getValue()->generate()).trimmed();

  auto prm_protocol = content_type_field->getParameter("protocol");
  if (!prm_protocol) {
    eml_data = "cannot get 'protocol' from 'Content-Type'";
    return kEML_FAILED;
  }

  /*
   * OpenPGP encrypted data is denoted by the "multipart/encrypted"
   * content type
   */
  if (content_type_value != "multipart/encrypted") {
    eml_data =
        "OpenPGP encrypted data is denoted by the 'multipart/encrypted' "
        "content type";
    return kEML_FAILED;
  }

  /*
   * MUST have a "protocol" parameter value of "application/pgp-encrypted"
   */
  auto prm_protocol_value = Q_SC(prm_protocol->getValue().generate());
  if (prm_protocol_value != "application/pgp-encrypted") {
    eml_data =
        "'protocol' parameter which MUST have a value of "
        "'application/pgp-encrypted' (MUST be quoted)";
    return kEML_FAILED;
  }

  auto from_field_value_text =
      ExtractFieldValueMailBox(header, vmime::fields::FROM);
  auto to_field_value_text =
      ExtractFieldValueAddressListItems(header, vmime::fields::TO);
  auto cc_field_value_text =
      ExtractFieldValueAddressListItems(header, vmime::fields::CC);
  auto bcc_field_value_text =
      ExtractFieldValueAddressListItems(header, vmime::fields::BCC);
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
    return kEML_FAILED;
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
    return kEML_FAILED;
  }

  part_mime_body_content->extractRaw(osa);
  osa.flush();

  auto part_mime_body_content_text = Q_SC(oss.str());
  FLOG_DEBUG("body part of raw content text: %1",
             Elide(part_mime_body_content_text));

  /*
   * A message complying with this
   * standard MUST contain a "Version: 1" field in this body.
   */
  if (!part_mime_body_content_text.contains("Version: 1")) {
    eml_data =
        "The first part MUST contain a 'Version: 1' field in this "
        "body.";
    return kEML_FAILED;
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
    return kEML_FAILED;
  }

  // DECODED, for the same reason as the signature part in VerifyEMLData():
  // body::generate() would hand the engine base64 text instead of the
  // ciphertext whenever the sender transfer-encoded this part.
  auto part_encr_body_content = DecodePartContent(part_sign);
  if (part_encr_body_content.trimmed().isEmpty()) {
    eml_data = "The second part is empty";
    return kEML_FAILED;
  }

  FLOG_DEBUG("body part of encrypt content: %1", Elide(part_encr_body_content));

  // The SDK sets *ps = nullptr and returns non-zero when it cannot
  // allocate the result, so both must be checked before the first
  // dereference below -- not after it, as this used to.
  auto in = GFBuf::Copy(part_encr_body_content);
  GFGpgResult r;
  if (GFGpgDecrypt(channel, in.View(), r.Out()) != GF_GPG_OK) {
    eml_data = SdkFailureText("Decrypt", r.ErrorString()).toUtf8();
    return kFAILED;
  }

  // The plaintext leaves wipeable memory here, which DataCopy() is named to
  // admit. It goes straight into the EML document the viewer renders, so a
  // Qt container is unavoidable at this boundary; the handle itself, and the
  // engine-side copy behind it, are still erased when r goes out of scope.
  eml_data = r.DataCopy();
  err = r.Error();
  capsule_id = r.CapsuleId();
  auto gpg_error_string = r.ErrorString();

  if (err != GPG_ERR_NO_ERROR) {
    eml_data = "Decrypt Failed: " + gpg_error_string.toUtf8();
    return kGPG_FAILED;
  }

  // The ciphertext was bounded on the way in; the plaintext is not bounded by
  // it. An OpenPGP compressed packet a few megabytes long expands to
  // gigabytes, so the expansion is refused here -- before it is handed on to
  // be parsed and copied -- rather than only at the parse.
  if (eml_data.size() > kMaxParseInputBytes) {
    MLogWarn("decrypted plaintext exceeds the parse ceiling; refusing it");
    eml_data.clear();
    eml_data.squeeze();
    eml_data = "Decrypted message is too large to display.";
    return kEML_FAILED;
  }

  // callback
  meta_data.from = from_field_value_text;
  meta_data.to = to_field_value_text;
  meta_data.cc = cc_field_value_text;
  meta_data.bcc_header = bcc_field_value_text;
  meta_data.reply_to = reply_to_field_value_text;
  meta_data.organization = organization_text;
  meta_data.subject = subject_field_value_text;
  meta_data.datetime = date_field_value;
  meta_data.encrypted_data = part_encr_body_content;

  // The plaintext is itself a MIME message, so walk it for the body and any
  // attachments. Nothing in the decrypted content reaches the user otherwise:
  // it used to be handed back as raw source and nothing more.
  vmime::shared_ptr<vmime::message> inner;
  if (CheckIfEMLMessage(eml_data, inner)) {
    if (ExtractParts(inner, meta_data) != 0) {
      MLogDebug("decrypted message exceeds the supported parsing limits");
    }
  }

  return kSUCCESS;
}
