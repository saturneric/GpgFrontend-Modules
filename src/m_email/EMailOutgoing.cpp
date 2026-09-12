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

#include "EMailOutgoing.h"

#include <QRegularExpression>
#include <QUuid>

#include "EMailHelper.h"

namespace {

/// Candidate names for a sent-mail folder, lowercased. Only ever consulted
/// when the user named the folder explicitly -- never as a guess of its own.
auto SentFolderNames() -> QStringList {
  return {"sent", "sent items", "sent messages", "sent mail", "outbox"};
}

}  // namespace

auto MailAddressOnly(const QString& mailbox) -> QString {
  const auto text = mailbox.trimmed();
  if (text.isEmpty()) return {};

  const auto open = text.lastIndexOf('<');
  const auto close = text.lastIndexOf('>');
  if (open >= 0 && close > open) {
    return text.mid(open + 1, close - open - 1).trimmed();
  }
  return text;
}

auto MailDedupeAddresses(const QStringList& addresses) -> QStringList {
  QStringList result;
  QStringList seen;

  for (const auto& entry : addresses) {
    const auto address = MailAddressOnly(entry);
    if (address.isEmpty()) continue;

    // Compared case-insensitively as a whole. The local part is technically
    // case-sensitive, but no real deployment treats it that way, and sending
    // someone two copies is the worse error of the two.
    const auto key = address.toLower();
    if (seen.contains(key)) continue;

    seen.append(key);
    result.append(address);
  }
  return result;
}

auto MailGenerateMessageId(const QString& sender) -> QString {
  auto domain = MailAddressOnly(sender).section('@', 1).trimmed();
  if (domain.isEmpty()) domain = "localhost";

  const auto left = QUuid::createUuid().toString(QUuid::WithoutBraces);
  return QString("%1@%2").arg(left, domain);
}

auto MailExtractMessageId(const QByteArray& eml) -> QString {
  // Read out of the header block only. Scanning the whole message would match
  // a References line in a quoted reply further down.
  const auto header_end = eml.indexOf("\r\n\r\n");
  const auto limit = header_end >= 0 ? header_end : eml.size();
  const auto header = QString::fromUtf8(eml.left(limit));

  static const QRegularExpression pattern(
      R"(^Message-ID:\s*<([^>]+)>)", QRegularExpression::CaseInsensitiveOption |
                                         QRegularExpression::MultilineOption);

  const auto match = pattern.match(header);
  return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

auto RankSentCandidate(const QString& name, bool special_use, bool is_override)
    -> int {
  // An explicit choice outranks anything discovered: the user is answering the
  // exact question discovery was trying to guess at.
  if (is_override) return 120;
  if (special_use) return 100;
  if (SentFolderNames().contains(name.trimmed().toLower())) return 80;
  return 0;
}

auto FreezeOutgoing(const EMailMetaData& meta, const EMailComposeState& compose,
                    const QByteArray& body,
                    const QList<EMailAttachment>& attachments,
                    const QByteArray& existing_source,
                    EMailOutgoingMessage& out) -> EMailFreezeResult {
  out = {};

  out.envelope_from = MailAddressOnly(meta.from);
  if (out.envelope_from.isEmpty()) return EMailFreezeResult::kNO_SENDER;

  // The envelope is built here, from compose state the message itself never
  // sees. This is the only place Bcc is allowed to appear.
  QStringList recipients;
  recipients += meta.to;
  recipients += meta.cc;
  recipients += compose.bcc;

  out.envelope_rcpt = MailDedupeAddresses(recipients);
  if (out.envelope_rcpt.isEmpty()) return EMailFreezeResult::kNO_RECIPIENT;

  // Which of the blind recipients are genuinely blind: one who also appears in
  // To or Cc is visible to everyone, and calling them blind would misdescribe
  // what the user is about to do.
  const auto visible = MailDedupeAddresses(meta.to + meta.cc);
  for (const auto& address : MailDedupeAddresses(compose.bcc)) {
    if (!visible.contains(address, Qt::CaseInsensitive)) {
      out.blind_rcpt.append(address);
    }
  }

  if (!existing_source.isEmpty()) {
    // Byte preservation. These bytes may carry a signature computed over
    // exactly these octets, so they are submitted as they are -- no rebuild,
    // and no Message-ID injected, because either would break it.
    out.eml = existing_source;
    out.message_id = MailExtractMessageId(existing_source);
    return EMailFreezeResult::kOK;
  }

  auto pinned = meta;
  if (pinned.message_id.trimmed().isEmpty()) {
    pinned.message_id = MailGenerateMessageId(out.envelope_from);
  }

  QString eml;
  if (BuildMimeEML(pinned, body, attachments, eml) != 0) {
    return EMailFreezeResult::kSERIALIZE_FAILED;
  }

  out.eml = eml.toUtf8();

  // Read back out of the bytes rather than trusted from the input: what was
  // actually written is what the recipient's server will index and what a
  // Sent-folder search has to match.
  out.message_id = MailExtractMessageId(out.eml);
  return EMailFreezeResult::kOK;
}
