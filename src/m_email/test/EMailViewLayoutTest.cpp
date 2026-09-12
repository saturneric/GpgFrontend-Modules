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

#include <gtest/gtest.h>

#include "EMailViewLayout.h"

// The details inspector's rules, checked without a widget. The view itself
// cannot be built in a test here -- these bodies run off the GUI thread -- so
// the decisions it makes are kept in a file that can be.

namespace {

TEST(EMailViewLayoutTest, NothingRememberedOpensAtTheDefaultSize) {
  const auto size = EMailClampDetailsSize(0, 0);
  EXPECT_EQ(size.first, kEMailDetailsDefaultWidth);
  EXPECT_EQ(size.second, kEMailDetailsDefaultHeight);
}

TEST(EMailViewLayoutTest, AnUnusableRememberedSizeIsBroughtBack) {
  // A size measured on a screen that is not the one it is being restored onto.
  // Too small to show a tab bar and a row, the window cannot be recovered from
  // by the person looking at it.
  const auto size = EMailClampDetailsSize(40, 10);
  EXPECT_EQ(size.first, kEMailDetailsMinWidth);
  EXPECT_EQ(size.second, kEMailDetailsMinHeight);
}

TEST(EMailViewLayoutTest, ARememberedSizeThatFitsIsKeptExactly) {
  const auto size = EMailClampDetailsSize(900, 700);
  EXPECT_EQ(size.first, 900);
  EXPECT_EQ(size.second, 700);
}

TEST(EMailViewLayoutTest, ASizeIsLargeEnoughInOneDimensionAtATime) {
  // Width fine, height unusable: only the height gives way.
  const auto size = EMailClampDetailsSize(900, 10);
  EXPECT_EQ(size.first, 900);
  EXPECT_EQ(size.second, kEMailDetailsMinHeight);
}

TEST(EMailViewLayoutTest, ARememberedTabOutlivingItsTabsLandsOnARealOne) {
  EXPECT_EQ(EMailClampDetailsTab(2, 3), 2);
  EXPECT_EQ(EMailClampDetailsTab(7, 3), 2);
  EXPECT_EQ(EMailClampDetailsTab(-1, 3), 0);

  // No tabs at all: still an index something can be told to select.
  EXPECT_EQ(EMailClampDetailsTab(2, 0), 0);
}

TEST(EMailViewLayoutTest, CrowdedActionRowsDropTheirWording) {
  EXPECT_TRUE(EMailShouldCompactActions(500, false));
  EXPECT_FALSE(EMailShouldCompactActions(1000, false));
  EXPECT_FALSE(EMailShouldCompactActions(1000, true));
}

TEST(EMailViewLayoutTest, TheActionDensityThresholdDoesNotFlap) {
  // The window edge is dragged across this range a pixel at a time, and the
  // labels must not blink on and off as it goes.
  for (int width = kEMailCompactActionsWidth; width < kEMailRoomyActionsWidth;
       ++width) {
    EXPECT_FALSE(EMailShouldCompactActions(width, false)) << width;
    EXPECT_TRUE(EMailShouldCompactActions(width, true)) << width;
  }
}

TEST(EMailViewLayoutTest, SecurityIsOnlyReDerivedWhereItCanBeSeen) {
  EXPECT_TRUE(EMailNeedsSecurityRefresh(true, true));

  // The window is shut, or another tab is in front: in both cases the next
  // visit picks the work up, which is where it is normally triggered anyway.
  EXPECT_FALSE(EMailNeedsSecurityRefresh(false, true));
  EXPECT_FALSE(EMailNeedsSecurityRefresh(true, false));
  EXPECT_FALSE(EMailNeedsSecurityRefresh(false, false));
}

}  // namespace
