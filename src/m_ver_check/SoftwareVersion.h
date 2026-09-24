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

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <optional>

/**
 * @brief What one lookup established about this build on the release server.
 *
 * Three states, not a bool: a lookup that could not be made (offline, rate
 * limited, a server error) says nothing, and must never read as "not found".
 */
enum class RemoteFact { kUnknown, kConfirmed, kNotFound };

/**
 * @brief How the release list turned out. A valid list with no release in
 * this build's series is an answer (kNoMatch), not a failure.
 */
enum class ListOutcome { kFailed, kNoMatch, kFound };

struct LatestRelease {
  QString version;       ///< normalized, e.g. "v2.1.9"
  QString published_at;  ///< as the server wrote it
  QString notes;         ///< markdown
  QString html_url;      ///< the release page
};

struct ListResult {
  ListOutcome outcome = ListOutcome::kFailed;
  LatestRelease latest;  ///< meaningful only for kFound
};

/**
 * @brief One update check, for one build.
 *
 * `complete` is set when the check that produced this result answered every
 * question itself. A result can be usable (the release list came back) while
 * incomplete (a lookup failed); only a complete one is trusted enough to skip
 * the next startup check.
 */
struct SoftwareVersion {
  static constexpr int kSchema = 2;

  QString current_version;
  QString local_commit_hash;

  ListResult list;
  RemoteFact tag_fact = RemoteFact::kUnknown;
  RemoteFact commit_fact = RemoteFact::kUnknown;
  bool complete = false;

  QDateTime checked_at;

  /// Whether this result describes the given build.
  [[nodiscard]] auto SameBuild(const QString& version,
                               const QString& commit) const -> bool;

  [[nodiscard]] auto ToJson() const -> QJsonObject;

  /// @return nothing for a malformed object or one from an older schema
  [[nodiscard]] static auto FromJson(const QJsonObject& obj)
      -> std::optional<SoftwareVersion>;

  /**
   * @brief Return the release series ("channel") a version belongs to, i.e. its
   * "major.minor" prefix (e.g. "2.1" for the stable track, "2.2" for the
   * mainline track). Returns an empty string if the version cannot be parsed.
   *
   * @param version a version string, with or without a leading 'v'.
   * @return QString the "major.minor" series identifier.
   */
  [[nodiscard]] static auto VersionSeries(const QString& version) -> QString;

  /**
   * @brief Whether two versions belong to the same release series.
   *
   * @return true if both versions share a non-empty "major.minor" series.
   */
  [[nodiscard]] static auto SameSeries(const QString& a, const QString& b)
      -> bool;
};
