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

#include <QElapsedTimer>
#include <QFile>
#include <QMap>
#include <QRegularExpression>
#include <QSet>

#include "ImToken.h"

/**
 * @file ImTokenTest.cpp
 * @brief The token format, moved out of the core unchanged. These are the
 *        core's InstantMessageOperator tests, with the phrase passed rather
 *        than stored, plus tokens the core itself wrote, which pin that the
 *        move changed not one byte of the wire.
 */

namespace {

constexpr auto kPhrase = "correct horse battery staple";

// A blob shaped like an encrypted OpenPGP message (old-format PKESK, tag 1).
auto PgpLikeBlob(int len = 200, uint8_t first = 0x84) -> QByteArray {
  QByteArray b;
  b.append(static_cast<char>(first));
  for (int i = 1; i < len; ++i) {
    b.append(static_cast<char>(((i * 37) + 11) & 0xFF));
  }
  return b;
}

/// Restores the per-message KDF cost around a test that needs the shipped one.
class ScopedKdfCost {
 public:
  explicit ScopedKdfCost(ImToken::KdfCost cost)
      : previous_(ImToken::CurrentKdfCost()) {
    ImToken::SetKdfCostForTesting(cost);
  }
  ~ScopedKdfCost() { ImToken::SetKdfCostForTesting(previous_); }

 private:
  ImToken::KdfCost previous_;
};

/// The suite runs a cheap per-message KDF, as the core's did; the tests that
/// reason about the shipped cost put it back for their duration.
class CheapKdf : public ::testing::Environment {
 public:
  void SetUp() override {
    ImToken::SetKdfCostForTesting({1, 8ULL * 1024 * 1024});
  }
};

const auto* const kCheapKdf =
    ::testing::AddGlobalTestEnvironment(new CheapKdf());

auto Encode(const QByteArray& blob, const QString& phrase = {}) -> QString {
  return ImToken::Encode(blob, phrase);
}

auto Decode(const QString& token, const QString& phrase = {})
    -> ImToken::DecodeResult {
  return ImToken::Decode(token, phrase);
}

auto Fixtures() -> QMap<QString, QString> {
  QMap<QString, QString> out;
  QFile f(QStringLiteral(GF_IM_FIXTURES));
  if (!f.open(QIODevice::ReadOnly)) return out;
  for (const auto& line : QString::fromUtf8(f.readAll()).split('\n')) {
    const auto at = line.indexOf('=');
    if (at > 0) out.insert(line.left(at), line.mid(at + 1));
  }
  return out;
}

}  // namespace

// --- compatibility with the core implementation ---------------------------

// Tokens the core wrote at the shipped cost decode to the same bytes here:
// the wire did not move with the code.
TEST(ImTokenTest, TokensTheCoreWroteStillDecode) {
  ScopedKdfCost cost(ImToken::DefaultKdfCost());
  const auto f = Fixtures();
  ASSERT_FALSE(f.isEmpty()) << "missing " << GF_IM_FIXTURES;
  const auto payload = QByteArray::fromHex(f["payload_hex"].toLatin1());
  ASSERT_EQ(payload, PgpLikeBlob());

  const auto by_default = Decode(f["default_token"]);
  EXPECT_TRUE(by_default.ok);
  EXPECT_EQ(by_default.pgp_message, payload);

  const auto by_phrase = Decode(f["phrase_token"], f["phrase"]);
  EXPECT_TRUE(by_phrase.ok);
  EXPECT_EQ(by_phrase.pgp_message, payload);

  // And each only under its own book.
  EXPECT_FALSE(Decode(f["phrase_token"]).ok);
  EXPECT_FALSE(Decode(f["default_token"], f["phrase"]).ok);
}

TEST(ImTokenTest, BookFingerprintsMatchTheCores) {
  const auto f = Fixtures();
  ASSERT_FALSE(f.isEmpty());
  EXPECT_EQ(ImToken::BookFingerprintOf(f["phrase"]), f["phrase_fingerprint"]);
  EXPECT_EQ(ImToken::BookFingerprintOf({}), f["default_fingerprint"]);
}

// A token this module writes is one the core's format reads: same shipped
// cost, same books, round trip.
TEST(ImTokenTest, ShippedCostRoundTrip) {
  ScopedKdfCost cost(ImToken::DefaultKdfCost());
  const auto token = Encode(PgpLikeBlob(), kPhrase);
  ASSERT_FALSE(token.isEmpty());
  const auto r = Decode(token, kPhrase);
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.pgp_message, PgpLikeBlob());
}

// --- the core's InstantMessageOperator tests ------------------------------

// A token wraps and recovers the OpenPGP message verbatim.
TEST(ImTokenTest, NormalRoundTrip) {
  const QByteArray blob = PgpLikeBlob();

  const auto token = Encode(blob);
  ASSERT_FALSE(token.isEmpty());
  // Single Base58 word (alphanumeric minus 0 O I l), no markdown/link chars.
  EXPECT_TRUE(QRegularExpression(QRegularExpression::anchoredPattern(
                                     QStringLiteral("[1-9A-HJ-NP-Za-km-z]+")))
                  .match(token)
                  .hasMatch());

  const auto r = Decode(token);
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.pgp_message, blob);
}

TEST(ImTokenTest, EncodeEmptyIsEmpty) {
  EXPECT_TRUE(Encode(QByteArray()).isEmpty());
}

TEST(ImTokenTest, DecodeNotToken) {
  EXPECT_FALSE(Decode("hello there, friend!").ok);
}

TEST(ImTokenTest, DecodeRejectsArmoredMessage) {
  const QString armored =
      "-----BEGIN PGP MESSAGE-----\n\nhF4Dabc=\n=abcd\n"
      "-----END PGP MESSAGE-----";
  EXPECT_FALSE(Decode(armored).ok);
}

// A messenger may wrap the token across lines / inject spaces; still decodes.
TEST(ImTokenTest, DecodeToleratesWhitespace) {
  const QByteArray blob = PgpLikeBlob();
  auto token = Encode(blob);
  token.insert(token.size() / 2, QStringLiteral("\n  "));

  const auto r = Decode(token);
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.pgp_message, blob);
}

// No stable wire marker: the same message encodes differently every time.
TEST(ImTokenTest, WhitenedTokenVariesPerMessage) {
  const QByteArray blob = PgpLikeBlob();
  const auto a = Encode(blob);
  const auto b = Encode(blob);
  ASSERT_FALSE(a.isEmpty());
  ASSERT_FALSE(b.isEmpty());
  EXPECT_NE(a, b);
  EXPECT_EQ(Decode(a).pgp_message, blob);
  EXPECT_EQ(Decode(b).pgp_message, blob);
}

// Random padding hides the true length.
TEST(ImTokenTest, LengthIsPadded) {
  const auto token = Encode(QByteArray(1, '\x84'));
  ASSERT_FALSE(token.isEmpty());
  EXPECT_GT(token.size(), 60);
}

// Padding is at least 30% of the frame.
TEST(ImTokenTest, PaddingIsAtLeastThirtyPercent) {
  for (const int payload : {200, 512, 1000}) {
    const auto token = Encode(PgpLikeBlob(payload));
    ASSERT_FALSE(token.isEmpty());
    const auto wire_bytes = static_cast<double>(token.size()) * 5.858 / 8.0;
    EXPECT_GT(wire_bytes, payload * 1.3)
        << "payload " << payload << " token " << token.size();
  }
}

// Token lengths are quantized onto a ladder.
TEST(ImTokenTest, LengthsQuantizeOntoALadder) {
  QSet<qsizetype> lengths;
  for (int payload = 200; payload < 260; ++payload) {
    const auto token = Encode(PgpLikeBlob(payload));
    ASSERT_FALSE(token.isEmpty());
    lengths.insert(token.size());
  }
  EXPECT_LT(lengths.size(), 20) << "distinct lengths: " << lengths.size();
}

// A token under one phrase neither decodes nor is recognised under another.
TEST(ImTokenTest, WrongBookDoesNotDecode) {
  const auto token = Encode(PgpLikeBlob(), kPhrase);
  ASSERT_FALSE(token.isEmpty());
  EXPECT_TRUE(Decode(token, kPhrase).ok);
  EXPECT_FALSE(Decode(token, "a completely different phrase").ok);
}

TEST(ImTokenTest, FormatVersionIsStable) {
  EXPECT_EQ(ImToken::FormatVersion(), 3);
}

TEST(ImTokenTest, ShortInputIsRejected) {
  EXPECT_FALSE(Decode(QStringLiteral("hi")).ok);
  EXPECT_FALSE(Decode(QString(37, 'z')).ok);
}

TEST(ImTokenTest, NonBase58InputIsRejected) {
  for (const QChar bad : {QChar('0'), QChar('O'), QChar('I'), QChar('l'),
                          QChar('+'), QChar('/'), QChar('='), QChar('.')}) {
    QString s(200, 'z');
    s[100] = bad;
    EXPECT_FALSE(Decode(s).ok)
        << "accepted a token containing '" << bad.toLatin1() << "'";
  }
}

// A big paste of ordinary text must not reach the decoder or the KDF.
TEST(ImTokenTest, PreFilterIsFastOnLargeInput) {
  ScopedKdfCost cost(ImToken::DefaultKdfCost());

  const QString prose = QStringLiteral("the quick brown fox. ").repeated(50000);
  const QString base58_only = QString(qsizetype{1024} * 1024, 'z');

  QElapsedTimer timer;
  timer.start();
  EXPECT_FALSE(Decode(prose).ok);
  EXPECT_FALSE(Decode(base58_only).ok);
  EXPECT_LT(timer.elapsed(), 500) << "elapsed: " << timer.elapsed() << "ms";
}

TEST(ImTokenTest, EncodeRejectsOversizedPayload) {
  EXPECT_TRUE(Encode(PgpLikeBlob(ImToken::MaxPayloadBytes() + 1)).isEmpty());
}

TEST(ImTokenTest, MaximumPayloadAlwaysRoundTrips) {
  const QByteArray blob = PgpLikeBlob(ImToken::MaxPayloadBytes());
  for (int i = 0; i < 10; ++i) {
    const auto token = Encode(blob);
    ASSERT_FALSE(token.isEmpty()) << "iteration " << i;
    const auto r = Decode(token);
    EXPECT_TRUE(r.ok) << "iteration " << i << " token " << token.size();
    EXPECT_EQ(r.pgp_message, blob) << "iteration " << i;
  }
}

TEST(ImTokenTest, MaxPayloadBytesIsPinned) {
  EXPECT_EQ(ImToken::MaxPayloadBytes(), 7000);
}

// The fingerprint identifies the book: stable for one phrase, different
// across phrases, and short enough to compare by eye.
TEST(ImTokenTest, BookFingerprintIdentifiesBook) {
  const auto fpr = ImToken::BookFingerprintOf(kPhrase);
  EXPECT_TRUE(QRegularExpression(QStringLiteral("^[0-9A-F]{4}-[0-9A-F]{4}$"))
                  .match(fpr)
                  .hasMatch());
  EXPECT_EQ(ImToken::BookFingerprintOf(kPhrase), fpr);

  const auto other = ImToken::BookFingerprintOf("a completely different one");
  EXPECT_NE(other, fpr);
  const auto def = ImToken::BookFingerprintOf({});
  EXPECT_NE(def, fpr);
  EXPECT_NE(def, other);

  // Whitespace around a phrase is not part of it.
  EXPECT_EQ(ImToken::BookFingerprintOf(QStringLiteral("  ") + kPhrase + " "),
            fpr);
}

// Every copy of GpgFrontend builds the same default book.
TEST(ImTokenTest, DefaultBookIsPinned) {
  EXPECT_EQ(ImToken::BookFingerprintOf({}), QStringLiteral("9138-57AC"));
}

TEST(ImTokenTest, GeneratePhraseIsLongAndUnique) {
  const auto a = ImToken::GeneratePhrase();
  const auto b = ImToken::GeneratePhrase();
  EXPECT_EQ(a.size(), 256);
  EXPECT_EQ(b.size(), 256);
  EXPECT_NE(a, b);
  EXPECT_TRUE(QRegularExpression(QRegularExpression::anchoredPattern(
                                     QStringLiteral("[1-9A-HJ-NP-Za-km-z]+")))
                  .match(a)
                  .hasMatch());
  EXPECT_NE(ImToken::BookFingerprintOf(a), ImToken::BookFingerprintOf({}));
}
