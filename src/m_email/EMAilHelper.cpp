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
#include <QTimeZone>

namespace {
MimeLogFn g_mime_log_sink = nullptr;
}  // namespace

void SetMimeLogSink(MimeLogFn fn) { g_mime_log_sink = fn; }

void MimeLog(const QString& message) {
  if (g_mime_log_sink != nullptr) g_mime_log_sink(message);
}

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

auto TryGetField(const vmime::shared_ptr<vmime::header>& header,
                 const QString& field_name)
    -> vmime::shared_ptr<vmime::headerField> {
  // Both header::getField() overloads create and insert the field when it is
  // absent, so reading an optional header through them silently adds an empty
  // one to the message -- which then gets signed, or sent. Ask first.
  const auto name = field_name.toStdString();
  if (!header->hasField(name)) return nullptr;
  return header->getField(name);
}

auto ExtractFieldValue(const vmime::shared_ptr<vmime::header>& header,
                       const QString& field_name) -> QString {
  auto field = TryGetField(header, field_name);
  if (!field) {
    MimeLog(QString("cannot get '%1' field from header").arg(field_name));
    return {};
  }

  auto field_value = field->getValue();
  if (!field_value) {
    MimeLog(QString("cannot get '%1' field value from header").arg(field_name));
    return {};
  }

  return Q_SC(field_value->generate());
}

auto ExtractFieldValueMailBox(const vmime::shared_ptr<vmime::header>& header,
                              const QString& field_name) -> QString {
  auto field = TryGetField(header, field_name);
  if (!field) {
    MimeLog(QString("cannot get '%1' field from header").arg(field_name));
    return {};
  }

  auto field_value = field->getValue<vmime::mailbox>();
  if (!field_value) {
    MimeLog(QString("cannot get '%1' field value from header").arg(field_name));
    return {};
  }

  return FormatMailBox(field_value);
}

auto ExtractFieldValueAddressList(
    const vmime::shared_ptr<vmime::header>& header, const QString& field_name)
    -> QString {
  auto field = TryGetField(header, field_name);
  if (!field) {
    MimeLog(QString("cannot get '%1' field from header").arg(field_name));
    return {};
  }

  auto field_value = field->getValue<vmime::addressList>();
  if (!field_value) {
    MimeLog(QString("cannot get '%1' field value from header").arg(field_name));
    return {};
  }

  return ExtractFieldValueAddressListItems(header, field_name).join(", ");
}

auto ExtractFieldValueAddressListItems(
    const vmime::shared_ptr<vmime::header>& header, const QString& field_name)
    -> QStringList {
  auto field = TryGetField(header, field_name);
  if (!field) return {};

  auto field_value = field->getValue<vmime::addressList>();
  if (!field_value) return {};

  // Returned as a list rather than a joined string: a display name may contain
  // a comma ("Doe, John"), and splitting a joined list back apart on "," tears
  // such a name in half and invents a recipient that was never addressed.
  QStringList mailboxes;
  for (const auto& mailbox : field_value->toMailboxList()->getMailboxList()) {
    mailboxes.append(FormatMailBox(mailbox));
  }
  return mailboxes;
}

auto ExtractFieldValueText(const vmime::shared_ptr<vmime::header>& header,
                           const QString& field_name) -> QString {
  if (!header->hasField(field_name.toStdString())) return {};

  auto field = TryGetField(header, field_name);
  if (!field) {
    MimeLog(QString("cannot get '%1' field from header").arg(field_name));
    return {};
  }

  auto field_value = field->getValue<vmime::text>();
  if (!field_value) {
    MimeLog(QString("cannot get '%1' field value from header").arg(field_name));
    return {};
  }

  return Q_SC(field_value->getConvertedText(vmime::charsets::UTF_8));
}

auto ExtractFieldValueDateTime(const vmime::shared_ptr<vmime::header>& header,
                               const QString& field_name) -> QDateTime {
  auto field = TryGetField(header, field_name);
  if (!field) {
    MimeLog(QString("cannot get '%1' field from header").arg(field_name));
    return {};
  }

  auto field_value = field->getValue<vmime::datetime>();
  if (!field_value) {
    MimeLog(QString("cannot get '%1' field value from header").arg(field_name));
    return {};
  }

  QDate date(field_value->getYear(), field_value->getMonth(),
             field_value->getDay());
  QTime time(field_value->getHour(), field_value->getMinute(),
             field_value->getSecond());
  field_value->getZone();

  auto zone = field_value->getZone();
  QDateTime datetime(date, time);

  int offset_sec = zone * 60;

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  QTimeZone tz(offset_sec);
  datetime.setTimeZone(tz);
#else
  datetime.setOffsetFromUtc(offset_sec);
#endif

  return datetime;
}

auto ParseEmailString(const QString& input, QString& name, QString& email)
    -> bool {
  QRegularExpressionMatch match = kNameEmailStringRegex.match(input);

  if (match.hasMatch()) {
    name = match.captured(1).trimmed();
    email = match.captured(2).trimmed();
    return true;
  }

  return false;
}

auto EncodeBase64WithLineBreaks(const QByteArray& data, int line_length)
    -> QString {
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
    MimeLog(QString("error parsing vmime data: %1").arg(e.what()));
    return false;
  }
}

auto BuildMimeEML(const EMailMetaData& meta_data, const QByteArray& body_data,
                  const QList<EMailAttachment>& attachments, QString& eml_data)
    -> int {
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
          vmime::mailbox(Q_TEXT(name), email.toStdString()));
    } else {
      plaintext_msg_builder.setExpeditor(vmime::mailbox(from.toStdString()));
    }

    for (const QString& recipient : recipient_list) {
      auto trimmed_recipient = recipient.trimmed();
      if (ParseEmailString(trimmed_recipient, name, email)) {
        plaintext_msg_builder.getRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(Q_TEXT(name),
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
            vmime::make_shared<vmime::mailbox>(Q_TEXT(name),
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
            vmime::make_shared<vmime::mailbox>(Q_TEXT(name),
                                               email.toStdString()));
      } else {
        plaintext_msg_builder.getBlindCopyRecipients().appendAddress(
            vmime::make_shared<vmime::mailbox>(
                trimmed_recipient.toStdString()));
      }
    }

    plaintext_msg_builder.setSubject(Q_TEXT(subject));

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

    // With nothing attached the message stays exactly what it has always been:
    // a single text/plain part. Attachments turn it into multipart/mixed, with
    // that same text part as the first child.
    for (const auto& att : attachments) {
      auto content = vmime::make_shared<vmime::stringContentHandler>();
      content->setData(std::string(att.data.constData(),
                                   static_cast<size_t>(att.data.size())));

      const auto name =
          att.filename.isEmpty() ? QString("attachment") : att.filename;
      const auto type = att.mime_type.isEmpty()
                            ? QString("application/octet-stream")
                            : att.mime_type;

      auto attachment = vmime::make_shared<vmime::defaultAttachment>(
          content, vmime::encoding("base64"),
          vmime::mediaType(type.toStdString()),
          vmime::text(att.description.toStdString(), vmime::charsets::UTF_8));

      vmime::attachmentHelper::addAttachment(plaintext_msg, attachment);

      // addAttachment does not carry a filename of its own, so set it on the
      // part it just appended. Without this the recipient sees a nameless blob.
      auto body = plaintext_msg->getBody();
      auto part = body->getPartAt(body->getPartCount() - 1);
      auto disposition =
          part->getHeader()->getField<vmime::contentDispositionField>(
              vmime::fields::CONTENT_DISPOSITION);
      disposition->setValue("attachment");
      disposition->setFilename(
          vmime::word(name.toStdString(), vmime::charsets::UTF_8));
    }

    eml_data =
        Q_SC(plaintext_msg->generate(vmime::lineLengthLimits::convenient));
    return 0;

  } catch (const vmime::exception& e) {
    eml_data = QString("VMIME Error: %1").arg(e.what());
    return -1;
  }
}

auto BuildPlainTextEML(const EMailMetaData& meta_data,
                       const QByteArray& body_data, QString& eml_data) -> int {
  return BuildMimeEML(meta_data, body_data, {}, eml_data);
}

auto GetMetaData(QByteArray& data, EMailMetaData& meta_data) {}

auto GetEMLMetaData(vmime::shared_ptr<vmime::message>& message,
                    EMailMetaData& meta_data) -> int {
  auto header = message->getHeader();

  meta_data.from = ExtractFieldValueMailBox(header, vmime::fields::FROM);
  meta_data.to = ExtractFieldValueAddressListItems(header, vmime::fields::TO);
  meta_data.cc = ExtractFieldValueAddressListItems(header, vmime::fields::CC);
  meta_data.bcc = ExtractFieldValueAddressListItems(header, vmime::fields::BCC);
  meta_data.subject = ExtractFieldValueText(header, vmime::fields::SUBJECT);
  meta_data.datetime = ExtractFieldValueDateTime(header, vmime::fields::DATE);
  meta_data.reply_to =
      ExtractFieldValueMailBox(header, vmime::fields::REPLY_TO);
  meta_data.organization =
      ExtractFieldValueText(header, vmime::fields::ORGANIZATION);

  return 0;
}
namespace {

// Windows refuses these as filenames whatever extension follows, and a file
// named after one can be a nuisance on other systems too.
const QStringList kReservedDeviceNames = {
    "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4",
    "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
    "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};

// Long enough for real filenames, short enough to leave room for a directory
// and a " (12)" suffix inside the usual 255-byte limit.
constexpr int kMaxFileNameLength = 180;

auto ExtensionForMimeType(const QString& mime_type) -> QString {
  static const QMap<QString, QString> kMap = {
      {"text/plain", "txt"},           {"text/html", "html"},
      {"application/pdf", "pdf"},      {"image/png", "png"},
      {"image/jpeg", "jpg"},           {"image/gif", "gif"},
      {"application/pgp-keys", "asc"}, {"application/zip", "zip"},
      {"message/rfc822", "eml"}};
  return kMap.value(mime_type.trimmed().toLower());
}

// Splits "name.ext" so a truncation or a " (2)" can be applied to the stem
// without destroying the extension.
auto SplitExtension(const QString& name) -> QPair<QString, QString> {
  const auto dot = name.lastIndexOf('.');
  if (dot <= 0 || dot == name.size() - 1) return {name, {}};
  return {name.left(dot), name.mid(dot)};
}

}  // namespace

auto SanitizeAttachmentFileName(const QString& raw, const QString& mime_type)
    -> QString {
  QString name = raw;

  // Take only the last component, then drop any separator that survives. A
  // sender controls this string, so "../../etc/passwd" and "C:\\evil" both have
  // to come out as an ordinary name in the directory the user picked.
  name.replace('\\', '/');
  const auto slash = name.lastIndexOf('/');
  if (slash >= 0) name = name.mid(slash + 1);

  QString cleaned;
  cleaned.reserve(name.size());
  for (const auto ch : name) {
    // Control characters, NUL and the characters Windows forbids outright.
    if (ch.unicode() < 0x20 || ch == QChar(0x7F)) continue;
    if (ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' ||
        ch == '>' || ch == '|' || ch == '/' || ch == '\\') {
      continue;
    }
    cleaned.append(ch);
  }

  // A name of nothing but dots and spaces (".", "..", "...") is not a name.
  while (cleaned.startsWith('.') || cleaned.startsWith(' '))
    cleaned.remove(0, 1);
  while (cleaned.endsWith('.') || cleaned.endsWith(' ')) cleaned.chop(1);
  cleaned = cleaned.trimmed();

  if (cleaned.isEmpty()) {
    const auto ext = ExtensionForMimeType(mime_type);
    cleaned = ext.isEmpty() ? QString("attachment")
                            : QString("attachment.%1").arg(ext);
  }

  auto [stem, ext] = SplitExtension(cleaned);

  // Reserved names are rejected on their stem, so "NUL.txt" is caught too.
  if (kReservedDeviceNames.contains(stem.toUpper())) {
    stem = QString("%1_").arg(stem);
  }

  if (stem.size() + ext.size() > kMaxFileNameLength) {
    stem = stem.left(qMax(1, kMaxFileNameLength - ext.size()));
  }

  return stem + ext;
}

auto UniqueAttachmentFileNames(const QList<EMailAttachment>& attachments,
                               const QStringList& taken) -> QStringList {
  QSet<QString> used;
  for (const auto& t : taken) used.insert(t.toLower());

  QStringList names;
  names.reserve(attachments.size());

  for (const auto& att : attachments) {
    const auto base = SanitizeAttachmentFileName(att.filename, att.mime_type);
    auto [stem, ext] = SplitExtension(base);

    auto candidate = base;
    int counter = 2;
    while (used.contains(candidate.toLower())) {
      candidate = QString("%1 (%2)%3").arg(stem).arg(counter++).arg(ext);
    }

    used.insert(candidate.toLower());
    names.append(candidate);
  }

  return names;
}

namespace {

auto PartContentType(const vmime::shared_ptr<const vmime::bodyPart>& part)
    -> QString {
  auto field = part->getHeader()->findField(vmime::fields::CONTENT_TYPE);
  if (!field) return "text/plain";  // the RFC 2045 default
  auto value = field->getValue<vmime::mediaType>();
  if (!value) return "text/plain";
  return Q_SC(value->generate()).trimmed().toLower();
}

auto PartDisposition(const vmime::shared_ptr<const vmime::bodyPart>& part)
    -> QString {
  auto field = part->getHeader()->findField(vmime::fields::CONTENT_DISPOSITION);
  if (!field) return {};
  auto value = field->getValue<vmime::contentDisposition>();
  if (!value) return {};
  return Q_SC(value->generate()).trimmed().toLower();
}

auto PartFileName(const vmime::shared_ptr<const vmime::bodyPart>& part)
    -> QString {
  // Content-Disposition filename= wins, with Content-Type name= as the older
  // fallback some senders still use.
  if (auto field =
          part->getHeader()->findField(vmime::fields::CONTENT_DISPOSITION)) {
    auto disp = vmime::dynamicCast<const vmime::contentDispositionField>(field);
    if (disp && disp->hasParameter("filename")) {
      return Q_SC(disp->getFilename().getBuffer());
    }
  }

  if (auto field = part->getHeader()->findField(vmime::fields::CONTENT_TYPE)) {
    auto ct = vmime::dynamicCast<const vmime::contentTypeField>(field);
    // findParameter, not getParameter: the latter is non-const and would
    // insert the parameter when it is missing.
    if (ct) {
      if (auto name = ct->findParameter("name")) {
        return Q_SC(name->getValue().getBuffer());
      }
    }
  }

  return {};
}

auto DecodePart(const vmime::shared_ptr<const vmime::bodyPart>& part)
    -> QByteArray {
  auto contents = part->getBody()->getContents();
  if (!contents) return {};

  std::ostringstream oss;
  vmime::utility::outputStreamAdapter osa(oss);
  contents->extract(osa);
  osa.flush();
  return QByteArray::fromStdString(oss.str());
}

// The OpenPGP control parts of RFC 3156 are structure, not content: showing
// them to the user as attachments would be noise.
auto IsProtocolPart(const QString& content_type) -> bool {
  return content_type == "application/pgp-encrypted" ||
         content_type == "application/pgp-signature";
}

struct WalkState {
  int parts_seen{0};
  qint64 bytes_seen{0};
  bool limit_hit{false};
};

void WalkPart(const vmime::shared_ptr<const vmime::bodyPart>& part,
              EMailMetaData& meta, const EMailParseLimits& limits,
              WalkState& state, int depth, bool inside_signed) {
  if (state.limit_hit) return;

  if (depth > limits.max_depth || ++state.parts_seen > limits.max_parts) {
    state.limit_hit = true;
    MimeLog(QString("message tree exceeds parse limits at depth %1, part %2")
                .arg(depth)
                .arg(state.parts_seen));
    return;
  }

  const auto content_type = PartContentType(part);
  const auto sub_parts = part->getBody()->getPartCount();

  if (sub_parts > 0) {
    // Everything below a multipart/signed is what the signature covers.
    const bool signed_subtree =
        inside_signed || content_type == "multipart/signed";

    for (size_t i = 0; i < sub_parts; ++i) {
      WalkPart(part->getBody()->getPartAt(i), meta, limits, state, depth + 1,
               signed_subtree);
      if (state.limit_hit) return;
    }
    return;
  }

  if (IsProtocolPart(content_type)) return;

  const auto data = DecodePart(part);
  state.bytes_seen += data.size();
  if (state.bytes_seen > limits.max_total_bytes) {
    state.limit_hit = true;
    MimeLog(QString("message exceeds the decoded size limit (%1 bytes)")
                .arg(state.bytes_seen));
    return;
  }

  const auto filename = PartFileName(part);
  const auto disposition = PartDisposition(part);

  // The first text/plain part with no filename is the body; anything else is
  // something the user may want to keep.
  const bool is_body = meta.body.isEmpty() && filename.isEmpty() &&
                       content_type == "text/plain" &&
                       !disposition.startsWith("attachment");
  if (is_body) {
    meta.body = data;
    meta.body_content_type = content_type;
    return;
  }

  EMailAttachment att;
  att.filename = filename;
  att.mime_type = content_type;
  att.disposition = disposition;
  att.data = data;
  att.is_openpgp_key = content_type == "application/pgp-keys";
  att.inside_signed_part = inside_signed;
  meta.attachments.append(att);
}

}  // namespace

auto ExtractParts(const vmime::shared_ptr<vmime::message>& message,
                  EMailMetaData& meta_data, const EMailParseLimits& limits)
    -> int {
  if (!message) return -1;

  WalkState state;
  try {
    WalkPart(message, meta_data, limits, state, 0, false);
  } catch (const vmime::exception& e) {
    MimeLog(QString("error walking message parts: %1").arg(e.what()));
    return -1;
  }

  return state.limit_hit ? -1 : 0;
}

auto BuildInnerPartHeader(const vmime::shared_ptr<vmime::header>& source)
    -> QString {
  // Copied rather than hand-picked field by field. The old code listed five
  // headers explicitly and rebuilt the part from them, which silently dropped
  // Content-Transfer-Encoding -- and since the body is carried over as a raw,
  // still-encoded slice, a base64 or quoted-printable body then arrived at the
  // recipient with nothing to say how to decode it. Every message with an
  // attachment has such a body.
  static const QStringList kDescriptiveFields = {
      vmime::fields::MIME_VERSION, vmime::fields::FROM,
      vmime::fields::TO,           vmime::fields::CC,
      vmime::fields::REPLY_TO,     vmime::fields::SUBJECT,
      vmime::fields::DATE,         vmime::fields::MESSAGE_ID};

  auto header = vmime::make_shared<vmime::header>();

  for (const auto& field : source->getFieldList()) {
    const auto name = Q_SC(field->getName());

    // Everything describing how to read the body has to travel with it.
    const bool is_content_field =
        name.startsWith("Content-", Qt::CaseInsensitive);
    const bool is_descriptive =
        std::any_of(kDescriptiveFields.cbegin(), kDescriptiveFields.cend(),
                    [&name](const QString& f) {
                      return name.compare(f, Qt::CaseInsensitive) == 0;
                    });

    if (!is_content_field && !is_descriptive) continue;

    header->appendField(vmime::dynamicCast<vmime::headerField>(field->clone()));
  }

  return Q_SC(header->generate(vmime::lineLengthLimits::convenient));
}
