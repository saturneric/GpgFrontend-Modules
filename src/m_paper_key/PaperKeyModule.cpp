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

#include "GFModuleDefine.h"
#include "extract.h"

GF_MODULE_API_DEFINE("com.bktus.gpgfrontend.module.paper_key", "PaperKey",
                     "1.0.0", "Integrated PaperKey Functions.", "Saturneric")

auto GFRegisterModule() -> int {
  LOG_DEBUG("paper key module registering");

  return 0;
}

auto GFActiveModule() -> int {
  LISTEN("REQUEST_TRANS_KEY_2_PAPER_KEY");
  LISTEN("REQUEST_TRANS_PAPER_KEY_2_KEY");
  return 0;
}

EXECUTE_MODULE() {
  FLOG_DEBUG("paper key module executing, event id: %1", event["event_id"]);

  if (event["event_id"] == "REQUEST_TRANS_KEY_2_PAPER_KEY") {
    if (event["secret_key"].isEmpty() || event["output_path"].isEmpty()) {
      CB_ERR(event, -1, "secret key or output path is empty");
    }

    QByteArray secret_key_data =
        QByteArray::fromBase64(event["secret_key"].toUtf8());

    QTemporaryFile secret_key_t_file;
    if (!secret_key_t_file.open()) {
      CB_ERR(event, -1, "unable to open temporary file");
    }

    secret_key_t_file.write(secret_key_data);
    secret_key_t_file.flush();
    secret_key_t_file.seek(0);

    FILE *file = fdopen(secret_key_t_file.handle(), "rb");
    if (file == nullptr) {
      CB_ERR(event, -1, "unable to convert QTemporaryFile to FILE*");
    }

    extract(file, event["output_path"].toUtf8(), AUTO);

    fclose(file);
  } else if (event["event_id"] == "REQUEST_TRANS_PAPER_KEY_2_KEY") {
    if (event["public_key"].isEmpty() || event["paper_key_secrets"].isEmpty()) {
      CB_ERR(event, -1, "public key or paper key secrets is empty");
    }

    QByteArray public_key_data =
        QByteArray::fromBase64(event["public_key"].toUtf8());

    QTemporaryFile public_key_t_file;
    if (!public_key_t_file.open()) {
      CB_ERR(event, -1, "unable to open temporary file");
    }

    public_key_t_file.write(public_key_data);
    public_key_t_file.flush();
    public_key_t_file.seek(0);

    FILE *pubring = fdopen(public_key_t_file.handle(), "rb");
    if (pubring == nullptr) {
      CB_ERR(event, -1, "unable to convert QTemporaryFile to FILE*");
    }

    QByteArray secrets_data =
        QByteArray::fromBase64(event["paper_key_secrets"].toUtf8());

    QTemporaryFile secrets_data_file;
    if (!secrets_data_file.open()) {
      CB_ERR(event, -1, "unable to open temporary file");
    }

    secrets_data_file.write(public_key_data);
    secrets_data_file.flush();
    secrets_data_file.seek(0);

    FILE *secrets = fdopen(secrets_data_file.handle(), "rb");
    if (secrets == nullptr) {
      CB_ERR(event, -1, "unable to convert QTemporaryFile to FILE*");
    }

    restore(pubring, secrets, AUTO, )
  }

  CB_SUCC(event);
}
END_EXECUTE_MODULE()

auto GFDeactivateModule() -> int { return 0; }

auto GFUnregisterModule() -> int {
  MLogDebug("paper key module unregistering");

  return 0;
}