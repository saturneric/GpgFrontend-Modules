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

#include "Utils.h"

#include <GFSDKBasic.h>
#include <GFSDKExtra.h>
#include <GFSDKLog.h>

#include "VersionCheckingModule.h"

void FillGrtWithVersionInfo(const SoftwareVersion& version) {
  GFModuleUpsertRTValue(GFGetModuleID(),
                        GFModuleStrDup("version.current_version"),
                        GFModuleStrDup(version.current_version.toUtf8()));
  GFModuleUpsertRTValue(GFGetModuleID(),
                        GFModuleStrDup("version.latest_version"),
                        GFModuleStrDup(version.latest_version.toUtf8()));
  GFModuleUpsertRTValue(GFGetModuleID(),
                        GFModuleStrDup("version.local_commit_hash"),
                        GFModuleStrDup(version.local_commit_hash.toUtf8()));

  GFModuleUpsertRTValueBool(
      GFGetModuleID(),
      GFModuleStrDup("version.current_version_publish_in_remote"),
      version.current_version_publish_in_remote ? 1 : 0);
  GFModuleUpsertRTValueBool(
      GFGetModuleID(),
      GFModuleStrDup("version.current_commit_hash_publish_in_remote"),
      version.current_commit_hash_publish_in_remote ? 1 : 0);
  GFModuleUpsertRTValueBool(GFGetModuleID(),
                            GFModuleStrDup("version.need_upgrade"),
                            version.NeedUpgrade() ? 1 : 0);
  GFModuleUpsertRTValueBool(GFGetModuleID(),
                            GFModuleStrDup("version.current_version_released"),
                            version.CurrentVersionReleased() ? 1 : 0);

  GFModuleUpsertRTValue(GFGetModuleID(), GFModuleStrDup("version.release_note"),
                        GFModuleStrDup(version.release_note.toUtf8()));

  GFModuleUpsertRTValue(GFGetModuleID(), GFModuleStrDup("version.api"),
                        GFModuleStrDup(version.api.toUtf8()));

  GFModuleUpsertRTValueBool(GFGetModuleID(),
                            GFModuleStrDup("version.loading_done"),
                            version.IsInfoValid() ? 1 : 0);
}