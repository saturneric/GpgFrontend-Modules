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
#include "GFModuleIdentity.h"
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
 * USAGE, once per module, with no arguments at all:
 *
 *     GF_MODULE_BOOTSTRAP()
 *
 * Identity comes from the generated GFModuleIdentity.h, which is built from
 * the module's own module.json.
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
 * @brief Define this module's bootstrap symbol, lifecycle table and identity.
 *
 * Takes no arguments. Everything it needs comes from `GFModuleIdentity.h`,
 * which `gf_add_module()` generates from the module's `module.json` -- so the
 * identifier and version the host cross-checks against the signed manifest
 * cannot disagree with it, because there is only one copy.
 *
 * It replaces `GF_MODULE_BOOTSTRAP_V2(id, name, ver, desc, author)`, whose
 * five arguments were the C++ half of the same five values the CMake call
 * repeated. Three of them -- name, description, author -- fed only
 * `GFGetModuleMetaData()`, which had no callers at all: the host reads that
 * metadata from the verified package manifest instead. So they are simply
 * gone, rather than moved.
 *
 * The module still writes its lifecycle functions under their classic names --
 * GFRegisterModule, GFActiveModule, GFExecuteModule, GFDeactivateModule,
 * GFUnregisterModule -- and this adapts them onto the table.
 *
 * Note the ORDER inside activate: the host's table has no separate register
 * step, so whatever the module did in GFRegisterModule -- typically
 * registering its translator -- has to run here, before GFActiveModule starts
 * subscribing to events.
 */
#define GF_MODULE_BOOTSTRAP()                                                  \
  /* Identity strings are BORROWED statics, not fresh allocations.         */  \
  /* They used to be DUP(...)ed on every call because the SDK entry points  */ \
  /* they were passed to freed their arguments. Now that arguments are      */ \
  /* borrowed, allocating here would simply leak -- and GFGetModuleID() is  */ \
  /* called on the order of seventy times across the modules.               */ \
  auto GFGetModuleID() -> const char* { return GF_MODULE_ID; }                 \
  using MEvent = QMap<QString, QString>;                                       \
  using EventHandler = std::function<int(const MEvent&)>;                      \
  namespace {                                                                  \
  static QMap<QString, EventHandler> gModuleEventHandlers;                     \
  static QMap<QString, EventHandler>& _gr_module_event_handlers =              \
      gModuleEventHandlers;                                                    \
  }                                                                            \
  DEFINE_EXECUTE_API_USING_STANDARD_EVEN_HANDLE_MODEL                          \
  static int GFBootstrapActivate(const GFHostApi* host, void*) {               \
    GFHostApiSlot() = host;                                                    \
    const int rc = GFRegisterModule();                                         \
    if (rc != 0) return rc;                                                    \
    return GFActiveModule();                                                   \
  }                                                                            \
  static void GFBootstrapUnregister() { (void)GFUnregisterModule(); }          \
  extern "C" GF_MODULE_EXPORT const GFModuleApi* GFModuleGetApi(               \
      uint32_t host_abi) {                                                     \
    /* Decline a host outside the range this module was built for, rather  */  \
    /* than loading and failing on the first mismatched call. */               \
    if (host_abi < GF_SDK_ABI_MIN_SUPPORTED ||                                 \
        host_abi > GF_SDK_ABI_VERSION) {                                       \
      return nullptr;                                                          \
    }                                                                          \
    /* Static: the host borrows this table and never frees it. */              \
    static const GFModuleApi kApi = {                                          \
        sizeof(GFModuleApi), GF_SDK_ABI_VERSION,     GF_MODULE_ID,             \
        GF_MODULE_VERSION,   &GFBootstrapActivate,   &GFExecuteModule,         \
        &GFDeactivateModule, &GFBootstrapUnregister,                           \
    };                                                                         \
    return &kApi;                                                              \
  }
