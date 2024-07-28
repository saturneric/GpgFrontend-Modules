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

#include "PaperKeyModule.h"

#include <QtCore>

#include "GFModuleCommonUtils.hpp"
#include "GFSDKBuildInfo.h"
#include "extract.h"

auto GFGetModuleGFSDKVersion() -> const char * {
  return DUP(GF_SDK_VERSION_STR);
}

auto GFGetModuleQtEnvVersion() -> const char * { return DUP(QT_VERSION_STR); }

auto GFGetModuleID() -> const char * {
  return DUP("com.bktus.gpgfrontend.module.paper_key");
}

auto GFGetModuleVersion() -> const char * { return DUP("1.0.0"); }

auto GFGetModuleMetaData() -> GFModuleMetaData * {
  return QMapToGFModuleMetaDataList(
      {{"Name", "PaperKey"},
       {"Description", "Integrated PaperKey Functions."},
       {"Author", "Saturneric"}});
}

auto GFRegisterModule() -> int {
  MLogDebug("paper key module registering");

  return 0;
}

auto GFActiveModule() -> int {
  MLogDebug("paper key module activating");
  GFModuleListenEvent(GFGetModuleID(), DUP("REQUEST_TRANS_KEY_2_PAPER_KEY"));
  GFModuleListenEvent(GFGetModuleID(), DUP("REQUEST_TRANS_PAPER_KEY_2_KEY"));
  return 0;
}

auto GFExecuteModule(GFModuleEvent *p_event) -> int {
  MLogDebug(
      QString("paper key module executing, event id: %1").arg(p_event->id));

  auto event = ConvertEventToMap(p_event);

  if (event["event_id"] == "REQUEST_TRANS_KEY_2_PAPER_KEY") {
    if (event["secret_key"].isEmpty() || event["output_path"].isEmpty()) {
      GFModuleTriggerModuleEventCallback(
          ConvertMapToEvent(event), GFGetModuleID(), 1,
          ConvertMapToParams(
              {{"ret", "-1"},
               {"reason", "secret key or output path is empty"}}));
      return -1;
    }

    QByteArray secret_key_data =
        QByteArray::fromBase64(event["secret_key"].toUtf8());

    QTemporaryFile secret_key_t_file;
    if (!secret_key_t_file.open()) {
      qWarning() << "Unable to open temporary file";
      MLogWarn("unable to open temporary file");
      return -1;
    }

    secret_key_t_file.write(secret_key_data);
    secret_key_t_file.flush();
    secret_key_t_file.seek(0);

    FILE *file = fdopen(secret_key_t_file.handle(), "rb");
    if (file == nullptr) {
      qDebug() << "Unable to convert QTemporaryFile to FILE*";
      return -1;
    }

    extract(file, event["output_path"].toUtf8(), AUTO);

    fclose(file);
  } else if (event["event_id"] == "REQUEST_TRANS_PAPER_KEY_2_KEY") {
    if (event["public_key"].isEmpty() || event["paper_key_secrets"].isEmpty()) {
      GFModuleTriggerModuleEventCallback(
          ConvertMapToEvent(event), GFGetModuleID(), 1,
          ConvertMapToParams(
              {{"ret", "-1"},
               {"reason", "public key or paper key secrets is empty"}}));
      return -1;
    }

    QByteArray public_key_data =
        QByteArray::fromBase64(event["public_key"].toUtf8());

    QTemporaryFile public_key_t_file;
    if (!public_key_t_file.open()) {
      GFModuleTriggerModuleEventCallback(
          ConvertMapToEvent(event), GFGetModuleID(), 1,
          ConvertMapToParams(
              {{"ret", "-1"}, {"reason", "unable to open temporary file"}}));
      return -1;
    }

    public_key_t_file.write(public_key_data);
    public_key_t_file.flush();
    public_key_t_file.seek(0);

    FILE *pubring = fdopen(public_key_t_file.handle(), "rb");
    if (pubring == nullptr) {
      GFModuleTriggerModuleEventCallback(
          ConvertMapToEvent(event), GFGetModuleID(), 1,
          ConvertMapToParams(
              {{"ret", "-1"},
               {"reason", "unable to convert QTemporaryFile to FILE*"}}));
      return -1;
    }

    QByteArray secrets_data =
        QByteArray::fromBase64(event["paper_key_secrets"].toUtf8());

    QTemporaryFile secrets_data_file;
    if (!secrets_data_file.open()) {
      GFModuleTriggerModuleEventCallback(
          ConvertMapToEvent(event), GFGetModuleID(), 1,
          ConvertMapToParams(
              {{"ret", "-1"}, {"reason", "unable to open temporary file"}}));
      return -1;
    }

    secrets_data_file.write(public_key_data);
    secrets_data_file.flush();
    secrets_data_file.seek(0);

    FILE *secrets = fdopen(secrets_data_file.handle(), "rb");
    if (secrets == nullptr) {
      GFModuleTriggerModuleEventCallback(
          ConvertMapToEvent(event), GFGetModuleID(), 1,
          ConvertMapToParams(
              {{"ret", "-1"},
               {"reason", "unable to convert QTemporaryFile to FILE*"}}));
      return -1;
    }

    restore(pubring, secrets, AUTO, )
  }

  GFModuleTriggerModuleEventCallback(ConvertMapToEvent(event), GFGetModuleID(),
                                     1, ConvertMapToParams({{"ret", "0"}}));
  return 0;
}

auto GFDeactiveModule() -> int { return 0; }

auto GFUnregisterModule() -> int {
  MLogDebug("paper key module unregistering");

  return 0;
}