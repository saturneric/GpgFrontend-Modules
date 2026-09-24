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
#include <QString>
#include <optional>

#include "SoftwareVersion.h"

/**
 * @file ReleaseCheck.h
 * @brief Everything an update check decides, with no network and no Host.
 *
 * Parsing the release server's answers and turning a result into a verdict
 * live here so they can be tested on their own; UpdateChecker only moves
 * bytes and keeps state.
 */

/// What a result means for the user, in precedence order (see Decide()).
enum class Verdict {
  kUnknown,                ///< the release list could not be read
  kUpdateAvailable,        ///< a newer release exists in this build's series
  kWithdrawnOrUnreleased,  ///< this version has no release (confirmed 404)
  kUnofficialBuild,        ///< this commit is not in the repository (confirmed)
  kUpToDate,               ///< nothing newer, and nothing known to be wrong
};

/**
 * @brief Extract a normalized "vX.Y.Z" version string from a raw release tag or
 * title. Returns an empty string when the raw text does not match.
 */
auto ExtractVersionFromRawTag(const QString& raw_tag) -> QString;

/// A version with a suffix after its numbers ("v2.2.0-rc1") is a prerelease.
auto IsPrereleaseVersion(const QString& version) -> bool;

/**
 * @brief The newest release in @p current's series from a GitHub release list.
 *
 * Drafts are skipped, and so are prereleases unless @p current is one itself.
 * @param http_status 0 when the request never got an HTTP answer
 */
auto ParseReleaseList(int http_status, const QByteArray& body,
                      const QString& current) -> ListResult;

/// 200 with a body that says what was asked -> confirmed; 404 -> not found;
/// anything else says nothing.
auto ClassifyLookup(int http_status, bool body_valid) -> RemoteFact;

/// `GET /releases/tags/<tag>`: is this version released?
auto ParseTagLookup(int http_status, const QByteArray& body) -> RemoteFact;

/// `GET /commits/<sha>`: is this commit in the repository? GitHub answers a
/// well-formed but unknown SHA with 422 "No commit found", which is as
/// definite as a 404.
auto ParseCommitLookup(int http_status, const QByteArray& body,
                       const QString& local_commit) -> RemoteFact;

/// The release list came back: the result can be shown and kept.
auto IsUsable(const SoftwareVersion& r) -> bool;

/// Usable, and its own check answered every question: good enough to skip
/// the next startup check.
auto IsAuthoritative(const SoftwareVersion& r) -> bool;

/// Whether a fresh check answered every question itself.
auto IsComplete(const SoftwareVersion& fresh) -> bool;

/**
 * @brief The new last-known-good result.
 *
 * An unusable @p fresh leaves @p prev as it was. A usable one replaces it,
 * except that a fact @p fresh could not establish keeps @p prev's answer when
 * both describe the same build. `complete` stays @p fresh's own: an inherited
 * fact fills the picture, it does not make this check authoritative.
 */
auto Merge(const std::optional<SoftwareVersion>& prev,
           const SoftwareVersion& fresh) -> std::optional<SoftwareVersion>;

/**
 * @brief The verdict, first match wins:
 *
 *   1. release list unusable                  -> kUnknown
 *   2. a newer release in this series         -> kUpdateAvailable
 *   3. tag lookup kNotFound                   -> kWithdrawnOrUnreleased
 *   4. commit lookup kNotFound                -> kUnofficialBuild
 *   5. otherwise                              -> kUpToDate
 *
 * An update comes first because installing it also answers 3 and 4. Only a
 * confirmed kNotFound reaches 3 or 4: a lookup that failed asserts nothing.
 */
auto Decide(const SoftwareVersion& r) -> Verdict;

/// Only a confirmed update interrupts startup; everything else waits for the
/// user to open the dialog.
auto ShouldPromptAtStartup(Verdict v) -> bool;
