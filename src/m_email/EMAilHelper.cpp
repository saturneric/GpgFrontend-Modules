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

#include "EMailHelper.h"

#include <QRegularExpression>

#include "GFModuleCommonUtils.hpp"

static const QRegularExpression kNameEmailStringRegex{
    R"(^\s*(.*)\s*<\s*([^<>@\s]+@[^<>@\s]+)\s*>\s*$)"};

auto IsValidMicalgFormat(const QString& prm_micalg_value) -> bool {
  QRegularExpression regex("^pgp-(\\w+)$");
  QRegularExpressionMatch match = regex.match(prm_micalg_value);
  return match.hasMatch();
}

auto FormatMailBox(const std::shared_ptr<vmime::mailbox>& m) -> QString {
  if (!m) return {"Unknown"};

  QString name = Q_SC(m->getName().getConvertedText(vmime::charsets::UTF_8));
  QString address =
      Q_SC(m->getEmail().toText().getConvertedText(vmime::charsets::UTF_8));

  if (!name.isEmpty()) {
    return QString("%1 <%2>").arg(name, address);
  }
  return address;
}

auto ExtractFieldValue(const vmime::shared_ptr<vmime::header>& header,
                       const QString& field_name) -> QString {
  auto field = header->getField(field_name.toStdString());
  if (!field) {
    FLOG_WARN("cannot get '%1' Field from header", field_name);
    return {};
  }

  auto field_value = field->getValue();
  if (!field_value) {
    FLOG_WARN("cannot get '%1' Field Value from header", field_name);
    return {};
  }

  return Q_SC(field_value->generate());
}

auto ExtractFieldValueMailBox(const vmime::shared_ptr<vmime::header>& header,
                              const QString& field_name) -> QString {
  auto field = header->getField(field_name.toStdString());
  if (!field) {
    FLOG_WARN("cannot get '%1' Field from header", field_name);
    return {};
  }

  auto field_value = field->getValue<vmime::mailbox>();
  if (!field_value) {
    FLOG_WARN("cannot get '%1' Field Value from header", field_name);
    return {};
  }

  return FormatMailBox(field_value);
}

auto ExtractFieldValueAddressList(
    const vmime::shared_ptr<vmime::header>& header,
    const QString& field_name) -> QString {
  auto field = header->getField(field_name.toStdString());
  if (!field) {
    FLOG_WARN("cannot get '%1' Field from header", field_name);
    return {};
  }

  auto field_value = field->getValue<vmime::addressList>();
  if (!field_value) {
    FLOG_WARN("cannot get '%1' Field Value from header", field_name);
    return {};
  }

  QStringList mailbox_list_strings;
  for (const auto& mailbox : field_value->toMailboxList()->getMailboxList()) {
    QString formatted_mailbox = FormatMailBox(mailbox);
    mailbox_list_strings.append(formatted_mailbox);
  }

  return mailbox_list_strings.join(", ");
}

auto ExtractFieldValueText(const vmime::shared_ptr<vmime::header>& header,
                           const QString& field_name) -> QString {
  if (!header->hasField(field_name.toStdString())) return {};

  auto field = header->getField(field_name.toStdString());
  if (!field) {
    FLOG_WARN("cannot get '%1' Field from header", field_name);
    return {};
  }

  auto field_value = field->getValue<vmime::text>();
  if (!field_value) {
    FLOG_WARN("cannot get '%1' Field Value from header", field_name);
    return {};
  }

  return Q_SC(field_value->getConvertedText(vmime::charsets::UTF_8));
}

auto ExtractFieldValueDateTime(const vmime::shared_ptr<vmime::header>& header,
                               const QString& field_name) -> QDateTime {
  auto field = header->getField(field_name.toStdString());
  if (!field) {
    FLOG_WARN("cannot get '%1' Field from header", field_name);
    return {};
  }

  auto field_value = field->getValue<vmime::datetime>();
  if (!field_value) {
    FLOG_WARN("cannot get '%1' Field Value from header", field_name);
    return {};
  }

  QDate date(field_value->getYear(), field_value->getMonth(),
             field_value->getDay());
  QTime time(field_value->getHour(), field_value->getMinute(),
             field_value->getSecond());
  field_value->getZone();

  auto zone = field_value->getZone();
  QDateTime datetime(date, time);
  datetime.setOffsetFromUtc(zone * 60);

  return datetime;
}

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

auto EncodeBase64WithLineBreaks(const QByteArray& data,
                                int line_length) -> QString {
  // Get the Base64 encoded data
  QByteArray base64_data = data.toBase64();

  // Split the base64 data into lines of the given line length
  QStringList lines;
  for (int i = 0; i < base64_data.size(); i += line_length) {
    lines.append(base64_data.mid(i, line_length));
  }

  // Join lines with CRLF
  return lines.join("\r\n");
}

auto CheckIfEMLMessage(const QByteArray& data,
                       vmime::shared_ptr<vmime::message>& message) -> bool {
  vmime::string vmime_data(data.constData(), data.size());

  message = vmime::make_shared<vmime::message>();
  try {
    message->parse(vmime_data);
    return message->getParsedLength() != 0 && !message->getHeader()->isEmpty();
  } catch (const vmime::exception& e) {
    FLOG_DEBUG("error occurred when parsing vmime data: %1", e.what());
    return false;
  }
}

auto BuildPlainTextEML(const EMailMetaData& meta_data,
                       const QByteArray& body_data, QString& eml_data) -> int {
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
    mime_part_body_content->setData(body_data.toStdString());
    plaintext_msg_body->setContents(mime_part_body_content);

    eml_data =
        Q_SC(plaintext_msg->generate(vmime::lineLengthLimits::convenient));
    return 0;

  } catch (const vmime::exception& e) {
    eml_data = QString("VMIME Error: %1").arg(e.what());
    return -1;
  }
}

auto GetMetaData(QByteArray& data, EMailMetaData& meta_data) {}

auto GetEMLMetaData(vmime::shared_ptr<vmime::message>& message,
                    EMailMetaData& meta_data) -> int {
  auto header = message->getHeader();

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

  meta_data.from = from_field_value_text;
  meta_data.to = to_field_value_text.split(',');
  meta_data.cc = cc_field_value_text.split(',');
  meta_data.bcc = bcc_field_value_text.split(',');
  meta_data.subject = subject_field_value_text;
  return 0;
}