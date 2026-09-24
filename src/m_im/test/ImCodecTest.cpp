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

#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "ImCodec.h"
#include "ImToken.h"

/**
 * @file ImCodecTest.cpp
 * @brief What the module answers the Host's decoder and encoder with.
 */

namespace {

auto Blob(int len = 200) -> QByteArray {
  QByteArray b;
  b.append('\x84');
  for (int i = 1; i < len; ++i) b.append(static_cast<char>((i * 37 + 11)));
  return b;
}

auto CardOf(const QString& json) -> QJsonObject {
  const auto doc = QJsonDocument::fromJson(json.toUtf8()).object();
  const auto cards = doc.value("cards").toArray();
  return cards.isEmpty() ? QJsonObject{} : cards.first().toObject();
}

}  // namespace

// An IM token: the decoder claims it and hands back the OpenPGP message.
TEST(ImCodecTest, ATokenIsClaimedAndUnwrapped) {
  const auto token = ImToken::Encode(Blob(), "phrase");
  ASSERT_FALSE(token.isEmpty());
  const auto a = ImCodec::DecodeInput(token.toUtf8(), "phrase");
  EXPECT_EQ(a.outcome, ImCodec::Outcome::kHandled);
  EXPECT_EQ(a.output, Blob());
  EXPECT_EQ(CardOf(a.cards).value("title").toString(), "Instant Messaging");
  EXPECT_EQ(CardOf(a.cards).value("status").toString(), "ok");
}

// Ordinary OpenPGP: not ours, so the Host's own decrypt runs on it.
TEST(ImCodecTest, ArmoredOpenPgpIsNotClaimed) {
  const QByteArray armored =
      "-----BEGIN PGP MESSAGE-----\n\nhF4DAAAAAAAAAAASAQdAabc=\n=abcd\n"
      "-----END PGP MESSAGE-----\n";
  const auto a = ImCodec::DecodeInput(armored, {});
  EXPECT_EQ(a.outcome, ImCodec::Outcome::kNotHandled);
  EXPECT_TRUE(a.output.isEmpty());
  EXPECT_TRUE(a.cards.isEmpty());
}

// Plain text: not ours either; the Host's normal fallback applies.
TEST(ImCodecTest, PlainTextIsNotClaimed) {
  EXPECT_EQ(ImCodec::DecodeInput("hello, see you at eight", {}).outcome,
            ImCodec::Outcome::kNotHandled);
  EXPECT_EQ(ImCodec::DecodeInput({}, {}).outcome,
            ImCodec::Outcome::kNotHandled);
}

// A token of another book is, to this book, random text: not claimed, so the
// behaviour is what it always was -- the text is decrypted as it is.
TEST(ImCodecTest, AnotherBooksTokenIsNotClaimed) {
  const auto token = ImToken::Encode(Blob(), "theirs");
  EXPECT_EQ(ImCodec::DecodeInput(token.toUtf8(), "mine").outcome,
            ImCodec::Outcome::kNotHandled);
}

TEST(ImCodecTest, EncodeMakesATokenTheDecoderClaims) {
  const auto e = ImCodec::EncodeOutput(Blob(), "phrase");
  ASSERT_EQ(e.outcome, ImCodec::Outcome::kHandled);
  const auto d = ImCodec::DecodeInput(e.output, "phrase");
  EXPECT_EQ(d.outcome, ImCodec::Outcome::kHandled);
  EXPECT_EQ(d.output, Blob());
}

// Too long for the format: a failure the user is told about, not a token.
TEST(ImCodecTest, AnOversizedMessageFailsWithAReason) {
  const auto e = ImCodec::EncodeOutput(Blob(ImToken::MaxPayloadBytes() + 1), {});
  EXPECT_EQ(e.outcome, ImCodec::Outcome::kFailed);
  EXPECT_TRUE(e.output.isEmpty());
  EXPECT_TRUE(e.error.contains("too long")) << e.error.toStdString();
}

// The default book only hides the format from naive scanners: its card warns.
TEST(ImCodecTest, TheDefaultBookCardWarns) {
  const auto e = ImCodec::EncodeOutput(Blob(), {});
  ASSERT_EQ(e.outcome, ImCodec::Outcome::kHandled);
  EXPECT_EQ(CardOf(e.cards).value("status").toString(), "warn");
}

// The phrase survives the cache's "no empty values" rule, and is read back
// the way the core wrote it, so a migrated value needs no conversion.
TEST(ImCodecTest, PhraseBlobRoundTripsIncludingEmpty) {
  EXPECT_EQ(ImCodec::DecodePhraseBlob(ImCodec::EncodePhraseBlob("  a b  ")),
            "a b");
  const auto cleared = ImCodec::EncodePhraseBlob({});
  EXPECT_FALSE(cleared.isEmpty());
  EXPECT_TRUE(ImCodec::DecodePhraseBlob(cleared).isEmpty());
  EXPECT_TRUE(ImCodec::DecodePhraseBlob({}).isEmpty());
  EXPECT_EQ(ImCodec::DecodePhraseBlob(QByteArray("\x01") + "legacy"),
            "legacy");
}
