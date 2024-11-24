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

#include "EMailModule.h"

#include <GFSDKBasic.h>
#include <GFSDKBuildInfo.h>
#include <GFSDKLog.h>

// qt
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonDocument>
#include <QString>

// vmime
#define VMIME_STATIC
#include <vmime/vmime.hpp>

#include "GFModuleCommonUtils.hpp"
#include "GFModuleDefine.h"

GF_MODULE_API_DEFINE("com.bktus.gpgfrontend.module.email", "Email", "1.0.0",
                     "Everything related to E-Mails.", "Saturneric")

DEFINE_TRANSLATIONS_STRUCTURE(ModuleEMail);

extern auto CalculateBinaryChacksum(const QString &path)
    -> std::optional<QString>;

auto GFRegisterModule() -> int {
  MLogDebug("email module registering...");

  REGISTER_TRANS_READER();

  vmime::datetime dl("Sat, 08 Oct 2005 14:07:52 +0200");
  dl.getDay();

  return 0;
}

auto GFActiveModule() -> int { return 0; }

EXECUTE_MODULE() {
  FLOG_DEBUG("email executing, event id: %1", event["event_id"]);

  CB_SUCC(event);
}
END_EXECUTE_MODULE()

auto GFDeactivateModule() -> int { return 0; }

auto GFUnregisterModule() -> int {
  MLogDebug("email module unregistering...");

  return 0;
}
