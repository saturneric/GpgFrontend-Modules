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

#include "ReleaseCheck.h"
#include "UpdateChecker.h"

namespace {

const UpdateChecker::Build kBuild{"v2.1.8", "abc123"};

/// Holds every request until the test answers it.
struct FakeServer {
  struct Pending {
    QUrl url;
    UpdateChecker::Reply reply;
  };
  std::vector<Pending> pending;
  int requests = 0;

  auto Fetcher() -> UpdateChecker::Fetcher {
    return [this](const QUrl& url, const UpdateChecker::Reply& reply) {
      ++requests;
      pending.push_back({url, reply});
    };
  }

  /// Answer every held request as a server would, @p ok or all failing.
  void AnswerAll(bool ok, const QString& latest = "v2.1.9") {
    auto held = std::move(pending);
    pending.clear();
    for (auto& p : held) {
      const auto path = p.url.path();
      if (!ok) {
        p.reply(0, {});
      } else if (path.endsWith("/releases")) {
        QJsonObject r;
        r["tag_name"] = latest;
        r["body"] = "notes";
        p.reply(200, QJsonDocument(QJsonArray{r}).toJson());
      } else if (path.contains("/releases/tags/")) {
        p.reply(200, R"({"tag_name":"v2.1.8"})");
      } else {
        p.reply(200, R"({"sha":"abc123ffff"})");
      }
    }
  }

  /// The list answers; the lookups hit a rate limit.
  void AnswerPartial() {
    auto held = std::move(pending);
    pending.clear();
    for (auto& p : held) {
      if (p.url.path().endsWith("/releases")) {
        QJsonObject r;
        r["tag_name"] = "v2.1.8";
        p.reply(200, QJsonDocument(QJsonArray{r}).toJson());
      } else {
        p.reply(403, R"({"message":"API rate limit exceeded"})");
      }
    }
  }
};

struct Harness {
  FakeServer server;
  QByteArray stored;
  QDateTime now = QDateTime::fromSecsSinceEpoch(1800000000);
  int saves = 0;

  auto Make(UpdateChecker::Build build = kBuild)
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
  EXPECT_EQ(h.server.requests, 3);

  h.server.AnswerAll(true);
  EXPECT_FALSE(c->State().checking);
  EXPECT_EQ(changes, 2);
  ASSERT_TRUE(c->State().last_good);
  EXPECT_EQ(Decide(*c->State().last_good), Verdict::kUpdateAvailable);
  EXPECT_TRUE(c->State().last_good->complete);
  EXPECT_EQ(h.saves, 1);
}

TEST(UpdateCheckerTest, SecondStartJoinsTheRunningCheck) {
  Harness h;
  auto c = h.Make();
  c->Start(CheckMode::kIfStale);
  c->Start(CheckMode::kForce);
  c->Start(CheckMode::kIfStale);
  EXPECT_EQ(h.server.requests, 3);

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
  EXPECT_EQ(s.last_good->list.latest.version, "v2.1.9");
  EXPECT_EQ(Decide(*s.last_good), Verdict::kUpdateAvailable);

  // And it survives a restart.
  auto restarted = h.Make();
  ASSERT_TRUE(restarted->State().last_good);
  EXPECT_EQ(restarted->State().last_good->list.latest.version, "v2.1.9");
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
  h.server.AnswerAll(true, "v2.1.8");
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

  c->Start(CheckMode::kIfStale);
  EXPECT_FALSE(c->State().checking);
  EXPECT_EQ(h.server.requests, 3);

  c->Start(CheckMode::kForce);
  EXPECT_TRUE(c->State().checking);
  EXPECT_EQ(h.server.requests, 6);
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
  EXPECT_EQ(s.last_good->tag_fact, RemoteFact::kUnknown);
  // A rate limit is not a withdrawn version.
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
  auto upgraded = h.Make({"v2.1.9", "def456"});
  EXPECT_FALSE(upgraded->State().last_good);
  EXPECT_FALSE(upgraded->IsFresh());

  auto same = h.Make();
  EXPECT_TRUE(same->State().last_good);
}

TEST(UpdateCheckerTest, StorageRejectsGarbageAndOldFormat) {
  EXPECT_FALSE(UpdateChecker::FromStorage("garbage", kBuild).last_good);
  EXPECT_FALSE(UpdateChecker::FromStorage(
                   R"({"api":"GitHub","latest_version":"v2.1.9"})", kBuild)
                   .last_good);
}

TEST(UpdateCheckerTest, NoCommitHashSkipsTheCommitLookup) {
  Harness h;
  auto c = h.Make({"v2.1.8", ""});
  c->Start(CheckMode::kForce);
  EXPECT_EQ(h.server.requests, 2);
  h.server.AnswerAll(true, "v2.1.8");
  ASSERT_TRUE(c->State().last_good);
  EXPECT_EQ(c->State().last_good->commit_fact, RemoteFact::kUnknown);
  EXPECT_FALSE(c->IsFresh());
}

TEST(UpdateCheckerTest, SynchronousFetcherStillFinishesOnce) {
  QByteArray stored;
  int changes = 0;
  UpdateChecker c(
      kBuild,
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

TEST(UpdateCheckerTest, StartupPromptOnlyForConfirmedUpdate) {
  Harness h;
  auto c = h.Make();
  EXPECT_FALSE(ShouldPromptOnStartup(c->State()));  // nothing known

  c->Start(CheckMode::kIfStale);
  EXPECT_FALSE(ShouldPromptOnStartup(c->State()));  // still checking
  h.server.AnswerAll(false);
  EXPECT_FALSE(ShouldPromptOnStartup(c->State()));  // failed

  c->Start(CheckMode::kIfStale);
  h.server.AnswerPartial();
  EXPECT_FALSE(ShouldPromptOnStartup(c->State()));  // partial, no update

  c->Start(CheckMode::kIfStale);
  h.server.AnswerAll(true);
  EXPECT_TRUE(ShouldPromptOnStartup(c->State()));  // confirmed update

  // A later failure does not unlearn a confirmed update.
  c->Start(CheckMode::kForce);
  h.server.AnswerAll(false);
  EXPECT_TRUE(ShouldPromptOnStartup(c->State()));
}

TEST(UpdateCheckerTest, StartupNeverPromptsForUnofficialOrWithdrawn) {
  UpdateSnapshot s;
  SoftwareVersion r;
  r.current_version = "v2.1.8";
  r.list.outcome = ListOutcome::kNoMatch;
  r.tag_fact = RemoteFact::kNotFound;
  r.commit_fact = RemoteFact::kNotFound;
  s.last_good = r;
  EXPECT_EQ(Decide(r), Verdict::kWithdrawnOrUnreleased);
  EXPECT_FALSE(ShouldPromptOnStartup(s));

  s.last_good->tag_fact = RemoteFact::kConfirmed;
  EXPECT_EQ(Decide(*s.last_good), Verdict::kUnofficialBuild);
  EXPECT_FALSE(ShouldPromptOnStartup(s));
}
