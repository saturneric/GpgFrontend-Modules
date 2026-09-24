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

#include "ReleaseCheck.h"
#include "VersionCheckingModule.h"

namespace {

auto VerdictName(Verdict v) -> QString {
  switch (v) {
    case Verdict::kUpdateAvailable:
      return QStringLiteral("update_available");
    case Verdict::kWithdrawnOrUnreleased:
      return QStringLiteral("withdrawn_or_unreleased");
    case Verdict::kUnofficialBuild:
      return QStringLiteral("unofficial_build");
    case Verdict::kUpToDate:
      return QStringLiteral("up_to_date");
    case Verdict::kUnknown:
      break;
  }
  return QStringLiteral("unknown");
}

}  // namespace

void FillGrtWithVersionInfo(const UpdateSnapshot& state) {
  // Plain QStrings: the SDK borrows its arguments.
  auto* ctx = GFModuleSdkContext();
  const auto ns = GFModuleId();
  const auto text = [&](const char* key, const QString& value) {
    gf::sdk::SetStateText(ctx, ns, QString::fromLatin1(key), value);
  };
  const auto flag = [&](const char* key, bool value) {
    gf::sdk::SetStateBool(ctx, ns, QString::fromLatin1(key), value);
  };

  const auto& good = state.last_good;
  text("version.verdict",
       good ? VerdictName(Decide(*good)) : VerdictName(Verdict::kUnknown));
  text("version.current_version", good ? good->current_version : QString{});
  text("version.latest_version", good ? good->list.latest.version : QString{});
  text("version.checked_at",
       good ? good->checked_at.toString(Qt::ISODate) : QString{});
  flag("version.last_attempt_failed", state.last_attempt_failed);
  flag("version.loading_done", good.has_value());
}
