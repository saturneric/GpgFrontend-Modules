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

#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <optional>

/**
 * @file NoteModel.h
 * @brief A note as a TextBundle v2 (https://textbundle.org/spec/), packed as
 * a TextPack: reading one, writing one, and the budgets both keep to.
 *
 * SDK-free: the ZIP itself is the Host's (GFSDKArchive.h); this sees entries.
 */

namespace gf::notes {

/// One ZIP entry, as read or as to be written.
struct PackEntry {
  QString path;
  QByteArray data;
  bool directory = false;
};

/// Absolute budgets. No compression ratio: it refuses legitimately
/// repetitive text, and these already bound what a bomb can cost.
struct Budgets {
  qint64 max_entries = 2000;
  qint64 max_total_bytes = 256LL * 1024 * 1024;
  qint64 max_body_bytes = 32LL * 1024 * 1024;
  qint64 max_asset_bytes = 20LL * 1024 * 1024;
  qint64 max_info_bytes = 1024LL * 1024;
};

/// The application's key in info.json, as the spec asks apps to use.
inline constexpr auto kAppKey = "com.bktus.gpgfrontend";

struct Note {
  /// Bundle folder inside the pack ("Name.textbundle/"), or "" for the root.
  QString root;
  /// The body's own name, kept: "text.markdown" or "text.md".
  QString body_name = QStringLiteral("text.markdown");
  QByteArray markdown;
  /// info.json, unknown keys and other apps' dictionaries included.
  QJsonObject info;
  /// "assets/<name>" -> bytes, everything under assets/.
  QMap<QString, QByteArray> assets;
  /// Anything else in the bundle, kept as it was and never shown.
  QList<PackEntry> preserved;

  /// Our own dictionary in info, or empty.
  [[nodiscard]] auto App() const -> QJsonObject;
  /// Merge @p fields into our dictionary, keeping what we do not set.
  void MergeApp(const QJsonObject& fields);
};

/// Read a pack's entries. nullopt with @p why on anything refused.
auto ReadPack(const QList<PackEntry>& entries, const Budgets& budgets,
              QString* why) -> std::optional<Note>;

/**
 * @brief The entries to write for @p note, in a "<bundle_name>.textbundle/"
 * folder: info.json, the body, every asset in @p keep_assets, preserved
 * entries. Assets not referenced and not ours to drop are kept too: only
 * @p drop_assets go.
 */
auto WritePack(const Note& note, const QString& bundle_name,
               const QStringList& drop_assets) -> QList<PackEntry>;

/// Whether @p entries stay inside @p budgets; why not, when they do not.
auto CheckBudgets(const QList<PackEntry>& entries, const Budgets& budgets)
    -> std::optional<QString>;

/// A file name safe as a bundle folder: no separators, never empty.
auto BundleNameFor(const QString& title) -> QString;

/// The bytes of the YAML front matter that opens @p markdown, or 0.
auto FrontMatterLength(const QByteArray& markdown) -> qsizetype;

/// An unsaved note, as the Host's recovery cache keeps it for the tab.
struct RecoveryRecord {
  Note note;
  QString path;        ///< where it is saved, when it has been
  bool plain = false;  ///< saved without encryption
};

/// @p record as text, without the assets in @p drop.
auto RecoveryToJson(const RecoveryRecord& record, const QStringList& drop)
    -> QByteArray;
/// The record RecoveryToJson() wrote; nullopt for anything else.
auto RecoveryFromJson(const QByteArray& bytes) -> std::optional<RecoveryRecord>;

}  // namespace gf::notes
