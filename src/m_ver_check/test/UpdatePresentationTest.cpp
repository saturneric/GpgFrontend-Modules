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
  EXPECT_TRUE(v.check_enabled);
  EXPECT_TRUE(v.check_enabled);
  EXPECT_EQ(v.check_label, "Check now");
  EXPECT_EQ(v.verification, "Update information not checked yet");
}

TEST(UpdatePresentationTest, FirstCheckInProgress) {
  UpdateSnapshot s;
  s.checking = true;
  const auto v = PresentUpdate(s, kNow);
  EXPECT_FALSE(v.check_enabled);
  EXPECT_FALSE(v.check_enabled);
  EXPECT_EQ(v.check_label, "Checking…");
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
  EXPECT_EQ(v.tone, UpdateTone::kUpdate);  // good news, not a warning
  EXPECT_TRUE(v.show_download);
  EXPECT_EQ(v.download_label, "Download");
  EXPECT_TRUE(v.show_notes);
  EXPECT_EQ(v.notes, "## What's new");
  EXPECT_EQ(v.verification,
            "Update information verified · checked 5 minute(s) ago");
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
  EXPECT_EQ(v.download_label, "Download");
  EXPECT_EQ(v.check_label, "Try again");
  // The age is the last success's, not the failed attempt's.
  EXPECT_EQ(v.verification,
            "Couldn't reach the update server. Showing the result from 5 "
            "minute(s) ago.");
}

TEST(UpdatePresentationTest, CheckingKeepsPreviousResultVisible) {
  UpdateSnapshot s;
  s.last_good = Update();
  s.checking = true;
  const auto v = PresentUpdate(s, kNow);
  EXPECT_FALSE(v.check_enabled);
  EXPECT_FALSE(v.check_enabled);
  EXPECT_TRUE(v.show_download);
  EXPECT_EQ(v.headline, "GpgFrontend 2.1.9 is available");
  EXPECT_EQ(v.check_label, "Checking…");
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

namespace {

auto Nightly(const QString& latest, RemoteFact tag = RemoteFact::kConfirmed,
             RemoteFact commit = RemoteFact::kConfirmed) -> SoftwareVersion {
  auto r = Known(ListOutcome::kFound, latest, tag, commit);
  r.current_version = "v2.3.0-alpha.414+g739ebdc4c445";
  r.nightly = true;  // as the checker records it
  return r;
}

}  // namespace

// A nightly reads the signed nightly channel, never GitHub: no line may say
// otherwise.
TEST(UpdatePresentationTest, ANightlyNamesItsOwnChannel) {
  UpdateSnapshot s;
  s.last_good = Nightly("2.3.0-alpha.414+g739ebdc4c445");
  auto v = PresentUpdate(s, kNow);
  EXPECT_EQ(v.headline, "You're up to date");
  EXPECT_EQ(v.detail, "2.3.0-alpha.414 is the newest nightly build.");
  // The count is "5 minute(s)" with no translator loaded: match the source.
  EXPECT_TRUE(v.verification.startsWith("Update information verified"))
      << v.verification.toStdString();

  s.last_good = Nightly("2.3.0-alpha.420+gabc");
  v = PresentUpdate(s, kNow);
  EXPECT_EQ(v.headline, "Nightly build 2.3.0-alpha.420 is available");
  EXPECT_EQ(v.detail, "You are using 2.3.0-alpha.414.");

  s.last_good = Nightly("2.3.0-alpha.414", RemoteFact::kNotFound);
  v = PresentUpdate(s, kNow);
  EXPECT_EQ(v.headline, "This nightly build has been withdrawn");

  s.last_good =
      Nightly("2.3.0-alpha.414", RemoteFact::kConfirmed, RemoteFact::kNotFound);
  v = PresentUpdate(s, kNow);
  EXPECT_TRUE(v.detail.contains("published builds"));

  for (const auto& r : {Nightly("2.3.0-alpha.414"), Nightly("2.3.0-alpha.420"),
                        Nightly("2.3.0-alpha.414", RemoteFact::kNotFound)}) {
    s.last_good = r;
    v = PresentUpdate(s, kNow);
    for (const auto& text : {v.headline, v.detail, v.verification}) {
      EXPECT_FALSE(text.contains("GitHub")) << text.toStdString();
    }
  }
}

TEST(UpdatePresentationTest, AReleaseBuildNamesTheSignedIndex) {
  UpdateSnapshot s;
  s.last_good = Current();
  const auto v = PresentUpdate(s, kNow);
  EXPECT_EQ(v.detail, "2.1.8 is the latest release.");
  EXPECT_TRUE(v.verification.startsWith("Update information verified"))
      << v.verification.toStdString();
  EXPECT_FALSE(v.verification.contains("GitHub"));
}

// The button leads to the file only when the checker made the link from a
// verified descriptor; without one, the downloads page. Only a nightly is
// installed in place.
TEST(UpdatePresentationTest, DownloadLeadsToTheVerifiedFile) {
  UpdateSnapshot s;
  auto r = Nightly("2.3.0-alpha.420");
  r.list.latest.html_url =
      "https://builds.bktus.com/gpgfrontend/nightly/x/windows-x86_64.msi";
  s.last_good = r;
  EXPECT_EQ(PresentUpdate(s, kNow).download_url, r.list.latest.html_url);

  r.list.latest.html_url.clear();  // no descriptor: the downloads page
  s.last_good = r;
  EXPECT_TRUE(PresentUpdate(s, kNow).download_url.isEmpty());

  auto release = Update();
  release.list.latest.html_url =
      "https://builds.bktus.com/gpgfrontend/releases/2.1.9/windows-x86_64.msi";
  release.list.latest.download_sha256 = QString(64, u'a');
  release.list.latest.download_size = 10;
  s.last_good = release;
  const auto v = PresentUpdate(s, kNow);
  EXPECT_EQ(v.download_url, release.list.latest.html_url);
  EXPECT_FALSE(v.installable);
}

// What a person reads: the version, not its leading "v" or build commit,
// which make a headline wrap and say nothing.
TEST(UpdatePresentationTest, VersionsReadCleanly) {
  EXPECT_EQ(DisplayVersion("v2.3.0-alpha.415+gbe5ff859f457"),
            "2.3.0-alpha.415");
  EXPECT_EQ(DisplayVersion("v2.2.4"), "2.2.4");
  EXPECT_EQ(DisplayVersion("2.4.0-rc.1"), "2.4.0-rc.1");
  EXPECT_EQ(DisplayVersion("2.3.0+dirty"), "2.3.0");
}

// An update the signed descriptor states by hash and size can be installed in
// place; the dialog adds whether this machine allows it.
TEST(UpdatePresentationTest, AnInstallableNightlyGoesFromInstallToRestart) {
  UpdateSnapshot s;
  auto r = Nightly("2.3.0-alpha.420+gabc");
  r.list.latest.html_url =
      "https://builds.bktus.com/gpgfrontend/nightly/x/linux-x86_64.AppImage";
  r.list.latest.download_sha256 = QString(64, u'a');
  r.list.latest.download_size = 1000;
  s.last_good = r;
  const auto base = PresentUpdate(s, kNow);
  ASSERT_TRUE(base.installable);

  using Stage = InstallState::Stage;
  auto v = PresentInstall(base, {});
  EXPECT_EQ(v.action, UpdateView::Action::kInstall);
  EXPECT_EQ(v.download_label, "Install update");

  InstallState progress;
  progress.stage = Stage::kDownloading;
  progress.received = 420;  // no total from the server: the descriptor's
  v = PresentInstall(base, progress);
  EXPECT_EQ(v.download_label, "Downloading… 42%");
  EXPECT_FALSE(v.download_enabled);
  EXPECT_FALSE(v.check_enabled);

  InstallState installed;
  installed.stage = Stage::kInstalled;
  v = PresentInstall(base, installed);
  EXPECT_EQ(v.tone, UpdateTone::kGood);
  EXPECT_EQ(v.headline, "Nightly build 2.3.0-alpha.420 is installed");
  EXPECT_EQ(v.detail, "Restart GpgFrontend to start using it.");
  EXPECT_EQ(v.action, UpdateView::Action::kRestart);
  EXPECT_EQ(v.download_label, "Restart now");
  EXPECT_TRUE(v.show_download);

  // A Host that cannot restart: the user does it.
  installed.can_restart = false;
  v = PresentInstall(base, installed);
  EXPECT_EQ(v.detail, "Close and reopen GpgFrontend to start using it.");
  EXPECT_FALSE(v.show_download);

  InstallState failed;
  failed.stage = Stage::kFailed;
  failed.error = "The download failed: HTTP 404";
  v = PresentInstall(base, failed);
  EXPECT_EQ(v.notice, failed.error);
  EXPECT_EQ(v.action, UpdateView::Action::kInstall);
  EXPECT_EQ(v.download_label, "Try again");
}

TEST(UpdatePresentationTest, OnlyAFileTheDescriptorStatesIsInstallable) {
  UpdateSnapshot s;
  auto r = Nightly("2.3.0-alpha.420");
  r.list.latest.html_url = "https://builds.bktus.com/gpgfrontend/x.AppImage";
  r.list.latest.download_sha256 = QString(64, u'a');
  r.list.latest.download_size = 0;  // no size: no install
  s.last_good = r;
  auto v = PresentUpdate(s, kNow);
  EXPECT_FALSE(v.installable);
  // Not installable: the view is left exactly as it was.
  EXPECT_EQ(PresentInstall(v, {}).download_label, "Download");
  EXPECT_EQ(PresentInstall(v, {}).action, UpdateView::Action::kOpen);

  // A release, whatever its link says, is never installed in place here.
  auto release = Update();
  release.list.latest.html_url = "https://builds.bktus.com/x.AppImage";
  release.list.latest.download_sha256 = QString(64, u'a');
  release.list.latest.download_size = 10;
  s.last_good = release;
  EXPECT_FALSE(PresentUpdate(s, kNow).installable);
}

// The dialog's blocks and its details: this build, the offer, the one
// verification line, each step of the last check, and what only the
// release server checked, never marked as verified here.

namespace {

/// A last check in which every step held.
auto Verified() -> CheckRecord {
  CheckRecord r;
  for (auto& step : r.steps) step.status = StepStatus::kVerified;
  r.root_version = 1;
  r.root_built_in = 1;
  r.root_keys = 3;
  r.root_signers = {"4E179325A34DCF45", "27458E01B54EAF5D"};
  r.root_expires = kNow.addYears(2);
  r.list_version = 1;
  r.list_signer = "9B09BDB5C65DAB85";
  r.list_keys = 1;
  r.list_expires = kNow.addYears(1);
  r.index_serial = 42;
  r.index_floor = 41;
  r.index_key = "0123456789ABCDEF";
  r.mirror = "update.bktus.com";
  r.index_expires = kNow.addDays(14);
  r.series_status = "stable";
  r.descriptor_sha256 = QString(64, u'd');
  r.notes_sha256 = QString(64, u'e');
  r.marker_base = "builds.bktus.com";
  r.repository = "saturneric/GpgFrontend";
  r.workflow = ".github/workflows/build.yml";
  r.pgp_fingerprints = {"12F7E8858CF15BEC9975FF3C5CA3DA246843FD03"};
  r.authenticode_subject = "CN=Example";
  return r;
}

/// The check stopped at @p id: it failed there, and what comes after it
/// in the check did not run (the commit marker is asked only after).
auto FailedAt(CheckStepId id, StepReason reason, const QString& detail = {})
    -> CheckRecord {
  auto r = Verified();
  r.Step(id) = {StepStatus::kFailed, reason, detail};
  for (auto i = static_cast<std::size_t>(id) + 1; i < kCheckSteps; ++i) {
    r.steps.at(i) = {};
  }
  return r;
}

auto Snapshot(const SoftwareVersion& good, const CheckRecord& record)
    -> UpdateSnapshot {
  UpdateSnapshot s;
  s.last_good = good;
  s.last_attempt = kNow;
  s.last_check = record;
  return s;
}

auto Release(const QString& os = "linux", const QString& kind = "appimage")
    -> UpdateChecker::Build {
  UpdateChecker::Build b{"v2.1.8", "abc123", {}};
  b.platform.os = os;
  b.platform.install_kind = kind;
  b.nightly = 0;
  return b;
}

auto WithFile(SoftwareVersion r) -> SoftwareVersion {
  r.list.latest.html_url =
      "https://builds.bktus.com/gpgfrontend/releases/2.1.9/"
      "GpgFrontend-2.1.9-linux-x86_64.AppImage";
  r.list.latest.download_sha256 = "0123abcd" + QString(48, u'5') + "89abcdef";
  r.list.latest.download_size = 2 * 1024 * 1024;
  return r;
}

void ExpectHostRowsNeverVerified(const UpdateView& v) {
  for (const auto& row : v.host_rows) {
    EXPECT_EQ(row.mark, DetailRow::Mark::kNone) << row.title.toStdString();
  }
}

}  // namespace

TEST(UpdatePresentationTest, UpToDateShowsTheBuildAndEveryStepVerified) {
  const auto v =
      PresentUpdate(Snapshot(Current(), Verified()), kNow, Release());
  EXPECT_EQ(v.headline, "You're up to date");
  EXPECT_EQ(v.build_version, "2.1.8");
  EXPECT_EQ(v.build_kind, "Release · series 2.1 (stable)");
  EXPECT_EQ(v.build_commit, "abc123");
  EXPECT_EQ(v.build_official, "Official build");
  EXPECT_FALSE(v.show_offer);
  EXPECT_EQ(v.newest, "Newest in 2.1: 2.1.8");
  EXPECT_EQ(v.verification_tone, UpdateTone::kGood);
  EXPECT_EQ(v.verification,
            "Update information verified · checked 5 minute(s) ago");

  ASSERT_EQ(v.device_rows.size(), 6);
  for (const auto i : {0, 1, 2, 3, 5}) {
    EXPECT_EQ(v.device_rows[i].mark, DetailRow::Mark::kVerified) << i;
  }
  EXPECT_EQ(v.device_rows[0].lines,
            QStringList({"Version 1, built in",
                         "2 of 3 root keys signed: 4E179325A34DCF45, "
                         "27458E01B54EAF5D",
                         v.device_rows[0].lines[2]}));
  EXPECT_EQ(v.device_rows[2].lines[0], "Serial 42 from update.bktus.com");
  EXPECT_EQ(v.device_rows[2].lines[1], "Signed by 0123456789ABCDEF");
  EXPECT_EQ(v.device_rows[3].lines[1], "Release details dddddddd…dddddddd");
  // Nothing offered, nothing downloaded.
  EXPECT_EQ(v.device_rows[kDownloadRow].mark, DetailRow::Mark::kNotApplicable);
  ExpectHostRowsNeverVerified(v);
  EXPECT_EQ(v.security_url,
            "https://github.com/saturneric/GpgFrontend/blob/main/SECURITY.md");
}

TEST(UpdatePresentationTest, AnUpdateShowsTheOfferAndAnUncheckedDownload) {
  auto v =
      PresentUpdate(Snapshot(WithFile(Update()), Verified()), kNow, Release());
  EXPECT_TRUE(v.show_offer);
  EXPECT_EQ(v.offer_version, "2.1.9");
  EXPECT_EQ(v.offer_file, "GpgFrontend-2.1.9-linux-x86_64.AppImage · " +
                              QLocale().formattedDataSize(2 * 1024 * 1024));
  EXPECT_EQ(v.offer_sha256.size(), 64);
  EXPECT_EQ(ShortHash(v.offer_sha256), "0123abcd…89abcdef");
  EXPECT_TRUE(v.newest.isEmpty());

  // The browser fetches it: never shown as verified.
  const auto& file = v.device_rows[kDownloadRow];
  EXPECT_EQ(file.mark, DetailRow::Mark::kInfo);
  EXPECT_EQ(file.lines[0], "Not checked, opens in your browser");
  EXPECT_EQ(file.copy, v.offer_sha256);

  ASSERT_EQ(v.host_rows.size(), 2);
  EXPECT_TRUE(v.host_rows[0].lines.contains("Tag v2.1.9"));
  EXPECT_TRUE(v.host_rows[1].lines.contains(
      "OpenPGP key 12F7E8858CF15BEC9975FF3C5CA3DA246843FD03"));
  // Authenticode is said only where it applies: Windows installers.
  EXPECT_FALSE(v.host_rows[1].lines.join(' ').contains("Authenticode"));
  v = PresentUpdate(Snapshot(WithFile(Update()), Verified()), kNow,
                    Release("windows", "msi"));
  EXPECT_TRUE(
      v.host_rows[1].lines.join(' ').contains("Authenticode, CN=Example"));
  ExpectHostRowsNeverVerified(v);
}

TEST(UpdatePresentationTest, NeverCheckedShowsTheBuildOnly) {
  UpdateChecker::Build build{"v2.2.4+gabc", "0123456789abcdef0123", {}};
  build.nightly = 0;
  const auto v = PresentUpdate({}, kNow, build);
  EXPECT_EQ(v.build_version, "2.2.4");
  EXPECT_EQ(v.build_kind, "Release · series 2.2");
  EXPECT_EQ(v.build_commit, "0123456789ab");
  EXPECT_TRUE(v.build_official.isEmpty());
  EXPECT_TRUE(v.device_rows.isEmpty());
  EXPECT_TRUE(v.host_rows.isEmpty());

  build.nightly = 1;
  EXPECT_EQ(PresentUpdate({}, kNow, build).build_kind, "Nightly");
}

TEST(UpdatePresentationTest, CheckingSaysSoOnTheVerificationLine) {
  auto s = Snapshot(Update(), Verified());
  s.checking = true;
  const auto v = PresentUpdate(s, kNow, Release());
  EXPECT_EQ(v.verification, "Checking the update information…");
  EXPECT_FALSE(v.check_enabled);
}

TEST(UpdatePresentationTest, AWithdrawnBuildIsPointedAtTheNewest) {
  const auto r = Known(ListOutcome::kFound, "v2.1.7", RemoteFact::kNotFound,
                       RemoteFact::kConfirmed);
  const auto v = PresentUpdate(Snapshot(r, Verified()), kNow, Release());
  EXPECT_EQ(v.tone, UpdateTone::kAttention);
  EXPECT_EQ(v.headline, "This version has been withdrawn");
  EXPECT_EQ(v.detail, "Please move to 2.1.7, the newest version.");
  EXPECT_TRUE(v.show_download);
  EXPECT_TRUE(v.show_offer);
  EXPECT_EQ(v.offer_version, "2.1.7");
}

TEST(UpdatePresentationTest,
     AnUnofficialBuildIsSaidButNotAVerificationFailure) {
  auto record = Verified();
  record.Step(CheckStepId::kCommit) = {StepStatus::kFailed,
                                       StepReason::kNotFound};
  const auto r = Known(ListOutcome::kFound, "v2.1.8", RemoteFact::kConfirmed,
                       RemoteFact::kNotFound);
  const auto v = PresentUpdate(Snapshot(r, record), kNow, Release());
  EXPECT_EQ(v.tone, UpdateTone::kAttention);
  EXPECT_EQ(v.headline, "This build isn't from the official repository");
  EXPECT_EQ(v.build_official, "Not from the official repository");
  EXPECT_EQ(v.device_rows[5].mark, DetailRow::Mark::kFailed);
  EXPECT_EQ(v.device_rows[5].lines[0],
            "This build's commit is not among the official builds.");
  // The update information itself verified.
  EXPECT_EQ(v.verification_tone, UpdateTone::kGood);
}

TEST(UpdatePresentationTest, AnEolSeriesOffersItsSuccessorOnlyWhereItRuns) {
  auto r = Current();
  r.notice = {"2.1", "eol", "", "2.2", "2.2.4", true, {}};
  auto v = PresentUpdate(Snapshot(r, Verified()), kNow, Release());
  EXPECT_TRUE(v.notice.contains("no longer gets fixes"));
  EXPECT_TRUE(v.show_download);
  EXPECT_TRUE(v.download_url.isEmpty());  // the downloads page

  r.notice.successor_runs_here = false;
  r.notice.unmet = "macos 15.0";
  v = PresentUpdate(Snapshot(r, Verified()), kNow, Release("macos", "dmg"));
  EXPECT_FALSE(v.show_download);
  EXPECT_TRUE(v.notice.contains("GpgFrontend 2.2.4 needs macOS 15.0 or later"))
      << v.notice.toStdString();

  r.notice.unmet.clear();  // a system no requirement is stated for
  v = PresentUpdate(Snapshot(r, Verified()), kNow, Release("freebsd", ""));
  EXPECT_TRUE(v.notice.contains("does not support this system"));
}

TEST(UpdatePresentationTest, AStoreBuildIsUpdatedByItsPackageSource) {
  const auto v = PresentUpdate(Snapshot(WithFile(Update()), Verified()), kNow,
                               Release("linux", "flatpak"));
  EXPECT_EQ(v.headline, "GpgFrontend 2.1.9 is available");
  EXPECT_FALSE(v.show_download);
  EXPECT_FALSE(v.installable);
  EXPECT_EQ(v.notice, "Updates for this build come from Flatpak.");
  EXPECT_EQ(v.device_rows[kDownloadRow].mark, DetailRow::Mark::kNotApplicable);
}

TEST(UpdatePresentationTest, ANewerBetaIsMentionedWhileBetasAreOff) {
  auto record = Verified();
  record.newer_beta = "v2.1.9-alpha.3";
  const auto v = PresentUpdate(Snapshot(Current(), record), kNow, Release());
  EXPECT_EQ(v.newer_beta,
            "Beta 2.1.9-alpha.3 is out; it is offered with beta versions on.");
}

TEST(UpdatePresentationTest, AnInPlaceUpdateIsVerifiedAfterDownload) {
  auto r = Nightly("2.3.0-alpha.420");
  r.list.latest.html_url = "https://builds.bktus.com/x/linux-x86_64.AppImage";
  r.list.latest.download_sha256 = QString(64, u'a');
  r.list.latest.download_size = 1000;
  const auto view = PresentUpdate(Snapshot(r, Verified()), kNow);
  ASSERT_TRUE(view.installable);

  auto v = PresentInstall(view, {});
  EXPECT_EQ(v.device_rows[kDownloadRow].mark, DetailRow::Mark::kNotRun);
  v = PresentInstall(view, {InstallState::Stage::kDownloading, 10, 1000});
  EXPECT_EQ(v.device_rows[kDownloadRow].mark, DetailRow::Mark::kNotRun);
  v = PresentInstall(view, {InstallState::Stage::kInstalled});
  EXPECT_EQ(v.download_label, "Restart now");
  EXPECT_EQ(v.device_rows[kDownloadRow].mark, DetailRow::Mark::kVerified);
  EXPECT_EQ(v.device_rows[kDownloadRow].lines[0],
            "Size and SHA-256 matched after download");
  v = PresentInstall(view, {InstallState::Stage::kFailed, 0, 0, "bad hash"});
  EXPECT_EQ(v.device_rows[kDownloadRow].mark, DetailRow::Mark::kFailed);
  EXPECT_EQ(v.device_rows[kDownloadRow].lines[0], "bad hash");
}

// Each reason a check can fail for: its own words on the verification line
// and in its row, what came after it not run, the last good result kept.
TEST(UpdatePresentationTest, EachFailureIsNamed) {
  struct Case {
    CheckStepId step;
    StepReason reason;
    QString detail;
    int row;
    QString says;
  };
  const QDateTime expiry =
      QDateTime::fromString("2027-01-10T00:00:00Z", Qt::ISODate);
  const auto day =
      QLocale().toString(expiry.toLocalTime().date(), QLocale::ShortFormat);
  const std::vector<Case> cases{
      {CheckStepId::kIndex,
       StepReason::kBadSignature,
       {},
       2,
       "the update index's signature is not valid"},
      {CheckStepId::kIndex, StepReason::kKeyNotListed, "FEDCBA9876543210", 2,
       "the update index is signed by a key that is not trusted "
       "(FEDCBA9876543210)"},
      {CheckStepId::kRoot, StepReason::kExpired, "2027-01-10T00:00:00Z", 0,
       "the root of trust expired on " + day},
      {CheckStepId::kIndexKeys, StepReason::kExpired, "2027-01-10T00:00:00Z", 1,
       "the signing key list expired on " + day},
      {CheckStepId::kIndex, StepReason::kExpired, "2027-01-10T00:00:00Z", 2,
       "the update index expired on " + day},
      {CheckStepId::kIndex, StepReason::kRollback, "41 40", 2,
       "the update index is older than one seen before (serial 40, 41 seen "
       "before)"},
      {CheckStepId::kRoot, StepReason::kRootWalkFailed, "why", 0,
       "a newer root of trust could not be verified"},
      {CheckStepId::kIndexKeys, StepReason::kListRejected, "why", 1,
       "the signing key list could not be verified"},
      {CheckStepId::kDescriptor,
       StepReason::kHashMismatch,
       {},
       3,
       "the release details don't match the update index"},
      {CheckStepId::kNotes,
       StepReason::kHashMismatch,
       {},
       3,
       "the release notes don't match the update index"},
      {CheckStepId::kIndex,
       StepReason::kOversized,
       {},
       2,
       "an answer from the server was too large"},
      {CheckStepId::kIndex,
       StepReason::kMalformed,
       {},
       2,
       "the update index could not be read"},
      {CheckStepId::kRoot,
       StepReason::kNoTrust,
       {},
       0,
       "this build was made without a built-in root of trust"},
  };
  for (const auto& c : cases) {
    SCOPED_TRACE(c.says.toStdString());
    auto s = Snapshot(Update(), FailedAt(c.step, c.reason, c.detail));
    s.last_attempt_failed = c.step <= CheckStepId::kIndex;
    const auto v = PresentUpdate(s, kNow, Release());
    EXPECT_EQ(v.headline, "GpgFrontend 2.1.9 is available");  // kept
    EXPECT_EQ(v.verification_tone, UpdateTone::kAttention);
    EXPECT_TRUE(v.verification.startsWith(
        "Update information couldn't be verified: " + c.says + "."))
        << v.verification.toStdString();
    EXPECT_EQ(v.verification.contains("Showing the result from 5 minute"),
              s.last_attempt_failed);
    EXPECT_EQ(v.device_rows[c.row].mark, DetailRow::Mark::kFailed);
    EXPECT_EQ(v.device_rows[c.row].lines[0],
              c.says.left(1).toUpper() + c.says.mid(1) + ".");
    for (auto i = c.row + 1; i < v.device_rows.size(); ++i) {
      if (i == kDownloadRow) continue;
      EXPECT_EQ(v.device_rows[i].mark, DetailRow::Mark::kNotRun) << i;
      EXPECT_EQ(v.device_rows[i].lines, QStringList{"Not run"}) << i;
    }
    ExpectHostRowsNeverVerified(v);
    EXPECT_TRUE(ReportText(v).contains("[failed] " +
                                       v.device_rows[c.row].title + ": "));
  }
}

TEST(UpdatePresentationTest, NoAnswerIsNotARefusal) {
  auto s = Snapshot(Update(), FailedAt(CheckStepId::kIndex,
                                       StepReason::kUnreachable, "HTTP 503"));
  s.last_attempt_failed = true;
  const auto v = PresentUpdate(s, kNow, Release());
  EXPECT_EQ(v.verification_tone, UpdateTone::kNeutral);
  EXPECT_EQ(v.verification,
            "Couldn't reach the update server. Showing the result from 5 "
            "minute(s) ago.");
  EXPECT_EQ(v.device_rows[2].lines,
            QStringList({"No update server answered.", "HTTP 503"}));

  // Release details nobody served cost the download link, nothing else.
  s = Snapshot(Update(),
               FailedAt(CheckStepId::kDescriptor, StepReason::kUnreachable));
  EXPECT_EQ(PresentUpdate(s, kNow, Release()).verification_tone,
            UpdateTone::kGood);
}

TEST(UpdatePresentationTest, TheReportSaysEverythingButNothingPersonal) {
  const auto v =
      PresentUpdate(Snapshot(WithFile(Update()), Verified()), kNow, Release());
  const auto report = ReportText(v);
  for (const auto* expected :
       {"GpgFrontend 2.1.8", "Commit abc123", "Release · series 2.1 (stable)",
        "Update information verified", "[verified] Root of trust",
        "4E179325A34DCF45", "Serial 42 from update.bktus.com",
        "Signed by 0123456789ABCDEF", "[not checked] Downloaded file",
        "Repository saturneric/GpgFrontend", "Signed by the maintainer"}) {
    EXPECT_TRUE(report.contains(QString::fromUtf8(expected))) << expected;
  }
  EXPECT_TRUE(report.contains("Checked " + kNow.toUTC().toString(Qt::ISODate)));
  // The full hash, once, for comparing a download by hand.
  EXPECT_TRUE(report.contains(v.offer_sha256));
  EXPECT_FALSE(report.contains("/home"));
  EXPECT_FALSE(report.contains("https://"));
}
