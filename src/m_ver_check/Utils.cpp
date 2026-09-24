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
  // Plain QStrings: the SDK borrows its arguments. Each of these used to be a
  // GFMemStrDup() that nothing freed, a dozen leaks per update check.
  auto* ctx = GFModuleSdkContext();
  const auto ns = GFModuleId();
  const auto text = [&](const char* key, const QString& value) {
    gf::sdk::SetStateText(ctx, ns, QString::fromLatin1(key), value);
  };
  const auto flag = [&](const char* key, bool value) {
    gf::sdk::SetStateBool(ctx, ns, QString::fromLatin1(key), value);
  };

  text("version.current_version", version.current_version);
  text("version.latest_version", version.latest_version);
  text("version.local_commit_hash", version.local_commit_hash);
  flag("version.current_version_publish_in_remote",
       version.current_version_publish_in_remote);
  flag("version.current_commit_hash_publish_in_remote",
       version.current_commit_hash_publish_in_remote);
  flag("version.need_upgrade", version.NeedUpgrade());
  flag("version.current_version_released", version.CurrentVersionReleased());
  text("version.release_note", version.release_note);
  text("version.api", version.api);
  flag("version.loading_done", version.IsInfoValid());
}