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

#include "NoteModel.h"

using namespace Qt::StringLiterals;
using namespace gf::notes;

namespace {

auto Info(const QJsonObject& extra = {}) -> QByteArray {
  QJsonObject info{{u"version"_s, 2},
                   {u"type"_s, u"net.daringfireball.markdown"_s}};
  for (auto it = extra.begin(); it != extra.end(); ++it)
    info.insert(it.key(), it.value());
  return QJsonDocument(info).toJson();
}

auto Read(const QList<PackEntry>& e, QString* why = nullptr)
    -> std::optional<Note> {
  return ReadPack(e, Budgets{}, why);
}

}  // namespace

TEST(NoteModelTest, ReadsTheOfficialFolderLayout) {
  // As textbundle.org's example.textpack: one top-level .textbundle folder,
  // with directory entries.
  const auto n = Read({{u"Example.textbundle/"_s, {}, true},
                       {u"Example.textbundle/text.markdown"_s, "# Hi\n", false},
                       {u"Example.textbundle/info.json"_s, Info(), false},
                       {u"Example.textbundle/assets/"_s, {}, true},
                       {u"Example.textbundle/assets/a.png"_s, "PNG", false}});
  ASSERT_TRUE(n.has_value());
  EXPECT_EQ(n->root, u"Example.textbundle/"_s);
  EXPECT_EQ(n->markdown, "# Hi\n");
  EXPECT_EQ(n->assets.value(u"assets/a.png"_s), "PNG");
}

TEST(NoteModelTest, ReadsTheRootLayoutAndUlyssesTextMd) {
  const auto n =
      Read({{u"info.json"_s, Info(), false}, {u"text.md"_s, "body", false}});
  ASSERT_TRUE(n.has_value());
  EXPECT_EQ(n->root, QString());
  EXPECT_EQ(n->body_name, u"text.md"_s) << "the name found is the name kept";
}

TEST(NoteModelTest, FinderMetadataIsIgnored) {
  const auto n =
      Read({{u"N.textbundle/info.json"_s, Info(), false},
            {u"N.textbundle/text.markdown"_s, "x", false},
            {u"__MACOSX/N.textbundle/._info.json"_s, "\x00\x05\x16", false},
            {u"__MACOSX/._N.textbundle"_s, "\x00", false}});
  ASSERT_TRUE(n.has_value());
  EXPECT_TRUE(n->preserved.isEmpty());
}

TEST(NoteModelTest, ExactlyOneMarkdownBody) {
  QString why;
  EXPECT_FALSE(Read({{u"info.json"_s, Info(), false}}, &why).has_value());
  EXPECT_FALSE(Read({{u"info.json"_s, Info(), false},
                     {u"text.md"_s, "a", false},
                     {u"text.markdown"_s, "b", false}},
                    &why)
                   .has_value());
  EXPECT_FALSE(
      Read({{u"info.json"_s, Info(), false}, {u"text.html"_s, "<p>", false}},
           &why)
          .has_value());
  EXPECT_FALSE(Read({{u"info.json"_s, Info({{u"version"_s, 9}}), false},
                     {u"text.md"_s, "a", false}},
                    &why)
                   .has_value());
  EXPECT_FALSE(
      Read({{u"a/info.json"_s, Info(), false}, {u"b/text.md"_s, "a", false}},
           &why)
          .has_value());
}

TEST(NoteModelTest, UnknownMetadataAndEntriesArePreserved) {
  const auto info =
      Info({{u"com.other.app"_s, QJsonObject{{u"k"_s, 1}}},
            {u"sourceURL"_s, u"file:///x"_s},
            {QLatin1StringView(kAppKey), QJsonObject{{u"future"_s, true}}}});
  auto n = Read({{u"N.textbundle/info.json"_s, info, false},
                 {u"N.textbundle/text.markdown"_s, "x", false},
                 {u"N.textbundle/other/thing.bin"_s, "\x01\x02", false},
                 {u"N.textbundle/assets/old.png"_s, "OLD", false},
                 {u"N.textbundle/assets/mine.png"_s, "MINE", false}});
  ASSERT_TRUE(n.has_value());
  n->MergeApp({{u"schema"_s, 1}, {u"note_id"_s, u"abc"_s}});
  EXPECT_TRUE(n->App().value(u"future"_s).toBool())
      << "unknown fields of our own kept";

  const auto written = WritePack(*n, u"N"_s, {u"assets/mine.png"_s});
  const auto back = Read(written);
  ASSERT_TRUE(back.has_value());
  EXPECT_EQ(
      back->info.value(u"com.other.app"_s).toObject().value(u"k"_s).toInt(), 1);
  EXPECT_EQ(back->info.value(u"sourceURL"_s).toString(), u"file:///x"_s);
  EXPECT_EQ(back->info.value(u"version"_s).toInt(), 2);
  EXPECT_FALSE(back->info.value(u"transient"_s).toBool());
  EXPECT_EQ(back->App().value(u"note_id"_s).toString(), u"abc"_s);
  EXPECT_TRUE(back->assets.contains(u"assets/old.png"_s));
  EXPECT_FALSE(back->assets.contains(u"assets/mine.png"_s))
      << "dropped as asked";
  ASSERT_EQ(back->preserved.size(), 1);
  EXPECT_EQ(back->preserved[0].data, "\x01\x02");
  EXPECT_EQ(back->root, u"N.textbundle/"_s);
}

TEST(NoteModelTest, BudgetsHoldOnReadAndOnWrite) {
  Budgets b;
  b.max_asset_bytes = 10;
  QString why;
  EXPECT_FALSE(ReadPack({{u"info.json"_s, Info(), false},
                         {u"text.md"_s, "x", false},
                         {u"assets/big.png"_s, QByteArray(11, 'x'), false}},
                        b, &why)
                   .has_value());

  Note n;
  n.info = QJsonObject{{u"version"_s, 2}};
  n.markdown = "x";
  n.assets.insert(u"assets/big.png"_s, QByteArray(11, 'x'));
  EXPECT_TRUE(CheckBudgets(WritePack(n, u"N"_s, {}), b).has_value())
      << "a note the reader would refuse is never written";
}

TEST(NoteModelTest, HighlyRepetitiveTextIsWithinBudget) {
  Note n;
  n.info = QJsonObject{{u"version"_s, 2}};
  n.markdown = QByteArray(30 * 1024 * 1024, 'a');
  const auto entries = WritePack(n, u"N"_s, {});
  EXPECT_FALSE(CheckBudgets(entries, Budgets{}).has_value());
  EXPECT_TRUE(ReadPack(entries, Budgets{}, nullptr).has_value());
}

TEST(NoteModelTest, BundleNamesAreSafe) {
  EXPECT_EQ(BundleNameFor(u"a/b:c"_s), u"a_b_c"_s);
  EXPECT_EQ(BundleNameFor(u"x.textpack.gpg"_s), u"x"_s);
  EXPECT_EQ(BundleNameFor(u"x.textpack"_s), u"x"_s);
  EXPECT_EQ(BundleNameFor(u"..."_s), u"Note"_s);
  EXPECT_EQ(BundleNameFor(QString()), u"Note"_s);
}

TEST(NoteModelTest, TheRecoveryRecordComesBackWhole) {
  RecoveryRecord r;
  r.note.root = u"N.textbundle/"_s;
  r.note.body_name = u"text.md"_s;
  r.note.markdown = "# Hi\n\n![](assets/a.png)\n";
  r.note.info = QJsonObject{{u"version"_s, 2}};
  r.note.assets.insert(u"assets/a.png"_s, QByteArray("\x89PNG\0x", 6));
  r.note.assets.insert(u"assets/gone.png"_s, "x");
  r.note.preserved.append({u"N.textbundle/extra/"_s, {}, true});
  r.path = u"/tmp/n.textpack.gpg"_s;
  r.plain = true;

  const auto back = RecoveryFromJson(RecoveryToJson(r, {u"assets/gone.png"_s}));
  ASSERT_TRUE(back.has_value());
  EXPECT_EQ(back->note.root, r.note.root);
  EXPECT_EQ(back->note.body_name, r.note.body_name);
  EXPECT_EQ(back->note.markdown, r.note.markdown);
  EXPECT_EQ(back->note.info, r.note.info);
  EXPECT_EQ(back->note.assets.value(u"assets/a.png"_s),
            r.note.assets.value(u"assets/a.png"_s));
  EXPECT_FALSE(back->note.assets.contains(u"assets/gone.png"_s))
      << "a dropped asset is not kept for recovery";
  ASSERT_EQ(back->note.preserved.size(), 1);
  EXPECT_TRUE(back->note.preserved[0].directory);
  EXPECT_EQ(back->path, r.path);
  EXPECT_TRUE(back->plain);

  EXPECT_FALSE(RecoveryFromJson(R"({"format":"other"})").has_value());
  EXPECT_FALSE(RecoveryFromJson("not json").has_value());
}

TEST(NoteModelTest, FrontMatterIsMeasuredInBytes) {
  EXPECT_EQ(FrontMatterLength("---\ntitle: é\n---\nbody"),
            QByteArray("---\ntitle: é\n---\n").size());
  EXPECT_EQ(FrontMatterLength("# no front matter\n---\n"), 0);
}
