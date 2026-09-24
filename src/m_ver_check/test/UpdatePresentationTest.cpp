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

#include "UpdatePresentation.h"

namespace {

const QDateTime kNow = QDateTime::fromSecsSinceEpoch(1800000000);

auto Known(ListOutcome list, const QString& latest, RemoteFact tag,
           RemoteFact commit) -> SoftwareVersion {
  SoftwareVersion r;
  r.current_version = "v2.1.8";
  r.local_commit_hash = "abc123";
  r.list.outcome = list;
  r.list.latest.version = latest;
  r.list.latest.notes = "## What's new";
  r.tag_fact = tag;
  r.commit_fact = commit;
  r.checked_at = kNow.addSecs(-5 * 60);
  return r;
}

auto Update() -> SoftwareVersion {
  return Known(ListOutcome::kFound, "v2.1.9", RemoteFact::kConfirmed,
               RemoteFact::kConfirmed);
}

auto Current() -> SoftwareVersion {
  return Known(ListOutcome::kFound, "v2.1.8", RemoteFact::kConfirmed,
               RemoteFact::kConfirmed);
}

}  // namespace

TEST(UpdatePresentationTest, NothingKnownYet) {
  const auto v = PresentUpdate({}, kNow);
  EXPECT_FALSE(v.show_download);
  EXPECT_FALSE(v.busy);
  EXPECT_TRUE(v.check_enabled);
  EXPECT_EQ(v.check_label, "Check now");
  EXPECT_TRUE(v.freshness.isEmpty());
}

TEST(UpdatePresentationTest, FirstCheckInProgress) {
  UpdateSnapshot s;
  s.checking = true;
  const auto v = PresentUpdate(s, kNow);
  EXPECT_TRUE(v.busy);
  EXPECT_FALSE(v.check_enabled);
  EXPECT_FALSE(v.show_download);
}

TEST(UpdatePresentationTest, FailedWithNothingKnownOffersTryAgain) {
  UpdateSnapshot s;
  s.last_attempt_failed = true;
  const auto v = PresentUpdate(s, kNow);
  EXPECT_EQ(v.headline, "Couldn't check for updates");
  EXPECT_EQ(v.check_label, "Try again");
  EXPECT_FALSE(v.show_download);
  // Generic: no guessed cause.
  EXPECT_FALSE(v.detail.contains("offline", Qt::CaseInsensitive));
  EXPECT_FALSE(v.detail.contains("rate", Qt::CaseInsensitive));
}

TEST(UpdatePresentationTest, UpdateAvailableOffersDownloadAndNotes) {
  UpdateSnapshot s;
  s.last_good = Update();
  const auto v = PresentUpdate(s, kNow);
  EXPECT_EQ(v.tone, UpdateTone::kAttention);
  EXPECT_TRUE(v.show_download);
  EXPECT_EQ(v.download_label, "Download v2.1.9");
  EXPECT_TRUE(v.show_notes);
  EXPECT_EQ(v.notes, "## What's new");
  EXPECT_EQ(v.freshness, "Last checked 5 minute(s) ago · GitHub Releases");
  EXPECT_TRUE(v.notice.isEmpty());
}

TEST(UpdatePresentationTest, UpToDateHasNoDownloadOrNotes) {
  UpdateSnapshot s;
  s.last_good = Current();
  const auto v = PresentUpdate(s, kNow);
  EXPECT_EQ(v.tone, UpdateTone::kGood);
  EXPECT_FALSE(v.show_download);
  EXPECT_FALSE(v.show_notes);
  EXPECT_EQ(v.check_label, "Check again");
}

TEST(UpdatePresentationTest, FailedRefreshKeepsKnownUpdate) {
  UpdateSnapshot s;
  s.last_good = Update();
  s.last_attempt = kNow;
  s.last_attempt_failed = true;
  const auto v = PresentUpdate(s, kNow);
  EXPECT_TRUE(v.show_download);
  EXPECT_EQ(v.download_label, "Download v2.1.9");
  EXPECT_FALSE(v.notice.isEmpty());
  EXPECT_EQ(v.check_label, "Try again");
  // Freshness is the last success, not the failed attempt.
  EXPECT_TRUE(v.freshness.contains("5 minute"));
}

TEST(UpdatePresentationTest, CheckingKeepsPreviousResultVisible) {
  UpdateSnapshot s;
  s.last_good = Update();
  s.checking = true;
  const auto v = PresentUpdate(s, kNow);
  EXPECT_TRUE(v.busy);
  EXPECT_FALSE(v.check_enabled);
  EXPECT_TRUE(v.show_download);
  EXPECT_EQ(v.headline, "GpgFrontend v2.1.9 is available");
}

TEST(UpdatePresentationTest, WithdrawnAndUnofficialAreCalmAndInDialogOnly) {
  UpdateSnapshot s;
  s.last_good = Known(ListOutcome::kFound, "v2.1.8", RemoteFact::kNotFound,
                      RemoteFact::kConfirmed);
  auto v = PresentUpdate(s, kNow);
  EXPECT_EQ(v.tone, UpdateTone::kAttention);
  EXPECT_FALSE(v.show_download);
  EXPECT_EQ(v.check_label, "Check again");

  s.last_good = Known(ListOutcome::kFound, "v2.1.8", RemoteFact::kConfirmed,
                      RemoteFact::kNotFound);
  v = PresentUpdate(s, kNow);
  EXPECT_EQ(v.tone, UpdateTone::kAttention);
  EXPECT_FALSE(v.show_download);
}

TEST(UpdatePresentationTest, RelativeTime) {
  EXPECT_EQ(RelativeTime(kNow.addSecs(-10), kNow), "just now");
  EXPECT_EQ(RelativeTime(kNow.addSecs(-3 * 3600), kNow), "3 hour(s) ago");
  EXPECT_FALSE(RelativeTime(kNow.addDays(-3), kNow).contains("ago"));
}
