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

#include <QStringList>

#include "GFModule.h"

class AppImageInstaller;
class QObject;
class UpdateChecker;
struct UpdateFile;

/// Whether to check at startup: the Host's own setting. The setup wizard
/// asks it, and this module's settings page and update dialog show the same
/// switch rather than a second one that could disagree. Absent means the
/// wizard has not asked yet, which is not a yes.
inline constexpr auto kProhibitUpdateCheckKey = "network/prohibit_update_check";

/**
 * @brief The module's one update checker, created on first use.
 *
 * GUI thread only. Null once the module is deactivating.
 */
auto VersionChecker() -> UpdateChecker*;

/**
 * @brief The AppImage an update may replace in place, or empty.
 *
 * Only the nightly application packaged as an AppImage, whose file sits where
 * this user can write. A stable release is downloaded and installed by the
 * user; everything else is updated by its own installer, its store or its
 * distribution.
 */
auto SelfUpdateImage() -> QString;

/// Whether the user asked a release build to be offered beta versions too
/// (this module's own setting; off until asked).
auto FollowBetaSetting() -> bool;

/// Stores that answer, and hands it to the checker for its next check.
void SetFollowBetaSetting(bool follow_beta);

/// Where release notes may link to (release/product.json's
/// notes_link_hosts); empty for a build without that file.
auto NotesLinkHosts() -> QStringList;

/// An installer for @p image, downloading through the module's network
/// access. GUI thread.
auto MakeAppImageInstaller(const QString& image, const UpdateFile& file,
                           QObject* parent) -> AppImageInstaller*;

/// Asks the Host to restart the application; false when it cannot (a Host
/// too old) or refuses.
auto RequestRestart() -> bool;
