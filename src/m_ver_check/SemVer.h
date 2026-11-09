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

#include <QString>
#include <QStringList>
#include <optional>

/**
 * @file SemVer.h
 * @brief SemVer 2.0.0 precedence, which update decisions need and
 * GFCompareSoftwareVersion does not give: it reads "0-alpha" as a number.
 *
 * The same ordering cmake/GitVersion.cmake builds versions for and the release
 * tooling (scripts/release/semver.py) publishes them by: a final release
 * above its prereleases, numeric identifiers numerically and below words,
 * words in ASCII order, a shorter list below a longer one it prefixes, build
 * metadata ignored.
 */
struct SemVer {
  int major = 0;
  int minor = 0;
  int patch = 0;
  QStringList prerelease;
  QString build;

  /// "2.4.0-alpha.12+g0123", with or without a leading "v"; nullopt when the
  /// text is not SemVer.
  static auto Parse(const QString& text) -> std::optional<SemVer>;

  [[nodiscard]] auto IsPrerelease() const -> bool {
    return !prerelease.isEmpty();
  }
};

/// <0, 0, >0 as @p a sorts before, with or after @p b. A side that is not
/// SemVer ("v2.1", a tag from before SemVer) is compared the old, numeric way.
auto SemVerCompare(const QString& a, const QString& b) -> int;
