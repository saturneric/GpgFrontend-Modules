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

#include <utility>

/**
 * @file
 * @brief The e-mail view's layout decisions, with no widget behind them.
 *
 * Everything here is a rule about geometry or about when work is worth doing,
 * and none of it needs a QWidget to be true. Kept apart from EMailPageView.cpp
 * for exactly that reason: this module's tests link Qt Core only, and a widget
 * built in a test would be built off the GUI thread anyway. Rules that live
 * here are checked; the same rules written inline in the view would not be.
 */

/// The smallest useful details window, and what it opens at when nothing has
/// been remembered. Below the minimum its lists are narrower than their own
/// column headings.
constexpr int kEMailDetailsMinWidth = 420;
constexpr int kEMailDetailsMinHeight = 320;
constexpr int kEMailDetailsDefaultWidth = 680;
constexpr int kEMailDetailsDefaultHeight = 520;

/// Below this the action row cannot carry five labelled actions as well as the
/// view controls and the security state.
constexpr int kEMailCompactActionsWidth = 640;

/// And the labels do not come back until there is room to spare, so dragging a
/// window edge across the threshold does not flap them on and off.
constexpr int kEMailRoomyActionsWidth = 700;

/// The settings keys the details window is remembered under.
constexpr auto kEMailDetailsWidthKey = "view/details_width";
constexpr auto kEMailDetailsHeightKey = "view/details_height";
constexpr auto kEMailDetailsTabKey = "view/details_tab";

/**
 * @brief What size the details window may actually open at.
 *
 * @param width  remembered width, or <= 0 for nothing remembered
 * @param height remembered height, or <= 0 for nothing remembered
 *
 * Never smaller than its minimum: a remembered size was measured on a screen
 * that is not necessarily the screen it is being restored onto, and a window
 * too small to show a tab bar and a row cannot be recovered from by the person
 * looking at it.
 */
auto EMailClampDetailsSize(int width, int height) -> std::pair<int, int>;

/**
 * @brief Which tab the details window may actually open on.
 *
 * A remembered index outlives the tabs it indexed: a build with one tab fewer
 * would otherwise open on nothing at all.
 */
auto EMailClampDetailsTab(int persisted, int count) -> int;

/**
 * @brief Whether the message actions should be shown as icons alone.
 *
 * @param surface_width the width the message surface actually has
 *
 * The actions give way rather than the view controls: which view you are
 * looking at, and whether the message is signed, are things to read at a
 * glance, while Reply and Forward are recognisable as icons and carry their
 * wording in a tooltip either way.
 */
auto EMailShouldCompactActions(int surface_width, bool currently_compact)
    -> bool;

/**
 * @brief Whether the Security tab is worth re-deriving right now.
 *
 * Verifying reads the message and writes nothing, but it can wait on the
 * agent, so it is only done for a tab someone is actually looking at. The one
 * definition of that condition, because it is asked in two places -- when the
 * window opens or changes tab, and when the keyring changes underneath it --
 * and the two drifting apart is how a freshly imported key fails to show up.
 */
auto EMailNeedsSecurityRefresh(bool details_visible, bool security_current)
    -> bool;
