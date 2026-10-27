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

#include <cstdint>

#include "GFSDKTypes.h"

/**
 * @file WebEngineScheme.h
 * @brief What has to happen before QtWebEngine starts, and what it enforces.
 */

/// Every GF_HTML_POLICY_* bit: this engine enforces all of them. Each is
/// accounted for in WebEngineView.h.
constexpr uint32_t kWebEngineEnforced =
    GF_HTML_POLICY_NO_NETWORK | GF_HTML_POLICY_PARTS_ONLY |
    GF_HTML_POLICY_NO_SCRIPT | GF_HTML_POLICY_NO_NAVIGATION |
    GF_HTML_POLICY_NO_POPUPS | GF_HTML_POLICY_NO_DOWNLOADS |
    GF_HTML_POLICY_NO_PERSISTENCE | GF_HTML_POLICY_NO_PERMISSIONS |
    GF_HTML_POLICY_NO_DNS_PREFETCH | GF_HTML_POLICY_NO_PLUGINS;

/**
 * @brief Register GF_HTML_PART_SCHEME with the engine.
 *
 * Only takes effect before any QtWebEngine object exists: the engine locks
 * its schemes when it starts and ignores a late registration. So this runs
 * first thing at activation, and answers whether the engine knows the scheme
 * afterwards -- which it may not, if something started the engine before.
 * A view is never served on a scheme the engine does not know.
 */
auto RegisterPartScheme() -> bool;

/**
 * @brief Chromium switches that keep the engine itself quiet.
 *
 * The views block every request they make; these stop the engine's own
 * background traffic -- component updates, reliability reports, pings,
 * prefetch. Appended to whatever QTWEBENGINE_CHROMIUM_FLAGS already says,
 * and, like the scheme, only read when the engine starts.
 */
void ApplyEngineSwitches();
