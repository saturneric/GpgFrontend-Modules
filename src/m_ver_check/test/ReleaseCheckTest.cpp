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

#include <QJsonObject>
#include <array>

#include "ReleaseCheck.h"

namespace {

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
  EXPECT_FALSE(back->other_series);
  EXPECT_TRUE(back->notice.series.isEmpty());
}

TEST(ReleaseCheckTest, SeriesNoticeAndBranchBuildSurviveStorage) {
  auto r = Result(ListOutcome::kNoMatch, RemoteFact::kConfirmed,
                  RemoteFact::kConfirmed);
  r.checked_at = QDateTime::fromSecsSinceEpoch(1800000000);
  r.other_series = true;
  r.notice = {"2.1", "legacy", "2027-12-31", "2.2", "2.2.4", true};
  const auto back = SoftwareVersion::FromJson(r.ToJson());
  ASSERT_TRUE(back);
  EXPECT_TRUE(back->other_series);
  EXPECT_EQ(back->notice.series, "2.1");
  EXPECT_EQ(back->notice.status, "legacy");
  EXPECT_EQ(back->notice.eol, "2027-12-31");
  EXPECT_EQ(back->notice.successor, "2.2");
  EXPECT_EQ(back->notice.successor_latest, "2.2.4");
  EXPECT_TRUE(back->notice.successor_runs_here);
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
