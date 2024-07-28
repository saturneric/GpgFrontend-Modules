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

#include <unistd.h>

#include <QtCore>

#include "GFModuleDefine.h"
#include "extract.h"
#include "restore.h"

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
    if (event["secret_key"].isEmpty()) CB_ERR(event, -1, "secret key is empty");

    QByteArray secret_key_rdata =
        QByteArray::fromBase64(event["secret_key"].toLatin1());

    QTemporaryFile secret_key_t_file;
    if (!secret_key_t_file.open())
      CB_ERR(event, -1, "unable to open temporary file");

    secret_key_t_file.write(secret_key_rdata);
    secret_key_t_file.flush();
    secret_key_t_file.reset();

    FILE *secret_key_file = fdopen(secret_key_t_file.handle(), "rb");
    if (secret_key_file == nullptr)
      CB_ERR(event, -1, "unable to convert QTemporaryFile to FILE*");

    QTemporaryFile paper_key_t_file;
    if (!paper_key_t_file.open())
      CB_ERR(event, -1, "unable to open temporary file");

    FILE *paper_key_fp = fdopen(dup(paper_key_t_file.handle()), "w");
    if (paper_key_fp == nullptr)
      CB_ERR(event, -1, "unable to convert QTemporaryFile to FILE*");

    auto ret = extract(secret_key_file, paper_key_fp, BASE16);
    paper_key_t_file.flush();
    paper_key_t_file.reset();

    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(ret)},
           {"paper_key", QString::fromLatin1(paper_key_t_file.readAll())},
       });
    return ret;
  }

  if (event["event_id"] == "REQUEST_TRANS_PAPER_KEY_2_KEY") {
    if (event["public_key"].isEmpty() || event["paper_key_secrets"].isEmpty())
      CB_ERR(event, -1, "public key or paper key secrets is empty");

    QByteArray public_key_data =
        QByteArray::fromBase64(event["public_key"].toLatin1());

    QTemporaryFile public_key_t_file;
    if (!public_key_t_file.open())
      CB_ERR(event, -1, "unable to open temporary file");

    public_key_t_file.write(public_key_data);
    public_key_t_file.flush();
    public_key_t_file.seek(0);

    FILE *pubring = fdopen(public_key_t_file.handle(), "rb");
    if (pubring == nullptr)
      CB_ERR(event, -1, "unable to convert QTemporaryFile to FILE*");

    QByteArray secrets_data =
        QByteArray::fromBase64(event["paper_key_secrets"].toLatin1());

    QTemporaryFile secrets_data_t_file;
    if (!secrets_data_t_file.open())
      CB_ERR(event, -1, "unable to open temporary file");

    secrets_data_t_file.write(secrets_data);
    secrets_data_t_file.flush();
    secrets_data_t_file.reset();

    FILE *secrets_fp = fdopen(secrets_data_t_file.handle(), "r");
    if (secrets_fp == nullptr)
      CB_ERR(event, -1, "unable to convert QTemporaryFile to FILE*");

    QTemporaryFile secret_key_t_file;
    if (!secret_key_t_file.open())
      CB_ERR(event, -1, "unable to open temporary file");

    FILE *secret_key_fp = fdopen(dup(secret_key_t_file.handle()), "wb");
    if (secret_key_fp == nullptr)
      CB_ERR(event, -1, "unable to convert QTemporaryFile to FILE*");

    auto ret = restore(pubring, secrets_fp, AUTO, secret_key_fp);
    secret_key_t_file.reset();
    FLOG_DEBUG("secret key temp file size: %1, ret: %2",
               secret_key_t_file.size(), ret);

    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(ret)},
           {"secret_key",
            QString::fromLocal8Bit(secret_key_t_file.readAll().toBase64())},
       });

    return ret;
  }

  CB_SUCC(event);
}
END_EXECUTE_MODULE()

auto GFDeactivateModule() -> int { return 0; }

auto GFUnregisterModule() -> int {
  MLogDebug("paper key module unregistering");

  return 0;
}