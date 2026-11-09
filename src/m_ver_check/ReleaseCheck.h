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

#include <optional>

#include "SoftwareVersion.h"

/**
 * @file ReleaseCheck.h
 * @brief Everything an update check decides, with no network and no Host.
 *
 * Turning a result into a verdict lives here so it can be tested on its
 * own; ReleaseIndex reads the signed index, UpdateChecker only moves bytes
 * and keeps state.
 */

/// What a result means for the user, in precedence order (see Decide()).
enum class Verdict {
  kUnknown,                ///< the release list could not be read
  kUpdateAvailable,        ///< a newer release exists in this build's series
  kWithdrawnOrUnreleased,  ///< the signed index says it is withdrawn
  kUnofficialBuild,        ///< no published build is of this commit
  kUpToDate,               ///< nothing newer, and nothing known to be wrong
};

/// The release list came back: the result can be shown and kept.
auto IsUsable(const SoftwareVersion& r) -> bool;

/// Usable, and its own check answered every question (complete), or got an
/// answer to every question it could ask, "unknown" among them (settled: a
/// release the index does not list, a build with no commit to look up): good
/// enough to skip the next startup check, and the next hourly one.
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
 *   3. tag fact kNotFound (withdrawn)         -> kWithdrawnOrUnreleased
 *   4. commit marker kNotFound                -> kUnofficialBuild
 *   5. otherwise                              -> kUpToDate
 *
 * An update comes first because installing it also answers 3 and 4. Only a
 * confirmed kNotFound reaches 3 or 4: a lookup that failed asserts nothing.
 */
auto Decide(const SoftwareVersion& r) -> Verdict;

