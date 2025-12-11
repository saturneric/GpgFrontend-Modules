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

#pragma once

#include "GFModuleCommonUtils.hpp"
#include "GFSDKBuildInfo.h"

#define GF_MODULE_API_DEFINE(id, name, ver, desc, author)                   \
  auto GFGetModuleGFSDKVersion() -> const char* {                           \
    return DUP(GF_SDK_VERSION_STR);                                         \
  }                                                                         \
  auto GFGetModuleQtEnvVersion() -> const char* {                           \
    return DUP(QT_VERSION_STR);                                             \
  }                                                                         \
  auto GFGetModuleID() -> const char* { return DUP((id)); }                 \
  auto GFGetModuleVersion() -> const char* { return DUP((ver)); }           \
  auto GFGetModuleMetaData() -> GFModuleMetaData* {                         \
    return QMapToGFModuleMetaDataList(                                      \
        {{"Name", (name)}, {"Description", (desc)}, {"Author", (author)}}); \
  }

#define GF_MODULE_API_DEFINE_V2(id, name, ver, desc, author)                \
  auto GFGetModuleGFSDKVersion() -> const char* {                           \
    return DUP(GF_SDK_VERSION_STR);                                         \
  }                                                                         \
  auto GFGetModuleQtEnvVersion() -> const char* {                           \
    return DUP(QT_VERSION_STR);                                             \
  }                                                                         \
  auto GFGetModuleID() -> const char* { return DUP((id)); }                 \
  auto GFGetModuleVersion() -> const char* { return DUP((ver)); }           \
  auto GFGetModuleMetaData() -> GFModuleMetaData* {                         \
    return QMapToGFModuleMetaDataList(                                      \
        {{"Name", (name)}, {"Description", (desc)}, {"Author", (author)}}); \
  }                                                                         \
  using MEvent = QMap<QString, QString>;                                    \
  using EventHandler = std::function<int(const MEvent&)>;                   \
  namespace {                                                               \
  static QMap<QString, EventHandler> Module##nameEventHandlers;             \
  static QMap<QString, EventHandler>& _gr_module_event_handlers =           \
      Module##nameEventHandlers;                                            \
  }                                                                         \
  DEFINE_EXECUTE_API_USING_STANDARD_EVEN_HANDLE_MODEL