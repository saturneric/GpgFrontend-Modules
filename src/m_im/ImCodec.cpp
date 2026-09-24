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

#include "ImCodec.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "ImToken.h"

namespace ImCodec {

namespace {

constexpr char kPhraseBlobVersion = '\x01';

auto Tr(const char* text) -> QString {
  return QCoreApplication::translate("GTrC", text);
}

/**
 * @brief The "Instant Messaging" card: how the token is framed and, above
 * all, whether a shared book phrase is in use -- the default book only hides
 * the format from naive scanners, so it is reported as a warning.
 */
auto BookCard(const QString& phrase, qsizetype payload, qsizetype token_len)
    -> QJsonObject {
  const bool configured = !phrase.isEmpty();
  QJsonArray fields;
  const auto add = [&fields](const QString& k, const QString& v) {
    fields.append(QJsonArray{k, v});
  };

  add(Tr("Encoding"), QStringLiteral("Base58 (Bitcoin/IPFS)"));
  add(Tr("Container Format"), QString("v%1").arg(ImToken::FormatVersion()));
  add(Tr("Message Book"), configured ? Tr("Shared phrase (Argon2id)")
                                     : Tr("Default, no shared phrase set"));
  // Lets both sides confirm they are on the same book. Only meaningful with a
  // phrase: everyone shares the default book, so its digest says nothing.
  if (configured) {
    add(Tr("Book Fingerprint"), ImToken::BookFingerprintOf(phrase));
  } else {
    add(Tr("Set a Phrase"), Tr("Settings > Instant Messaging"));
  }
  if (payload > 0) {
    add(Tr("OpenPGP Payload"), Tr("%1 bytes").arg(payload));
  }
  if (token_len > 0) {
    add(Tr("Token Length"), Tr("%1 characters").arg(token_len));
  }
  if (payload > 0 && token_len > 0) {
    // Random padding plus Base58 expansion. The padding part is deliberately
    // random, so this ratio does not pin down the true payload length.
    const auto ratio =
        static_cast<double>(token_len) / static_cast<double>(payload);
    add(Tr("Wire Overhead"),
        QString("+%1%").arg((ratio * 100.0) - 100.0, 0, 'f', 0));
  }

  return QJsonObject{
      {QStringLiteral("title"), Tr("Instant Messaging")},
      {QStringLiteral("status"),
       configured ? QStringLiteral("ok") : QStringLiteral("warn")},
      {QStringLiteral("fields"), fields}};
}

auto CardsJson(const QJsonObject& card) -> QString {
  const QJsonObject payload{
      {QStringLiteral("description"),
       Tr("An Instant Messaging section followed by the OpenPGP result.")},
      {QStringLiteral("cards"), QJsonArray{card}}};
  return QString::fromUtf8(
      QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

}  // namespace

auto DecodeInput(const QByteArray& input, const QString& phrase) -> Answer {
  // The token format rejects ordinary text and armored OpenPGP on its shape
  // alone, before the memory-hard derivation.
  const auto result = ImToken::Decode(QString::fromUtf8(input), phrase);
  if (!result.ok) return {};

  Answer a;
  a.outcome = Outcome::kHandled;
  a.output = result.pgp_message;
  a.cards = CardsJson(BookCard(phrase, 0, 0));
  return a;
}

auto EncodeOutput(const QByteArray& pgp, const QString& phrase) -> Answer {
  Answer a;
  a.outcome = Outcome::kFailed;

  // The payload limit exists so a token we emit is one we can read back.
  // Checked here so the user is told what went wrong and what to do about it.
  const auto limit = ImToken::MaxPayloadBytes();
  if (pgp.size() > limit) {
    a.error = Tr("This message is too long to send as an instant message.\n\n"
                 "The encrypted message is %1 bytes, and the "
                 "instant-messaging format carries at most %2. Shorten the "
                 "text, or send it as a normal OpenPGP message instead.")
                  .arg(pgp.size())
                  .arg(limit);
    return a;
  }

  const auto token = ImToken::Encode(pgp, phrase);
  if (token.isEmpty()) {
    a.error = Tr("Failed to prepare the instant message: the encrypted "
                 "message could not be converted into a token.");
    return a;
  }

  a.outcome = Outcome::kHandled;
  a.output = token.toUtf8();
  a.cards = CardsJson(BookCard(phrase, pgp.size(), token.size()));
  return a;
}

auto EncodePhraseBlob(const QString& phrase) -> QByteArray {
  QByteArray blob(1, kPhraseBlobVersion);
  blob.append(phrase.trimmed().toUtf8());
  return blob;
}

auto DecodePhraseBlob(const QByteArray& blob) -> QString {
  if (blob.isEmpty() || blob.at(0) != kPhraseBlobVersion) return {};
  return QString::fromUtf8(blob.mid(1)).trimmed();
}

}  // namespace ImCodec
