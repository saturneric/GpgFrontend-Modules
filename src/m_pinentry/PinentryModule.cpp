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

#include "PinentryModule.h"

#include <qapplication.h>
#include <qobjectdefs.h>
#include <qthread.h>

#include "GFModuleCommonUtils.hpp"
#include "GFModuleDefine.h"
#include "GpgPassphraseContext.h"
#include "RaisePinentry.h"

GF_MODULE_API_DEFINE("com.bktus.gpgfrontend.module.pinentry", "Pinentry",
                     "1.0.0", "A simple tiny pinentry.", "Saturneric")

auto GFRegisterModule() -> int {
  MLogDebug("pinentry module registering");

  return 0;
}

auto GFActiveModule() -> int {
  LISTEN("REQUEST_PIN_ENTRY");
  return 0;
}

auto GFExecuteModule(GFModuleEvent *p_event) -> int {
  MLogDebug(
      QString("pinentry module executing, event id: %1").arg(p_event->id));

  auto event = ConvertEventToMap(p_event);

  if (event["prev_was_bad"].isEmpty() || event["ask_for_new"].isEmpty()) {
    GFModuleTriggerModuleEventCallback(
        ConvertMapToEvent(event), GFGetModuleID(),
        ConvertMapToParams({{"ret", "-1"}, {"passphrase", ""}}));
    return -1;
  }

  QMetaObject::invokeMethod(
      QApplication::instance()->thread(), [p_event, event]() -> int {
        auto *p = new RaisePinentry(
            nullptr,
            SecureCreateQSharedObject<GpgPassphraseContext>(
                event["uid_hint"], event["passphrase_info"],
                event["prev_was_bad"].toInt(), event["ask_for_new"].toInt()));

        QObject::connect(
            p, &RaisePinentry::SignalUserInputPassphraseCallback, p,
            [event](const QSharedPointer<GpgPassphraseContext> &c) {
              GFModuleTriggerModuleEventCallback(
                  ConvertMapToEvent(event), GFGetModuleID(),
                  ConvertMapToParams({{"passphrase", c->GetPassphrase()}}));
            });

        p->Exec();
        return 0;
      });

  return 0;
}

auto GFDeactivateModule() -> int { return 0; }

auto GFUnregisterModule() -> int {
  MLogDebug("pinentry module unregistering");

  return 0;
}