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

#include "VersionCheckingModule.h"

#include <GFSDKBasic.h>
#include <GFSDKBuildInfo.h>
#include <GFSDKExtra.h>
#include <GFSDKLog.h>
#include <GFSDKUI.h>

#include <QMetaType>
#include <QtNetwork>

#include "GFModuleCommonUtils.hpp"
#include "GFModuleDefine.h"
#include "SoftwareVersion.h"
#include "UpdateTab.h"
#include "VersionCheckTask.h"

GF_MODULE_API_DEFINE("com.bktus.gpgfrontend.module.version_checking",
                     "VersionChecking", "1.0.0",
                     "Try checking GpgFrontend version.", "Saturneric");

DEFINE_TRANSLATIONS_STRUCTURE(ModuleVersionChecking);

auto GFRegisterModule() -> int {
  MLogInfo("version checking module registering");

  REGISTER_TRANS_READER();

  GFUIMountEntry(DUP("AboutDialogTabs"),
                 QMapToMetaDataArray({
                     {"TabTitle", GC_TR("Update")},
                 }),
                 1, UpdateTabFactory);

  return 0;
}

auto GFActiveModule() -> int {
  MLogInfo("version checking module activating");

  LISTEN("APPLICATION_LOADED");
  LISTEN("CHECK_APPLICATION_VERSION");

  return 0;
}

EXECUTE_MODULE() {
  FLOG_INFO("version checking module executing, event id: %s",
            event["event_id"]);

  auto* task = new VersionCheckTask();
  QObject::connect(task, &VersionCheckTask::SignalUpgradeVersion,
                   QThread::currentThread(),
                   [event](const SoftwareVersion&) { CB_SUCC(event); });
  QObject::connect(task, &VersionCheckTask::SignalUpgradeVersion, task,
                   &QObject::deleteLater);
  task->Run();

  return 0;
}
END_EXECUTE_MODULE()

auto GFDeactivateModule() -> int { return 0; }

auto GFUnregisterModule() -> int { return 0; }