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

#include "EMailModule.h"

#include <GFSDKBasic.h>
#include <GFSDKBuildInfo.h>
#include <GFSDKLog.h>

// qt
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonDocument>
#include <QString>

#include "EMailMetaDataDialog.h"

// vmime
#define VMIME_STATIC
#include <vmime/vmime.hpp>

// vmime extend
#include <vmime/contentTypeField.hpp>

#include "GFModuleCommonUtils.hpp"
#include "GFModuleDefine.h"

//
#include "EMailHelper.h"
#include "EMailMetaDataDialog.h"

GF_MODULE_API_DEFINE_V2("com.bktus.gpgfrontend.module.email", "Email", "1.0.0",
                        "Everything related to E-Mails.", "Saturneric")

DEFINE_TRANSLATIONS_STRUCTURE(ModuleEMail);

auto GFRegisterModule() -> int {
  MLogDebug("email module registering...");

  LISTEN("EMAIL_VERIFY_EML_DATA");
  LISTEN("EMAIL_DECRYPT_EML_DATA");
  LISTEN("EMAIL_EXPORT_EML_DATA");
  return 0;
}

auto GFActiveModule() -> int { return 0; }

REGISTER_EVENT_HANDLER(EMAIL_VERIFY_EML_DATA, [](const MEvent& event) -> int {
  if (event["eml_data"].isEmpty()) CB_ERR(event, -1, "eml_data is empty");

  auto data = QByteArray::fromBase64(QString(event["eml_data"]).toLatin1());
  vmime::string vmime_data(data.constData(), data.size());

  auto message = vmime::make_shared<vmime::message>();
  try {
    message->parse(vmime_data);
  } catch (const vmime::exception& e) {
    FLOG_DEBUG("error when parsing vmime data: %1", e.what());
    CB_ERR(event, -2, "Error when parsing eml raw data");
  }

  auto header = message->getHeader();

  auto content_type_field =
      header->getField<vmime::contentTypeField>(vmime::fields::CONTENT_TYPE);
  if (!content_type_field) {
    CB_ERR(event, -2, "Cannot get 'Content-Type' Field from header");
  }

  auto content_type_value =
      Q_SC(content_type_field->getValue()->generate()).trimmed();

  auto prm_protocol = content_type_field->getParameter("protocol");
  if (!prm_protocol) {
    CB_ERR(event, -2, "Cannot get 'protocol' from 'Content-Type'");
  }

  /*
   * OpenPGP signed messages are denoted by the "multipart/signed" content
   * type.
   */
  if (content_type_value != "multipart/signed")
    CB_ERR(event, -2,
           "OpenPGP signed messages are denoted by the 'multipart/signed' "
           "content type");

  /*
   * with a "protocol" parameter which MUST have a value of
   * "application/pgp-signature"
   */
  auto prm_protocol_value = Q_SC(prm_protocol->getValue().generate());
  if (prm_protocol_value != "application/pgp-signature")
    CB_ERR(event, -2,
           "The 'protocol' parameter which MUST have a value of "
           "'application/pgp-signature' (MUST be quoted)");

  auto prm_micalg = content_type_field->getParameter("micalg");
  if (!prm_micalg) {
    CB_ERR(event, -2, "cannot get 'micalg' from 'Content-Type'");
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
    CB_ERR(event, -2,
           "The 'micalg' parameter MUST contain exactly one hash-symbol of the "
           "format 'pgp-<hash-identifier>'");
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
  if (part_count != 2)
    CB_ERR(event, -2,
           "The multipart/signed body MUST consist of exactly two parts");

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

  if (part_mime_content_text.isEmpty())
    CB_ERR(event, -2, "mime raw data part is empty");

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

  if (part_sign_content_type_value != "application/pgp-signature")
    CB_ERR(event, -2,
           "The second body MUST be labeled with a content type of "
           "'application/pgp-signature'");

  auto part_sign_body_content =
      QByteArray::fromStdString(part_sign->getBody()->generate());
  if (part_sign_body_content.trimmed().isEmpty())
    CB_ERR(event, -2, "The signature part is empty");

  FLOG_DEBUG("body part of signature content: %1", part_sign_body_content);

  // callback
  CB(event, GFGetModuleID(),
     {
         {"ret", QString::number(0)},
         {"mime", QString::fromLatin1(part_mime_content_text.toBase64())},
         {"mime_hash", part_mime_content_hash.toHex()},
         {"signature", QString::fromLatin1(part_sign_body_content.toBase64())},
         {"from", from_field_value_text},
         {"to", to_field_value_text},
         {"cc", cc_field_value_text},
         {"bcc", bcc_field_value_text},
         {"subject", subject_field_value_text},
         {"datetime", QString::number(date_field_value.toMSecsSinceEpoch())},
         {"micalg", prm_micalg_value},
         {"public_keys", public_keys_buffer.join("\n")},
     });

  return 0;
});

REGISTER_EVENT_HANDLER(EMAIL_DECRYPT_EML_DATA, [](const MEvent& event) -> int {
  if (event["eml_data"].isEmpty()) CB_ERR(event, -1, "eml_data is empty");

  auto data = QByteArray::fromBase64(QString(event["eml_data"]).toLatin1());
  vmime::string vmime_data(data.constData(), data.size());

  auto message = vmime::make_shared<vmime::message>();
  try {
    message->parse(vmime_data);
  } catch (const vmime::exception& e) {
    FLOG_DEBUG("error when parsing vmime data: %1", e.what());
    CB_ERR(event, -2, "error when parsing vmime data");
  }

  auto header = message->getHeader();

  auto content_type_field =
      header->getField<vmime::contentTypeField>(vmime::fields::CONTENT_TYPE);
  if (!content_type_field) {
    CB_ERR(event, -2, "cannot get 'Content-Type' Field from header");
  }

  auto content_type_value =
      Q_SC(content_type_field->getValue()->generate()).trimmed();

  auto prm_protocol = content_type_field->getParameter("protocol");
  if (!prm_protocol) {
    CB_ERR(event, -2, "cannot get 'protocol' from 'Content-Type'");
  }

  /*
   * OpenPGP encrypted data is denoted by the "multipart/encrypted"
   * content type
   */
  if (content_type_value != "multipart/encrypted")
    CB_ERR(event, -2,
           "OpenPGP encrypted data is denoted by the 'multipart/encrypted' "
           "content type");

  /*
   * MUST have a "protocol" parameter value of "application/pgp-encrypted"
   */
  auto prm_protocol_value = Q_SC(prm_protocol->getValue().generate());
  if (prm_protocol_value != "application/pgp-encrypted")
    CB_ERR(event, -2,
           "'protocol' parameter which MUST have a value of "
           "'application/pgp-encrypted' (MUST be quoted)");

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
  if (part_count != 2)
    CB_ERR(event, -2,
           "The multipart/signed body MUST consist of exactly two parts");

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
    CB_ERR(event, -2, "Cannot get the content of the first part's body");
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
    CB_ERR(event, -2,
           "The first part MUST contain a 'Version: 1' field in this body.");
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

  if (part_sign_content_type_value != "application/octet-stream")
    CB_ERR(event, -2,
           "The second part MUST be labeled with a content type of "
           "'application/octet-stream'");

  auto part_encr_body_content =
      QByteArray::fromStdString(part_sign->getBody()->generate());
  if (part_encr_body_content.trimmed().isEmpty())
    CB_ERR(event, -2, "The second part is empty");

  FLOG_DEBUG("body part of encrypt content: %1", part_encr_body_content);

  // callback
  CB(event, GFGetModuleID(),
     {
         {"ret", QString::number(0)},
         {"encrypted", QString::fromLatin1(part_encr_body_content.toBase64())},
         {"from", from_field_value_text},
         {"to", to_field_value_text},
         {"cc", cc_field_value_text},
         {"bcc", bcc_field_value_text},
         {"subject", subject_field_value_text},
         {"datetime", QString::number(date_field_value.toMSecsSinceEpoch())},
     });

  return 0;
});

REGISTER_EVENT_HANDLER(EMAIL_EXPORT_EML_DATA, [](const MEvent& event) -> int {
  if (event["body_data"].isEmpty()) CB_ERR(event, -1, "body_data is empty");
  if (event["channel"].isEmpty()) CB_ERR(event, -1, "channel is empty");
  if (event["sign_key"].isEmpty()) CB_ERR(event, -1, "sign_key is empty");

  auto channel = event.value("channel", "0").toInt();
  auto sign_key = event.value("sign_key", "");

  FLOG_DEBUG("eml sign key: %1", sign_key);

  auto data = QByteArray::fromBase64(QString(event["body_data"]).toLatin1());

  auto* dialog = GUI_OBJECT(CreateEMailMetaDataDialog, {data});
  auto* r_dialog =
      qobject_cast<EMailMetaDataDialog*>(static_cast<QObject*>(dialog));
  if (r_dialog == nullptr)
    CB_ERR(event, -1, "convert dialog to r_dialog failed");
  r_dialog->SetChannel(channel);
  r_dialog->SetSignKey(sign_key);

  GFUIShowDialog(dialog, nullptr);

  QObject::connect(r_dialog, &EMailMetaDataDialog::SignalEMLDataGenerateSuccess,
                   r_dialog, [=](QString eml_data) {
                     // callback
                     CB(event, GFGetModuleID(),
                        {
                            {"ret", QString::number(0)},
                            {"eml_data", eml_data},
                        });
                   });

  QObject::connect(r_dialog, &EMailMetaDataDialog::SignalEMLDataGenerateFailed,
                   r_dialog, [=](QString error) {
                     // callback
                     CB_ERR(event, -1, "Generate EML Data Failed: " + error);
                   });

  return 0;
});
auto GFDeactivateModule() -> int { return 0; }

auto GFUnregisterModule() -> int {
  MLogDebug("email module unregistering...");

  return 0;
}
