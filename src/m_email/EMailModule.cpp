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

// vmime
#define VMIME_STATIC
#include <vmime/vmime.hpp>

// vmime extend
#include <vmime/contentTypeField.hpp>

#include "GFModuleCommonUtils.hpp"
#include "GFModuleDefine.h"

GF_MODULE_API_DEFINE_V2("com.bktus.gpgfrontend.module.email", "Email", "1.0.0",
                        "Everything related to E-Mails.", "Saturneric")

DEFINE_TRANSLATIONS_STRUCTURE(ModuleEMail);

auto inline Q_SC(const std::string& s) -> QString {
  return QString::fromStdString(s);
}

auto GFRegisterModule() -> int {
  MLogDebug("email module registering...");

  LISTEN("EMAIL_VERIFY_EML_DATA");

  return 0;
}

auto GFActiveModule() -> int { return 0; }

REGISTER_EVENT_HANDLER(EMAIL_VERIFY_EML_DATA, [](const MEvent& event) -> int {
  if (event["eml_data"].isEmpty()) CB_ERR(event, -1, "eml_data is empty");

  auto data = QByteArray::fromBase64(QString(event["eml_data"]).toLatin1());
  auto hash = QCryptographicHash::hash(data, QCryptographicHash::Sha1);
  FLOG_DEBUG("E-Mail Raw Data SHA-1 Hash: %1", hash.toHex());

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

  auto content_type_value =
      Q_SC(content_type_field->getValue()->generate()).trimmed();

  /*
   * OpenPGP signed messages are denoted by the "multipart/signed" content type.
   */
  if (content_type_value != "multipart/signed")
    CB_ERR(event, -2, "Content-Type must be multipart/signed");

  auto prm_protocol = content_type_field->getParameter("protocol");
  auto prm_protocol_value = Q_SC(prm_protocol->getValue().generate());

  /*
   * with a "protocol" parameter which MUST have a value of
   * "application/pgp-signature"
   */
  if (prm_protocol_value != "application/pgp-signature")
    CB_ERR(event, -2,
           "protocol of Content-Type MUST be application/pgp-signature");

  auto body = message->getBody();
  auto content_type = body->getContentType();
  auto part_count = body->getPartCount();

  FLOG_DEBUG("body content type: %1", content_type.generate());
  FLOG_DEBUG("body page count: %1", part_count);

  /*
   * The multipart/signed body MUST consist of exactly two parts.
   */
  if (part_count != 2)
    CB_ERR(event, -2, "body MUST consist of exactly two parts");

  /*
    The first part contains the signed data in MIME canonical format,
    including a set of appropriate content headers describing the data.
  */
  auto part_mime = body->getPartAt(0);
  auto part_mime_parse_offset = part_mime->getParsedOffset();
  auto part_mime_parse_length = part_mime->getParsedLength();

  auto part_mime_content_text = QByteArray::fromStdString(
      vmime_data.substr(part_mime_parse_offset, part_mime_parse_length));

  FLOG_DEBUG("body part of raw offset: %1, length: %2", part_mime_parse_offset,
             part_mime_parse_length);
  FLOG_DEBUG("body part of raw content left: %1",
             part_mime_content_text.left(64));
  FLOG_DEBUG("body part of raw content right: %1",
             part_mime_content_text.right(64));

  auto part_mime_content_hash = QCryptographicHash::hash(
      part_mime_content_text, QCryptographicHash::Sha1);
  FLOG_DEBUG("body part of raw content hash: %1",
             part_mime_content_hash.toHex());

  if (part_mime_content_text.isEmpty())
    CB_ERR(event, -2, "mime raw data part is empty");

  /*
   * The second body MUST contain the OpenPGP digital signature.  It MUST
   * be labeled with a content type of "application/pgp-signature"
   */
  auto part_sign = body->getPartAt(1);
  auto part_sign_header = part_sign->getHeader();
  auto part_sign_content_type = part_sign_header->ContentType();
  auto part_sign_content_type_value =
      Q_SC(part_sign_content_type->getValue()->generate());

  if (part_sign_content_type_value != "application/pgp-signature")
    CB_ERR(
        event, -2,
        "signature part must have a Content-Type of application/pgp-signature");

  auto part_sign_body_content =
      QByteArray::fromStdString(part_sign->getBody()->generate());
  if (part_sign_body_content.trimmed().isEmpty())
    CB_ERR(event, -2, "signature part is empty");

  FLOG_DEBUG("body part of signature content: %1", part_sign_body_content);

  // callback
  CB(event, GFGetModuleID(),
     {
         {"ret", QString::number(0)},
         {"mime", QString::fromLatin1(part_mime_content_text.toBase64())},
         {"signature", QString::fromLatin1(part_sign_body_content.toBase64())},
     });

  return 0;
});

auto GFDeactivateModule() -> int { return 0; }

auto GFUnregisterModule() -> int {
  MLogDebug("email module unregistering...");

  return 0;
}
