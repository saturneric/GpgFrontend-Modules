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

/**
 * @file GFModule.h
 * @brief The one header a module includes.
 *
 * Everything a module needs, grouped by what it is for. Include this; the
 * individual headers are there for when you want to see what is in each area,
 * not because you have to pick.
 *
 * | Header            | What lives there                                 |
 * | ----------------- | ------------------------------------------------ |
 * | GFModuleLog.h     | `LOG_*`, `FLOG_*`, `MLog*`                       |
 * | GFModuleEvent.h   | `LISTEN`, `CB_*`, handlers, event conversion     |
 * | GFModuleI18n.h    | translation context and `.qm` reader             |
 * | GFModuleMemory.h  | who frees what: `DUP`/`UDUP`, SDK allocation     |
 * | GFModuleConvert.h | QString/QMap <-> C, `FormatString`               |
 *
 * Deliberately NOT here: `GFModuleBootstrap.h`, and through it the generated
 * `GFModuleIdentity.h`. Exactly one translation unit per module bootstraps, so
 * only that one includes it. Everything else -- including the sources a
 * module's own gtest target compiles without the SDK -- then builds without
 * needing anything generated.
 *
 * A module declares itself in `module.json` beside its `CMakeLists.txt`, and
 * `gf_add_module()` turns that one file into the generated identity, the
 * translation wiring and the signed package. Nothing about a module's identity
 * is written twice.
 */

#include "GFModuleConvert.h"
#include "GFModuleEvent.h"
#include "GFModuleExport.h"
#include "GFModuleI18n.h"
#include "GFModuleLog.h"
#include "GFModuleMemory.h"
#include "GFSDKBuildInfo.h"
