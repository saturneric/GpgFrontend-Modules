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

#include "NoteProtection.h"

using namespace Qt::StringLiterals;
using namespace gf::notes;

namespace {

const QString kAlice = u"AAAA000000000000000000000000000000000001"_s;
const QString kBob = u"BBBB000000000000000000000000000000000002"_s;

}  // namespace

TEST(NoteProtectionTest, CheckedKeysAreTheReaders) {
  const auto c = ProtectionConfig::ForCheckedKeys({kBob, kAlice.toLower()});
  EXPECT_EQ(c.recipients, (QStringList{kAlice, kBob}));
  EXPECT_FALSE(c.passphrase);
}

TEST(NoteProtectionTest, NoCheckedKeyMeansAPassphrase) {
  const auto c = ProtectionConfig::ForCheckedKeys({u" "_s});
  EXPECT_TRUE(c.recipients.isEmpty());
  EXPECT_TRUE(c.passphrase);
}

TEST(NoteProtectionTest, TheSameSelectionInAnotherOrderIsTheSameReaders) {
  EXPECT_EQ(ProtectionConfig::ForCheckedKeys({kAlice, kBob}),
            ProtectionConfig::ForCheckedKeys({kBob, kAlice, kAlice}));
  EXPECT_FALSE(ProtectionConfig::ForCheckedKeys({kAlice}) ==
               ProtectionConfig::ForCheckedKeys({kAlice, kBob}));
  EXPECT_FALSE(ProtectionConfig::ForCheckedKeys({}) ==
               ProtectionConfig::ForCheckedKeys({kAlice}));
}
