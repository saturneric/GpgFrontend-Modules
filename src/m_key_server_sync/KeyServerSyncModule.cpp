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

#include "KeyServerSyncModule.h"

#include <QtCore>

#include "GFModuleCommonUtils.hpp"
#include "GFModuleDefine.h"
#include "VKSInterface.h"

GF_MODULE_API_DEFINE("com.bktus.gpgfrontend.module.key_server_sync",
                     "KeyServerSync", "1.0.0",
                     "Sync Information From Trusted Key Server.", "Saturneric")

auto GFRegisterModule() -> int {
  LOG_DEBUG("key server sync module registering");

  return 0;
}

auto GFActiveModule() -> int {
  LISTEN("REQUEST_GET_PUBLIC_KEY_BY_FINGERPRINT");
  LISTEN("REQUEST_GET_PUBLIC_KEY_BY_KEY_ID");
  LISTEN("REQUEST_UPLOAD_PUBLIC_KEY");
  return 0;
}

EXECUTE_MODULE() {
  FLOG_DEBUG("key server sync module executing, event id: %1",
             event["event_id"]);

  if (event["event_id"] == "REQUEST_GET_PUBLIC_KEY_BY_FINGERPRINT") {
    if (event["fingerprint"].isEmpty())
      CB_ERR(event, -1, "fingerprint is empty");

    QByteArray fingerprint = event["fingerprint"].toLatin1();
    FLOG_DEBUG("try to get key info of fingerprint: %1", fingerprint);

    auto* vks = new VKSInterface();
    QObject::connect(vks, &VKSInterface::SignalKeyRetrieved,
                     QThread::currentThread(), [event](const QString& key) {
                       // callback
                       CB(event, GFGetModuleID(),
                          {
                              {"ret", QString::number(0)},
                              {"key_data", key},
                          });
                     });
    QObject::connect(vks, &VKSInterface::SignalKeyRetrieved, vks,
                     &VKSInterface::deleteLater);

    QObject::connect(vks, &VKSInterface::SignalErrorOccurred,
                     QThread::currentThread(),
                     [event](const QString& error, const QString& data) {
                       CB(event, GFGetModuleID(),
                          {
                              {"ret", QString::number(-1)},
                              {"error_msg", error},
                              {"reply_data", data},
                          });
                     });
    QObject::connect(vks, &VKSInterface::SignalKeyRetrieved, vks,
                     &VKSInterface::deleteLater);
    vks->GetByFingerprint(fingerprint);

    return 0;
  }

  if (event["event_id"] == "REQUEST_GET_PUBLIC_KEY_BY_KEY_ID") {
    if (event["key_id"].isEmpty()) CB_ERR(event, -1, "key_id is empty");

    QByteArray key_id = event["key_id"].toLatin1();
    FLOG_DEBUG("try to get key info of key id: %1", key_id);

    auto* vks = new VKSInterface();
    QObject::connect(vks, &VKSInterface::SignalKeyRetrieved,
                     QThread::currentThread(), [event](const QString& key) {
                       // callback
                       CB(event, GFGetModuleID(),
                          {
                              {"ret", QString::number(0)},
                              {"key_data", key},
                          });
                     });
    QObject::connect(vks, &VKSInterface::SignalKeyRetrieved, vks,
                     &VKSInterface::deleteLater);
    QObject::connect(vks, &VKSInterface::SignalErrorOccurred,
                     QThread::currentThread(),
                     [event](const QString& error, const QString& data) {
                       CB(event, GFGetModuleID(),
                          {
                              {"ret", QString::number(-1)},
                              {"error_msg", error},
                              {"reply_data", data},
                          });
                     });
    QObject::connect(vks, &VKSInterface::SignalKeyRetrieved, vks,
                     &VKSInterface::deleteLater);
    vks->GetByKeyId(key_id);

    return 0;
  }

  if (event["event_id"] == "REQUEST_UPLOAD_PUBLIC_KEY") {
    if (event["key_text"].isEmpty()) CB_ERR(event, -1, "key_text is empty");

    QByteArray key_text = event["key_text"].toLatin1();
    FLOG_DEBUG("try to get key info of key id: %1", key_text);

    auto* vks = new VKSInterface();
    QObject::connect(
        vks, &VKSInterface::SignalKeyUploaded, QThread::currentThread(),
        [event](const QString& fpr, const QJsonObject& status,
                const QString& token) {
          CB(event, GFGetModuleID(),
             {
                 {"ret", QString::number(0)},
                 {"fingerprint", fpr},
                 {"status", QString::fromUtf8(QJsonDocument(status).toJson())},
                 {"token", token},
             });
        });
    QObject::connect(vks, &VKSInterface::SignalKeyRetrieved, vks,
                     &VKSInterface::deleteLater);
    QObject::connect(vks, &VKSInterface::SignalErrorOccurred,
                     QThread::currentThread(),
                     [event](const QString& error, const QString& data) {
                       CB(event, GFGetModuleID(),
                          {
                              {"ret", QString::number(-1)},
                              {"error_msg", error},
                              {"reply_data", data},
                          });
                     });
    QObject::connect(vks, &VKSInterface::SignalKeyRetrieved, vks,
                     &VKSInterface::deleteLater);
    vks->UploadKey(key_text);
    return 0;
  }

  CB_ERR(event, -1, "the type of this event is not supported");
}
END_EXECUTE_MODULE()

auto GFDeactivateModule() -> int { return 0; }

auto GFUnregisterModule() -> int {
  MLogDebug("paper key module unregistering");

  return 0;
}