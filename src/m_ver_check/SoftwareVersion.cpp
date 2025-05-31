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

#include "SoftwareVersion.h"

#include <GFSDKBasic.h>
#include <GFSDKExtra.h>
#include <GFSDKLog.h>

#include <QString>

#include "GFModuleCommonUtils.hpp"

auto SoftwareVersion::NeedUpgrade() const -> bool {
  MLogDebug(QString("compare version current: %1 latest %2, result: %3")
                .arg(current_version)
                .arg(latest_version)
                .arg(GFCompareSoftwareVersion(
                    GFModuleStrDup(current_version.toUtf8()),
                    GFModuleStrDup(latest_version.toUtf8()))));

  return !latest_version.isEmpty() &&
         GFCompareSoftwareVersion(GFModuleStrDup(current_version.toUtf8()),
                                  GFModuleStrDup(latest_version.toUtf8())) < 0;
}

auto SoftwareVersion::VersionWithdrawn() const -> bool {
  return !latest_version.isEmpty() && !current_version_publish_in_remote;
}

auto SoftwareVersion::CurrentVersionReleased() const -> bool {
  return !latest_version.isEmpty() && current_version_publish_in_remote;
}

auto SoftwareVersion::GitCommitHashMismatch() const -> bool {
  if (remote_commit_hash_by_tag.isEmpty()) return false;

  FLOG_DEBUG("remote commit hash: %1, local commit hash: %2",
             remote_commit_hash_by_tag, local_commit_hash);
  return remote_commit_hash_by_tag.trimmed() != local_commit_hash.trimmed();
}
