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

#include "EMailViewLayout.h"

#include <algorithm>

auto EMailClampDetailsSize(int width, int height) -> std::pair<int, int> {
  const auto wanted_width = width > 0 ? width : kEMailDetailsDefaultWidth;
  const auto wanted_height = height > 0 ? height : kEMailDetailsDefaultHeight;

  // Only a lower bound. There is no sensible upper one here: the window may
  // legitimately be as large as whatever screen it is opening on, and this
  // function does not know what that is.
  return {std::max(wanted_width, kEMailDetailsMinWidth),
          std::max(wanted_height, kEMailDetailsMinHeight)};
}

auto EMailClampDetailsTab(int persisted, int count) -> int {
  if (count <= 0) return 0;
  return std::clamp(persisted, 0, count - 1);
}

auto EMailShouldCompactActions(int surface_width, bool currently_compact)
    -> bool {
  // Hysteresis: without it the labels flicker on and off while a window edge
  // is being dragged across the threshold.
  return currently_compact ? surface_width < kEMailRoomyActionsWidth
                           : surface_width < kEMailCompactActionsWidth;
}

auto EMailNeedsSecurityRefresh(bool details_visible, bool security_current)
    -> bool {
  return details_visible && security_current;
}
