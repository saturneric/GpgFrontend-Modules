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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimeZone>
#include <QUrl>

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

    // Blind recipients are deliberately NOT handed to the builder.
    //
    // vmime::messageBuilder turns blind copy recipients into a real Bcc:
    // header, and this function produces the bytes the user saves and sends.
    // Writing that header tells every recipient exactly who was blind-copied,
    // which is the one thing BCC exists to prevent. Blind recipients belong to
    // EMailComposeState, beside the document rather than inside it; they
    // select encryption recipients and never reach the message.

    plaintext_msg_builder.setSubject(Q_TEXT(subject));

    // vmime::messageBuilder refuses to construct a message with no recipient.
    // That is a rule about sending, and this function is also what a draft is
    // serialized through while it is being written -- an unaddressed draft
    // still has to round-trip, or the headers and attachments the user has
    // already entered are lost the moment anything reads the document back.
    // Blind recipients no longer count towards being addressed: since no Bcc
    // header is written, a message with only blind recipients really does
    // reach the builder with no addressee, and must take the draft path.
    const bool addressed = !recipient_list.isEmpty() || !cc_list.isEmpty();

    vmime::shared_ptr<vmime::message> plaintext_msg;
    if (addressed) {
      plaintext_msg = plaintext_msg_builder.construct();
    } else {
      plaintext_msg = vmime::make_shared<vmime::message>();
      auto draft_header = plaintext_msg->getHeader();
      draft_header->Subject()->setValue(Q_TEXT(subject));
      if (!from.trimmed().isEmpty()) {
        if (ParseEmailString(from, name, email)) {
          draft_header->From()->setValue(
              vmime::mailbox(Q_TEXT(name), email.toStdString()));
        } else {
          draft_header->From()->setValue(vmime::mailbox(from.toStdString()));
        }
      }
      draft_header->Date()->setValue(vmime::datetime::now());
      draft_header->MimeVersion()->setValue(vmime::SUPPORTED_MIME_VERSION);
    }

    auto plaintext_msg_header = plaintext_msg->getHeader();

    // Threading headers, when the caller supplied them. Written here rather
    // than by the builder, which has no notion of them, and only when present
    // so an ordinary new message does not gain empty fields.
    if (!meta_data.in_reply_to.trimmed().isEmpty()) {
      plaintext_msg_header->InReplyTo()->setValue(
          meta_data.in_reply_to.trimmed().toStdString());
    }
    if (!meta_data.references.isEmpty()) {
      plaintext_msg_header->References()->setValue(
          meta_data.references.join(" ").toStdString());
    }

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
  meta_data.bcc_header =
      ExtractFieldValueAddressListItems(header, vmime::fields::BCC);
  meta_data.message_id = ExtractFieldValue(header, vmime::fields::MESSAGE_ID);
  meta_data.in_reply_to = ExtractFieldValue(header, vmime::fields::IN_REPLY_TO);
  const auto references =
      ExtractFieldValue(header, vmime::fields::REFERENCES).trimmed();
  if (!references.isEmpty()) {
    meta_data.references =
        references.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
  }
  meta_data.subject = ExtractFieldValueText(header, vmime::fields::SUBJECT);
  meta_data.datetime = ExtractFieldValueDateTime(header, vmime::fields::DATE);
  // Reply-To is an address list, not a single mailbox (RFC 5322 allows several,
  // and vmime registers it as addressList). Reading it as a mailbox made
  // getValue<mailbox>() return null every time, so this field was silently
  // always empty -- which meant replies ignored it and nothing could notice a
  // Reply-To pointing somewhere unrelated.
  meta_data.reply_to =
      ExtractFieldValueAddressList(header, vmime::fields::REPLY_TO);
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

}  // namespace

namespace {

// Parameter of a Content-Type field, without vmime's insert-on-read behaviour.
auto ContentTypeParam(const vmime::shared_ptr<const vmime::bodyPart>& part,
                      const std::string& name) -> QString {
  auto field = part->getHeader()->findField(vmime::fields::CONTENT_TYPE);
  if (!field) return {};
  auto ct = vmime::dynamicCast<const vmime::contentTypeField>(field);
  if (!ct) return {};
  auto param = ct->findParameter(name);
  if (!param) return {};
  return Q_SC(param->getValue().getBuffer()).trimmed();
}

auto PartTransferEncoding(const vmime::shared_ptr<const vmime::bodyPart>& part)
    -> QString {
  auto field =
      part->getHeader()->findField(vmime::fields::CONTENT_TRANSFER_ENCODING);
  if (!field) return {};
  auto value = field->getValue<vmime::encoding>();
  if (!value) return {};
  return Q_SC(value->generate()).trimmed().toLower();
}

// Content-ID without the angle brackets, which is the form an HTML "cid:"
// reference actually uses.
auto PartContentId(const vmime::shared_ptr<const vmime::bodyPart>& part)
    -> QString {
  auto field = part->getHeader()->findField("Content-Id");
  if (!field) return {};
  auto value = field->getValue();
  if (!value) return {};
  auto id = Q_SC(value->generate()).trimmed();
  if (id.startsWith('<') && id.endsWith('>')) id = id.mid(1, id.size() - 2);
  return id;
}

// Every header field of a part, decoded, in the order it was written. The
// undecoded truth stays available through RawHeaderBlock().
auto PartHeaderFields(const vmime::shared_ptr<const vmime::bodyPart>& part)
    -> QList<QPair<QString, QString>> {
  QList<QPair<QString, QString>> fields;
  for (const auto& field : part->getHeader()->getFieldList()) {
    auto name = Q_SC(field->getName());
    QString value;

    // Text fields carry RFC 2047 encoded-words; anything else is generated
    // as-is. getValue<text>() simply returns null when the field is not text.
    if (auto text_value = field->getValue<vmime::text>()) {
      value = Q_SC(text_value->getConvertedText(vmime::charsets::UTF_8));
    } else if (auto raw_value = field->getValue()) {
      value = Q_SC(raw_value->generate());
    }

    fields.append({name, value.trimmed()});
  }
  return fields;
}

struct TreeState {
  int parts_seen{0};
  qint64 bytes_seen{0};
  bool limit_hit{false};
  int next_index{0};
  int next_region{0};
  int next_alt_group{0};
};

void BuildNode(const vmime::shared_ptr<const vmime::bodyPart>& part,
               EMailPart& node, const EMailParseLimits& limits,
               TreeState& state, QList<EMailSignatureRegion>& regions,
               int depth, const QList<int>& covering, int alt_group) {
  if (state.limit_hit) return;

  if (depth > limits.max_depth || ++state.parts_seen > limits.max_parts) {
    state.limit_hit = true;
    MimeLog(QString("message tree exceeds parse limits at depth %1, part %2")
                .arg(depth)
                .arg(state.parts_seen));
    return;
  }

  node.index = state.next_index++;
  node.depth = depth;
  node.content_type = PartContentType(part);
  node.charset = ContentTypeParam(part, "charset");
  node.disposition = PartDisposition(part);
  node.filename = PartFileName(part);
  node.content_id = PartContentId(part);
  node.transfer_encoding = PartTransferEncoding(part);
  node.header_fields = PartHeaderFields(part);
  node.covered_by_regions = covering;
  node.alternative_group = alt_group;
  node.is_protocol_part = IsProtocolPart(node.content_type);
  node.is_openpgp_key = node.content_type == "application/pgp-keys";

  node.raw_offset = static_cast<qint64>(part->getParsedOffset());
  node.raw_length = static_cast<qint64>(part->getParsedLength());
  if (auto body = part->getBody()) {
    node.body_offset = static_cast<qint64>(body->getParsedOffset());
    node.body_length = static_cast<qint64>(body->getParsedLength());
  }

  const auto sub_parts = part->getBody()->getPartCount();
  node.is_multipart = sub_parts > 0;

  if (!node.is_multipart) {
    node.data = DecodePart(part);
    node.decoded_size = node.data.size();

    // Protocol parts stay out of the size budget: they are structure, and the
    // limit exists to bound the content a message can make us hold. Counting
    // them would also change the limit behaviour this walk has to preserve.
    if (!node.is_protocol_part) {
      state.bytes_seen += node.decoded_size;
      if (state.bytes_seen > limits.max_total_bytes) {
        state.limit_hit = true;
        MimeLog(QString("message exceeds the decoded size limit (%1 bytes)")
                    .arg(state.bytes_seen));
        return;
      }
    }
    return;
  }

  // A multipart/signed opens a region covering its FIRST part in full,
  // headers included -- that is the entity RFC 3156 signs, and the same slice
  // the verify path hashes. The signature part itself is not covered.
  auto child_covering = covering;
  const bool opens_region = node.content_type == "multipart/signed";
  int region_id = -1;
  int region_slot = -1;
  if (opens_region) {
    EMailSignatureRegion region;
    region.region_id = state.next_region++;
    region.nesting_depth = covering.size();
    region.declared_micalg = ContentTypeParam(part, "micalg");

    auto signed_entity = part->getBody()->getPartAt(0);
    region.raw_offset = static_cast<qint64>(signed_entity->getParsedOffset());
    region.raw_length = static_cast<qint64>(signed_entity->getParsedLength());

    region_id = region.region_id;
    region_slot = static_cast<int>(regions.size());
    regions.append(region);
  }

  // One alternative group per multipart/alternative, so siblings can be told
  // apart from alternatives further down the tree.
  const bool opens_alternative = node.content_type == "multipart/alternative";
  const int child_alt_group =
      opens_alternative ? state.next_alt_group++ : alt_group;

  for (size_t i = 0; i < sub_parts; ++i) {
    EMailPart child;
    auto covering_for_child = child_covering;
    if (opens_region && i == 0) covering_for_child.append(region_id);

    BuildNode(part->getBody()->getPartAt(i), child, limits, state, regions,
              depth + 1, covering_for_child, child_alt_group);
    if (state.limit_hit) return;

    if (region_slot >= 0) {
      // Note where the two halves of this region ended up, so a nested region
      // can later be verified on its own bytes rather than the outermost
      // ones. The signature is the first pgp-signature part beside the entity.
      if (i == 0) {
        regions[region_slot].signed_part_index = child.index;
        // Encrypt-then-sign: the entity under this signature is the encrypted
        // blob, so the signature authenticates ciphertext and not the message
        // that will be read. Recorded here, where the shape is still visible.
        regions[region_slot].covers_ciphertext_only =
            child.content_type == "multipart/encrypted";
      } else if (regions[region_slot].signature_part_index < 0 &&
                 child.content_type == "application/pgp-signature") {
        regions[region_slot].signature_part_index = child.index;
      }
    }

    node.children.append(child);
  }
}

// Body candidacy, matching the rule ExtractParts() has always used.
auto IsBodyCandidate(const EMailPart& part) -> bool {
  return !part.is_multipart && !part.is_protocol_part &&
         part.filename.isEmpty() &&
         !part.disposition.startsWith("attachment") &&
         (part.content_type == "text/plain" ||
          part.content_type == "text/html");
}

void CollectFlat(const EMailPart& node, QList<const EMailPart*>& out) {
  out.append(&node);
  for (const auto& child : node.children) CollectFlat(child, out);
}

}  // namespace

auto ParseMimeTree(const vmime::shared_ptr<vmime::message>& message,
                   const QByteArray& raw, EMailPart& root,
                   QList<EMailSignatureRegion>& regions,
                   const EMailParseLimits& limits) -> int {
  Q_UNUSED(raw)
  if (!message) return -1;

  TreeState state;
  try {
    BuildNode(message, root, limits, state, regions, 0, {}, -1);
  } catch (const vmime::exception& e) {
    MimeLog(QString("error parsing message tree: %1").arg(e.what()));
    return -1;
  }

  return state.limit_hit ? -1 : 0;
}

auto FlattenMimeTree(const EMailPart& root) -> QList<const EMailPart*> {
  QList<const EMailPart*> out;
  CollectFlat(root, out);
  return out;
}

auto RawHeaderBlock(const EMailPart& part, const QByteArray& raw)
    -> QByteArray {
  if (part.raw_offset < 0 || part.body_offset < 0) return {};
  if (part.body_offset < part.raw_offset) return {};
  if (part.body_offset > raw.size()) return {};
  return raw.mid(static_cast<int>(part.raw_offset),
                 static_cast<int>(part.body_offset - part.raw_offset));
}

auto SelectBodyPart(const EMailPart& root, bool prefer_html)
    -> const EMailPart* {
  const auto flat = FlattenMimeTree(root);

  // Within one multipart/alternative the members are the same content in
  // different forms, so exactly one of them is the body. Picking whichever is
  // walked first -- what the flat rule does -- lands on an arbitrary member
  // and hides the rest.
  const QString wanted = prefer_html ? "text/html" : "text/plain";
  const QString fallback = prefer_html ? "text/plain" : "text/html";

  const EMailPart* best = nullptr;
  const EMailPart* best_fallback = nullptr;

  for (const auto* part : flat) {
    if (!IsBodyCandidate(*part)) continue;
    if (part->content_type == wanted) {
      if (best == nullptr) best = part;
    } else if (part->content_type == fallback) {
      if (best_fallback == nullptr) best_fallback = part;
    }
  }

  return best != nullptr ? best : best_fallback;
}

auto ClassifyOpenPGPStructure(const EMailPart& root,
                              const QList<EMailSignatureRegion>& regions)
    -> EMailSecurityState {
  const auto flat = FlattenMimeTree(root);

  bool has_signed = false;
  bool has_encrypted = false;
  bool malformed = false;

  for (const auto* part : flat) {
    if (part->content_type == "multipart/signed") {
      has_signed = true;
      // RFC 3156 is exact about this: two parts, and a signature part.
      if (part->children.size() != 2) malformed = true;
    } else if (part->content_type == "multipart/encrypted") {
      has_encrypted = true;
      if (part->children.size() != 2) malformed = true;
    }
  }

  // A signed subtree that produced no region means the structure did not hold
  // up well enough to say what is being signed.
  if (has_signed && regions.isEmpty()) malformed = true;

  if (malformed) return EMailSecurityState::kMALFORMED_PGP;
  if (has_signed && has_encrypted) return EMailSecurityState::kSIGNED_ENCRYPTED;
  if (has_encrypted) return EMailSecurityState::kENCRYPTED;
  if (has_signed) return EMailSecurityState::kSIGNED;
  return EMailSecurityState::kPLAIN;
}

auto AddressOfUid(const QString& uid) -> QString {
  QString name;
  QString email;
  if (ParseEmailString(uid, name, email)) return email.trimmed().toLower();

  // A bare address with no display name never reaches the angle-bracket form.
  const auto trimmed = uid.trimmed();
  if (trimmed.contains('@') && !trimmed.contains(' ')) return trimmed.toLower();
  return {};
}

namespace {

auto InfoRoot(const QByteArray& info_json) -> QJsonObject {
  const auto doc = QJsonDocument::fromJson(info_json);
  return doc.isObject() ? doc.object() : QJsonObject{};
}

// A recipient the sender asked the engine not to name. GnuPG reports this as
// an all-zero key id; it means "deliberately anonymous", not "unknown".
auto IsHiddenKeyId(const QString& key_id) -> bool {
  if (key_id.isEmpty()) return false;
  for (const auto c : key_id) {
    if (c != '0') return false;
  }
  return true;
}

}  // namespace

auto ParseSignatureResults(const QByteArray& info_json, int region_id)
    -> QList<EMailSignatureResult> {
  QList<EMailSignatureResult> results;

  const auto signatures = InfoRoot(info_json).value("signatures").toArray();
  for (const auto& entry : signatures) {
    const auto obj = entry.toObject();

    EMailSignatureResult result;
    // Stamped from the call site, which knows which region's bytes were just
    // verified. Nothing here depends on the order results arrive in.
    result.region_id = region_id;
    result.fingerprint = obj.value("fingerprint").toString();
    result.pubkey_algo = obj.value("pubkeyAlgo").toString();
    result.hash_algo = obj.value("hashAlgo").toString();
    result.uid = obj.value("uid").toString();
    result.validity = obj.value("validity").toInt();

    const auto sign_time = obj.value("signTime").toString();
    if (!sign_time.isEmpty()) {
      result.sign_time = QDateTime::fromString(sign_time, Qt::ISODate);
    }

    for (const auto& warning : obj.value("warnings").toArray()) {
      result.warnings.append(warning.toString());
    }

    results.append(result);
  }

  return results;
}

auto ParseRecipientInfos(const QByteArray& info_json)
    -> QList<EMailRecipientInfo> {
  QList<EMailRecipientInfo> recipients;

  const auto entries = InfoRoot(info_json).value("recipients").toArray();
  for (const auto& entry : entries) {
    const auto obj = entry.toObject();

    EMailRecipientInfo info;
    info.uid = obj.value("uid").toString();
    info.fingerprint = obj.value("fingerprint").toString();
    info.key_id = obj.value("keyId").toString();
    info.pubkey_algo = obj.value("pubkeyAlgo").toString();
    info.key_found = obj.value("keyFound").toBool();
    info.algo_is_primary_key = obj.value("algoIsPrimaryKey").toBool();
    info.hidden = IsHiddenKeyId(info.key_id);

    recipients.append(info);
  }

  return recipients;
}

void CheckMicalgAgreement(QList<EMailSignatureResult>& results,
                          const EMailSignatureRegion& region) {
  // "pgp-sha256" in the header against "SHA256" from the signature: compare
  // the part that carries the meaning, not the spelling.
  auto normalize = [](QString value) {
    value = value.trimmed().toLower();
    if (value.startsWith("pgp-")) value = value.mid(4);
    value.remove('-');
    return value;
  };

  const auto declared = normalize(region.declared_micalg);
  if (declared.isEmpty()) return;

  for (auto& result : results) {
    if (result.region_id != region.region_id) continue;
    const auto actual = normalize(result.hash_algo);
    if (actual.isEmpty()) continue;
    result.micalg_mismatch = actual != declared;
  }
}

auto MatchRecipients(const QStringList& to, const QStringList& cc,
                     const QStringList& bcc,
                     const QList<EMailRecipientInfo>& encrypted)
    -> QList<EMailRecipientRow> {
  QList<EMailRecipientRow> rows;
  QList<bool> claimed;
  claimed.reserve(encrypted.size());
  for (int i = 0; i < encrypted.size(); ++i) claimed.append(false);

  const auto match_address = [&](const QString& address, const QString& field) {
    const auto wanted = AddressOfUid(address);

    EMailRecipientRow row;
    row.address = address;
    row.header_field = field;

    for (int i = 0; i < encrypted.size(); ++i) {
      if (claimed[i]) continue;
      // Only a recipient whose key is actually in the keyring can be tied to
      // an address; an unknown key id names nobody.
      if (!encrypted[i].key_found) continue;
      if (wanted.isEmpty()) continue;
      if (AddressOfUid(encrypted[i].uid) != wanted) continue;

      claimed[i] = true;
      row.match = RecipientMatch::kMATCHED;
      row.info = encrypted[i];
      rows.append(row);
      return;
    }

    // Addressed but not encrypted to: they were told this message is for them
    // and they cannot open it. This is the one case that warrants a warning.
    row.match = RecipientMatch::kADDRESSED_NOT_ENCRYPTED;
    rows.append(row);
  };

  for (const auto& address : to) match_address(address, "To");
  for (const auto& address : cc) match_address(address, "Cc");
  for (const auto& address : bcc) match_address(address, "Bcc");

  for (int i = 0; i < encrypted.size(); ++i) {
    if (claimed[i]) continue;

    EMailRecipientRow row;
    row.info = encrypted[i];
    // Not a finding. A message is routinely encrypted to keys that never
    // appear in a header, and a withheld key id is a choice the sender made
    // rather than something going wrong.
    row.match = encrypted[i].hidden ? RecipientMatch::kHIDDEN_RECIPIENT
                                    : RecipientMatch::kENCRYPTED_NOT_ADDRESSED;
    rows.append(row);
  }

  return rows;
}

namespace {

// "Re: " / "Fwd: " only once, however many times a message has been round the
// houses. Stacking prefixes is the classic way a subject line becomes
// unreadable after three exchanges.
auto PrefixSubject(const QString& subject, const QString& prefix) -> QString {
  const auto trimmed = subject.trimmed();
  if (trimmed.startsWith(prefix, Qt::CaseInsensitive)) return trimmed;
  return prefix + trimmed;
}

auto NormalizedAddresses(const QStringList& addresses) -> QStringList {
  QStringList out;
  for (const auto& address : addresses) {
    const auto trimmed = address.trimmed();
    if (!trimmed.isEmpty()) out.append(trimmed);
  }
  return out;
}

// Deduplicate by address rather than by display name: the same person may
// appear once bare and once with a name, and they should be addressed once.
auto WithoutDuplicatesOrSelf(const QStringList& addresses,
                             const QString& self_address, QStringList& seen)
    -> QStringList {
  const auto self = AddressOfUid(self_address);
  QStringList out;
  for (const auto& address : addresses) {
    const auto key = AddressOfUid(address);
    if (key.isEmpty()) continue;
    if (!self.isEmpty() && key == self) continue;
    if (seen.contains(key)) continue;
    seen.append(key);
    out.append(address);
  }
  return out;
}

}  // namespace

void BuildDerivedMetaData(const EMailMetaData& source, EMailReplyMode mode,
                          const QString& self_address, EMailMetaData& out) {
  out = EMailMetaData{};

  // The user's own identity on the new message, as far as it can be known
  // from the old one: whoever the original was addressed to.
  out.from = self_address;

  if (mode == EMailReplyMode::kFORWARD) {
    out.subject = PrefixSubject(source.subject, "Fwd: ");
    // No recipients: a forward is addressed by the person forwarding it, and
    // guessing here is how mail goes to the wrong people.
    out.attachments = source.attachments;
  } else {
    out.subject = PrefixSubject(source.subject, "Re: ");

    // Reply-To exists precisely so the sender can redirect answers; honouring
    // From instead would send the reply somewhere the sender asked it not to
    // go.
    const auto reply_target =
        source.reply_to.trimmed().isEmpty() ? source.from : source.reply_to;

    QStringList seen;
    out.to = WithoutDuplicatesOrSelf(NormalizedAddresses({reply_target}),
                                     self_address, seen);

    if (mode == EMailReplyMode::kREPLY_ALL) {
      // Everyone who was visibly addressed, minus whoever is already in To and
      // minus the user. Bcc is deliberately not carried: those recipients were
      // blind, and a reply-all that exposes them defeats the original choice.
      auto others = NormalizedAddresses(source.to);
      others += NormalizedAddresses(source.cc);
      out.cc = WithoutDuplicatesOrSelf(others, self_address, seen);
    }
  }

  // Threading headers, so the answer attaches to what it answers. References
  // accumulates the chain and In-Reply-To names the immediate parent.
  if (!source.message_id.isEmpty()) {
    out.in_reply_to = source.message_id;
    out.references = source.references;
    if (!out.references.contains(source.message_id)) {
      out.references.append(source.message_id);
    }
  }
}

auto BuildQuotedBody(const EMailMetaData& source, EMailReplyMode mode)
    -> QByteArray {
  const auto text = QString::fromUtf8(source.body);

  if (mode == EMailReplyMode::kFORWARD) {
    QString out = "\n\n---------- Forwarded message ----------\n";
    if (!source.from.isEmpty()) out += "From: " + source.from + "\n";
    if (!source.to.isEmpty()) out += "To: " + source.to.join("; ") + "\n";
    if (!source.cc.isEmpty()) out += "Cc: " + source.cc.join("; ") + "\n";
    if (source.datetime.isValid()) {
      out += "Date: " + source.datetime.toString(Qt::ISODate) + "\n";
    }
    if (!source.subject.isEmpty()) out += "Subject: " + source.subject + "\n";
    out += "\n" + text;
    return out.toUtf8();
  }

  QString out;
  if (!source.from.isEmpty()) {
    out += source.datetime.isValid()
               ? QString("\nOn %1, %2 wrote:\n")
                     .arg(source.datetime.toString(Qt::ISODate), source.from)
               : QString("\n%1 wrote:\n").arg(source.from);
  } else {
    out += "\n";
  }

  // Quote every line, empty ones included: a blank line left unquoted reads as
  // the end of the quotation.
  const auto lines = text.split('\n');
  for (const auto& line : lines) {
    out += line.isEmpty() ? QString(">\n") : QString("> %1\n").arg(line);
  }

  return out.toUtf8();
}

namespace {

// Fields that RFC 5322 allows at most once. A second copy is not merely
// untidy: readers disagree about which one wins, so a message carrying two
// can show one thing to the person and another to whatever checked it.
auto SingletonFieldNames() -> QStringList {
  return {"From",    "Sender", "Reply-To",   "To",          "Cc",        "Bcc",
          "Subject", "Date",   "Message-ID", "In-Reply-To", "References"};
}

// Does this text mix scripts inside one label? Latin plus Cyrillic in one
// domain is the classic homograph construction and essentially never
// legitimate.
auto MixesScripts(const QString& text) -> bool {
  bool latin = false;
  bool other = false;
  for (const auto c : text) {
    if (!c.isLetter()) continue;
    const auto script = c.script();
    if (script == QChar::Script_Latin) {
      latin = true;
    } else if (script != QChar::Script_Common &&
               script != QChar::Script_Inherited) {
      other = true;
    }
  }
  return latin && other;
}

auto DomainOf(const QString& address) -> QString {
  const auto email = AddressOfUid(address);
  const auto at = email.lastIndexOf('@');
  return at < 0 ? QString() : email.mid(at + 1);
}

}  // namespace

auto LooksLikeSpoofedAddress(const QString& address) -> bool {
  const auto domain = DomainOf(address);
  if (domain.isEmpty()) return false;

  if (MixesScripts(domain)) return true;

  // A domain that is not plain ASCII has a punycode form that differs from
  // what is displayed; that gap is exactly what a homograph attack lives in.
  const auto ace = QString::fromLatin1(QUrl::toAce(domain));
  return !ace.isEmpty() && ace.compare(domain, Qt::CaseInsensitive) != 0;
}

auto HasBareLineFeeds(const QByteArray& bytes) -> bool {
  for (qsizetype i = 0; i < bytes.size(); ++i) {
    if (bytes[i] != '\n') continue;
    if (i == 0 || bytes[i - 1] != '\r') return true;
  }
  return false;
}

auto InspectMessage(const EMailMetaData& meta, const EMailPart& root,
                    const QList<EMailSignatureRegion>& regions,
                    const QByteArray& raw) -> QList<EMailFinding> {
  QList<EMailFinding> findings;

  // --- the signed bytes are still the signed bytes -------------------------

  for (const auto& region : regions) {
    if (raw.isEmpty() || region.raw_offset < 0 || region.raw_length <= 0) {
      continue;
    }
    if (region.raw_offset + region.raw_length > raw.size()) continue;

    if (!HasBareLineFeeds(raw.mid(region.raw_offset, region.raw_length))) {
      continue;
    }

    findings.append(
        {EMailFindingLevel::kWARN,
         QObject::tr("Signed part no longer in canonical form"),
         QObject::tr(
             "The signed part contains line endings that are not CRLF, which "
             "is not the form a signature is computed over. Something rewrote "
             "this message after it was signed, normally a program that "
             "changed line endings while saving or copying it rather than an "
             "attack. The signature cannot verify against these bytes, and "
             "importing the sender's key will not change that. Checking it "
             "needs the original, unmodified message.")});
  }

  // --- header integrity ----------------------------------------------------

  QMap<QString, int> counts;
  for (const auto& field : root.header_fields) {
    counts[field.first.toLower()] += 1;
  }

  for (const auto& name : SingletonFieldNames()) {
    const auto count = counts.value(name.toLower(), 0);
    if (count <= 1) continue;
    findings.append(
        {EMailFindingLevel::kRISK, QObject::tr("Duplicate %1 header").arg(name),
         QObject::tr("This message carries %1 copies of a header that may "
                     "appear only once. Different mail programs pick "
                     "different copies, so what you see here may not be what "
                     "another reader sees.")
             .arg(count)});
  }

  // --- identity ------------------------------------------------------------

  for (const auto& address : QStringList{meta.from} + meta.to + meta.cc) {
    if (address.trimmed().isEmpty()) continue;
    if (!LooksLikeSpoofedAddress(address)) continue;
    findings.append(
        {EMailFindingLevel::kRISK, QObject::tr("Address may be disguised"),
         QObject::tr("The domain in \"%1\" uses characters that can be drawn "
                     "to look like a different, familiar address.")
             .arg(address)});
  }

  // A Reply-To on another domain is ordinary for mailing lists and support
  // systems, so this is a note rather than an accusation -- but it is the
  // mechanism behind most reply-hijacking, and it is invisible until you look.
  if (!meta.reply_to.trimmed().isEmpty() && !meta.from.trimmed().isEmpty()) {
    const auto reply_domain = DomainOf(meta.reply_to);
    const auto from_domain = DomainOf(meta.from);
    if (!reply_domain.isEmpty() && !from_domain.isEmpty() &&
        reply_domain.compare(from_domain, Qt::CaseInsensitive) != 0) {
      findings.append(
          {EMailFindingLevel::kWARN, QObject::tr("Replies go somewhere else"),
           QObject::tr("This message is from \"%1\" but replies would be sent "
                       "to \"%2\", which is a different domain.")
               .arg(from_domain, reply_domain)});
    }
  }

  // --- structure -----------------------------------------------------------

  const auto flat = FlattenMimeTree(root);

  for (const auto* part : flat) {
    if (part->content_type == "multipart/signed" &&
        part->children.size() != 2) {
      findings.append(
          {EMailFindingLevel::kRISK, QObject::tr("Malformed signed section"),
           QObject::tr("A signed section must contain exactly two parts; this "
                       "one contains %1. It may not verify, and what it "
                       "covers is ambiguous.")
               .arg(part->children.size())});
    }
    if (part->content_type == "multipart/encrypted" &&
        part->children.size() != 2) {
      findings.append(
          {EMailFindingLevel::kRISK, QObject::tr("Malformed encrypted section"),
           QObject::tr("An encrypted section must contain exactly two parts; "
                       "this one contains %1.")
               .arg(part->children.size())});
    }
  }

  // Content smuggled in beside a signature is the reason coverage is tracked
  // per part rather than per message.
  if (!regions.isEmpty()) {
    int uncovered = 0;
    for (const auto* part : flat) {
      if (part->is_multipart || part->is_protocol_part) continue;
      if (part->covered_by_regions.isEmpty()) ++uncovered;
    }
    if (uncovered > 0) {
      findings.append(
          {EMailFindingLevel::kWARN, QObject::tr("Not everything is signed"),
           QObject::tr("%1 part(s) of this message sit outside the signature. "
                       "They arrived unauthenticated and could have been "
                       "added or changed by anyone in the path.")
               .arg(uncovered)});
    }
  }

  // A signature over ciphertext is the case most easily misread as "this
  // message is signed by X". It is not: signing someone else's encrypted blob
  // takes no key of theirs and no knowledge of what is inside it.
  for (const auto& region : regions) {
    if (!region.covers_ciphertext_only) continue;
    findings.append(
        {EMailFindingLevel::kWARN,
         QObject::tr("Signature covers the encrypted data only"),
         QObject::tr("This signature was made over the encrypted block, not "
                     "over the message inside it. It shows who sent the "
                     "ciphertext along; it does not say who wrote what you "
                     "are reading, and anyone could have signed a copy of "
                     "this same block.")});
  }

  // --- OpenPGP material carried as attachments -----------------------------

  int attached_keys = 0;
  int detached_signatures = 0;
  for (const auto* part : flat) {
    if (part->is_openpgp_key) ++attached_keys;
    // A pgp-signature part that is NOT the second half of a multipart/signed
    // is a detached signature riding along as a file, which is a different
    // thing and is not verified by opening the message.
    if (part->content_type == "application/pgp-signature" &&
        part->covered_by_regions.isEmpty()) {
      bool belongs_to_a_region = false;
      for (const auto& region : regions) {
        if (region.signature_part_index == part->index) {
          belongs_to_a_region = true;
          break;
        }
      }
      if (!belongs_to_a_region) ++detached_signatures;
    }
  }

  if (attached_keys > 0) {
    findings.append(
        {EMailFindingLevel::kNOTE, QObject::tr("Public key attached"),
         QObject::tr("This message carries %1 OpenPGP public key(s). A key "
                     "arriving in a message proves nothing about who sent it: "
                     "anyone can attach any key, including one they made for "
                     "the name on the From line.")
             .arg(attached_keys)});
  }

  if (detached_signatures > 0) {
    findings.append(
        {EMailFindingLevel::kNOTE, QObject::tr("Detached signature attached"),
         QObject::tr("This message carries %1 signature file(s) that are not "
                     "part of its own signed structure. Opening the message "
                     "does not check them; they sign something else.")
             .arg(detached_signatures)});
  }

  // A Bcc header on a received message means the sender's tooling disclosed
  // recipients who were meant to be blind.
  if (!meta.bcc_header.isEmpty()) {
    findings.append(
        {EMailFindingLevel::kWARN, QObject::tr("Blind recipients are visible"),
         QObject::tr("This message carries a Bcc header naming %1 "
                     "recipient(s). Anyone who received it can see who was "
                     "blind-copied.")
             .arg(meta.bcc_header.size())});
  }

  return findings;
}

auto PreflightMessage(const EMailMetaData& meta, const EMailPart& root,
                      const QList<EMailSignatureRegion>& regions,
                      const QStringList& compose_bcc) -> QList<EMailFinding> {
  QList<EMailFinding> findings;

  if (regions.isEmpty()) {
    findings.append(
        {EMailFindingLevel::kNOTE, QObject::tr("Nothing is signed"),
         QObject::tr("This message carries no signature, so the recipient "
                     "cannot tell that it came from you or that it arrived "
                     "unchanged.")});
  }

  const auto flat = FlattenMimeTree(root);
  int unsigned_parts = 0;
  for (const auto* part : flat) {
    if (part->is_multipart || part->is_protocol_part) continue;
    if (!regions.isEmpty() && part->covered_by_regions.isEmpty()) {
      ++unsigned_parts;
    }
  }

  if (unsigned_parts > 0) {
    findings.append(
        {EMailFindingLevel::kWARN, QObject::tr("Some parts are not signed"),
         QObject::tr("%1 part(s) of this message would go out without the "
                     "signature covering them.")
             .arg(unsigned_parts)});
  }

  // The blind recipients are about to become an encryption decision, and the
  // one thing that must never happen is for them to become a header.
  if (!compose_bcc.isEmpty()) {
    findings.append(
        {EMailFindingLevel::kNOTE, QObject::tr("Blind recipients"),
         QObject::tr("%1 blind recipient(s) will be included in encryption "
                     "but will not appear anywhere in the message.")
             .arg(compose_bcc.size())});
  }

  if (!meta.bcc_header.isEmpty()) {
    findings.append(
        {EMailFindingLevel::kRISK, QObject::tr("Bcc header present"),
         QObject::tr("This message carries a Bcc header. Sending it would "
                     "tell every recipient who was blind-copied.")});
  }

  for (const auto& address : QStringList{meta.from} + meta.to + meta.cc) {
    if (address.trimmed().isEmpty()) continue;
    if (!LooksLikeSpoofedAddress(address)) continue;
    findings.append(
        {EMailFindingLevel::kRISK, QObject::tr("Recipient may be disguised"),
         QObject::tr("The domain in \"%1\" uses characters that can be drawn "
                     "to look like a different address. Check it before "
                     "sending.")
             .arg(address)});
  }

  if (meta.to.isEmpty() && meta.cc.isEmpty() && compose_bcc.isEmpty()) {
    findings.append({EMailFindingLevel::kWARN, QObject::tr("No recipients"),
                     QObject::tr("This message is not addressed to anyone.")});
  }

  return findings;
}

auto ExtractParts(const vmime::shared_ptr<vmime::message>& message,
                  EMailMetaData& meta_data, const EMailParseLimits& limits)
    -> int {
  if (!message) return -1;

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  if (ParseMimeTree(message, {}, root, regions, limits) != 0) return -1;

  meta_data.signature_regions = regions;

  // The flattening rule is kept exactly as it was, deliberately. This function
  // feeds the crypto paths and the result cards, and the tree was introduced
  // underneath it rather than to change it -- the alternative-aware selection
  // lives in SelectBodyPart(), which the views use. A differential test pins
  // the two implementations together across the corpus.
  for (const auto* part : FlattenMimeTree(root)) {
    if (part->is_multipart || part->is_protocol_part) continue;

    const bool is_body = meta_data.body.isEmpty() && part->filename.isEmpty() &&
                         part->content_type == "text/plain" &&
                         !part->disposition.startsWith("attachment");
    if (is_body) {
      meta_data.body = part->data;
      meta_data.body_content_type = part->content_type;
      continue;
    }

    EMailAttachment att;
    att.filename = part->filename;
    att.mime_type = part->content_type;
    att.disposition = part->disposition;
    att.data = part->data;
    att.is_openpgp_key = part->is_openpgp_key;
    // Being covered by any region is exactly what "inside the signed part"
    // meant before regions existed.
    att.inside_signed_part = !part->covered_by_regions.isEmpty();
    meta_data.attachments.append(att);
  }

  return 0;
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
