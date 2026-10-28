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

#include "NoteModel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

using namespace Qt::StringLiterals;

namespace gf::notes {

namespace {

auto IsPlatformJunk(const QString& path) -> bool {
  // Finder's AppleDouble files: metadata of the archive, not of the note.
  if (path.startsWith(u"__MACOSX/"_s)) return true;
  const auto name = path.section(u'/', -1);
  return name.startsWith(u"._"_s) || name == u".DS_Store"_s;
}

}  // namespace

auto Note::App() const -> QJsonObject {
  return info.value(QLatin1StringView(kAppKey)).toObject();
}

void Note::MergeApp(const QJsonObject& fields) {
  auto app = App();
  for (auto it = fields.begin(); it != fields.end(); ++it) {
    if (it.value().isUndefined() || it.value().isNull()) {
      app.remove(it.key());
    } else {
      app.insert(it.key(), it.value());
    }
  }
  info.insert(QLatin1StringView(kAppKey), app);
}

auto ReadPack(const QList<PackEntry>& entries, const Budgets& budgets,
              QString* why) -> std::optional<Note> {
  auto fail = [why](const QString& reason) -> std::optional<Note> {
    if (why != nullptr) *why = reason;
    return std::nullopt;
  };

  QList<PackEntry> files;
  QSet<QString> tops;
  for (const auto& e : entries) {
    if (IsPlatformJunk(e.path)) continue;
    const auto top = e.path.section(u'/', 0, 0);
    if (e.path.contains(u'/'))
      tops.insert(top + u"/"_s);
    else
      tops.insert(top);
    if (!e.directory) files.append(e);
  }
  if (files.size() > budgets.max_entries) return fail(u"too many entries"_s);

  // Root layout, or exactly one top-level folder (whatever it is called).
  QString root;
  if (tops.size() == 1 && tops.cbegin()->endsWith(u'/')) root = *tops.cbegin();

  Note note;
  note.root = root;
  QStringList bodies;
  bool have_info = false;
  qint64 total = 0;
  for (const auto& e : files) {
    if (!e.path.startsWith(root))
      return fail(u"more than one top-level item"_s);
    const auto rel = e.path.mid(root.size());
    total += e.data.size();
    if (total > budgets.max_total_bytes)
      return fail(u"the note is too large"_s);

    if (rel == u"info.json"_s) {
      if (e.data.size() > budgets.max_info_bytes)
        return fail(u"info.json is too large"_s);
      QJsonParseError err{};
      const auto doc = QJsonDocument::fromJson(e.data, &err);
      if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return fail(u"info.json is not a JSON object"_s);
      }
      note.info = doc.object();
      have_info = true;
    } else if (!rel.contains(u'/') && rel.startsWith(u"text."_s)) {
      bodies.append(rel);
      if (e.data.size() > budgets.max_body_bytes)
        return fail(u"the text is too large"_s);
      note.body_name = rel;
      note.markdown = e.data;
    } else if (rel.startsWith(u"assets/"_s)) {
      if (e.data.size() > budgets.max_asset_bytes)
        return fail(u"an asset is too large"_s);
      note.assets.insert(rel, e.data);
    } else {
      note.preserved.append({rel, e.data, false});
    }
  }

  if (!have_info) return fail(u"no info.json"_s);
  const auto version = note.info.value(u"version"_s).toInt(0);
  if (version != 1 && version != 2)
    return fail(u"unknown TextBundle version"_s);
  if (bodies.size() != 1) return fail(u"not exactly one text.* body"_s);
  if (note.body_name != u"text.markdown"_s && note.body_name != u"text.md"_s) {
    return fail(u"the body is not Markdown"_s);
  }
  return note;
}

auto WritePack(const Note& note, const QString& bundle_name,
               const QStringList& drop_assets) -> QList<PackEntry> {
  const auto root = bundle_name + u".textbundle/"_s;
  auto info = note.info;
  info.insert(u"version"_s, 2);
  info.insert(u"type"_s, u"net.daringfireball.markdown"_s);
  // A saved note is not a hand-off between apps.
  info.insert(u"transient"_s, false);
  info.insert(u"creatorIdentifier"_s, QLatin1StringView(kAppKey));

  QList<PackEntry> out;
  out.append({root, {}, true});
  out.append({root + u"info.json"_s,
              QJsonDocument(info).toJson(QJsonDocument::Indented), false});
  out.append({root + note.body_name, note.markdown, false});
  bool any_asset = false;
  for (auto it = note.assets.cbegin(); it != note.assets.cend(); ++it) {
    if (drop_assets.contains(it.key())) continue;
    if (!any_asset) out.append({root + u"assets/"_s, {}, true});
    any_asset = true;
    out.append({root + it.key(), it.value(), false});
  }
  for (const auto& e : note.preserved) {
    out.append({root + e.path, e.data, e.directory});
  }
  return out;
}

auto CheckBudgets(const QList<PackEntry>& entries, const Budgets& budgets)
    -> std::optional<QString> {
  qint64 files = 0;
  qint64 total = 0;
  for (const auto& e : entries) {
    if (e.directory) continue;
    ++files;
    total += e.data.size();
    const auto name = e.path.section(u'/', -1);
    const auto limit = e.path.contains(u"/assets/"_s) ? budgets.max_asset_bytes
                       : name == u"info.json"_s       ? budgets.max_info_bytes
                       : name.startsWith(u"text."_s)  ? budgets.max_body_bytes
                                                      : budgets.max_total_bytes;
    if (e.data.size() > limit) {
      return QStringLiteral("%1 is %2 MiB, over its %3 MiB limit")
          .arg(name)
          .arg(e.data.size() / (1024 * 1024))
          .arg(limit / (1024 * 1024));
    }
  }
  if (files > budgets.max_entries) return QStringLiteral("too many files");
  if (total > budgets.max_total_bytes) {
    return QStringLiteral("the note is %1 MiB, over its %2 MiB limit")
        .arg(total / (1024 * 1024))
        .arg(budgets.max_total_bytes / (1024 * 1024));
  }
  return std::nullopt;
}

auto BundleNameFor(const QString& title) -> QString {
  static const QRegularExpression kUnsafe(u"[\\\\/:*?\"<>|\\x00-\\x1f]"_s);
  auto name = title;
  name.replace(kUnsafe, u"_"_s);
  name = name.trimmed();
  if (name.endsWith(u".textpack.gpg"_s)) name.chop(13);
  if (name.endsWith(u".textpack"_s)) name.chop(9);
  while (name.startsWith(u'.')) name.remove(0, 1);
  return name.isEmpty() ? u"Note"_s : name.left(100);
}

auto FrontMatterLength(const QByteArray& markdown) -> qsizetype {
  static const QRegularExpression kFront(
      u"\\A---\\r?\\n[\\s\\S]*?\\r?\\n(---|\\.\\.\\.)\\r?\\n?"_s);
  const auto m = kFront.match(QString::fromUtf8(markdown));
  return m.hasMatch() ? m.captured(0).toUtf8().size() : 0;
}

namespace {

constexpr auto kRecoveryFormat = "gpgfrontend-note-recovery";

}  // namespace

auto RecoveryToJson(const RecoveryRecord& record, const QStringList& drop)
    -> QByteArray {
  const auto& n = record.note;
  QJsonObject assets;
  for (auto it = n.assets.cbegin(); it != n.assets.cend(); ++it) {
    if (drop.contains(it.key())) continue;
    assets.insert(it.key(), QString::fromLatin1(it.value().toBase64()));
  }
  QJsonArray preserved;
  for (const auto& e : n.preserved) {
    preserved.append(
        QJsonObject{{u"path"_s, e.path},
                    {u"data"_s, QString::fromLatin1(e.data.toBase64())},
                    {u"directory"_s, e.directory}});
  }
  return QJsonDocument(QJsonObject{
                           {u"format"_s, QLatin1String(kRecoveryFormat)},
                           {u"version"_s, 1},
                           {u"path"_s, record.path},
                           {u"plain"_s, record.plain},
                           {u"root"_s, n.root},
                           {u"body_name"_s, n.body_name},
                           {u"markdown"_s, QString::fromUtf8(n.markdown)},
                           {u"info"_s, n.info},
                           {u"assets"_s, assets},
                           {u"preserved"_s, preserved},
                       })
      .toJson(QJsonDocument::Compact);
}

auto RecoveryFromJson(const QByteArray& bytes)
    -> std::optional<RecoveryRecord> {
  const auto json = QJsonDocument::fromJson(bytes).object();
  if (json.value(u"format"_s).toString() != QLatin1String(kRecoveryFormat)) {
    return std::nullopt;
  }
  RecoveryRecord record;
  auto& note = record.note;
  note.root = json.value(u"root"_s).toString();
  note.body_name = json.value(u"body_name"_s).toString(note.body_name);
  note.markdown = json.value(u"markdown"_s).toString().toUtf8();
  note.info = json.value(u"info"_s).toObject();
  const auto assets = json.value(u"assets"_s).toObject();
  for (auto it = assets.begin(); it != assets.end(); ++it) {
    note.assets.insert(
        it.key(), QByteArray::fromBase64(it.value().toString().toLatin1()));
  }
  for (const auto& v : json.value(u"preserved"_s).toArray()) {
    const auto o = v.toObject();
    note.preserved.append(
        {o.value(u"path"_s).toString(),
         QByteArray::fromBase64(o.value(u"data"_s).toString().toLatin1()),
         o.value(u"directory"_s).toBool()});
  }
  record.path = json.value(u"path"_s).toString();
  record.plain = json.value(u"plain"_s).toBool();
  return record;
}

}  // namespace gf::notes
