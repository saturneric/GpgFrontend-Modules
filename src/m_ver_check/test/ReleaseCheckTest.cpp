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
#include <array>

#include "ReleaseCheck.h"

namespace {

auto Release(const QString& tag, bool draft = false, bool prerelease = false)
    -> QJsonObject {
  QJsonObject r;
  r["tag_name"] = tag;
  r["draft"] = draft;
  r["prerelease"] = prerelease;
  r["published_at"] = "2026-01-01T00:00:00Z";
  r["body"] = "notes for " + tag;
  r["html_url"] = "https://github.com/saturneric/GpgFrontend/releases/" + tag;
  return r;
}

auto List(const QJsonArray& releases) -> QByteArray {
  return QJsonDocument(releases).toJson();
}

auto Result(ListOutcome list, RemoteFact tag, RemoteFact commit,
            const QString& latest = "v2.1.9") -> SoftwareVersion {
  SoftwareVersion r;
  r.current_version = "v2.1.8";
  r.local_commit_hash = "abc123";
  r.list.outcome = list;
  if (list == ListOutcome::kFound) r.list.latest.version = latest;
  r.tag_fact = tag;
  r.commit_fact = commit;
  return r;
}

constexpr std::array<RemoteFact, 3> kFacts = {
    RemoteFact::kUnknown, RemoteFact::kConfirmed, RemoteFact::kNotFound};

}  // namespace

TEST(ReleaseCheckTest, ExtractVersionNormalizesTags) {
  EXPECT_EQ(ExtractVersionFromRawTag("v2.1.9"), "v2.1.9");
  EXPECT_EQ(ExtractVersionFromRawTag("v2.1.9-rc1"), "v2.1.9");
  EXPECT_EQ(ExtractVersionFromRawTag("2.1.9"), "");
  EXPECT_EQ(ExtractVersionFromRawTag("release"), "");
}

TEST(ReleaseCheckTest, PrereleaseVersionDetection) {
  EXPECT_FALSE(IsPrereleaseVersion("v2.1.9"));
  EXPECT_FALSE(IsPrereleaseVersion("2.1"));
  EXPECT_TRUE(IsPrereleaseVersion("v2.2.0-rc1"));
  EXPECT_TRUE(IsPrereleaseVersion("v2.2.0beta"));
}

TEST(ReleaseCheckTest, PicksNewestReleaseInOwnSeries) {
  const auto body = List({Release("v2.2.3"), Release("v2.1.10"),
                          Release("v2.1.9"), Release("v2.0.7")});
  const auto stable = ParseReleaseList(200, body, "v2.1.8");
  ASSERT_EQ(stable.outcome, ListOutcome::kFound);
  EXPECT_EQ(stable.latest.version, "v2.1.10");
  EXPECT_EQ(stable.latest.notes, "notes for v2.1.10");
  EXPECT_TRUE(stable.latest.html_url.endsWith("v2.1.10"));

  const auto mainline = ParseReleaseList(200, body, "v2.2.0");
  ASSERT_EQ(mainline.outcome, ListOutcome::kFound);
  EXPECT_EQ(mainline.latest.version, "v2.2.3");
}

TEST(ReleaseCheckTest, SkipsDraftsAndForeignTags) {
  const auto body =
      List({Release("v2.1.12", true), Release("nightly"), Release("v2.1.9")});
  const auto r = ParseReleaseList(200, body, "v2.1.8");
  ASSERT_EQ(r.outcome, ListOutcome::kFound);
  EXPECT_EQ(r.latest.version, "v2.1.9");
}

TEST(ReleaseCheckTest, PrereleasesOnlyForPrereleaseBuilds) {
  const auto body = List({Release("v2.1.11", false, true), Release("v2.1.9")});
  EXPECT_EQ(ParseReleaseList(200, body, "v2.1.8").latest.version, "v2.1.9");
  EXPECT_EQ(ParseReleaseList(200, body, "v2.1.8-rc1").latest.version,
            "v2.1.11");
}

TEST(ReleaseCheckTest, ValidListWithoutMatchIsNotAFailure) {
  EXPECT_EQ(ParseReleaseList(200, List({}), "v2.1.8").outcome,
            ListOutcome::kNoMatch);
  EXPECT_EQ(ParseReleaseList(200, List({Release("v2.2.0")}), "v2.1.8").outcome,
            ListOutcome::kNoMatch);
}

TEST(ReleaseCheckTest, BrokenListIsAFailure) {
  const auto good = List({Release("v2.1.9")});
  EXPECT_EQ(ParseReleaseList(403, good, "v2.1.8").outcome,
            ListOutcome::kFailed);
  EXPECT_EQ(ParseReleaseList(0, {}, "v2.1.8").outcome, ListOutcome::kFailed);
  EXPECT_EQ(ParseReleaseList(200, "{\"message\":\"x\"}", "v2.1.8").outcome,
            ListOutcome::kFailed);
  EXPECT_EQ(ParseReleaseList(200, "<html>", "v2.1.8").outcome,
            ListOutcome::kFailed);
}

TEST(ReleaseCheckTest, ClassifyLookup) {
  EXPECT_EQ(ClassifyLookup(200, true), RemoteFact::kConfirmed);
  EXPECT_EQ(ClassifyLookup(200, false), RemoteFact::kUnknown);
  EXPECT_EQ(ClassifyLookup(404, false), RemoteFact::kNotFound);
  for (const int status : {0, 301, 401, 403, 422, 429, 500, 502, 503}) {
    EXPECT_EQ(ClassifyLookup(status, false), RemoteFact::kUnknown) << status;
  }
}

TEST(ReleaseCheckTest, TagLookup) {
  EXPECT_EQ(ParseTagLookup(200, R"({"tag_name":"v2.1.8"})"),
            RemoteFact::kConfirmed);
  EXPECT_EQ(ParseTagLookup(200, "garbage"), RemoteFact::kUnknown);
  EXPECT_EQ(ParseTagLookup(404, R"({"message":"Not Found"})"),
            RemoteFact::kNotFound);
  EXPECT_EQ(ParseTagLookup(403, R"({"message":"API rate limit exceeded"})"),
            RemoteFact::kUnknown);
}

TEST(ReleaseCheckTest, CommitLookup) {
  EXPECT_EQ(ParseCommitLookup(200, R"({"sha":"abc123def"})", "abc123"),
            RemoteFact::kConfirmed);
  EXPECT_EQ(ParseCommitLookup(200, R"({"sha":"fff000"})", "abc123"),
            RemoteFact::kUnknown);
  EXPECT_EQ(ParseCommitLookup(404, {}, "abc123"), RemoteFact::kNotFound);
  EXPECT_EQ(
      ParseCommitLookup(422, R"({"message":"No commit found for SHA: abc123"})",
                        "abc123"),
      RemoteFact::kNotFound);
  EXPECT_EQ(
      ParseCommitLookup(422, R"({"message":"Validation Failed"})", "abc123"),
      RemoteFact::kUnknown);
  EXPECT_EQ(ParseCommitLookup(200, R"({"sha":"abc123"})", ""),
            RemoteFact::kUnknown);
}

TEST(ReleaseCheckTest, VerdictPrecedence) {
  using F = RemoteFact;
  using L = ListOutcome;
  // An update wins over everything the lookups found.
  EXPECT_EQ(Decide(Result(L::kFound, F::kNotFound, F::kNotFound)),
            Verdict::kUpdateAvailable);
  // Then a missing release, then a missing commit.
  EXPECT_EQ(Decide(Result(L::kFound, F::kNotFound, F::kNotFound, "v2.1.8")),
            Verdict::kWithdrawnOrUnreleased);
  EXPECT_EQ(Decide(Result(L::kNoMatch, F::kConfirmed, F::kNotFound)),
            Verdict::kUnofficialBuild);
  EXPECT_EQ(Decide(Result(L::kNoMatch, F::kConfirmed, F::kConfirmed)),
            Verdict::kUpToDate);
  EXPECT_EQ(Decide(Result(L::kFound, F::kConfirmed, F::kConfirmed, "v2.1.8")),
            Verdict::kUpToDate);
  // A dev build ahead of the latest release is not offered a downgrade.
  EXPECT_EQ(Decide(Result(L::kFound, F::kConfirmed, F::kConfirmed, "v2.1.7")),
            Verdict::kUpToDate);
}

TEST(ReleaseCheckTest, VerdictTableForEveryCombination) {
  for (const auto list :
       {ListOutcome::kFailed, ListOutcome::kNoMatch, ListOutcome::kFound}) {
    for (const auto tag : kFacts) {
      for (const auto commit : kFacts) {
        for (const QString latest : {"v2.1.9", "v2.1.8"}) {
          const auto r = Result(list, tag, commit, latest);
          const auto v = Decide(r);
          Verdict expected = Verdict::kUpToDate;
          if (list == ListOutcome::kFailed) {
            expected = Verdict::kUnknown;
          } else if (list == ListOutcome::kFound && latest == "v2.1.9") {
            expected = Verdict::kUpdateAvailable;
          } else if (tag == RemoteFact::kNotFound) {
            expected = Verdict::kWithdrawnOrUnreleased;
          } else if (commit == RemoteFact::kNotFound) {
            expected = Verdict::kUnofficialBuild;
          }
          EXPECT_EQ(v, expected);
        }
      }
    }
  }
}

TEST(ReleaseCheckTest, UnknownFactsNeverAssertAnything) {
  for (const auto list :
       {ListOutcome::kFailed, ListOutcome::kNoMatch, ListOutcome::kFound}) {
    const auto v = Decide(
        Result(list, RemoteFact::kUnknown, RemoteFact::kUnknown, "v2.1.8"));
    EXPECT_NE(v, Verdict::kWithdrawnOrUnreleased);
    EXPECT_NE(v, Verdict::kUnofficialBuild);
  }
}

TEST(ReleaseCheckTest, CompletenessAndAuthority) {
  auto full = Result(ListOutcome::kFound, RemoteFact::kConfirmed,
                     RemoteFact::kNotFound);
  EXPECT_TRUE(IsComplete(full));

  auto partial =
      Result(ListOutcome::kFound, RemoteFact::kConfirmed, RemoteFact::kUnknown);
  EXPECT_TRUE(IsUsable(partial));
  EXPECT_FALSE(IsComplete(partial));
  partial.complete = IsComplete(partial);
  EXPECT_FALSE(IsAuthoritative(partial));

  auto failed = Result(ListOutcome::kFailed, RemoteFact::kConfirmed,
                       RemoteFact::kConfirmed);
  EXPECT_FALSE(IsUsable(failed));
  EXPECT_FALSE(IsComplete(failed));
}

TEST(ReleaseCheckTest, MergeKeepsPreviousOnFailure) {
  const auto prev = Result(ListOutcome::kFound, RemoteFact::kConfirmed,
                           RemoteFact::kConfirmed);
  const auto failed =
      Result(ListOutcome::kFailed, RemoteFact::kUnknown, RemoteFact::kUnknown);
  const auto merged = Merge(prev, failed);
  ASSERT_TRUE(merged);
  EXPECT_EQ(merged->list.outcome, ListOutcome::kFound);
  EXPECT_EQ(merged->list.latest.version, "v2.1.9");

  EXPECT_FALSE(Merge(std::nullopt, failed));
}

TEST(ReleaseCheckTest, MergeFillsUnknownFactsForSameBuildOnly) {
  auto prev = Result(ListOutcome::kFound, RemoteFact::kConfirmed,
                     RemoteFact::kNotFound);
  prev.complete = true;
  auto fresh =
      Result(ListOutcome::kNoMatch, RemoteFact::kUnknown, RemoteFact::kUnknown);
  fresh.complete = IsComplete(fresh);

  const auto merged = Merge(prev, fresh);
  ASSERT_TRUE(merged);
  EXPECT_EQ(merged->list.outcome, ListOutcome::kNoMatch);
  EXPECT_EQ(merged->tag_fact, RemoteFact::kConfirmed);
  EXPECT_EQ(merged->commit_fact, RemoteFact::kNotFound);
  // Inherited facts fill the picture; they do not make the check authoritative.
  EXPECT_FALSE(IsAuthoritative(*merged));

  auto other_build = prev;
  other_build.local_commit_hash = "fff000";
  const auto unrelated = Merge(other_build, fresh);
  ASSERT_TRUE(unrelated);
  EXPECT_EQ(unrelated->tag_fact, RemoteFact::kUnknown);
  EXPECT_EQ(unrelated->commit_fact, RemoteFact::kUnknown);
}

TEST(ReleaseCheckTest, MergeKeepsFreshKnownFacts) {
  const auto prev =
      Result(ListOutcome::kFound, RemoteFact::kNotFound, RemoteFact::kNotFound);
  const auto fresh = Result(ListOutcome::kFound, RemoteFact::kConfirmed,
                            RemoteFact::kConfirmed);
  const auto merged = Merge(prev, fresh);
  ASSERT_TRUE(merged);
  EXPECT_EQ(merged->tag_fact, RemoteFact::kConfirmed);
  EXPECT_EQ(merged->commit_fact, RemoteFact::kConfirmed);
}

TEST(ReleaseCheckTest, SoftwareVersionJsonRoundTrip) {
  auto r = Result(ListOutcome::kFound, RemoteFact::kConfirmed,
                  RemoteFact::kNotFound);
  r.list.latest.notes = "## Changes";
  r.list.latest.html_url = "https://example.invalid/r";
  r.complete = true;
  r.checked_at = QDateTime::fromSecsSinceEpoch(4102444800);  // past 2038

  const auto back = SoftwareVersion::FromJson(r.ToJson());
  ASSERT_TRUE(back);
  EXPECT_EQ(back->current_version, r.current_version);
  EXPECT_EQ(back->local_commit_hash, r.local_commit_hash);
  EXPECT_EQ(back->list.outcome, ListOutcome::kFound);
  EXPECT_EQ(back->list.latest.version, "v2.1.9");
  EXPECT_EQ(back->list.latest.notes, "## Changes");
  EXPECT_EQ(back->list.latest.html_url, "https://example.invalid/r");
  EXPECT_EQ(back->tag_fact, RemoteFact::kConfirmed);
  EXPECT_EQ(back->commit_fact, RemoteFact::kNotFound);
  EXPECT_TRUE(back->complete);
  EXPECT_EQ(back->checked_at, r.checked_at);
}

TEST(ReleaseCheckTest, OldCacheFormatIsRejected) {
  // What 1.5 and earlier wrote.
  QJsonObject old;
  old["api"] = "GitHub";
  old["latest_version"] = "v2.1.9";
  old["current_version"] = "v2.1.8";
  old["current_version_publish_in_remote"] = true;
  old["timestamp"] = 1700000000;
  EXPECT_FALSE(SoftwareVersion::FromJson(old));

  auto found_without_version =
      Result(ListOutcome::kFound, RemoteFact::kUnknown, RemoteFact::kUnknown)
          .ToJson();
  found_without_version["latest"] = QJsonObject{};
  EXPECT_FALSE(SoftwareVersion::FromJson(found_without_version));
}
