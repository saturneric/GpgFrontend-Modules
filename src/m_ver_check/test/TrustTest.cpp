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

// The update trust, as the checker holds it: the built-in root walked
// forward to the one an index names, the index-keys list it names, and the
// index taken only on a key that list holds.

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <memory>

#include "IndexFixture.h"
#include "RootTrust.h"

using test_index::Comment;
using test_index::Keys;
using test_index::kNow;
using test_index::ListDoc;
using test_index::Mirrors;
using test_index::RootDoc;
using test_index::Signer;
using test_index::SignList;
using test_index::SignRoot;
using test_index::TrustFor;

namespace {

constexpr auto kBase = "https://u.example/";
constexpr auto kCommit = "0123456789abcdef0123456789abcdef01234567";
constexpr auto kNightly = "2.3.0-alpha.9";

struct World {
  Signer index1, index2;
  Mirrors mirrors;
  QByteArray stored;
  QStringList warnings;
  EmbeddedTrust built = TrustFor(index1);  // root 1 (a, b; g), list 1

  auto Make() -> std::unique_ptr<UpdateChecker> {
    return std::make_unique<UpdateChecker>(
        UpdateChecker::Build{"v2.3.0-alpha.5", kCommit,
                             ReleaseChannel{"gpgfrontend", {kBase}, {kBase},
                                            built},
                             {}, 1, false,
                             [this](const QString& w) { warnings << w; }},
        mirrors.Fetcher(),
        UpdateChecker::Store{[this] { return stored; },
                             [this](const QByteArray& d) { stored = d; }},
        [] { return kNow; });
  }

  void Root(qint64 n, const QByteArray& doc, const QList<const Signer*>& by) {
    const auto url =
        QStringLiteral("%1gpgfrontend/v1/root/%2.root.json").arg(kBase).arg(n);
    mirrors.Put(url, doc);
    const auto sigs = SignRoot(doc, n, by);
    for (auto it = sigs.begin(); it != sigs.end(); ++it) {
      mirrors.Put(url + u'.' + it.key() + QStringLiteral(".minisig"),
                  it.value());
    }
  }

  void List(qint64 v, const QByteArray& doc, const Signer& by) {
    const auto url =
        QStringLiteral("%1gpgfrontend/v1/index-keys/%2.json").arg(kBase).arg(v);
    mirrors.Put(url, doc);
    mirrors.Put(url + QStringLiteral(".minisig"), SignList(doc, v, by));
  }

  /// An index of @p serial signed by @p by, naming root @p root and list
  /// @p list (0: from before an index named them).
  void Index(qint64 serial, const Signer& by, qint64 root = 0,
             qint64 list = 0) {
    QJsonObject hints;
    if (root > 0) {
      hints = {{"root_version", root}, {"index_keys_version", list}};
    }
    const auto index = test_index::Index(serial, kNightly, "gpgfrontend",
                                         kNow.addDays(14), {}, hints);
    mirrors.Serve(kBase, index, by.Sign(index, Comment(serial)));
  }

  /// One check: whether an index was taken.
  auto Check() -> bool {
    auto c = Make();
    c->Start(CheckMode::kForce);
    return !c->State().last_attempt_failed;
  }

  [[nodiscard]] auto Warned(const QString& what) const -> bool {
    return std::any_of(warnings.begin(), warnings.end(),
                       [&](const QString& w) { return w.contains(what); });
  }
};

/// <dir>/<sub>/<name> and every signature beside it, as the release tooling
/// writes them: <name>.<keyid>.minisig, or <name>.minisig (under "").
auto TrustFile(const QDir& dir, const QString& sub, const QString& name)
    -> SignedFile {
  const auto read = [&](const QString& file) {
    QFile f(dir.filePath(sub + u'/' + file));
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
  };
  SignedFile f{read(name), {}};
  for (const auto& sig :
       QDir(dir.filePath(sub)).entryList({name + QStringLiteral(".*")})) {
    const auto id = sig.mid(name.size() + 1).section(u'.', 0, 0);
    f.signatures.insert(id == QStringLiteral("minisig") ? QString() : id,
                        read(sig));
  }
  return f;
}

/// Root 1 of the test keys (a, b; g) with @p edit applied, signed by a and
/// b: parsed, or why not.
auto EditedRoot(const std::function<void(QJsonObject&)>& edit, QString* why)
    -> std::optional<TrustRoot> {
  const auto& k = Keys();
  auto obj = QJsonDocument::fromJson(RootDoc(1, {&k.a, &k.b}, k.g)).object();
  edit(obj);
  const auto doc = QJsonDocument(obj).toJson();
  return ParseRoot({doc, SignRoot(doc, 1, {&k.a, &k.b})}, "gpgfrontend", why);
}

/// @p obj's roles.<role>.<field> set to @p value.
void SetRole(QJsonObject& obj, const QString& role, const QString& field,
             const QJsonValue& value) {
  auto roles = obj["roles"].toObject();
  auto r = roles[role].toObject();
  r[field] = value;
  roles[role] = r;
  obj["roles"] = roles;
}

}  // namespace

TEST(TrustTest, AnIndexFromBeforeTheHintsVerifiesOnTheBuiltInTrust) {
  World w;
  w.Index(3, w.index1);
  EXPECT_TRUE(w.Check());
}

TEST(TrustTest, TheRootsAreWalkedToTheOneTheIndexNames) {
  World w;
  const auto& k = Keys();
  // Root 2: c replaces b. Root 3: g2 replaces g, with list 2 (index2).
  w.Root(2, RootDoc(2, {&k.a, &k.c}, k.g), {&k.a, &k.b, &k.c});
  w.Root(3, RootDoc(3, {&k.a, &k.c}, k.g2), {&k.a, &k.c});
  w.List(2, ListDoc(2, {&w.index2}), k.g2);
  w.Index(5, w.index2, 3, 2);
  EXPECT_TRUE(w.Check()) << w.warnings.join('\n').toStdString();

  // Kept: with nothing of it served any more, the next check still holds.
  w.mirrors.table.clear();
  w.Index(6, w.index2, 3, 2);
  EXPECT_TRUE(w.Check()) << w.warnings.join('\n').toStdString();
}

TEST(TrustTest, ARootNeedsEnoughOfTheOldKeysAndOfItsOwn) {
  const auto& k = Keys();
  {
    World w;  // signed by its own new keys only
    w.Root(2, RootDoc(2, {&k.c, &k.g2}, k.g), {&k.c, &k.g2});
    w.Index(5, w.index1, 2, 1);
    EXPECT_TRUE(w.Check());  // still root 1, whose list holds index1
    EXPECT_TRUE(w.Warned("too few of root 1's keys"));
  }
  {
    World w;  // its own threshold (all three) not met
    w.Root(2, RootDoc(2, {&k.a, &k.b, &k.c}, k.g, 3), {&k.a, &k.b});
    w.Index(5, w.index1, 2, 1);
    EXPECT_TRUE(w.Check());  // still root 1
    EXPECT_TRUE(w.Warned("too few of its own keys"));
  }
  {
    World w;  // a skipped version
    w.Root(3, RootDoc(3, {&k.a, &k.b}, k.g), {&k.a, &k.b});
    w.Index(5, w.index1, 3, 1);
    EXPECT_TRUE(w.Check());  // still root 1
    EXPECT_TRUE(w.Warned("2.root.json: HTTP 404"));
  }
}

TEST(TrustTest, AKeyListedTwiceCountsOnce) {
  // Root keys a, a and b, two needed: a alone must not meet it.
  const auto& k = Keys();
  const auto doc = RootDoc(1, {&k.a, &k.a, &k.b}, k.g, 2);
  QString why;
  EXPECT_FALSE(ParseRoot({doc, SignRoot(doc, 1, {&k.a})}, "gpgfrontend", &why));
  EXPECT_TRUE(why.contains("no usable roles")) << why.toStdString();
}

TEST(TrustTest, ARootIsReadAsTheReleaseToolingReadsIt) {
  QString why;
  ASSERT_TRUE(EditedRoot([](QJsonObject&) {}, &why)) << why.toStdString();

  // trust.py refuses each of these; so does the client.
  const std::vector<std::pair<const char*, std::function<void(QJsonObject&)>>>
      refused{
          {"G with a threshold other than 1",
           [](QJsonObject& o) { SetRole(o, "index_list", "threshold", 2); }},
          {"a threshold that is not a whole number",
           [](QJsonObject& o) { SetRole(o, "root", "threshold", 1.5); }},
          {"no host_policy", [](QJsonObject& o) { o.remove("host_policy"); }},
          {"a host_policy without its workflow",
           [](QJsonObject& o) {
             o["host_policy"] = QJsonObject{{"repository", "o/r"}};
           }},
          {"an expiry with an offset",
           [](QJsonObject& o) { o["expires"] = "2099-01-05T00:00:00+02:00"; }},
          {"an expiry with fractions of a second",
           [](QJsonObject& o) { o["expires"] = "2099-01-05T00:00:00.000Z"; }},
          {"an expiry not zero-padded",
           [](QJsonObject& o) { o["expires"] = "2099-1-5T0:0:0Z"; }},
          {"a key listed under no id",
           [](QJsonObject& o) {
             auto keys = o["keys"].toObject();
             keys[""] = "garbage";
             o["keys"] = keys;
           }},
      };
  for (const auto& [what, edit] : refused) {
    EXPECT_FALSE(EditedRoot(edit, nullptr)) << what;
  }
}

TEST(TrustTest, TimesAreReadInOneShapeOnly) {
  EXPECT_EQ(ReadUtcTime(QJsonValue("2028-01-05T01:02:03Z")),
            QDateTime(QDate(2028, 1, 5), QTime(1, 2, 3), QTimeZone::UTC));
  for (const auto* bad :
       {"2028-01-05T01:02:03", "2028-01-05T01:02:03+00:00", "2028-01-05",
        "2028-01-05T01:02:03.5Z", " 2028-01-05T01:02:03Z", ""}) {
    EXPECT_FALSE(ReadUtcTime(QJsonValue(bad)).isValid()) << bad;
  }
  EXPECT_FALSE(ReadUtcTime(QJsonValue(5)).isValid());
  EXPECT_EQ(ReadCount(QJsonValue(7)), 7);
  EXPECT_EQ(ReadCount(QJsonValue(1.5)), 0);
  EXPECT_EQ(ReadCount(QJsonValue(-3)), 0);
  EXPECT_EQ(ReadCount(QJsonValue("7")), 0);
}

TEST(TrustTest, OnlyKeyIdsOfAnUncheckedRootAreFetched) {
  // A root nobody has verified yet names which signatures to fetch: each
  // becomes a URL, so only key ids, each once, and only so many.
  const auto& k = Keys();
  const auto a = test_index::KeyId(k.a);
  QJsonArray ids{a, a, "../../x?y", a.toLower(), "", 5};
  for (int i = 0; i < 40; ++i) {
    ids.append(QStringLiteral("%1").arg(i, 16, 10, QLatin1Char('0')));
  }
  QJsonObject root{
      {"roles", QJsonObject{{"root", QJsonObject{{"keyids", ids}}}}}};
  const auto named = NamedRootKeys(QJsonDocument(root).toJson());
  EXPECT_EQ(named.size(), kMaxRootKeys);
  EXPECT_EQ(named.first(), a);
  EXPECT_EQ(named.count(a), 1);
  for (const auto& id : named) {
    EXPECT_EQ(id.size(), 16) << id.toStdString();
    EXPECT_EQ(id, id.toUpper()) << id.toStdString();
  }
}

TEST(TrustTest, ARootSaysWhoSignedItAndWhatTheHostChecks) {
  const auto& k = Keys();
  const auto root = ParseRoot(TrustFor(Signer()).root, "gpgfrontend");
  ASSERT_TRUE(root);
  EXPECT_EQ(QSet<QString>(root->signed_by.begin(), root->signed_by.end()),
            QSet<QString>({test_index::KeyId(k.a), test_index::KeyId(k.b)}));
  EXPECT_EQ(root->repository, "o/r");
  EXPECT_EQ(root->workflow, ".github/workflows/build.yml");

  // One the release tooling wrote, with a whole host_policy.
  const QDir dir(QStringLiteral(GF_TRUST_FIXTURE));
  const auto made =
      ParseRoot(TrustFile(dir, "root", "1.root.json"), "gpgfrontend");
  ASSERT_TRUE(made);
  EXPECT_GE(made->signed_by.size(), made->threshold);
  EXPECT_EQ(made->repository, "saturneric/GpgFrontend");
  EXPECT_EQ(made->workflow, ".github/workflows/build.yml");
  EXPECT_EQ(made->pgp_fingerprints,
            QStringList{"12F7E8858CF15BEC9975FF3C5CA3DA246843FD03"});
  EXPECT_EQ(made->authenticode_subject, "CN=Example");
}

TEST(TrustTest, AnIndexSignedByAKeyTheListDoesNotHoldIsRefused) {
  World w;
  w.Index(5, Signer(), 1, 1);
  EXPECT_FALSE(w.Check());
  EXPECT_TRUE(w.Warned("which index-keys 1 does not list"));
}

TEST(TrustTest, AKeyRotatedOutSignsNothingAfterTheWalk) {
  World w;
  const auto& k = Keys();
  w.List(2, ListDoc(2, {&w.index2}), k.g);
  w.Index(5, w.index2, 1, 2);
  ASSERT_TRUE(w.Check());
  // index1 is listed by the built-in list 1, no longer by list 2.
  w.Index(6, w.index1, 1, 1);
  EXPECT_FALSE(w.Check());
  EXPECT_TRUE(w.Warned("which index-keys 2 does not list"));
}

TEST(TrustTest, AListGDidNotSignIsRefused) {
  World w;
  const auto& k = Keys();
  w.List(2, ListDoc(2, {&w.index2}), k.a);  // a root key, not G
  w.Index(5, w.index2, 1, 2);
  EXPECT_FALSE(w.Check());
  EXPECT_TRUE(w.Warned("not signed by root 1's index_list key"));
}

TEST(TrustTest, AListIsNeverRolledBack) {
  const auto& k = Keys();
  const auto root = ParseRoot(TrustFor(Signer()).root, "gpgfrontend");
  ASSERT_TRUE(root);
  const auto list = ListDoc(1, {&k.a});
  const SignedFile file{list, {{QString(), SignList(list, 1, k.g)}}};
  EXPECT_TRUE(ParseIndexKeys(*root, file, "gpgfrontend", 1));
  QString why;
  EXPECT_FALSE(ParseIndexKeys(*root, file, "gpgfrontend", 2, &why));
  EXPECT_TRUE(why.contains("older than 2"));
}

TEST(TrustTest, AnExpiredListSignsNothing) {
  World w;
  const auto& k = Keys();
  const auto list = ListDoc(1, {&w.index1}, kNow.addDays(-2));
  w.built.index_keys = {list, {{QString(), SignList(list, 1, k.g)}}};
  w.Index(5, w.index1);
  EXPECT_FALSE(w.Check());
  EXPECT_TRUE(w.Warned("has expired"));
}

TEST(TrustTest, ANewGInvalidatesTheOldList) {
  World w;
  const auto& k = Keys();
  w.Root(2, RootDoc(2, {&k.a, &k.b}, k.g2), {&k.a, &k.b});
  // The built-in list 1, g's, still names index1; root 2's G is g2.
  w.Index(5, w.index1, 2, 1);
  EXPECT_FALSE(w.Check());
  // After a restart too: the kept root 2 does not take g's list.
  w.mirrors.table.clear();
  w.Index(6, w.index1);
  EXPECT_FALSE(w.Check());
  EXPECT_TRUE(w.Warned("no index-keys list that root 2 signs"));
}

TEST(TrustTest, TheSerialFloorIsPerKey) {
  World w;
  const auto& k = Keys();
  const auto list = ListDoc(1, {&w.index1, &w.index2});
  w.built.index_keys = {list, {{QString(), SignList(list, 1, k.g)}}};
  w.Index(1000000000000, w.index1);  // a huge serial under index1
  ASSERT_TRUE(w.Check());
  w.Index(5, w.index2);  // blocks nothing under index2
  EXPECT_TRUE(w.Check());
  w.Index(4, w.index1);  // under index1 it is a replay
  EXPECT_FALSE(w.Check());
  EXPECT_TRUE(w.Warned("older than 1000000000000"));
}

TEST(TrustTest, ThePythonMadeFixturesHold) {
  const QDir dir(QStringLiteral(GF_TRUST_FIXTURE));
  const auto root_file = [&](int n) {
    return TrustFile(dir, QStringLiteral("root"),
                     QStringLiteral("%1.root.json").arg(n));
  };
  QString why;
  auto root = ParseRoot(root_file(1), "gpgfrontend", &why);
  ASSERT_TRUE(root) << why.toStdString();
  for (int n : {2, 3}) {
    root = AcceptNextRoot(*root, root_file(n), "gpgfrontend", &why);
    ASSERT_TRUE(root) << why.toStdString();
  }
  EXPECT_EQ(root->version, 3);
  EXPECT_TRUE(ParseIndexKeys(
      *root, TrustFile(dir, QStringLiteral("index-keys"), "2.json"),
      "gpgfrontend", 0));
  // List 1 is g1's; root 3 replaced g1 with g2.
  EXPECT_FALSE(ParseIndexKeys(
      *root, TrustFile(dir, QStringLiteral("index-keys"), "1.json"),
      "gpgfrontend", 0));
}

// scripts/release/rehearse.sh's update store, read as a client of it would,
// built with its trust, by what its <dir>/client.env says
// (`set -a; . <dir>/client.env; set +a` first): GF_REHEARSAL_UPDATE_DIR
// (<dir>/www/update), GF_REHEARSAL_TRUST (<dir>/trust, root 1 and list 1),
// GF_REHEARSAL_PRODUCT, and the nightly it published, GF_REHEARSAL_NIGHTLY,
// to run as. Skipped without them.
TEST(TrustTest, TheRehearsalsUpdateStoreVerifies) {
  const auto update = qEnvironmentVariable("GF_REHEARSAL_UPDATE_DIR");
  const QDir trust(qEnvironmentVariable("GF_REHEARSAL_TRUST"));
  if (update.isEmpty()) GTEST_SKIP() << "no rehearsal to read";
  const ReleaseChannel channel{
      qEnvironmentVariable("GF_REHEARSAL_PRODUCT", "gpgfrontend"),
      {"https://rehearsal.example/"},
      {"https://rehearsal.example/"},
      {TrustFile(trust, QStringLiteral("root"), "1.root.json"),
       TrustFile(trust, QStringLiteral("index-keys"), "1.json")}};
  QStringList warnings;
  UpdateChecker c(
      {qEnvironmentVariable("GF_REHEARSAL_NIGHTLY", "v2.3.0-alpha.1"),
       kCommit,
       channel,
       {},
       1,
       false,
       [&](const QString& w) { warnings << w; }},
      [&](const QUrl& url, const UpdateChecker::Reply& reply) {
        QFile f(update + url.path());
        if (f.open(QIODevice::ReadOnly)) {
          reply(200, f.readAll());
        } else {
          reply(404, {});
        }
      },
      {}, [] { return QDateTime::currentDateTimeUtc(); });
  c.Start(CheckMode::kForce);
  EXPECT_FALSE(c.State().last_attempt_failed)
      << warnings.join('\n').toStdString();
  ASSERT_TRUE(c.State().last_good);
  EXPECT_EQ(c.State().last_good->list.outcome, ListOutcome::kFound);
}
