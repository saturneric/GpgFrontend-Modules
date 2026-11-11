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

#include <QTextBlock>
#include <QTextDocument>
#include <QTranslator>

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
  EXPECT_EQ(v.verification, "Not checked yet");
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

TEST(UpdatePresentationTest, FailedWithNothingKnownOffersCheckAgain) {
  UpdateSnapshot s;
  s.last_attempt = kNow;
  s.last_attempt_failed = true;
  const auto v = PresentUpdate(s, kNow);
  EXPECT_EQ(v.headline, "Couldn't reach the update server");
  EXPECT_EQ(v.tone, UpdateTone::kAttention);
  EXPECT_EQ(v.check_label, "Check again");
  EXPECT_TRUE(v.show_check);
  EXPECT_FALSE(v.detail.contains("try again later", Qt::CaseInsensitive));
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
  EXPECT_EQ(v.verification, "Verified · checked 5 minute(s) ago");
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
  EXPECT_EQ(v.check_label, "Check again");
  // The age is the last success's, not the failed attempt's.
  EXPECT_EQ(v.notice,
            "Couldn't reach the update server. Showing the result from 5 "
            "minute(s) ago.");
  EXPECT_EQ(v.verification, "Server not reached · checked just now");
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
  EXPECT_TRUE(v.verification.startsWith("Verified"))
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
  EXPECT_TRUE(v.verification.startsWith("Verified"))
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

// The dialog's blocks and its details: this build, the offer, one short
// status line, each step of the last check, and what only the release
// server checked, never marked as verified here.

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

/// The check stopped at @p id: it failed there, and what comes after it in
/// the check did not run.
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
  s.last_attempt = good.checked_at;
  s.last_check = record;
  return s;
}

/// A release build of a tree with release/root (the channel is configured).
auto Release(const QString& os = "linux", const QString& kind = "appimage")
    -> UpdateChecker::Build {
  UpdateChecker::Build b{"v2.1.8", "abc123",
                         ReleaseChannel{"gpgfrontend",
                                        {"https://update.example/"},
                                        {"https://builds.example/"},
                                        {{"root", {}}, {"list", {}}}}};
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

auto Titles(const QList<DetailRow>& rows) -> QStringList {
  QStringList titles;
  for (const auto& row : rows) titles << row.title;
  return titles;
}

void ExpectHostRowsNeverVerified(const UpdateView& v) {
  for (const auto& row : v.host_rows) {
    EXPECT_EQ(row.mark, DetailRow::Mark::kNone) << row.title.toStdString();
  }
}

}  // namespace

TEST(UpdatePresentationTest, ABuildWithoutTrustIsAStateNotAFailure) {
  UpdateChecker::Build build{"v2.3.0-alpha.496+ga7abc7903d41",
                             "a7abc7903d41aaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                             {}};
  build.nightly = 1;
  for (const auto& s : {UpdateSnapshot{},  // before any check
                        [] {  // after one: the root step says so too
                          UpdateSnapshot s;
                          s.last_attempt = kNow;
                          s.last_attempt_failed = true;
                          s.last_check = FailedAt(CheckStepId::kRoot,
                                                  StepReason::kNoTrust);
                          return s;
                        }()}) {
    const auto v = PresentUpdate(s, kNow, build);
    EXPECT_EQ(v.tone, UpdateTone::kNeutral);
    EXPECT_EQ(v.headline, "Updates aren't available for this build");
    EXPECT_EQ(v.detail,
              "It was built without the release trust files. Official builds "
              "include them and check for updates automatically.");
    EXPECT_FALSE(v.show_check);
    EXPECT_EQ(v.verification, "Not checked");
    EXPECT_EQ(v.build_version, "2.3.0-alpha.496");
    EXPECT_EQ(v.build_kind, "Nightly");
    EXPECT_EQ(v.build_commit, "a7abc7903d41");
    EXPECT_EQ(v.build_official, "Not checked");
    ASSERT_EQ(v.device_rows.size(), 1);
    EXPECT_EQ(v.device_rows[0].title, "Root of trust");
    EXPECT_EQ(v.device_rows[0].result, "Not included in this build");
    EXPECT_EQ(v.device_rows[0].mark, DetailRow::Mark::kNotChecked);
    EXPECT_TRUE(v.host_rows.isEmpty());
    EXPECT_TRUE(ReportText(v).contains(
        "[not checked] Root of trust: Not included in this build"));
  }
}

TEST(UpdatePresentationTest, UpToDateShowsTheBuildAndEveryStepVerified) {
  const auto v =
      PresentUpdate(Snapshot(Current(), Verified()), kNow, Release());
  EXPECT_EQ(v.headline, "You're up to date");
  EXPECT_EQ(v.tone, UpdateTone::kGood);
  EXPECT_EQ(v.build_kind, "Release · series 2.1 (stable)");
  EXPECT_EQ(v.build_official, "Yes");
  EXPECT_FALSE(v.show_offer);
  EXPECT_EQ(v.newest_label, "Newest in 2.1");
  EXPECT_EQ(v.newest, "2.1.8");
  EXPECT_TRUE(v.show_check);
  EXPECT_EQ(v.verification_tone, UpdateTone::kGood);
  EXPECT_EQ(v.verification, "Verified · checked 5 minute(s) ago");

  // Nothing offered: no downloaded file row.
  EXPECT_EQ(Titles(v.device_rows),
            QStringList({"Root of trust", "Signing key list", "Update index",
                         "Release details and notes", "Official build"}));
  for (const auto& row : v.device_rows) {
    EXPECT_EQ(row.mark, DetailRow::Mark::kVerified) << row.title.toStdString();
  }
  // Title, a short result, then labelled facts; ids and hashes only in
  // the tooltip (and the report).
  EXPECT_EQ(v.device_rows[0].result, "Verified");
  EXPECT_EQ(v.device_rows[0].facts,
            "Version 1 (built in) · Valid until " +
                kNow.addYears(2).toUTC().date().toString(Qt::ISODate) +
                " · Signed by 2 of 3 root keys");
  EXPECT_EQ(v.device_rows[0].tip,
            "Root keys: 4E179325A34DCF45, 27458E01B54EAF5D");
  EXPECT_EQ(v.device_rows[1].tip, "Signed by 9B09BDB5C65DAB85");
  EXPECT_TRUE(v.device_rows[2].facts.startsWith(
      "Serial 42 · update.bktus.com · Valid until "));
  EXPECT_TRUE(v.device_rows[2].facts.endsWith(" · No rollback"));
  EXPECT_EQ(v.device_rows[2].tip, "Signed by 0123456789ABCDEF");
  EXPECT_EQ(v.device_rows[3].facts, "Match the hashes in the update index");
  EXPECT_EQ(v.device_rows[3].tip, "Release details: " + QString(64, u'd') +
                                      "\nRelease notes: " + QString(64, u'e'));
  EXPECT_EQ(v.device_rows[4].facts, "Confirmed by builds.bktus.com");
  for (const auto& row : v.device_rows) {
    EXPECT_FALSE(row.facts.contains("4E179325A34DCF45"));
    EXPECT_FALSE(row.facts.contains(QString(64, u'd')));
  }
  ExpectHostRowsNeverVerified(v);
  EXPECT_EQ(v.security_url,
            "https://github.com/saturneric/GpgFrontend/blob/main/SECURITY.md");
}

TEST(UpdatePresentationTest, AnUpdateShowsTheOfferAndAnUncheckedDownload) {
  auto v =
      PresentUpdate(Snapshot(WithFile(Update()), Verified()), kNow, Release());
  EXPECT_EQ(v.tone, UpdateTone::kUpdate);
  EXPECT_TRUE(v.show_offer);
  EXPECT_EQ(v.offer_version, "2.1.9");
  EXPECT_EQ(v.offer_file, "GpgFrontend-2.1.9-linux-x86_64.AppImage");
  EXPECT_EQ(v.offer_size, "2.0 MiB");
  EXPECT_EQ(ShortHash(v.offer_sha256), "0123abcd…89abcdef");
  EXPECT_TRUE(v.newest.isEmpty());

  // The browser fetches it: never shown as verified.
  const auto file = std::find_if(v.device_rows.begin(), v.device_rows.end(),
                                 [](const auto& row) { return row.download; });
  ASSERT_NE(file, v.device_rows.end());
  EXPECT_EQ(file->mark, DetailRow::Mark::kNotChecked);
  EXPECT_EQ(file->result, "Not checked");
  EXPECT_EQ(file->facts,
            "Opens in your browser. Compare it with the SHA-256 above.");
  // The hash is in the Offered block, once: none here.
  EXPECT_TRUE(file->tip.isEmpty());
  EXPECT_FALSE(
      ReportText(v).contains("[not checked] Downloaded file: Not "
                             "checked · Opens in your browser. "
                             "Compare it with the SHA-256 above. · "));

  ASSERT_EQ(v.host_rows.size(), 2);
  EXPECT_EQ(v.host_rows[0].facts,
            "saturneric/GpgFrontend · build.yml · Tag v2.1.9");
  EXPECT_EQ(v.host_rows[1].title, "Signed by the maintainer");
  EXPECT_EQ(v.host_rows[1].result, "OpenPGP");
  EXPECT_EQ(v.host_rows[1].facts,
            "12F7 E885 8CF1 5BEC 9975 FF3C 5CA3 DA24 6843 FD03");
  EXPECT_EQ(v.host_rows[1].tip, "12F7E8858CF15BEC9975FF3C5CA3DA246843FD03");
  // Authenticode is said only where it applies: Windows installers.
  v = PresentUpdate(Snapshot(WithFile(Update()), Verified()), kNow,
                    Release("windows", "msi"));
  ASSERT_EQ(v.host_rows.size(), 3);
  EXPECT_EQ(v.host_rows[2].result, "Authenticode");
  EXPECT_EQ(v.host_rows[2].facts, "CN=Example");
  ExpectHostRowsNeverVerified(v);
}

TEST(UpdatePresentationTest, NeverCheckedShowsTheBuildOnly) {
  auto build = Release();
  build.version = "v2.2.4+gabc";
  build.commit = "0123456789abcdef0123";
  const auto v = PresentUpdate({}, kNow, build);
  EXPECT_EQ(v.build_version, "2.2.4");
  EXPECT_EQ(v.build_kind, "Release · series 2.2");
  EXPECT_EQ(v.build_commit, "0123456789ab");
  EXPECT_EQ(v.build_official, "Not checked");
  EXPECT_EQ(v.verification, "Not checked yet");
  EXPECT_EQ(v.check_label, "Check now");
  EXPECT_TRUE(v.device_rows.isEmpty());
  EXPECT_TRUE(v.host_rows.isEmpty());
}

TEST(UpdatePresentationTest, CheckingSaysSoOnTheStatusLine) {
  auto s = Snapshot(Update(), Verified());
  s.checking = true;
  const auto v = PresentUpdate(s, kNow, Release());
  EXPECT_EQ(v.verification, "Checking…");
  EXPECT_FALSE(v.check_enabled);
}

TEST(UpdatePresentationTest, AWithdrawnBuildIsPointedAtTheNewest) {
  const auto r = Known(ListOutcome::kFound, "v2.1.7", RemoteFact::kNotFound,
                       RemoteFact::kConfirmed);
  const auto v = PresentUpdate(Snapshot(r, Verified()), kNow, Release());
  EXPECT_EQ(v.tone, UpdateTone::kAttention);
  EXPECT_EQ(v.headline, "This version has been withdrawn");
  EXPECT_EQ(v.detail, "Move to 2.1.7, the newest version.");
  EXPECT_TRUE(v.show_download);
  EXPECT_EQ(v.offer_version, "2.1.7");
}

TEST(UpdatePresentationTest, AnUnofficialBuildIsNotAVerificationFailure) {
  auto record = Verified();
  record.Step(CheckStepId::kCommit) = {StepStatus::kFailed,
                                       StepReason::kNotFound};
  const auto r = Known(ListOutcome::kFound, "v2.1.8", RemoteFact::kConfirmed,
                       RemoteFact::kNotFound);
  const auto v = PresentUpdate(Snapshot(r, record), kNow, Release());
  EXPECT_EQ(v.headline, "This build isn't from the official repository");
  EXPECT_EQ(v.build_official, "No, not from the official repository");
  EXPECT_EQ(v.device_rows.back().mark, DetailRow::Mark::kFailed);
  EXPECT_EQ(v.device_rows.back().result, "Not among the official builds");
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
  EXPECT_FALSE(v.show_download);
  EXPECT_FALSE(v.installable);
  EXPECT_EQ(v.notice, "Updates for this build come from Flatpak.");
  EXPECT_FALSE(Titles(v.device_rows).contains("Downloaded file"));
}

TEST(UpdatePresentationTest, ANewerBetaIsMentionedWhileBetasAreOff) {
  auto record = Verified();
  record.newer_beta = "v2.1.9-alpha.3";
  const auto v = PresentUpdate(Snapshot(Current(), record), kNow, Release());
  EXPECT_EQ(v.newer_beta, "A newer beta, 2.1.9-alpha.3, is available");
}

TEST(UpdatePresentationTest, AnInPlaceUpdateIsVerifiedAfterDownload) {
  auto r = Nightly("2.3.0-alpha.420");
  r.list.latest.html_url = "https://builds.bktus.com/x/linux-x86_64.AppImage";
  r.list.latest.download_sha256 = QString(64, u'a');
  r.list.latest.download_size = 1000;
  const auto view = PresentUpdate(Snapshot(r, Verified()), kNow);
  ASSERT_TRUE(view.installable);
  const auto file = [](const UpdateView& v) {
    return *std::find_if(v.device_rows.begin(), v.device_rows.end(),
                         [](const auto& row) { return row.download; });
  };

  EXPECT_EQ(file(PresentInstall(view, {})).mark, DetailRow::Mark::kNotRun);
  auto v = PresentInstall(view, {InstallState::Stage::kInstalled});
  EXPECT_EQ(v.download_label, "Restart now");
  EXPECT_EQ(file(v).mark, DetailRow::Mark::kVerified);
  EXPECT_TRUE(
      file(v).facts.startsWith("Size and SHA-256 matched after download"));
  v = PresentInstall(view, {InstallState::Stage::kFailed, 0, 0, "bad hash"});
  EXPECT_EQ(file(v).mark, DetailRow::Mark::kFailed);
  EXPECT_EQ(file(v).result, "bad hash");
}

TEST(UpdatePresentationTest, NoAnswerIsAWarningWithCheckAgain) {
  UpdateSnapshot s;
  s.last_attempt = kNow;
  s.last_attempt_failed = true;
  s.last_check =
      FailedAt(CheckStepId::kIndex, StepReason::kUnreachable, "HTTP 503");
  auto v = PresentUpdate(s, kNow, Release());
  EXPECT_EQ(v.tone, UpdateTone::kAttention);
  EXPECT_EQ(v.headline, "Couldn't reach the update server");
  EXPECT_EQ(v.check_label, "Check again");
  EXPECT_TRUE(v.show_check);
  EXPECT_EQ(v.verification_tone, UpdateTone::kAttention);
  EXPECT_EQ(v.verification, "Server not reached · checked just now");
  EXPECT_EQ(v.device_rows[2].result, "No server answered");
  EXPECT_EQ(v.device_rows[2].facts, "HTTP 503");

  // Release details nobody served cost the download link, nothing else.
  s = Snapshot(Update(),
               FailedAt(CheckStepId::kDescriptor, StepReason::kUnreachable));
  EXPECT_EQ(PresentUpdate(s, kNow, Release()).verification_tone,
            UpdateTone::kGood);
}

// Each reason a check can be refused for: an error, stated once in full in
// the header, short in its row, never on the status line.
TEST(UpdatePresentationTest, EachRefusalIsNamedOnce) {
  struct Case {
    CheckStepId step;
    StepReason reason;
    QString detail;
    QString says;
  };
  const QDateTime expiry =
      QDateTime::fromString("2027-01-10T00:00:00Z", Qt::ISODate);
  const auto day = expiry.toUTC().date().toString(Qt::ISODate);
  const std::vector<Case> cases{
      {CheckStepId::kIndex,
       StepReason::kBadSignature,
       {},
       "the update index's signature is not valid"},
      {CheckStepId::kIndex, StepReason::kKeyNotListed, "FEDCBA9876543210",
       "the update index is signed by a key that is not trusted"},
      {CheckStepId::kRoot, StepReason::kExpired, "2027-01-10T00:00:00Z",
       "the root of trust expired on " + day},
      {CheckStepId::kIndexKeys, StepReason::kExpired, "2027-01-10T00:00:00Z",
       "the signing key list expired on " + day},
      {CheckStepId::kIndex, StepReason::kExpired, "2027-01-10T00:00:00Z",
       "the update index expired on " + day},
      {CheckStepId::kIndex, StepReason::kRollback, "41 40",
       "the update index is older than one seen before (serial 40, 41 seen "
       "before)"},
      {CheckStepId::kRoot, StepReason::kRootWalkFailed, "why",
       "a newer root of trust could not be verified"},
      {CheckStepId::kIndexKeys, StepReason::kListRejected, "why",
       "the signing key list could not be verified"},
      {CheckStepId::kDescriptor,
       StepReason::kHashMismatch,
       {},
       "the release details don't match the update index"},
      {CheckStepId::kNotes,
       StepReason::kHashMismatch,
       {},
       "the release notes don't match the update index"},
      {CheckStepId::kIndex,
       StepReason::kOversized,
       {},
       "an answer from the server was too large"},
      {CheckStepId::kIndex,
       StepReason::kMalformed,
       {},
       "the update index could not be read"},
  };
  for (const auto& c : cases) {
    SCOPED_TRACE(c.says.toStdString());
    // Nothing known yet: the header is the refusal.
    UpdateSnapshot first;
    first.last_attempt = kNow;
    first.last_attempt_failed = c.step <= CheckStepId::kIndex;
    first.last_check = FailedAt(c.step, c.reason, c.detail);
    auto v = PresentUpdate(first, kNow, Release());
    if (c.step <= CheckStepId::kIndex) {
      EXPECT_EQ(v.tone, UpdateTone::kError);
      EXPECT_EQ(v.headline, "Update information couldn't be verified");
      EXPECT_EQ(v.detail, c.says.left(1).toUpper() + c.says.mid(1) + ".");
    }
    EXPECT_EQ(v.verification_tone, UpdateTone::kError);
    EXPECT_TRUE(v.verification.startsWith("Not verified · checked"));
    EXPECT_FALSE(v.verification.contains(c.says));
    EXPECT_TRUE(v.show_check);

    // A result from before: it stays, and the notice says why it is old.
    auto s = Snapshot(Update(), FailedAt(c.step, c.reason, c.detail));
    s.last_attempt = kNow;
    s.last_attempt_failed = c.step <= CheckStepId::kIndex;
    v = PresentUpdate(s, kNow, Release());
    EXPECT_EQ(v.headline, "GpgFrontend 2.1.9 is available");
    EXPECT_TRUE(v.notice.startsWith(
        "Update information couldn't be verified: " + c.says + "."))
        << v.notice.toStdString();
    EXPECT_EQ(v.notice.contains("Showing the result from 5 minute"),
              s.last_attempt_failed);
    // Stated once: not again on the status line or in the row.
    EXPECT_FALSE(v.verification.contains(c.says));
    const auto failed = std::find_if(
        v.device_rows.begin(), v.device_rows.end(),
        [](const auto& row) { return row.mark == DetailRow::Mark::kFailed; });
    ASSERT_NE(failed, v.device_rows.end());
    EXPECT_FALSE(failed->result.isEmpty());
    // The short form, not the header's sentence again.
    EXPECT_LT(failed->result.size(), c.says.size())
        << failed->result.toStdString();
    ExpectHostRowsNeverVerified(v);
    EXPECT_TRUE(ReportText(v).contains("[failed] " + failed->title + ": "));
  }
}

TEST(UpdatePresentationTest, WhatDidNotRunAfterAFailureIsOneRow) {
  auto s = Snapshot(
      Update(), FailedAt(CheckStepId::kIndexKeys, StepReason::kListRejected));
  s.last_attempt_failed = true;
  const auto v = PresentUpdate(s, kNow, Release());
  EXPECT_EQ(Titles(v.device_rows),
            QStringList(
                {"Root of trust", "Signing key list", "Later steps not run"}));
  EXPECT_EQ(v.device_rows[2].mark, DetailRow::Mark::kNotRun);
}

TEST(UpdatePresentationTest, StepsThatDoNotApplyAreLeftOut) {
  auto record = Verified();
  record.Step(CheckStepId::kDescriptor).status = StepStatus::kNotApplicable;
  record.Step(CheckStepId::kNotes).status = StepStatus::kNotApplicable;
  record.Step(CheckStepId::kCommit).status = StepStatus::kNotApplicable;
  const auto v = PresentUpdate(Snapshot(Current(), record), kNow, Release());
  EXPECT_EQ(Titles(v.device_rows),
            QStringList({"Root of trust", "Signing key list", "Update index"}));
}

TEST(UpdatePresentationTest, TheReportSaysEverythingButNothingPersonal) {
  const auto v =
      PresentUpdate(Snapshot(WithFile(Update()), Verified()), kNow, Release());
  const auto report = ReportText(v);
  for (const auto* expected :
       {"GpgFrontend 2.1.8", "Commit abc123", "Release · series 2.1 (stable)",
        "Verified · checked", "[verified] Root of trust", "4E179325A34DCF45",
        "Serial 42 · update.bktus.com", "0123456789ABCDEF",
        "[not checked] Downloaded file", "saturneric/GpgFrontend",
        "Signed by the maintainer"}) {
    EXPECT_TRUE(report.contains(QString::fromUtf8(expected))) << expected;
  }
  EXPECT_TRUE(report.contains(
      "Checked " + kNow.addSecs(-300).toUTC().toString(Qt::ISODate)));
  // The full hashes, for comparing a download by hand.
  EXPECT_TRUE(report.contains(v.offer_sha256));
  EXPECT_TRUE(report.contains(QString(64, u'd')));
  EXPECT_FALSE(report.contains("/home"));
  EXPECT_FALSE(report.contains("https://"));
}

TEST(UpdatePresentationTest, NotesHeadingsStayAtTheDialogsScale) {
  QTextDocument doc;
  doc.setMarkdown("# One\n\n## Two\n\n### Three\n\nBody text\n");
  TameNotesHeadings(&doc, 10);
  QList<qreal> sizes;
  for (auto block = doc.begin(); block.isValid(); block = block.next()) {
    const auto fragment = block.begin().fragment();
    if (block.blockFormat().headingLevel() > 0) {
      EXPECT_EQ(fragment.charFormat().fontWeight(), QFont::Bold);
      EXPECT_FALSE(
          fragment.charFormat().hasProperty(QTextFormat::FontSizeAdjustment));
      sizes << fragment.charFormat().fontPointSize();
    }
  }
  EXPECT_EQ(sizes, QList<qreal>({11, 11, 10}));
}

TEST(UpdatePresentationTest, SizesAreWholeMebibytesFromTen) {
  auto r = WithFile(Update());
  r.list.latest.download_size = 118 * 1024 * 1024 + 300 * 1024;
  EXPECT_EQ(PresentUpdate(Snapshot(r, Verified()), kNow, Release()).offer_size,
            "118 MiB");
  r.list.latest.download_size = 7 * 1024 * 1024 + 300 * 1024;
  EXPECT_EQ(PresentUpdate(Snapshot(r, Verified()), kNow, Release()).offer_size,
            "7.3 MiB");
}

TEST(UpdatePresentationTest, DatesReadOneWayOnly) {
  auto record = Verified();
  record.root_expires =
      QDateTime::fromString("2028-11-06T18:12:35Z", Qt::ISODate);
  const auto v = PresentUpdate(Snapshot(Current(), record), kNow, Release());
  EXPECT_TRUE(v.device_rows[0].facts.contains("Valid until 2028-11-06"))
      << v.device_rows[0].facts.toStdString();
  EXPECT_FALSE(v.device_rows[0].facts.contains("11/6/28"));
}

namespace {

/// English plural forms, as en_US.ts gives them: proves the count goes
/// through the translation mechanism, not a literal "(s)".
class EnglishPlurals : public QTranslator {
 public:
  auto translate(const char* context, const char* source,
                 const char* disambiguation, int n) const -> QString override {
    Q_UNUSED(context);
    Q_UNUSED(disambiguation);
    if (QLatin1StringView(source) == u"%n key(s)") {
      return n == 1 ? QStringLiteral("%n key") : QStringLiteral("%n keys");
    }
    return {};
  }
  [[nodiscard]] auto isEmpty() const -> bool override { return false; }
};

}  // namespace

TEST(UpdatePresentationTest, CountsArePluralsOfTheTranslation) {
  // A translator needs an application to be installed in.
  int argc = 0;
  std::unique_ptr<QCoreApplication> app;
  if (QCoreApplication::instance() == nullptr) {
    app = std::make_unique<QCoreApplication>(argc, nullptr);
  }
  EnglishPlurals plurals;
  QCoreApplication::installTranslator(&plurals);
  auto record = Verified();
  auto v = PresentUpdate(Snapshot(Current(), record), kNow, Release());
  EXPECT_TRUE(v.device_rows[1].facts.contains("1 key ·"))
      << v.device_rows[1].facts.toStdString();
  record.list_keys = 3;
  v = PresentUpdate(Snapshot(Current(), record), kNow, Release());
  EXPECT_TRUE(v.device_rows[1].facts.contains("3 keys ·"));
  QCoreApplication::removeTranslator(&plurals);
}

TEST(UpdatePresentationTest, OnlyAReleaseIsSignedByTheMaintainer) {
  auto release =
      PresentUpdate(Snapshot(Current(), Verified()), kNow, Release());
  EXPECT_TRUE(Titles(release.host_rows).contains("Signed by the maintainer"));
  auto nightly =
      PresentUpdate(Snapshot(Nightly("2.3.0-alpha.414"), Verified()), kNow);
  EXPECT_FALSE(Titles(nightly.host_rows).contains("Signed by the maintainer"));
  EXPECT_EQ(Titles(nightly.host_rows), QStringList{"Built by GitHub Actions"});
}

TEST(UpdatePresentationTest, TheServerGroupNamesOnlyWhatTheRootSays) {
  // No repository in the root's host_policy: no server group, rather than
  // an empty or made-up name.
  auto record = Verified();
  record.repository.clear();
  auto v = PresentUpdate(Snapshot(Current(), record), kNow, Release());
  EXPECT_TRUE(v.host_rows.isEmpty());
  EXPECT_TRUE(v.security_url.isEmpty());
  // Otherwise the repository as the root names it, never empty.
  v = PresentUpdate(Snapshot(Current(), Verified()), kNow, Release());
  ASSERT_FALSE(v.host_rows.isEmpty());
  EXPECT_TRUE(v.host_rows[0].facts.startsWith("saturneric/GpgFrontend · "));
  EXPECT_EQ(v.host_rows[0].tip, ".github/workflows/build.yml");
}
