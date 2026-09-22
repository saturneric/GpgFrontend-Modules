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

#include <GFSDKLog.h>

#include <QRegularExpression>

#include "VersionCheckingModule.h"

auto ExtractVersionFromRawTag(const QString& raw_tag) -> QString {
  static const QRegularExpression kVersionRe(
      R"(^[vV](\d+\.)?(\d+\.)?(\*|\d+))");
  auto match = kVersionRe.match(raw_tag);
  return match.hasMatch() ? match.captured(0) : QString{};
}

void FillGrtWithVersionInfo(const SoftwareVersion& version) {
  gf::sdk::SetStateText(GFModuleSdkContext(), GFGetModuleID(),
                        GFMemStrDup(GFModuleSdkContext(), GF_ARENA_NORMAL,
                                    "version.current_version"),
                        GFMemStrDup(GFModuleSdkContext(), GF_ARENA_NORMAL,
                                    version.current_version.toUtf8()));
  gf::sdk::SetStateText(GFModuleSdkContext(), GFGetModuleID(),
                        GFMemStrDup(GFModuleSdkContext(), GF_ARENA_NORMAL,
                                    "version.latest_version"),
                        GFMemStrDup(GFModuleSdkContext(), GF_ARENA_NORMAL,
                                    version.latest_version.toUtf8()));
  gf::sdk::SetStateText(GFModuleSdkContext(), GFGetModuleID(),
                        GFMemStrDup(GFModuleSdkContext(), GF_ARENA_NORMAL,
                                    "version.local_commit_hash"),
                        GFMemStrDup(GFModuleSdkContext(), GF_ARENA_NORMAL,
                                    version.local_commit_hash.toUtf8()));

  gf::sdk::SetStateBool(
      GFModuleSdkContext(), GFGetModuleID(),
      GFMemStrDup(GFModuleSdkContext(), GF_ARENA_NORMAL,
                  "version.current_version_publish_in_remote"),
      version.current_version_publish_in_remote ? 1 : 0);
  gf::sdk::SetStateBool(
      GFModuleSdkContext(), GFGetModuleID(),
      GFMemStrDup(GFModuleSdkContext(), GF_ARENA_NORMAL,
                  "version.current_commit_hash_publish_in_remote"),
      version.current_commit_hash_publish_in_remote ? 1 : 0);
  gf::sdk::SetStateBool(GFModuleSdkContext(), GFGetModuleID(),
                        GFMemStrDup(GFModuleSdkContext(), GF_ARENA_NORMAL,
                                    "version.need_upgrade"),
                        version.NeedUpgrade() ? 1 : 0);
  gf::sdk::SetStateBool(GFModuleSdkContext(), GFGetModuleID(),
                        GFMemStrDup(GFModuleSdkContext(), GF_ARENA_NORMAL,
                                    "version.current_version_released"),
                        version.CurrentVersionReleased() ? 1 : 0);

  gf::sdk::SetStateText(GFModuleSdkContext(), GFGetModuleID(),
                        GFMemStrDup(GFModuleSdkContext(), GF_ARENA_NORMAL,
                                    "version.release_note"),
                        GFMemStrDup(GFModuleSdkContext(), GF_ARENA_NORMAL,
                                    version.release_note.toUtf8()));

  gf::sdk::SetStateText(
      GFModuleSdkContext(), GFGetModuleID(),
      GFMemStrDup(GFModuleSdkContext(), GF_ARENA_NORMAL, "version.api"),
      GFMemStrDup(GFModuleSdkContext(), GF_ARENA_NORMAL, version.api.toUtf8()));

  gf::sdk::SetStateBool(GFModuleSdkContext(), GFGetModuleID(),
                        GFMemStrDup(GFModuleSdkContext(), GF_ARENA_NORMAL,
                                    "version.loading_done"),
                        version.IsInfoValid() ? 1 : 0);
}