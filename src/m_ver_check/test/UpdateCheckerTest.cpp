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
#include <memory>
#include <vector>

#include "IndexFixture.h"
#include "ReleaseCheck.h"
#include "UpdateChecker.h"

// The checker's state machine, driven for a release build through the signed
// index: joining, caching, failing, acknowledging. Every request is held
// until the test answers it, as a slow network would.

namespace {

constexpr auto kCommitA = "abc1230000000000000000000000000000000000";
constexpr auto kCommitB = "def4560000000000000000000000000000000000";
constexpr auto kBase = "https://update.example/";

/// Holds every request until the test answers it, and answers as the
/// release server's buckets would.
struct FakeServer {
  struct Pending {
    QUrl url;
    UpdateChecker::Reply reply;
  };
  test_index::Signer signer;
  std::vector<Pending> pending;
  QStringList asked;
  int requests = 0;
  qint64 serial = 0;

  auto Fetcher() -> UpdateChecker::Fetcher {
    return [this](const QUrl& url, const UpdateChecker::Reply& reply) {
      ++requests;
      asked << url.toString();
      pending.push_back({url, reply});
    };
  }

  /// The index of a 2.1 series whose newest release is @p latest, with
  /// 2.1.8 (the running build) listed as well.
  auto Signed(const QString& latest) -> std::pair<QByteArray, QByteArray> {
    ++serial;
    const auto entry = [](const QString& v) {
      return QJsonObject{{"descriptor", "releases/" + v + "/release.json"},
                         {"sha256", QString(64, u'0')}};
    };
    const QJsonObject extra{
        {"series", QJsonObject{{"2.1", QJsonObject{{"status", "stable"},
                                                   {"latest", latest}}}}},
        {"releases",
         QJsonObject{{"2.1.8", entry("2.1.8")}, {latest, entry(latest)}}}};
    const auto index = test_index::Index(serial, "", "gpgfrontend",
                                         Now().addDays(14), {}, extra);
    return {index, signer.Sign(index, test_index::Comment(serial))};
  }

  /// Answer every held request (and those the answers lead to), @p ok as
  /// the server would, or all failing.
  void AnswerAll(bool ok, const QString& latest = "2.1.9",
                 int marker_status = 200) {
    const auto [index, signature] = Signed(latest);
    while (!pending.empty()) {
      auto held = std::move(pending);
      pending.clear();
      for (auto& p : held) {
        const auto url = p.url.toString();
        if (!ok) {
          p.reply(0, {});
        } else if (url.endsWith("/v1/index.json")) {
          p.reply(200, index);
        } else if (url.endsWith("/v1/index.json.minisig")) {
          p.reply(200, signature);
        } else if (url.contains("/commits/")) {
          p.reply(marker_status, {});
        } else {
          p.reply(404, {});  // descriptors: no download link, no verdict
        }
      }
    }
  }

  /// The index answers; the commit marker hits a server error.
  void AnswerPartial() { AnswerAll(true, "2.1.8", 503); }

  static auto Now() -> QDateTime {
    return QDateTime::fromSecsSinceEpoch(1800000000);
  }
};

auto Channel(const FakeServer& server) -> ReleaseChannel {
  return {"gpgfrontend", {kBase}, {kBase}, {server.signer.PublicKey()}};
}

struct Harness {
  FakeServer server;
  QByteArray stored;
  QDateTime now = FakeServer::Now();
  int saves = 0;

  auto Build(const QString& version = "v2.1.8",
             const QString& commit = kCommitA) const -> UpdateChecker::Build {
    return {version, commit, Channel(server)};
  }

  auto Make() -> std::unique_ptr<UpdateChecker> { return Make(Build()); }

  auto Make(const UpdateChecker::Build& build)
      -> std::unique_ptr<UpdateChecker> {
    return std::make_unique<UpdateChecker>(
        build, server.Fetcher(),
        UpdateChecker::Store{[this] { return stored; },
                             [this](const QByteArray& d) {
                               stored = d;
                               ++saves;
                             }},
        [this] { return now; });
  }
};

}  // namespace

TEST(UpdateCheckerTest, StartIsVisibleSynchronously) {
  Harness h;
  auto c = h.Make();
  int changes = 0;
  QObject::connect(c.get(), &UpdateChecker::Changed, [&] { ++changes; });

  c->Start(CheckMode::kIfStale);
  EXPECT_TRUE(c->State().checking);
  EXPECT_EQ(changes, 1);
  EXPECT_EQ(h.server.requests, 1);  // the index first; the rest follows it

  h.server.AnswerAll(true);
  EXPECT_FALSE(c->State().checking);
  EXPECT_EQ(changes, 2);
  ASSERT_TRUE(c->State().last_good);
  EXPECT_EQ(Decide(*c->State().last_good), Verdict::kUpdateAvailable);
  EXPECT_TRUE(c->State().last_good->complete);
  EXPECT_EQ(h.saves, 1);
}

TEST(UpdateCheckerTest, AnAnswerTooLargeIsNoAnswer) {
  Harness h;
  auto c = h.Make();
  c->Start(CheckMode::kForce);
  ASSERT_EQ(h.server.pending.size(), 1U);
  auto held = std::move(h.server.pending);
  h.server.pending.clear();
  held.front().reply(200, QByteArray(UpdateChecker::kMaxFetchBytes + 1, '{'));
  // The only base: nothing verified, nothing claimed.
  EXPECT_FALSE(c->State().checking);
  EXPECT_FALSE(c->State().last_good);
  EXPECT_TRUE(c->State().last_attempt_failed);
}

TEST(UpdateCheckerTest, AReplyAnsweredTwiceFinishesOnce) {
  Harness h;
  auto c = h.Make();
  c->Start(CheckMode::kForce);
  const auto [index, signature] = h.server.Signed("2.1.9");
  while (!h.server.pending.empty()) {
    auto held = std::move(h.server.pending);
    h.server.pending.clear();
    for (auto& p : held) {
      const auto url = p.url.toString();
      if (url.endsWith("/v1/index.json")) {
        p.reply(200, index);
      } else if (url.endsWith(".minisig")) {
        p.reply(200, signature);
      } else if (url.contains("/commits/")) {
        p.reply(200, {});
        p.reply(200, {});  // a transport that calls back twice
      } else {
        p.reply(404, {});
      }
    }
  }
  EXPECT_EQ(h.saves, 1);
  EXPECT_FALSE(c->State().checking);
}

TEST(UpdateCheckerTest, ABetaSettingChangedDuringACheckAsksAgain) {
  Harness h;
  auto c = h.Make();
  c->Start(CheckMode::kIfStale);
  c->SetFollowBeta(true);
  // The running check asked without betas; once done, another asks with.
  h.server.AnswerAll(true);
  EXPECT_EQ(h.server.asked.filter("/v1/index.json.minisig").size(), 2);
  ASSERT_TRUE(c->State().last_good);
  EXPECT_TRUE(c->State().last_good->follow_beta);
  EXPECT_TRUE(c->IsFresh());

  // Set back and forth while nothing runs: no check of its own.
  const auto before = h.server.requests;
  c->SetFollowBeta(false);
  c->SetFollowBeta(true);
  EXPECT_EQ(h.server.requests, before);
}

TEST(UpdateCheckerTest, AReleaseTheIndexDoesNotListIsAskedDaily) {
  Harness h;
  // 2.1.7: a release from before the index lists it.
  auto c = h.Make(h.Build("v2.1.7"));
  c->Start(CheckMode::kIfStale);
  h.server.AnswerAll(true);
  ASSERT_TRUE(c->State().last_good);
  EXPECT_EQ(c->State().last_good->tag_fact, RemoteFact::kUnknown);
  // "Not listed" is an answer: no new check on the next hourly tick.
  EXPECT_TRUE(c->IsFresh());
  const auto before = h.server.requests;
  c->Start(CheckMode::kIfStale);
  EXPECT_EQ(h.server.requests, before);
  // Still known after a restart.
  EXPECT_TRUE(h.Make(h.Build("v2.1.7"))->IsFresh());

  // A marker the server could not answer is a lookup that failed: asked
  // again on the next tick.
  Harness g;
  auto d = g.Make(g.Build("v2.1.7"));
  d->Start(CheckMode::kIfStale);
  g.server.AnswerPartial();
  EXPECT_FALSE(d->IsFresh());
}

TEST(UpdateCheckerTest, AClockSetBackIsNotFreshForEver) {
  Harness h;
  auto c = h.Make();
  c->Start(CheckMode::kIfStale);
  h.server.AnswerAll(true);
  ASSERT_TRUE(c->IsFresh());
  // Checked "in the future": how old the result is cannot be told.
  h.now = h.now.addDays(-3);
  EXPECT_FALSE(c->IsFresh());
}

TEST(UpdateCheckerTest, ADamagedStoreOnlyForgetsTheSerial) {
  Harness h;
  for (const auto* bad :
       {R"({"schema":2,"max_serial":-5})", R"({"schema":2,"max_serial":"9"})",
        R"({"schema":2,"max_serial":1.5})"}) {
    EXPECT_EQ(UpdateChecker::FromStorage(bad, h.Build()).max_serial, 0) << bad;
  }
  EXPECT_EQ(
      UpdateChecker::FromStorage(R"({"schema":2,"max_serial":7})", h.Build())
          .max_serial,
      7);
}

TEST(UpdateCheckerTest, SecondStartJoinsTheRunningCheck) {
  Harness h;
  auto c = h.Make();
  c->Start(CheckMode::kIfStale);
  c->Start(CheckMode::kForce);
  c->Start(CheckMode::kIfStale);
  EXPECT_EQ(h.server.requests, 1);

  int changes = 0;
  QObject::connect(c.get(), &UpdateChecker::Changed, [&] { ++changes; });
  h.server.AnswerAll(true);
  EXPECT_EQ(changes, 1);
  EXPECT_FALSE(c->State().checking);
}

TEST(UpdateCheckerTest, FailedRefreshKeepsLastKnownGood) {
  Harness h;
  auto c = h.Make();
  c->Start(CheckMode::kForce);
  h.server.AnswerAll(true);
  const auto first_checked = c->State().last_good->checked_at;

  h.now = h.now.addSecs(3600);
  c->Start(CheckMode::kForce);
  EXPECT_TRUE(c->State().checking);
  // The previous result stays readable while checking.
  ASSERT_TRUE(c->State().last_good);
  h.server.AnswerAll(false);

  const auto& s = c->State();
  EXPECT_TRUE(s.last_attempt_failed);
  EXPECT_EQ(s.last_attempt, h.now);
  ASSERT_TRUE(s.last_good);
  EXPECT_EQ(s.last_good->checked_at, first_checked);
  EXPECT_EQ(s.last_good->list.latest.version, "2.1.9");
  EXPECT_EQ(Decide(*s.last_good), Verdict::kUpdateAvailable);

  // And it survives a restart.
  auto restarted = h.Make();
  ASSERT_TRUE(restarted->State().last_good);
  EXPECT_EQ(restarted->State().last_good->list.latest.version, "2.1.9");
  EXPECT_TRUE(restarted->State().last_attempt_failed);
}

TEST(UpdateCheckerTest, SuccessfulRefreshClearsFailure) {
  Harness h;
  auto c = h.Make();
  c->Start(CheckMode::kForce);
  h.server.AnswerAll(false);
  EXPECT_TRUE(c->State().last_attempt_failed);
  EXPECT_FALSE(c->State().last_good);

  c->Start(CheckMode::kForce);
  h.server.AnswerAll(true, "2.1.8");
  EXPECT_FALSE(c->State().last_attempt_failed);
  ASSERT_TRUE(c->State().last_good);
  EXPECT_EQ(Decide(*c->State().last_good), Verdict::kUpToDate);
}

TEST(UpdateCheckerTest, ForceBypassesFreshCache) {
  Harness h;
  auto c = h.Make();
  c->Start(CheckMode::kForce);
  h.server.AnswerAll(true);
  EXPECT_TRUE(c->IsFresh());

  const int asked = h.server.requests;
  c->Start(CheckMode::kIfStale);
  EXPECT_FALSE(c->State().checking);
  EXPECT_EQ(h.server.requests, asked);

  c->Start(CheckMode::kForce);
  EXPECT_TRUE(c->State().checking);
  EXPECT_EQ(h.server.requests, asked + 1);
}

TEST(UpdateCheckerTest, StaleCacheIsRechecked) {
  Harness h;
  {
    auto c = h.Make();
    c->Start(CheckMode::kForce);
    h.server.AnswerAll(true);
  }
  h.now = h.now.addSecs(UpdateChecker::kFreshSeconds + 1);
  auto c = h.Make();
  EXPECT_FALSE(c->IsFresh());
  c->Start(CheckMode::kIfStale);
  EXPECT_TRUE(c->State().checking);
}

TEST(UpdateCheckerTest, PartialResultDoesNotSuppressRetry) {
  Harness h;
  auto c = h.Make();
  c->Start(CheckMode::kIfStale);
  h.server.AnswerPartial();

  const auto& s = c->State();
  EXPECT_FALSE(s.last_attempt_failed);
  ASSERT_TRUE(s.last_good);
  EXPECT_FALSE(s.last_good->complete);
  EXPECT_EQ(s.last_good->commit_fact, RemoteFact::kUnknown);
  // A server error is not an unofficial build.
  EXPECT_EQ(Decide(*s.last_good), Verdict::kUpToDate);
  EXPECT_FALSE(c->IsFresh());

  c->Start(CheckMode::kIfStale);
  EXPECT_TRUE(c->State().checking);
}

TEST(UpdateCheckerTest, StoredResultForAnotherBuildIsDropped) {
  Harness h;
  {
    auto c = h.Make();
    c->Start(CheckMode::kForce);
    h.server.AnswerAll(true);
  }
  auto upgraded = h.Make(h.Build("v2.1.9", kCommitB));
  EXPECT_FALSE(upgraded->State().last_good);
  EXPECT_FALSE(upgraded->IsFresh());

  auto same = h.Make();
  EXPECT_TRUE(same->State().last_good);
}

TEST(UpdateCheckerTest, StorageRejectsGarbageAndOldFormat) {
  const Harness h;
  EXPECT_FALSE(UpdateChecker::FromStorage("garbage", h.Build()).last_good);
  EXPECT_FALSE(UpdateChecker::FromStorage(
                   R"({"api":"GitHub","latest_version":"v2.1.9"})", h.Build())
                   .last_good);
}

TEST(UpdateCheckerTest, NoCommitHashSkipsTheCommitLookup) {
  Harness h;
  auto c = h.Make(h.Build("v2.1.8", ""));
  c->Start(CheckMode::kForce);
  h.server.AnswerAll(true, "2.1.8");
  for (const auto& url : h.server.asked) {
    EXPECT_FALSE(url.contains("/commits/")) << url.toStdString();
  }
  ASSERT_TRUE(c->State().last_good);
  EXPECT_EQ(c->State().last_good->commit_fact, RemoteFact::kUnknown);
  // Nothing more could be asked: the result stands for a day, as any other.
  EXPECT_FALSE(c->State().last_good->complete);
  EXPECT_TRUE(c->IsFresh());
}

TEST(UpdateCheckerTest, SynchronousFetcherStillFinishesOnce) {
  QByteArray stored;
  int changes = 0;
  const Harness h;
  UpdateChecker c(
      h.Build(),
      [](const QUrl&, const UpdateChecker::Reply& reply) { reply(0, {}); },
      {[&] { return stored; }, [&](const QByteArray& d) { stored = d; }});
  QObject::connect(&c, &UpdateChecker::Changed, [&] { ++changes; });
  c.Start(CheckMode::kForce);
  EXPECT_FALSE(c.State().checking);
  EXPECT_TRUE(c.State().last_attempt_failed);
  EXPECT_EQ(changes, 2);
}

TEST(UpdateCheckerTest, ReplyAfterDestructionIsIgnored) {
  Harness h;
  auto c = h.Make();
  c->Start(CheckMode::kForce);
  c.reset();
  h.server.AnswerAll(true);  // must not touch the dead checker
  EXPECT_EQ(h.saves, 0);
}

TEST(UpdateCheckerTest, DiscoveredUpdateAsksForAttention) {
  Harness h;
  auto c = h.Make();
  EXPECT_EQ(AttentionFor(c->State()), UpdateAttention::kNone);  // nothing known

  c->Start(CheckMode::kIfStale);
  EXPECT_EQ(AttentionFor(c->State()), UpdateAttention::kNone);  // checking
  h.server.AnswerAll(true, "2.1.9");
  EXPECT_EQ(AttentionFor(c->State()), UpdateAttention::kUpdateAvailable);
}

TEST(UpdateCheckerTest, OpeningThePageAcknowledgesTheUpdate) {
  Harness h;
  auto c = h.Make();
  c->Start(CheckMode::kForce);
  h.server.AnswerAll(true, "2.1.9");

  int changes = 0;
  QObject::connect(c.get(), &UpdateChecker::Changed, [&] { ++changes; });
  c->Acknowledge();
  EXPECT_EQ(changes, 1);
  EXPECT_EQ(c->State().last_seen_update_version, "2.1.9");
  EXPECT_EQ(AttentionFor(c->State()), UpdateAttention::kNone);

  // Nothing new to acknowledge: no change, no write.
  const int saves = h.saves;
  c->Acknowledge();
  EXPECT_EQ(changes, 1);
  EXPECT_EQ(h.saves, saves);

  // The page still offers the update itself.
  EXPECT_EQ(Decide(*c->State().last_good), Verdict::kUpdateAvailable);
}

TEST(UpdateCheckerTest, SameUpdateRediscoveredStaysQuiet) {
  Harness h;
  auto c = h.Make();
  c->Start(CheckMode::kForce);
  h.server.AnswerAll(true, "2.1.9");
  c->Acknowledge();

  c->Start(CheckMode::kForce);
  h.server.AnswerAll(true, "2.1.9");
  EXPECT_EQ(AttentionFor(c->State()), UpdateAttention::kNone);
}

TEST(UpdateCheckerTest, NewerUpdateAsksAgain) {
  Harness h;
  auto c = h.Make();
  c->Start(CheckMode::kForce);
  h.server.AnswerAll(true, "2.1.9");
  c->Acknowledge();

  c->Start(CheckMode::kForce);
  h.server.AnswerAll(true, "2.1.10");
  EXPECT_EQ(AttentionFor(c->State()), UpdateAttention::kUpdateAvailable);
  c->Acknowledge();
  EXPECT_EQ(c->State().last_seen_update_version, "2.1.10");
  EXPECT_EQ(AttentionFor(c->State()), UpdateAttention::kNone);
}

TEST(UpdateCheckerTest, FailedOrUnknownCheckNeverAsksForAttention) {
  Harness h;
  auto c = h.Make();
  c->Start(CheckMode::kForce);
  h.server.AnswerAll(false);
  EXPECT_EQ(AttentionFor(c->State()), UpdateAttention::kNone);

  c->Start(CheckMode::kForce);
  h.server.AnswerPartial();  // a marker not answered, no newer release
  EXPECT_EQ(AttentionFor(c->State()), UpdateAttention::kNone);
}

TEST(UpdateCheckerTest, FailureAfterAcknowledgementStaysQuiet) {
  Harness h;
  auto c = h.Make();
  c->Start(CheckMode::kForce);
  h.server.AnswerAll(true, "2.1.9");
  c->Acknowledge();

  c->Start(CheckMode::kForce);
  h.server.AnswerAll(false);
  EXPECT_TRUE(c->State().last_attempt_failed);
  EXPECT_EQ(c->State().last_seen_update_version, "2.1.9");
  EXPECT_EQ(AttentionFor(c->State()), UpdateAttention::kNone);
}

TEST(UpdateCheckerTest, FailureBeforeAcknowledgementKeepsAttention) {
  Harness h;
  auto c = h.Make();
  c->Start(CheckMode::kForce);
  h.server.AnswerAll(true, "2.1.9");
  c->Start(CheckMode::kForce);
  h.server.AnswerAll(false);
  // A confirmed update is not unlearned by a failed refresh.
  EXPECT_EQ(AttentionFor(c->State()), UpdateAttention::kUpdateAvailable);
}

TEST(UpdateCheckerTest, AcknowledgementPersistsAcrossRestart) {
  Harness h;
  {
    auto c = h.Make();
    c->Start(CheckMode::kForce);
    h.server.AnswerAll(true, "2.1.9");
    c->Acknowledge();
  }
  auto restarted = h.Make();
  EXPECT_EQ(restarted->State().last_seen_update_version, "2.1.9");
  EXPECT_EQ(AttentionFor(restarted->State()), UpdateAttention::kNone);

  // Also across a rebuild, whose stored result is dropped.
  auto rebuilt = h.Make(h.Build("v2.1.8", kCommitB));
  EXPECT_FALSE(rebuilt->State().last_good);
  EXPECT_EQ(rebuilt->State().last_seen_update_version, "2.1.9");
  rebuilt->Start(CheckMode::kIfStale);
  h.server.AnswerAll(true, "2.1.9");
  EXPECT_EQ(AttentionFor(rebuilt->State()), UpdateAttention::kNone);
}

TEST(UpdateCheckerTest, WithdrawnOrUnofficialNeverAskForAttention) {
  UpdateSnapshot s;
  SoftwareVersion r;
  r.current_version = "v2.1.8";
  r.list.outcome = ListOutcome::kNoMatch;
  r.tag_fact = RemoteFact::kNotFound;
  r.commit_fact = RemoteFact::kNotFound;
  s.last_good = r;
  EXPECT_EQ(Decide(r), Verdict::kWithdrawnOrUnreleased);
  EXPECT_EQ(AttentionFor(s), UpdateAttention::kNone);

  s.last_good->tag_fact = RemoteFact::kConfirmed;
  EXPECT_EQ(Decide(*s.last_good), Verdict::kUnofficialBuild);
  EXPECT_EQ(AttentionFor(s), UpdateAttention::kNone);
}
