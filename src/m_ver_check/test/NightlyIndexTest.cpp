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

#include "IndexFixture.h"
#include "Minisign.h"
#include "ReleaseCheck.h"
#include "ReleaseIndex.h"
#include "SemVer.h"
#include "UpdateChecker.h"

// The nightly update path: SemVer order, the minisign check of the index,
// reading it, and the checker driving them against a fake set of mirrors.

using test_index::Comment;
using test_index::Index;
using test_index::kNow;
using test_index::Mirrors;
using test_index::Signer;

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

TEST(MinisignTest, KeyIdsAreTheToolsAndOnlyPrehashedSignaturesHaveOne) {
  const auto id = MinisignPublicKeyId(kToolKey);
  EXPECT_EQ(id.size(), 16);
  EXPECT_EQ(id, id.toUpper());
  EXPECT_EQ(MinisignSignatureKeyId(kToolSignature), id);
  // The update trust takes prehashed signatures only.
  EXPECT_TRUE(MinisignSignatureKeyId(kToolLegacySignature).isEmpty());
  EXPECT_TRUE(MinisignSignatureKeyId("garbage").isEmpty());
  EXPECT_TRUE(MinisignPublicKeyId("not a key").isEmpty());
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
  EXPECT_EQ(ChooseUpdate(*f, "v2.3.0-alpha.5", true).list.outcome,
            ListOutcome::kFound);
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

TEST(ReleaseIndexTest, AnIndexSaysWhenItWasSignedAndIsShortLived) {
  const auto with = [](const QJsonValue& generated_at,
                       const QDateTime& expires = kNow.addDays(14)) {
    return ParseIndex(Index(5, "", "gpgfrontend", expires, {},
                            {{"generated_at", generated_at}}),
                      Comment(5), "gpgfrontend", kNow);
  };
  EXPECT_TRUE(with(kNow.toString(Qt::ISODate)));
  // Without it, or not a time: refused, as anything else unreadable.
  EXPECT_FALSE(with(QJsonValue()));
  EXPECT_FALSE(with("yesterday"));
  // Valid for longer than any index the host signs: whoever signed it, it is
  // not replayed to a client for a year.
  EXPECT_FALSE(with(kNow.toString(Qt::ISODate), kNow.addDays(365)));
  EXPECT_TRUE(with(kNow.addDays(-1).toString(Qt::ISODate), kNow.addDays(59)));
}

TEST(ReleaseIndexTest, NoNightlyIsAnAnswer) {
  const auto f = ParseIndex(Index(5, ""), Comment(5), "gpgfrontend", kNow);
  ASSERT_TRUE(f.has_value());
  EXPECT_EQ(ChooseUpdate(*f, "v2.3.0-alpha.5", true).list.outcome,
            ListOutcome::kNoMatch);
}

TEST(ReleaseIndexTest, CommitMarkers) {
  EXPECT_EQ(ParseCommitMarker(200), RemoteFact::kConfirmed);
  EXPECT_EQ(ParseCommitMarker(404), RemoteFact::kNotFound);
  EXPECT_EQ(ParseCommitMarker(0), RemoteFact::kUnknown);
  EXPECT_EQ(ParseCommitMarker(503), RemoteFact::kUnknown);
}

// ----------------------------------------------------------------- checker

namespace {

constexpr auto kCommit = "0123456789abcdef0123456789abcdef01234567";
constexpr auto kMirrorA = "https://a.example/";
constexpr auto kMirrorB = "https://b.example/";

struct Nightly {
  Signer signer;
  Mirrors mirrors;
  QByteArray stored;

  auto Channel() const -> ReleaseChannel {
    return {
        "gpgfrontend", {kMirrorA, kMirrorB}, {kMirrorA}, {signer.PublicKey()}};
  }

  auto Make(const QString& version = "v2.3.0-alpha.5+g0123456789ab")
      -> std::unique_ptr<UpdateChecker> {
    return std::make_unique<UpdateChecker>(
        UpdateChecker::Build{version, kCommit, Channel()}, mirrors.Fetcher(),
        UpdateChecker::Store{[this] { return stored; },
                             [this](const QByteArray& d) { stored = d; }},
        [] { return kNow; });
  }

  void Publish(const QString& base, qint64 serial, const QString& nightly,
               const QStringList& withdrawn = {}) {
    const auto index =
        Index(serial, nightly, "gpgfrontend", kNow.addDays(14), withdrawn);
    mirrors.Serve(base, index, signer.Sign(index, Comment(serial)));
  }

  void MarkOfficial() {
    mirrors.table[QString(kMirrorA) + "gpgfrontend/commits/" + kCommit] = {200,
                                                                           {}};
  }

  auto Verdict(UpdateChecker& c) -> ::Verdict {
    return c.State().last_good ? Decide(*c.State().last_good)
                               : Verdict::kUnknown;
  }
};

}  // namespace

TEST(NightlyCheckTest, ANewerNightlyIsAnUpdate) {
  Nightly n;
  n.Publish(kMirrorA, 3, "2.3.0-alpha.9+gffff");
  n.MarkOfficial();
  auto c = n.Make();
  c->Start(CheckMode::kForce);

  EXPECT_FALSE(c->State().checking);
  EXPECT_EQ(n.Verdict(*c), Verdict::kUpdateAvailable);
  EXPECT_EQ(c->State().last_good->list.latest.version, "2.3.0-alpha.9+gffff");
  EXPECT_EQ(c->State().max_serial, 3);
  EXPECT_TRUE(IsAuthoritative(*c->State().last_good));
  // No GitHub question is asked by a nightly.
  for (const auto& url : n.mirrors.asked) {
    EXPECT_FALSE(url.contains("api.github.com")) << url.toStdString();
  }
}

TEST(NightlyCheckTest, TheSameNightlyIsUpToDate) {
  Nightly n;
  n.Publish(kMirrorA, 3, "2.3.0-alpha.5+g0123456789ab");
  n.MarkOfficial();
  auto c = n.Make();
  c->Start(CheckMode::kForce);
  EXPECT_EQ(n.Verdict(*c), Verdict::kUpToDate);
}

TEST(NightlyCheckTest, AnOlderBuildOfTheSameNumberIsNotNewer) {
  // Build metadata never orders: only the alpha number does.
  Nightly n;
  n.Publish(kMirrorA, 3, "2.3.0-alpha.4+gffff");
  n.MarkOfficial();
  auto c = n.Make();
  c->Start(CheckMode::kForce);
  EXPECT_EQ(n.Verdict(*c), Verdict::kUpToDate);
}

TEST(NightlyCheckTest, AMissingFirstMirrorFallsThrough) {
  Nightly n;
  n.Publish(kMirrorB, 3, "2.3.0-alpha.9");
  n.MarkOfficial();
  auto c = n.Make();
  c->Start(CheckMode::kForce);
  EXPECT_EQ(n.Verdict(*c), Verdict::kUpdateAvailable);
}

TEST(NightlyCheckTest, ABadSignatureOnTheFirstMirrorFallsThrough) {
  Nightly n;
  const auto forged = Index(4, "9.9.9");
  n.mirrors.Serve(kMirrorA, forged, Signer().Sign(forged, Comment(4)));
  n.Publish(kMirrorB, 3, "2.3.0-alpha.9");
  n.MarkOfficial();
  auto c = n.Make();
  c->Start(CheckMode::kForce);
  EXPECT_EQ(c->State().last_good->list.latest.version, "2.3.0-alpha.9");
  EXPECT_EQ(c->State().max_serial, 3);
}

TEST(NightlyCheckTest, NothingVerifiedIsUnknownNeverAVerdict) {
  Nightly n;
  const auto forged = Index(4, "9.9.9");
  n.mirrors.Serve(kMirrorA, forged, Signer().Sign(forged, Comment(4)));
  auto c = n.Make();
  c->Start(CheckMode::kForce);
  EXPECT_TRUE(c->State().last_attempt_failed);
  EXPECT_EQ(n.Verdict(*c), Verdict::kUnknown);
  // The commit marker is asked only after an index verified.
  for (const auto& url : n.mirrors.asked) {
    EXPECT_FALSE(url.contains("/commits/")) << url.toStdString();
  }
}

TEST(NightlyCheckTest, AnOlderIndexThanOneSeenIsAReplay) {
  Nightly n;
  n.Publish(kMirrorA, 10, "2.3.0-alpha.9");
  n.MarkOfficial();
  n.Make()->Start(CheckMode::kForce);

  // Mirror A now serves a validly signed but older index; B the newest.
  n.Publish(kMirrorA, 9, "2.3.0-alpha.7");
  n.Publish(kMirrorB, 11, "2.3.0-alpha.12");
  auto c = n.Make();
  ASSERT_EQ(c->State().max_serial, 10);  // remembered across runs
  c->Start(CheckMode::kForce);
  EXPECT_EQ(c->State().last_good->list.latest.version, "2.3.0-alpha.12");
  EXPECT_EQ(c->State().max_serial, 11);

  // Only the replay left: refused, the last good result stands.
  n.mirrors.table.erase(QString(kMirrorB) + "gpgfrontend/v1/index.json");
  n.Publish(kMirrorA, 9, "2.3.0-alpha.7");
  auto d = n.Make();
  d->Start(CheckMode::kForce);
  EXPECT_TRUE(d->State().last_attempt_failed);
  EXPECT_EQ(d->State().last_good->list.latest.version, "2.3.0-alpha.12");
}

TEST(NightlyCheckTest, AnUnknownCommitIsAnUnofficialBuild) {
  Nightly n;
  n.Publish(kMirrorA, 3, "2.3.0-alpha.5");
  auto c = n.Make();  // no marker for kCommit
  c->Start(CheckMode::kForce);
  EXPECT_EQ(n.Verdict(*c), Verdict::kUnofficialBuild);
}

TEST(NightlyCheckTest, AWithdrawnVersionSaysSo) {
  Nightly n;
  n.Publish(kMirrorA, 3, "2.3.0-alpha.5", {"2.3.0-alpha.5"});
  n.MarkOfficial();
  auto c = n.Make("v2.3.0-alpha.5");
  c->Start(CheckMode::kForce);
  EXPECT_EQ(n.Verdict(*c), Verdict::kWithdrawnOrUnreleased);
}

TEST(NightlyCheckTest, NoChannelNoRequestNoClaim) {
  Nightly n;
  auto c = std::make_unique<UpdateChecker>(
      UpdateChecker::Build{"v2.3.0-alpha.5", kCommit, {}}, n.mirrors.Fetcher(),
      UpdateChecker::Store{}, [] { return kNow; });
  c->Start(CheckMode::kForce);
  EXPECT_TRUE(n.mirrors.asked.isEmpty());
  EXPECT_FALSE(c->State().checking);
  EXPECT_FALSE(c->State().last_good.has_value());
}

TEST(NightlyCheckTest, AReleaseBuildReadsTheSignedIndexToo) {
  Nightly n;
  n.Publish(kMirrorA, 3, "2.3.0-alpha.9");
  auto c = std::make_unique<UpdateChecker>(
      UpdateChecker::Build{"v2.2.4", kCommit, n.Channel()}, n.mirrors.Fetcher(),
      UpdateChecker::Store{}, [] { return kNow; });
  c->Start(CheckMode::kForce);
  ASSERT_FALSE(n.mirrors.asked.isEmpty());
  for (const auto& url : n.mirrors.asked) {
    EXPECT_FALSE(url.contains("github.com")) << url.toStdString();
  }
  // The index lists no 2.2 release: nothing is offered, and no nightly.
  ASSERT_TRUE(c->State().last_good.has_value());
  EXPECT_EQ(c->State().last_good->list.outcome, ListOutcome::kNoMatch);
}

TEST(NightlyCheckTest, TheSerialSurvivesAnotherBuild) {
  Nightly n;
  n.Publish(kMirrorA, 7, "2.3.0-alpha.9");
  n.MarkOfficial();
  n.Make()->Start(CheckMode::kForce);
  // An upgrade to another build keeps what was seen, even with no result.
  const auto s =
      UpdateChecker::FromStorage(n.stored, {"v2.3.0-alpha.9", "x", {}});
  EXPECT_EQ(s.max_serial, 7);
  EXPECT_FALSE(s.last_good.has_value());
}

// What the module's hourly timer does: kIfStale on every tick. The network is
// asked once a day after a success, and again within the hour after a
// failure.
TEST(NightlyCheckTest, HourlyTicksAskOnceADay) {
  Nightly n;
  n.Publish(kMirrorA, 3, "2.3.0-alpha.9");
  n.MarkOfficial();
  QDateTime now = kNow;
  auto c = std::make_unique<UpdateChecker>(
      UpdateChecker::Build{"v2.3.0-alpha.5+g0123456789ab", kCommit,
                           n.Channel()},
      n.mirrors.Fetcher(),
      UpdateChecker::Store{[&n] { return n.stored; },
                           [&n](const QByteArray& d) { n.stored = d; }},
      [&now] { return now; });

  const auto tick = [&] {
    const auto before = n.mirrors.asked.size();
    c->Start(CheckMode::kIfStale);
    return n.mirrors.asked.size() > before;
  };

  EXPECT_TRUE(tick());  // startup: nothing known yet
  for (int hour = 1; hour < 24; ++hour) {
    now = kNow.addSecs(hour * 3600);
    EXPECT_FALSE(tick()) << "asked again after " << hour << " h";
  }
  now = kNow.addSecs(24 * 3600 + 60);
  EXPECT_TRUE(tick());  // a day on

  // The mirrors fail: the next tick retries, it does not wait a day.
  n.mirrors.table.clear();
  now = now.addSecs(24 * 3600 + 60);
  EXPECT_TRUE(tick());
  EXPECT_TRUE(c->State().last_attempt_failed);
  now = now.addSecs(3600);
  EXPECT_TRUE(tick());
}

// The Help-menu badge for a nightly: one offer seen, the next waits a day.
TEST(NightlyCheckTest, ANightlyBadgeComesAtMostOnceADay) {
  Nightly n;
  QDateTime now = kNow;
  const auto make = [&] {
    return std::make_unique<UpdateChecker>(
        UpdateChecker::Build{"v2.3.0-alpha.5+g0123456789ab", kCommit,
                             n.Channel()},
        n.mirrors.Fetcher(),
        UpdateChecker::Store{[&n] { return n.stored; },
                             [&n](const QByteArray& d) { n.stored = d; }},
        [&now] { return now; });
  };
  n.MarkOfficial();

  n.Publish(kMirrorA, 3, "2.3.0-alpha.9");
  auto c = make();
  c->Start(CheckMode::kForce);
  EXPECT_EQ(c->Attention(), UpdateAttention::kUpdateAvailable);
  c->Acknowledge();  // the user opened the dialog
  EXPECT_EQ(c->Attention(), UpdateAttention::kNone);

  // Two hours on, main has moved: the dialog knows, the badge waits.
  now = kNow.addSecs(2 * 3600);
  n.Publish(kMirrorA, 4, "2.3.0-alpha.12");
  c->Start(CheckMode::kForce);
  EXPECT_EQ(c->State().last_good->list.latest.version, "2.3.0-alpha.12");
  EXPECT_EQ(c->Attention(), UpdateAttention::kNone);

  // A day after the last one was seen, it may ask again, across a restart.
  now = kNow.addSecs(24 * 3600 + 1);
  // The stored result of this same build is enough: no check needed first.
  auto restarted = make();
  EXPECT_EQ(restarted->Attention(), UpdateAttention::kUpdateAvailable);
  restarted->Acknowledge();
  EXPECT_EQ(restarted->Attention(), UpdateAttention::kNone);
}

TEST(NightlyCheckTest, AReleaseBuildBadgeIsNotHeldBack) {
  UpdateSnapshot s;
  SoftwareVersion r;
  r.current_version = "v2.2.4";
  r.list.outcome = ListOutcome::kFound;
  r.list.latest.version = "v2.2.6";
  r.tag_fact = RemoteFact::kConfirmed;
  r.commit_fact = RemoteFact::kConfirmed;
  s.last_good = r;
  s.last_seen_update_version = "v2.2.5";
  s.last_seen_at = kNow.addSecs(-3600);  // seen an hour ago
  EXPECT_EQ(AttentionFor(s, kNow), UpdateAttention::kUpdateAvailable);
}

// ------------------------------------------------------------- downloads

namespace {

// The shape the signing host writes (scripts/release/gfrelease promote).
auto Descriptor() -> QByteArray {
  const auto art = [](const char* os, const char* arch, const char* kind,
                      const char* flavor, const char* name,
                      const char* macos = nullptr) {
    QJsonObject a{
        {"os", os},
        {"arch", arch},
        {"kind", kind},
        {"flavor", flavor},
        {"path", QStringLiteral("nightly/2.3.0-alpha.9/%1").arg(name)}};
    if (macos != nullptr) a["min_os"] = QJsonObject{{"macos", macos}};
    return a;
  };
  QJsonArray artifacts{
      art("linux", "x86_64", "appimage", "installed", "linux-x86_64.AppImage"),
      art("linux", "x86_64", "appimage", "portable",
          "linux-x86_64-portable.AppImage"),
      art("linux", "aarch64", "appimage", "installed",
          "linux-aarch64.AppImage"),
      art("windows", "x86_64", "msi", "installed", "windows-x86_64.msi"),
      art("windows", "x86_64", "zip", "portable",
          "windows-x86_64-portable.zip"),
      art("macos", "arm64", "dmg", "installed", "macos-15.dmg", "15.0"),
      art("macos", "arm64", "dmg", "installed", "macos-26.dmg", "26.0"),
      art("macos", "x86_64", "dmg", "installed", "macos-15-intel.dmg", "15.0"),
  };
  return QJsonDocument(QJsonObject{{"artifacts", artifacts}}).toJson();
}

auto Pick(const char* os, const char* arch, const char* version = "",
          const char* kind = "") -> QString {
  return SelectArtifact(Descriptor(), {os, arch, version, kind})
      .path.section(u'/', -1);
}

}  // namespace

TEST(DownloadTest, TheInstalledFileForThisMachine) {
  EXPECT_EQ(Pick("linux", "x86_64"), "linux-x86_64.AppImage");
  EXPECT_EQ(Pick("linux", "arm64"), "linux-aarch64.AppImage");  // same arch
  EXPECT_EQ(Pick("windows", "x86_64"), "windows-x86_64.msi");
  EXPECT_EQ(Pick("windows", "arm64"), "");  // nothing built for it
}

TEST(DownloadTest, MacOSTakesTheNewestItRuns) {
  EXPECT_EQ(Pick("macos", "arm64", "26.1"), "macos-26.dmg");
  EXPECT_EQ(Pick("macos", "arm64", "15.6"), "macos-15.dmg");
  EXPECT_EQ(Pick("macos", "arm64", "14.7"), "");  // too old for any
  EXPECT_EQ(Pick("macos", "x86_64", "26.1"), "macos-15-intel.dmg");
}

TEST(DownloadTest, AnUnsafePathIsNeverOffered) {
  QJsonArray artifacts{QJsonObject{{"os", "linux"},
                                   {"arch", "x86_64"},
                                   {"flavor", "installed"},
                                   {"path", "../other/evil.AppImage"}}};
  EXPECT_EQ(SelectArtifact(
                QJsonDocument(QJsonObject{{"artifacts", artifacts}}).toJson(),
                {"linux", "x86_64", ""})
                .path,
            "");
  for (const auto* bad : {"", "a/../b", "/etc/x", "https://x/y", "a%2fb",
                          "a//b", "./a", "a/b\n", "a/b\n/c"}) {
    EXPECT_FALSE(IsSafeRelativePath(bad)) << bad;
  }
  EXPECT_TRUE(IsSafeRelativePath("nightly/2.3.0-alpha.9-gabc/release.json"));
}

// What an in-place update checks its download against comes with the path,
// and only in the shape the signing host writes.
TEST(DownloadTest, TheChosenFileComesWithItsHashAndSize) {
  const auto digest = QString(64, u'0');
  QJsonArray artifacts{QJsonObject{{"os", "linux"},
                                   {"arch", "x86_64"},
                                   {"kind", "appimage"},
                                   {"flavor", "installed"},
                                   {"path", "nightly/x/linux.AppImage"},
                                   {"sha256", digest},
                                   {"size", 123456}}};
  const auto doc =
      QJsonDocument(QJsonObject{{"artifacts", artifacts}}).toJson();
  auto c = SelectArtifact(doc, {"linux", "x86_64", "", "appimage"});
  EXPECT_EQ(c.path, "nightly/x/linux.AppImage");
  EXPECT_EQ(c.sha256, digest);
  EXPECT_EQ(c.size, 123456);

  // A hash that is not one, or no size: the path stays a link, nothing more.
  auto a = artifacts[0].toObject();
  a["sha256"] = "ABC";
  a.remove("size");
  c = SelectArtifact(
      QJsonDocument(QJsonObject{{"artifacts", QJsonArray{a}}}).toJson(),
      {"linux", "x86_64", "", "appimage"});
  EXPECT_EQ(c.path, "nightly/x/linux.AppImage");
  EXPECT_TRUE(c.sha256.isEmpty());
  EXPECT_EQ(c.size, 0);
}

namespace {

auto WithDescriptor(QByteArray index, const QString& path,
                    const QByteArray& descriptor) -> QByteArray {
  auto obj = QJsonDocument::fromJson(index).object();
  obj["nightly"] =
      QJsonObject{{"version", obj["channels"].toObject()["nightly"]},
                  {"descriptor", path},
                  {"sha256", Sha256Hex(descriptor)}};
  return QJsonDocument(obj).toJson();
}

}  // namespace

TEST(DownloadTest, TheIndexNamesItsNightlysDescriptor) {
  const auto index =
      WithDescriptor(Index(5, "2.3.0-alpha.9"),
                     "nightly/2.3.0-alpha.9/release.json", Descriptor());
  const auto f = ParseIndex(index, Comment(5), "gpgfrontend", kNow);
  ASSERT_TRUE(f.has_value());
  EXPECT_EQ(f->nightly_descriptor, "nightly/2.3.0-alpha.9/release.json");
  EXPECT_EQ(f->nightly_descriptor_sha256.size(), 64);

  // An unsafe path is dropped, and the index still stands.
  const auto bad = WithDescriptor(Index(5, "2.3.0-alpha.9"),
                                  "../demo/release.json", Descriptor());
  const auto g = ParseIndex(bad, Comment(5), "gpgfrontend", kNow);
  ASSERT_TRUE(g.has_value());
  EXPECT_TRUE(g->nightly_descriptor.isEmpty());
}

TEST(DownloadTest, TheCheckerLinksTheVerifiedFile) {
  Nightly n;
  const auto descriptor = Descriptor();
  const QString path = "nightly/2.3.0-alpha.9/release.json";
  const auto index =
      WithDescriptor(Index(3, "2.3.0-alpha.9", "gpgfrontend", kNow.addDays(14)),
                     path, descriptor);
  n.mirrors.Serve(kMirrorA, index, n.signer.Sign(index, Comment(3)));
  n.mirrors.table[QString(kMirrorA) + "gpgfrontend/" + path] = {200,
                                                                descriptor};
  n.MarkOfficial();

  auto c = std::make_unique<UpdateChecker>(
      UpdateChecker::Build{"v2.3.0-alpha.5", kCommit, n.Channel(),
                           Platform{"windows", "x86_64", ""}},
      n.mirrors.Fetcher(), UpdateChecker::Store{}, [] { return kNow; });
  c->Start(CheckMode::kForce);
  EXPECT_EQ(Decide(*c->State().last_good), Verdict::kUpdateAvailable);
  EXPECT_EQ(c->State().last_good->list.latest.html_url,
            QString(kMirrorA) +
                "gpgfrontend/nightly/2.3.0-alpha.9/windows-x86_64.msi");

  // A descriptor that is not the one the index signed: no link, same verdict.
  n.mirrors.table[QString(kMirrorA) + "gpgfrontend/" + path] = {
      200, Descriptor().replace("windows-x86_64.msi", "evil.msi")};
  c->Start(CheckMode::kForce);
  EXPECT_EQ(Decide(*c->State().last_good), Verdict::kUpdateAvailable);
  EXPECT_TRUE(c->State().last_good->list.latest.html_url.isEmpty());
}

TEST(DownloadTest, TheSameKindOfPackageAsTheRunningOne) {
  EXPECT_EQ(Pick("linux", "x86_64", "", "appimage"), "linux-x86_64.AppImage");
  EXPECT_EQ(Pick("linux", "x86_64", "", "appimage-portable"),
            "linux-x86_64-portable.AppImage");
  EXPECT_EQ(Pick("windows", "x86_64", "", "msi"), "windows-x86_64.msi");
  EXPECT_EQ(Pick("windows", "x86_64", "", "portable-zip"),
            "windows-x86_64-portable.zip");
  EXPECT_EQ(Pick("macos", "arm64", "26.1", "dmg"), "macos-26.dmg");
  // Kept up to date by something else: no file to download.
  for (const auto* kind : {"mas", "flatpak", "distro", "msix", "android"}) {
    EXPECT_EQ(Pick("linux", "x86_64", "", kind), "") << kind;
  }
  // From a checkout, or a Host too old to say: the installed package.
  EXPECT_EQ(Pick("windows", "x86_64", "", "source"), "windows-x86_64.msi");
  EXPECT_EQ(Pick("windows", "x86_64", "", ""), "windows-x86_64.msi");
}

// The Host says which application this is; the version's shape is only the
// fallback for a Host too old to say.
TEST(NightlyCheckTest, TheHostsWordBeatsTheVersionsShape) {
  Nightly n;
  n.Publish(kMirrorA, 3, "2.3.0-alpha.9");
  n.MarkOfficial();
  const auto make = [&](const QString& version, int nightly) {
    return std::make_unique<UpdateChecker>(
        UpdateChecker::Build{version, kCommit, n.Channel(), {}, nightly},
        n.mirrors.Fetcher(), UpdateChecker::Store{}, [] { return kNow; });
  };

  // A release whose version merely looks untagged reads its series'
  // releases, not the nightly channel.
  n.mirrors.asked.clear();
  auto release = make("v2.3.0-alpha.5", 0);
  release->Start(CheckMode::kForce);
  ASSERT_FALSE(n.mirrors.asked.isEmpty());
  EXPECT_TRUE(n.mirrors.asked.first().startsWith(kMirrorA));
  ASSERT_TRUE(release->State().last_good);
  EXPECT_FALSE(release->State().last_good->nightly);
  EXPECT_EQ(release->State().last_good->list.outcome, ListOutcome::kNoMatch);

  // The nightly application reads its channel whatever its version says.
  n.mirrors.asked.clear();
  auto c = make("v2.3.0", 1);
  c->Start(CheckMode::kForce);
  EXPECT_TRUE(n.mirrors.asked.first().startsWith(kMirrorA));
  EXPECT_TRUE(c->State().last_good->nightly);

  // And the record says so after a restart.
  const auto stored = UpdateChecker::ToStorage(c->State());
  const auto back = UpdateChecker::FromStorage(stored, {"v2.3.0", kCommit, {}});
  ASSERT_TRUE(back.last_good.has_value());
  EXPECT_TRUE(back.last_good->nightly);
}
