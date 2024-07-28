/**
 * Copyright (C) 2021 Saturneric <eric@bktus.com>
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
#include "SoftwareVersion.h"
#include "UpdateTab.h"
#include "VersionCheckTask.h"

class GTrC {
  Q_DECLARE_TR_FUNCTIONS(GTrC)
};

auto GFGetModuleGFSDKVersion() -> const char* {
  return DUP(GF_SDK_VERSION_STR);
}

auto GFGetModuleQtEnvVersion() -> const char* { return DUP(QT_VERSION_STR); }

auto GFGetModuleID() -> const char* {
  return DUP("com.bktus.gpgfrontend.module.version_checking");
}

auto GFGetModuleVersion() -> const char* { return DUP("1.0.0"); }

auto GFGetModuleMetaData() -> GFModuleMetaData* {
  return QMapToGFModuleMetaDataList(
      {{"Name", "VersionChecking"},
       {"Description", "Try checking GpgFrontend version."},
       {"Author", "Saturneric"}});
}

auto GFRegisterModule() -> int {
  MLogInfo("version checking module registering");
  return 0;
}

auto GFActiveModule() -> int {
  MLogInfo("version checking module activating");

  GFModuleListenEvent(GFGetModuleID(), DUP("APPLICATION_LOADED"));
  GFModuleListenEvent(GFGetModuleID(), DUP("CHECK_APPLICATION_VERSION"));

  // load translations
  QFile f(
      QString(":/i18n/ModuleVersionChecking.%1.qm").arg(GFAppActiveLocale()));
  if (f.exists() && f.open(QIODevice::ReadOnly)) {
    auto f_n = f.fileName().toUtf8();
    MLogInfoS("version checking module loading, locale: %s, path: %s",
              GFAppActiveLocale(), f_n.data());
    auto b = f.readAll();
    GFAppRegisterTranslator(AllocBufferAndCopy(b), b.size());
  }

  GFUIMountEntry(DUP("AboutDialogTabs"),
                 QMapToMetaDataArray({{"TabTitle", GTrC::tr("Update")}}), 1,
                 UpdateTabFactory);

  return 0;
}

auto GFExecuteModule(GFModuleEvent* event) -> int {
  MLogInfoS("version checking module executing, event id: %s", event->id);

  auto* task = new VersionCheckTask();
  QObject::connect(task, &VersionCheckTask::SignalUpgradeVersion,
                   QThread::currentThread(), [event](const SoftwareVersion&) {
                     GFModuleTriggerModuleEventCallback(
                         event, GFGetModuleID(), 1,
                         ConvertMapToParams({{"ret", "0"}}));
                   });
  QObject::connect(task, &VersionCheckTask::SignalUpgradeVersion, task,
                   &QObject::deleteLater);
  task->Run();

  return 0;
}

auto GFDeactiveModule() -> int { return 0; }

auto GFUnregisterModule() -> int { return 0; }