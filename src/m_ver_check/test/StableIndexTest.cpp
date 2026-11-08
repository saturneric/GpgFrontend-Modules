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

#include <QCryptographicHash>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QTextBlock>
#include <QTextDocument>
#include <QUrl>
#include <map>
#include <memory>

#include "IndexFixture.h"
#include "ReleaseCheck.h"
#include "ReleaseIndex.h"
#include "UpdateChecker.h"
#include "UpdatePresentation.h"

// The stable line on the signed index: what a release build is offered, how
// its series' state reaches the page, and the checker's whole chain for it
// (index -> descriptor -> notes -> commit marker), minisign only.

using test_index::Comment;
using test_index::Index;
using test_index::kNow;
using test_index::Signer;

namespace {

constexpr auto kCommit = "0123456789abcdef0123456789abcdef01234567";
constexpr auto kBase = "https://update.example/";
constexpr auto kBuilds = "https://builds.example/";

auto Sha(const QByteArray& data) -> QString {
  return QString::fromLatin1(
      QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

/// A stable release's descriptor, as the offline stage writes it.
auto StableDescriptor(const QString& version) -> QByteArray {
  const auto art = [&](const char* os, const char* kind, const char* name) {
    return QJsonObject{
        {"os", os},
        {"arch", "x86_64"},
        {"kind", kind},
        {"flavor", "installed"},
        {"name", name},
        {"sha256", QString(64, u'a')},
        {"size", 1234},
        {"path", QStringLiteral("releases/%1/%2").arg(version, name)}};
  };
  return QJsonDocument(
             QJsonObject{{"kind", "stable"},
                         {"version", version},
                         {"artifacts",
                          QJsonArray{art("linux", "appimage",
                                         "GpgFrontend-x-linux-x86_64.AppImage"),
                                     art("windows", "msi",
                                         "GpgFrontend-x-windows-x86_64.msi")}}})
      .toJson();
}

/// The parts of an index that release builds read.
struct Stable {
  QJsonObject series;
  QJsonObject releases;
  QString stable;

  void Series(const QString& key, const QString& status, const QString& latest,
              const QString& successor = {},
              const QJsonObject& platforms = {}) {
    QJsonObject s{{"status", status},
                  {"latest", latest.isEmpty() ? QJsonValue(QJsonValue::Null)
                                              : QJsonValue(latest)},
                  {"platforms", platforms}};
    if (!successor.isEmpty()) s["successor"] = successor;
    if (status == "legacy") s["eol"] = "2027-12-31";
    series[key] = s;
  }

  void Release(const QString& version, const QByteArray& descriptor,
               const QByteArray& notes = {}) {
    QJsonObject r{{"descriptor", "releases/" + version + "/release.json"},
                  {"sha256", Sha(descriptor)}};
    if (!notes.isEmpty()) {
      const auto path = "notes/" + version + "/" + Sha(notes) + ".md";
      r["notes"] = path;
      r["notes_sha256"] = Sha(notes);
    }
    releases[version] = r;
  }

  [[nodiscard]] auto Extra() const -> QJsonObject {
    return {
        {"series", series},
        {"releases", releases},
        {"channels",
         QJsonObject{{"stable", stable.isEmpty() ? QJsonValue(QJsonValue::Null)
                                                 : QJsonValue(stable)},
                     {"nightly", QJsonValue::Null}}}};
  }
};

auto Parse(const Stable& s, qint64 serial = 5) -> IndexFacts {
  const auto f = ParseIndex(
      Index(serial, "", "gpgfrontend", kNow.addDays(14), {}, s.Extra()),
      Comment(serial), "gpgfrontend", kNow);
  EXPECT_TRUE(f.has_value());
  return f.value_or(IndexFacts{});
}

auto Linux(const QString& glibc = "2.39") -> Platform {
  return {"linux", "x86_64", "", "appimage", glibc};
}

}  // namespace

// ------------------------------------------------------------------ model

TEST(StableIndexTest, ReadsSeriesReleasesAndTheStableChannel) {
  Stable s;
  s.Series("2.2", "stable", "2.2.4", {}, {{"linux_glibc", "2.35"}});
  s.Series("2.1", "legacy", "2.1.13", "2.2");
  s.Release("2.2.4", StableDescriptor("2.2.4"), "Notes.\n");
  s.stable = "2.2.4";
  const auto f = Parse(s);
  EXPECT_EQ(f.stable_version, "2.2.4");
  EXPECT_EQ(f.series.value("2.2").latest, "2.2.4");
  EXPECT_EQ(f.series.value("2.2").platforms.value("linux_glibc"), "2.35");
  EXPECT_EQ(f.series.value("2.1").successor, "2.2");
  EXPECT_EQ(f.series.value("2.1").eol, "2027-12-31");
  ASSERT_TRUE(f.releases.contains("2.2.4"));
  EXPECT_EQ(f.releases.value("2.2.4").notes_sha256, Sha("Notes.\n"));
}

TEST(StableIndexTest, AMalformedEntryCostsThatEntryOnly) {
  Stable s;
  s.Series("2.2", "stable", "2.2.4");
  s.Release("2.2.4", StableDescriptor("2.2.4"));
  s.releases["2.2.3"] = QJsonObject{{"descriptor", "../escape/release.json"},
                                    {"sha256", QString(64, u'1')}};
  s.releases["2.2.2"] =
      QJsonObject{{"descriptor", "releases/2.2.2/r.json"}, {"sha256", "short"}};
  s.releases["not-a-version"] = s.releases["2.2.4"];
  const auto f = Parse(s);
  EXPECT_EQ(f.releases.keys(), QStringList{"2.2.4"});
}

TEST(StableIndexTest, AMalformedStableChannelRefusesTheIndex) {
  Stable s;
  auto extra = s.Extra();
  extra["channels"] = QJsonObject{{"stable", 7}, {"nightly", QJsonValue::Null}};
  EXPECT_FALSE(
      ParseIndex(Index(5, "", "gpgfrontend", kNow.addDays(14), {}, extra),
                 Comment(5), "gpgfrontend", kNow));
}

TEST(StableIndexTest, AReleaseBuildIsOfferedItsOwnSeriesOnly) {
  Stable s;
  s.Series("2.3", "mainline", "");
  s.Series("2.2", "stable", "2.2.4");
  s.Series("2.1", "legacy", "2.1.13", "2.2");
  s.Release("2.2.4", StableDescriptor("2.2.4"), "Notes.\n");
  s.Release("2.1.13", StableDescriptor("2.1.13"));
  const auto f = Parse(s);

  const auto two_two = ChooseUpdate(f, "v2.2.3", false);
  ASSERT_EQ(two_two.list.outcome, ListOutcome::kFound);
  EXPECT_EQ(two_two.list.latest.version, "2.2.4");
  EXPECT_EQ(two_two.descriptor, "releases/2.2.4/release.json");
  EXPECT_EQ(two_two.notes_sha256, Sha("Notes.\n"));

  // 2.1 is offered 2.1's newest, never the newer series.
  EXPECT_EQ(ChooseUpdate(f, "v2.1.10", false).list.latest.version, "2.1.13");
  // A series with nothing listed: no offer, not a failure.
  EXPECT_EQ(ChooseUpdate(f, "v2.3.0", false).list.outcome,
            ListOutcome::kNoMatch);
  EXPECT_EQ(ChooseUpdate(f, "v9.9.9", false).list.outcome,
            ListOutcome::kNoMatch);
  // The historical beta of 2.2 is offered 2.2's newest.
  EXPECT_EQ(ChooseUpdate(f, "v2.2.0-beta.1", false).list.latest.version,
            "2.2.4");
}

TEST(StableIndexTest, MainNightliesFollowTheChannelAcrossACut) {
  const auto f = ParseIndex(Index(5, "2.4.0-alpha.2+gabc"), Comment(5),
                            "gpgfrontend", kNow);
  ASSERT_TRUE(f);
  const auto main = ChooseUpdate(*f, "v2.3.0-alpha.400+gdef", true);
  EXPECT_EQ(main.list.outcome, ListOutcome::kFound);
  EXPECT_FALSE(main.other_series);
  // A release branch's nightly (a fix build of 2.2) is offered nothing.
  const auto branch = ChooseUpdate(*f, "v2.2.5-alpha.3+g123", true);
  EXPECT_EQ(branch.list.outcome, ListOutcome::kNoMatch);
  EXPECT_TRUE(branch.other_series);
  // The historical beta.dev nightlies of main stay main's.
  EXPECT_FALSE(ChooseUpdate(*f, "v2.2.0-beta.1.dev.4+gabc", true).other_series);
}

TEST(StableIndexTest, ListedWithdrawnAndUnknownReleases) {
  Stable s;
  s.Series("2.2", "stable", "2.2.4");
  s.Release("2.2.4", StableDescriptor("2.2.4"));
  s.Release("2.2.3", StableDescriptor("2.2.3"));
  auto extra = s.Extra();
  const auto f = ParseIndex(
      Index(5, "", "gpgfrontend", kNow.addDays(14), {"2.2.3"}, extra),
      Comment(5), "gpgfrontend", kNow);
  ASSERT_TRUE(f);
  EXPECT_EQ(ReleaseFact(*f, "v2.2.4"), RemoteFact::kConfirmed);
  EXPECT_EQ(ReleaseFact(*f, "v2.2.3"), RemoteFact::kNotFound);
  // Older than the index (2.1.4): unknown, never "unreleased".
  EXPECT_EQ(ReleaseFact(*f, "v2.1.4"), RemoteFact::kUnknown);
}

TEST(StableIndexTest, ASeriesNoticeNamesTheSuccessorOnlyWhereItRuns) {
  Stable s;
  s.Series(
      "2.2", "stable", "2.2.4", {},
      {{"windows", "10.0.17763"}, {"macos", "15.0"}, {"linux_glibc", "2.35"}});
  s.Series("2.1", "legacy", "2.1.13", "2.2");
  s.Series("2.0", "eol", "", "2.2");
  const auto f = Parse(s);

  const auto here = NoticeFor(f, "v2.1.10", Linux("2.39"));
  EXPECT_EQ(here.series, "2.1");
  EXPECT_EQ(here.status, "legacy");
  EXPECT_EQ(here.eol, "2027-12-31");
  EXPECT_EQ(here.successor_latest, "2.2.4");
  EXPECT_TRUE(here.successor_runs_here);

  EXPECT_FALSE(NoticeFor(f, "v2.1.10", Linux("2.31")).successor_runs_here);
  EXPECT_FALSE(NoticeFor(f, "v2.1.10", Linux("")).successor_runs_here);
  EXPECT_FALSE(
      NoticeFor(f, "v2.1.10", {"windows", "x86_64", "10.0.17134", "msi", ""})
          .successor_runs_here);
  EXPECT_TRUE(
      NoticeFor(f, "v2.1.10", {"windows", "x86_64", "10.0.22631", "msi", ""})
          .successor_runs_here);
  EXPECT_FALSE(NoticeFor(f, "v2.1.10", {"macos", "arm64", "14.6", "dmg", ""})
                   .successor_runs_here);
  EXPECT_EQ(NoticeFor(f, "v2.0.7", Linux()).status, "eol");
  // A supported series has nothing to say.
  EXPECT_TRUE(NoticeFor(f, "v2.2.3", Linux()).series.isEmpty());
}

TEST(StableIndexTest, NotesLinkOnlyWhereTheProductAllows) {
  const QStringList hosts{"bktus.com", "*.bktus.com",
                          "github.com/saturneric/GpgFrontend"};
  for (const auto* ok :
       {"https://bktus.com/", "https://www.gpgfrontend.bktus.com/x",
        "https://github.com/saturneric/GpgFrontend/issues/1",
        "https://github.com/saturneric/gpgfrontend"}) {
    EXPECT_TRUE(IsAllowedNotesLink(QUrl(ok), hosts)) << ok;
  }
  for (const auto* bad :
       {"http://bktus.com/", "https://evilbktus.com/",
        "https://bktus.com.evil/",
        "https://github.com/saturneric/GpgFrontendEvil",
        "https://github.com/someone/else", "https://user@bktus.com/",
        "javascript:alert(1)", "file:///etc/passwd", "notes.md"}) {
    EXPECT_FALSE(IsAllowedNotesLink(QUrl(bad), hosts)) << bad;
  }
  EXPECT_FALSE(IsAllowedNotesLink(QUrl("https://bktus.com/"), {}));
}

TEST(StableIndexTest, RestrictedNotesKeepOnlyAllowedLinks) {
  if (QGuiApplication::instance() == nullptr) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
  }
  static int argc = 1;
  static std::array<char*, 1> argv{const_cast<char*>("test")};
  const auto app = QGuiApplication::instance() == nullptr
                       ? std::make_unique<QGuiApplication>(argc, argv.data())
                       : nullptr;
  QTextDocument doc;
  doc.setMarkdown(
      "See [the page](https://bktus.com/x) and [this](https://evil.example/)."
      "\n");
  RestrictNotesLinks(&doc, {"bktus.com"});
  QStringList anchors;
  for (auto block = doc.begin(); block.isValid(); block = block.next()) {
    for (auto it = block.begin(); !it.atEnd(); ++it) {
      if (it.fragment().charFormat().isAnchor()) {
        anchors << it.fragment().charFormat().anchorHref();
      }
    }
  }
  EXPECT_EQ(anchors, QStringList{"https://bktus.com/x"});
  EXPECT_TRUE(doc.toPlainText().contains("this"));
}

TEST(StableIndexTest, TheSeriesNoticeIsWorded) {
  SoftwareVersion::Notice n{"2.1", "legacy", "2027-12-31",
                            "2.2", "2.2.4",  true};
  EXPECT_TRUE(SeriesNoticeText(n).contains("2.2.4"));
  n.successor_runs_here = false;
  EXPECT_FALSE(SeriesNoticeText(n).contains("2.2.4"));
  n.status = "eol";
  EXPECT_FALSE(SeriesNoticeText(n).isEmpty());
  EXPECT_TRUE(SeriesNoticeText({}).isEmpty());
}

TEST(StableIndexTest, GarbageNeverCrashesTheReaders) {
  Stable s;
  s.Series("2.2", "stable", "2.2.4");
  s.Release("2.2.4", StableDescriptor("2.2.4"), "Notes.\n");
  const auto good = Index(5, "2.3.0-alpha.1", "gpgfrontend", kNow.addDays(14),
                          {"2.2.1"}, s.Extra());
  auto* rng = QRandomGenerator::global();
  for (int round = 0; round < 2000; ++round) {
    auto bytes = good;
    if (round % 3 == 0) {
      bytes.truncate(static_cast<qsizetype>(rng->bounded(bytes.size())));
    } else {
      for (int k = 0; k < 4; ++k) {
        bytes[static_cast<qsizetype>(rng->bounded(bytes.size()))] =
            static_cast<char>(rng->bounded(256));
      }
    }
    const auto f = ParseIndex(bytes, Comment(5), "gpgfrontend", kNow);
    if (f) {
      ChooseUpdate(*f, "v2.2.3", false);
      ChooseUpdate(*f, "v2.3.0-alpha.0", true);
      NoticeFor(*f, "v2.1.9", Linux());
      ReleaseFact(*f, "v2.2.4");
    }
    SelectArtifact(bytes, Linux());
    IsSafeRelativePath(QString::fromUtf8(bytes.left(64)));
  }
  SUCCEED();
}

// ---------------------------------------------------------------- checker

namespace {

/// The release server's buckets as a URL table, answered at once.
struct Server {
  Signer signer;
  std::map<QString, std::pair<int, QByteArray>> table;
  QStringList asked;
  QByteArray stored;

  auto Fetcher() -> UpdateChecker::Fetcher {
    return [this](const QUrl& url, const UpdateChecker::Reply& reply) {
      asked << url.toString();
      const auto it = table.find(url.toString());
      if (it == table.end()) {
        reply(404, {});
      } else {
        reply(it->second.first, it->second.second);
      }
    };
  }

  void Publish(const Stable& s, qint64 serial = 5) {
    const auto index =
        Index(serial, "", "gpgfrontend", kNow.addDays(14), {}, s.Extra());
    table[QString(kBase) + "gpgfrontend/v1/index.json"] = {200, index};
    table[QString(kBase) + "gpgfrontend/v1/index.json.minisig"] = {
        200, signer.Sign(index, Comment(serial))};
  }

  void Put(const QString& path, const QByteArray& data, int status = 200) {
    table[QString(kBuilds) + "gpgfrontend/" + path] = {status, data};
  }

  auto Make(const QString& version = "v2.2.3",
            const Platform& platform = Linux())
      -> std::unique_ptr<UpdateChecker> {
    return std::make_unique<UpdateChecker>(
        UpdateChecker::Build{
            version,
            kCommit,
            {"gpgfrontend", {kBase}, {kBuilds}, {signer.PublicKey()}},
            platform,
            0},
        Fetcher(),
        UpdateChecker::Store{[this] { return stored; },
                             [this](const QByteArray& d) { stored = d; }},
        [] { return kNow; });
  }
};

/// A 2.2 series with 2.2.4 released (descriptor and notes stored), and the
/// running 2.2.3 listed.
auto Released(Server& server, const QByteArray& notes = "## 2.2.4\n- Fixes.\n")
    -> Stable {
  Stable s;
  s.Series("2.2", "stable", "2.2.4");
  const auto descriptor = StableDescriptor("2.2.4");
  s.Release("2.2.4", descriptor, notes);
  s.Release("2.2.3", StableDescriptor("2.2.3"));
  s.stable = "2.2.4";
  server.Put("releases/2.2.4/release.json", descriptor);
  server.Put("notes/2.2.4/" + Sha(notes) + ".md", notes);
  server.Put(QStringLiteral("commits/") + kCommit, {});
  return s;
}

}  // namespace

TEST(StableCheckTest, ANewerReleaseIsOfferedWithItsFileAndNotes) {
  Server server;
  server.Publish(Released(server));
  auto c = server.Make();
  c->Start(CheckMode::kForce);
  ASSERT_TRUE(c->State().last_good);
  const auto& r = *c->State().last_good;
  EXPECT_EQ(Decide(r), Verdict::kUpdateAvailable);
  EXPECT_EQ(r.list.latest.version, "2.2.4");
  EXPECT_EQ(
      r.list.latest.html_url,
      QString(kBuilds) +
          "gpgfrontend/releases/2.2.4/GpgFrontend-x-linux-x86_64.AppImage");
  EXPECT_EQ(r.list.latest.download_sha256, QString(64, u'a'));
  EXPECT_EQ(r.list.latest.notes, "## 2.2.4\n- Fixes.\n");
  EXPECT_EQ(r.tag_fact, RemoteFact::kConfirmed);
  EXPECT_EQ(r.commit_fact, RemoteFact::kConfirmed);
  EXPECT_TRUE(IsAuthoritative(r));
  for (const auto& url : server.asked) {
    EXPECT_FALSE(url.contains("github.com")) << url.toStdString();
    EXPECT_FALSE(url.endsWith(".sig")) << url.toStdString();  // no OpenPGP
  }

  const auto view = PresentUpdate(c->State(), kNow);
  EXPECT_EQ(view.download_url, r.list.latest.html_url);
  EXPECT_FALSE(view.installable);  // a stable release is not put in place
  EXPECT_TRUE(view.show_notes);
  EXPECT_TRUE(view.freshness.contains("Signed update index"));
}

TEST(StableCheckTest, TheLatestReleaseIsUpToDate) {
  Server server;
  server.Publish(Released(server));
  auto c = server.Make("v2.2.4");
  c->Start(CheckMode::kForce);
  EXPECT_EQ(Decide(*c->State().last_good), Verdict::kUpToDate);
}

TEST(StableCheckTest, ADescriptorNotTheIndexsGivesNoLink) {
  Server server;
  server.Publish(Released(server));
  server.Put("releases/2.2.4/release.json", StableDescriptor("6.6.6"));
  auto c = server.Make();
  c->Start(CheckMode::kForce);
  const auto& r = *c->State().last_good;
  EXPECT_EQ(Decide(r), Verdict::kUpdateAvailable);  // the verdict stands
  EXPECT_TRUE(r.list.latest.html_url.isEmpty());
  EXPECT_TRUE(PresentUpdate(c->State(), kNow).download_url.isEmpty());
}

TEST(StableCheckTest, NotesNotTheIndexsAreNotShown) {
  Server server;
  const auto s = Released(server);
  server.Publish(s);
  server.Put("notes/2.2.4/" + Sha("## 2.2.4\n- Fixes.\n") + ".md",
             "## 2.2.4\n- Something else.\n");
  auto c = server.Make();
  c->Start(CheckMode::kForce);
  EXPECT_TRUE(c->State().last_good->list.latest.notes.isEmpty());
  EXPECT_FALSE(PresentUpdate(c->State(), kNow).show_notes);
}

TEST(StableCheckTest, NotesTooLargeAreNotTaken) {
  Server server;
  const QByteArray huge(UpdateChecker::kMaxNotesBytes + 1, 'x');
  server.Publish(Released(server, huge));
  auto c = server.Make();
  c->Start(CheckMode::kForce);
  EXPECT_TRUE(c->State().last_good->list.latest.notes.isEmpty());
}

TEST(StableCheckTest, AnIndexAndSignatureOutOfStepAreAskedAgain) {
  Server server;
  const auto s = Released(server);
  server.Publish(s, 5);
  // The signature of the next index, the body of this one: the publish
  // window. A second read of the same base finds them agreeing.
  const auto index_url = QString(kBase) + "gpgfrontend/v1/index.json";
  const auto good = server.table[index_url];
  server.Publish(s, 6);
  auto next_sig = server.table[index_url + ".minisig"];
  server.table[index_url] = good;
  int reads = 0;
  auto c = std::make_unique<UpdateChecker>(
      UpdateChecker::Build{
          "v2.2.3",
          kCommit,
          {"gpgfrontend", {kBase}, {kBuilds}, {server.signer.PublicKey()}},
          Linux(),
          0},
      [&](const QUrl& url, const UpdateChecker::Reply& reply) {
        if (url.toString() == index_url && ++reads == 2) {
          server.Publish(s, 6);  // the server has finished publishing
        }
        server.Fetcher()(url, reply);
      },
      UpdateChecker::Store{}, [] { return kNow; });
  server.table[index_url + ".minisig"] = next_sig;
  c->Start(CheckMode::kForce);
  EXPECT_EQ(reads, 2);
  ASSERT_TRUE(c->State().last_good);
  EXPECT_EQ(c->State().max_serial, 6);
}

TEST(StableCheckTest, NothingVerifiedClaimsNothing) {
  Server server;
  auto c = server.Make();
  c->Start(CheckMode::kForce);
  EXPECT_FALSE(c->State().last_good.has_value());
  EXPECT_TRUE(c->State().last_attempt_failed);
}

TEST(StableCheckTest, ALegacySeriesShowsItsNotice) {
  Server server;
  Stable s;
  s.Series("2.2", "stable", "2.2.4", {}, {{"linux_glibc", "2.35"}});
  s.Series("2.1", "legacy", "2.1.13", "2.2");
  s.Release("2.1.13", StableDescriptor("2.1.13"));
  s.Release("2.2.4", StableDescriptor("2.2.4"));
  server.Publish(s);
  server.Put(QStringLiteral("commits/") + kCommit, {});
  auto c = server.Make("v2.1.13", Linux("2.39"));
  c->Start(CheckMode::kForce);
  const auto& r = *c->State().last_good;
  EXPECT_EQ(Decide(r), Verdict::kUpToDate);
  EXPECT_EQ(r.notice.successor_latest, "2.2.4");
  EXPECT_TRUE(PresentUpdate(c->State(), kNow).notice.contains("2.2.4"));
}

TEST(StableCheckTest, AReleaseBranchNightlyIsOfferedNothing) {
  Server server;
  const auto index = Index(5, "2.3.0-alpha.9", "gpgfrontend", kNow.addDays(14));
  server.table[QString(kBase) + "gpgfrontend/v1/index.json"] = {200, index};
  server.table[QString(kBase) + "gpgfrontend/v1/index.json.minisig"] = {
      200, server.signer.Sign(index, Comment(5))};
  server.Put(QStringLiteral("commits/") + kCommit, {});
  auto c = std::make_unique<UpdateChecker>(
      UpdateChecker::Build{
          "v2.2.5-alpha.3+g123",
          kCommit,
          {"gpgfrontend", {kBase}, {kBuilds}, {server.signer.PublicKey()}},
          Linux(),
          1},
      server.Fetcher(), UpdateChecker::Store{}, [] { return kNow; });
  c->Start(CheckMode::kForce);
  const auto& r = *c->State().last_good;
  EXPECT_TRUE(r.other_series);
  EXPECT_NE(Decide(r), Verdict::kUpdateAvailable);
  UpdateSnapshot snapshot = c->State();
  EXPECT_EQ(AttentionFor(snapshot, kNow), UpdateAttention::kNone);
  EXPECT_TRUE(
      PresentUpdate(snapshot, kNow).headline.contains("release branch"));
}

// Beta tags are gone, but old builds still carry their versions: the tagged
// 2.2.0-beta.1 is a release build, main's untagged builds after it stay
// nightlies (the Host's word wins over both, when it gives it).
TEST(StableIndexTest, HistoricalBetaVersionsKeepTheirIdentity) {
  EXPECT_FALSE(IsNightlyBuild("v2.2.0-beta.1"));
  EXPECT_TRUE(IsNightlyBuild("v2.2.0-beta.1.dev.4+gabc"));
  EXPECT_TRUE(IsNightlyBuild("v2.3.0-alpha.12+gabc"));
  EXPECT_FALSE(IsNightlyBuild("v2.2.4"));
}
