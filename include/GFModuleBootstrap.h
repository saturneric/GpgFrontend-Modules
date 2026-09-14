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

#include <GFSDKModuleApi.h>

#include "GFModuleExport.h"
#include "GFSDKBuildInfo.h"

/**
 * @file GFModuleBootstrap.h
 * @brief Define a module through the single bootstrap symbol.
 *
 * Replaces GF_MODULE_API_DECLARE + GF_MODULE_API_DEFINE, which between them
 * required a module to export ten separate symbols. Here a module exports
 * exactly one, and describes itself through a versioned table.
 *
 * WHAT A MODULE GAINS. The host hands `activate` a GFHostApi table instead of
 * the module linking ~130 global SDK symbols. That is what makes it possible
 * for the host to withhold a capability (hand over a table with the gpg group
 * null), and what lets a test supply its own table instead of compiling a
 * parallel set of stub symbols that drift from the real SDK.
 *
 * USAGE, once per module:
 *
 *     GF_MODULE_BOOTSTRAP("com.example.module", "1.0.0",
 *                         MyActivate, MyExecute, MyDeactivate, MyUnregister)
 *
 * The host api pointer is stashed in GFHost() for the module's own code to
 * reach without threading it through every function. It is valid from the
 * moment activate() is called until the module is unloaded.
 */

/// The host table this module was activated with. Null before activation.
inline const GFHostApi*& GFHostApiSlot() {
  static const GFHostApi* host = nullptr;
  return host;
}

/// Convenience accessor for module code.
inline auto GFHost() -> const GFHostApi* { return GFHostApiSlot(); }

/**
 * @brief Define the module's bootstrap symbol and lifecycle table.
 *
 * @param id     module identifier, e.g. "com.example.module"
 * @param ver    module version, e.g. "1.0.0"
 * @param fn_act int(const GFHostApi*, void*) -- setup; host api is borrowed
 * @param fn_exe int(GFModuleEvent*)          -- handle one event
 * @param fn_dea int(void)                    -- cancel work, drop registrations
 * @param fn_unr void(void)                   -- final teardown
 */
#define GF_MODULE_BOOTSTRAP(id, ver, fn_act, fn_exe, fn_dea, fn_unr)         \
  static int GFBootstrapActivate(const GFHostApi* host, void* reserved) {    \
    GFHostApiSlot() = host;                                                  \
    return (fn_act)(host, reserved);                                         \
  }                                                                          \
  extern "C" GF_MODULE_EXPORT const GFModuleApi* GFModuleGetApi(             \
      uint32_t host_abi) {                                                   \
    /* Decline a host outside the range this module was built for, rather  */\
    /* than loading and failing on the first mismatched call. */             \
    if (host_abi < GF_SDK_ABI_MIN_SUPPORTED ||                               \
        host_abi > GF_SDK_ABI_VERSION) {                                     \
      return nullptr;                                                        \
    }                                                                        \
    /* Static: the host borrows this table and never frees it. */            \
    static const GFModuleApi kApi = {                                        \
        sizeof(GFModuleApi), GF_SDK_ABI_VERSION, (id), (ver),                \
        &GFBootstrapActivate, (fn_exe), (fn_dea), (fn_unr),                  \
    };                                                                       \
    return &kApi;                                                            \
  }


/**
 * @brief Bootstrap a module that already has the classic lifecycle functions.
 *
 * Adapts GFRegisterModule / GFActiveModule / GFExecuteModule /
 * GFDeactivateModule / GFUnregisterModule onto the table, so porting a module
 * is a one-line change at the top of the file rather than a rewrite of its
 * entry points. It also keeps GFGetModuleID() and friends defined, because
 * module code calls GFGetModuleID() constantly (LISTEN, CB, the translator
 * reader) and those calls should not have to change.
 *
 * Note the ORDER inside activate: the host's table has no separate register
 * step, so whatever the module did in GFRegisterModule -- typically
 * registering its translator -- has to run here, before GFActiveModule starts
 * subscribing to events.
 */
#define GF_MODULE_BOOTSTRAP_V2(id, name, ver, desc, author)                  \
  /* Identity strings are BORROWED statics, not fresh allocations.         */\
  /* They used to be DUP(...)ed on every call because the SDK entry points  */\
  /* they were passed to freed their arguments. Now that arguments are      */\
  /* borrowed, allocating here would simply leak -- and GFGetModuleID() is  */\
  /* called on the order of seventy times across the modules.               */\
  auto GFGetModuleGFSDKVersion() -> const char* {                            \
    return GF_SDK_VERSION_STR;                                               \
  }                                                                          \
  auto GFGetModuleGFSDKABIVersion() -> int { return GF_SDK_ABI_VERSION; }    \
  auto GFGetModuleQtEnvVersion() -> const char* { return QT_VERSION_STR; }   \
  auto GFGetModuleID() -> const char* { return (id); }                       \
  auto GFGetModuleVersion() -> const char* { return (ver); }                 \
  auto GFGetModuleMetaData() -> GFModuleMetaData* {                          \
    return QMapToGFModuleMetaDataList(                                       \
        {{"Name", (name)}, {"Description", (desc)}, {"Author", (author)}});  \
  }                                                                          \
  using MEvent = QMap<QString, QString>;                                     \
  using EventHandler = std::function<int(const MEvent&)>;                    \
  namespace {                                                                \
  /* NOTE: `Module##nameEventHandlers` pastes onto the IDENTIFIER          */\
  /* `nameEventHandlers`, so it does not substitute the `name` parameter   */\
  /* at all -- it always yields ModulenameEventHandlers. That is inherited */\
  /* from GF_MODULE_API_DEFINE_V2 and kept deliberately: `name` is a string*/\
  /* literal here, and actually pasting it would not compile.              */\
  static QMap<QString, EventHandler> Module##nameEventHandlers;             \
  static QMap<QString, EventHandler>& _gr_module_event_handlers =            \
      Module##nameEventHandlers;                                            \
  }                                                                          \
  DEFINE_EXECUTE_API_USING_STANDARD_EVEN_HANDLE_MODEL                        \
  static int GFBootstrapActivate(const GFHostApi* host, void*) {             \
    GFHostApiSlot() = host;                                                  \
    const int rc = GFRegisterModule();                                       \
    if (rc != 0) return rc;                                                  \
    return GFActiveModule();                                                 \
  }                                                                          \
  static void GFBootstrapUnregister() { (void)GFUnregisterModule(); }        \
  extern "C" GF_MODULE_EXPORT const GFModuleApi* GFModuleGetApi(             \
      uint32_t host_abi) {                                                   \
    if (host_abi < GF_SDK_ABI_MIN_SUPPORTED ||                               \
        host_abi > GF_SDK_ABI_VERSION) {                                     \
      return nullptr;                                                        \
    }                                                                        \
    static const GFModuleApi kApi = {                                        \
        sizeof(GFModuleApi),                                                 \
        GF_SDK_ABI_VERSION,                                                  \
        (id),                                                                \
        (ver),                                                               \
        &GFBootstrapActivate,                                                \
        &GFExecuteModule,                                                    \
        &GFDeactivateModule,                                                 \
        &GFBootstrapUnregister,                                              \
    };                                                                       \
    return &kApi;                                                            \
  }
