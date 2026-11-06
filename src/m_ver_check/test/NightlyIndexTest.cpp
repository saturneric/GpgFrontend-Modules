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
#include <sodium.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <array>
#include <map>
#include <memory>

#include "Minisign.h"
#include "ReleaseCheck.h"
#include "ReleaseIndex.h"
#include "SemVer.h"
#include "UpdateChecker.h"

// The nightly update path: SemVer order, the minisign check of the index,
// reading it, and the checker driving them against a fake set of mirrors.

namespace {

// Made by the minisign tool itself (0.12), so the check here is the one the
// release tooling's signatures meet: a prehashed signature, a legacy one, the
// key that made both, and another key.
constexpr auto kToolKey =
    "RWT5ZoIvMPnEFZArUcjVAcaxOSPfQg/FiKw6cGTFFEg5kJj8f6+SfYha";
constexpr auto kOtherToolKey =
    "RWQ9bjxPE/GOnWAD9diJOW2yTdjnU51kiEh9dX0yOOx7A+Ya8bDWR1Kw";
constexpr auto kToolMessage = "{\"product\":\"gpgfrontend\",\"serial\":7}\n";
constexpr auto kToolSignature =
    "untrusted comment: signature from minisign secret key\n"
    "RUT5ZoIvMPnEFYWxgCi2jyXv0o08+"
    "K9L0X4rTMK6vtnzQU479ivd5fXu9x9WQY6NXaI3pbwjyfs"
    "FawUiwv5D2E+uJBMMgQjNgQE=\n"
    "trusted comment: product=gpgfrontend serial=7\n"
    "cERBc409s113Hj7oqSGpS4pMMtCzAbjHbDHQ3sam+TX4Vni6CrBzFpXTQlDop4QobnKoAD+"
    "IBzA"
    "wLUZxKkODDg==\n";
constexpr auto kToolLegacySignature =
    "untrusted comment: signature from minisign secret key\n"
    "RWT5ZoIvMPnEFcpZMKRxJd6yEUhr8oFzRYgORi22vy2bBqZG8mEFzJnsE5QTkEcAy1Bs45pgYK"
    "x"
    "QCTQkBhZrT+20gvU6QJJXbgI=\n"
    "trusted comment: legacy\n"
    "d1hUw/q07ZeadjbYosx21d3zPIWjmPTNIJ4xLaY0Y8/"
    "BjhlCYby3LDRHXiXvS6dxDsOoCXMp13p"
    "q6Zs0KswLCA==\n";

/// Signs as minisign does (prehashed "ED"), so a test can sign any index.
class Signer {
 public:
  Signer() {
    if (sodium_init() < 0) std::abort();
    crypto_sign_keypair(pk_.data(), sk_.data());
    randombytes_buf(id_.data(), id_.size());
  }

  [[nodiscard]] auto PublicKey() const -> QString {
    QByteArray raw("Ed");
    raw += Raw(id_) + Raw(pk_);
    return QString::fromLatin1(raw.toBase64());
  }

  [[nodiscard]] auto Sign(const QByteArray& message,
                          const QString& comment) const -> QByteArray {
    std::array<unsigned char, crypto_generichash_BYTES_MAX> hash{};
    crypto_generichash(hash.data(), hash.size(), U(message), message.size(),
                       nullptr, 0);
    std::array<unsigned char, crypto_sign_BYTES> sig{};
    crypto_sign_detached(sig.data(), nullptr, hash.data(), hash.size(),
                         sk_.data());
    const auto trusted = comment.toUtf8();
    const auto bound = Raw(sig) + trusted;
    std::array<unsigned char, crypto_sign_BYTES> global{};
    crypto_sign_detached(global.data(), nullptr, U(bound), bound.size(),
                         sk_.data());
    QByteArray line("ED");
    line += Raw(id_) + Raw(sig);
    return "untrusted comment: test\n" + line.toBase64() +
           "\ntrusted comment: " + trusted + "\n" + Raw(global).toBase64() +
           "\n";
  }

 private:
  template <std::size_t N>
  static auto Raw(const std::array<unsigned char, N>& a) -> QByteArray {
    return {reinterpret_cast<const char*>(a.data()), static_cast<qsizetype>(N)};
  }
  static auto U(const QByteArray& a) -> const unsigned char* {
    return reinterpret_cast<const unsigned char*>(a.constData());
  }

  std::array<unsigned char, crypto_sign_PUBLICKEYBYTES> pk_{};
  std::array<unsigned char, crypto_sign_SECRETKEYBYTES> sk_{};
  std::array<unsigned char, 8> id_{};
};

const QDateTime kNow =
    QDateTime::fromString("2026-11-06T12:00:00Z", Qt::ISODate);

auto Index(qint64 serial, const QString& nightly,
           const QString& product = "gpgfrontend",
           const QDateTime& expires = kNow.addDays(14),
           const QStringList& withdrawn = {}) -> QByteArray {
  QJsonObject channels{
      {"stable", QJsonValue::Null},
      {"beta", QJsonValue::Null},
      {"nightly",
       nightly.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(nightly)}};
  QJsonArray w;
  for (const auto& v : withdrawn)
    w.append(QJsonObject{{"version", v}, {"reason", "r"}});
  QJsonObject index{{"schema", 1},
                    {"product", product},
                    {"serial", serial},
                    {"generated_at", kNow.toString(Qt::ISODate)},
                    {"expires", expires.toUTC().toString(Qt::ISODate)},
                    {"channels", channels},
                    {"withdrawn", w}};
  return QJsonDocument(index).toJson();
}

auto Comment(qint64 serial, const QString& product = "gpgfrontend") -> QString {
  return QStringLiteral("product=%1 serial=%2").arg(product).arg(serial);
}

}  // namespace

// ------------------------------------------------------------------ SemVer

TEST(SemVerTest, PrecedenceFollowsTheSpec) {
  const QStringList order{"1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-alpha.beta",
                          "1.0.0-beta",  "1.0.0-beta.2",  "1.0.0-beta.11",
                          "1.0.0-rc.1",  "1.0.0"};
  for (qsizetype i = 0; i + 1 < order.size(); ++i) {
    EXPECT_LT(SemVerCompare(order[i], order[i + 1]), 0)
        << order[i].toStdString() << " < " << order[i + 1].toStdString();
    EXPECT_GT(SemVerCompare(order[i + 1], order[i]), 0);
  }
}

TEST(SemVerTest, OurBuildsSortAsTheyAreMade) {
  // alpha (untagged) < beta < beta.dev < rc < final < next patch's alpha
  const QStringList order{"v2.3.0-alpha.400+gabc", "v2.4.0-alpha.1",
                          "v2.4.0-beta.1",         "v2.4.0-beta.1.dev.3",
                          "v2.4.0-rc.1",           "v2.4.0",
                          "v2.4.1-alpha.1"};
  for (qsizetype i = 0; i + 1 < order.size(); ++i) {
    EXPECT_LT(SemVerCompare(order[i], order[i + 1]), 0)
        << order[i].toStdString();
  }
  EXPECT_LT(SemVerCompare("2.3.0-alpha.9", "2.3.0-alpha.10"), 0);
}

TEST(SemVerTest, BuildMetadataAndPrefixDoNotOrder) {
  EXPECT_EQ(SemVerCompare("v2.3.0-alpha.3+gabc", "2.3.0-alpha.3+gdef.dirty"),
            0);
  EXPECT_EQ(SemVerCompare("v2.2.4", "2.2.4"), 0);
}

TEST(SemVerTest, NotSemVerFallsBackToNumbers) {
  EXPECT_FALSE(SemVer::Parse("v2.1").has_value());
  EXPECT_GT(SemVerCompare("v2.1.0", "v2.1"), 0);
  EXPECT_LT(SemVerCompare("v2.1", "v2.2"), 0);
}

TEST(SemVerTest, NightlyBuildsAreTheUntaggedOnes) {
  EXPECT_TRUE(IsNightlyBuild("v2.3.0-alpha.398+g0123456789ab.dirty"));
  EXPECT_TRUE(IsNightlyBuild("2.4.0-beta.1.dev.3+gabc"));
  EXPECT_FALSE(IsNightlyBuild("v2.4.0-beta.1"));
  EXPECT_FALSE(IsNightlyBuild("v2.4.0-rc.2"));
  EXPECT_FALSE(IsNightlyBuild("v2.2.4"));
  EXPECT_FALSE(IsNightlyBuild("garbage"));
}

// ---------------------------------------------------------------- minisign

TEST(MinisignTest, TheToolsSignaturesVerify) {
  EXPECT_EQ(MinisignVerify({kToolKey}, kToolMessage, kToolSignature),
            std::optional<QString>("product=gpgfrontend serial=7"));
  EXPECT_EQ(MinisignVerify({kToolKey}, kToolMessage, kToolLegacySignature),
            std::optional<QString>("legacy"));
}

TEST(MinisignTest, AnyPinnedKeyWillDo) {
  EXPECT_TRUE(
      MinisignVerify({kOtherToolKey, kToolKey}, kToolMessage, kToolSignature));
}

TEST(MinisignTest, EverythingElseIsRefused) {
  EXPECT_FALSE(MinisignVerify({kOtherToolKey}, kToolMessage, kToolSignature));
  EXPECT_FALSE(MinisignVerify({}, kToolMessage, kToolSignature));
  EXPECT_FALSE(MinisignVerify({kToolKey}, QByteArray(kToolMessage) + " ",
                              kToolSignature));

  // A trusted comment edited after signing: the global signature fails.
  auto edited = QByteArray(kToolSignature);
  edited.replace("serial=7", "serial=8");
  EXPECT_FALSE(MinisignVerify({kToolKey}, kToolMessage, edited));

  EXPECT_FALSE(MinisignVerify({kToolKey}, kToolMessage, "garbage"));
  EXPECT_FALSE(MinisignVerify({"not base64!"}, kToolMessage, kToolSignature));
}

TEST(MinisignTest, TheTestSignerSignsAsTheToolDoes) {
  Signer s;
  const auto sig = s.Sign("data", "hello");
  EXPECT_EQ(MinisignVerify({s.PublicKey()}, "data", sig),
            std::optional<QString>("hello"));
  EXPECT_FALSE(MinisignVerify({Signer().PublicKey()}, "data", sig));
}

// ------------------------------------------------------------------- index

TEST(ReleaseIndexTest, ReadsWhatItActsOn) {
  const auto f = ParseIndex(Index(5, "2.3.0-alpha.9+gabc", "gpgfrontend",
                                  kNow.addDays(14), {"2.2.3"}),
                            Comment(5), "gpgfrontend", kNow);
  ASSERT_TRUE(f.has_value());
  EXPECT_EQ(f->serial, 5);
  EXPECT_EQ(f->nightly_version, "2.3.0-alpha.9+gabc");
  EXPECT_EQ(f->withdrawn, QStringList{"2.2.3"});
  EXPECT_EQ(NightlyList(*f).outcome, ListOutcome::kFound);
  EXPECT_EQ(WithdrawnFact(*f, "v2.2.3"), RemoteFact::kNotFound);
  EXPECT_EQ(WithdrawnFact(*f, "v2.2.4"), RemoteFact::kConfirmed);
}

TEST(ReleaseIndexTest, AnotherProductsIndexIsRefused) {
  // In the document, in the signed comment, or in both.
  EXPECT_FALSE(
      ParseIndex(Index(5, "", "demo"), Comment(5), "gpgfrontend", kNow));
  EXPECT_FALSE(
      ParseIndex(Index(5, ""), Comment(5, "demo"), "gpgfrontend", kNow));
  EXPECT_FALSE(ParseIndex(Index(5, "", "demo"), Comment(5, "demo"),
                          "gpgfrontend", kNow));
}

TEST(ReleaseIndexTest, TheSerialMustBeTheSignedOne) {
  EXPECT_FALSE(ParseIndex(Index(6, ""), Comment(5), "gpgfrontend", kNow));
}

TEST(ReleaseIndexTest, ExpiryHasADaysGrace) {
  EXPECT_TRUE(ParseIndex(Index(5, "", "gpgfrontend", kNow.addSecs(-3600)),
                         Comment(5), "gpgfrontend", kNow));
  EXPECT_FALSE(ParseIndex(Index(5, "", "gpgfrontend", kNow.addDays(-2)),
                          Comment(5), "gpgfrontend", kNow));
}

TEST(ReleaseIndexTest, NoNightlyIsAnAnswer) {
  const auto f = ParseIndex(Index(5, ""), Comment(5), "gpgfrontend", kNow);
  ASSERT_TRUE(f.has_value());
  EXPECT_EQ(NightlyList(*f).outcome, ListOutcome::kNoMatch);
}

TEST(ReleaseIndexTest, CommitMarkers) {
  EXPECT_EQ(ParseCommitMarker(200), RemoteFact::kConfirmed);
  EXPECT_EQ(ParseCommitMarker(404), RemoteFact::kNotFound);
  EXPECT_EQ(ParseCommitMarker(0), RemoteFact::kUnknown);
  EXPECT_EQ(ParseCommitMarker(503), RemoteFact::kUnknown);
}
