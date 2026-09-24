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

#include "KeyServerBatchLogic.h"

using KeyServerBatchLogic::BatchKey;

namespace {

auto Key(const QString& fpr, qint64 channel = 0) -> BatchKey {
  return {channel, fpr.right(16), fpr};
}

}  // namespace

TEST(KeyServerBatchLogicTest, OneKeyOrASelectionBecomeOneList) {
  const auto one = KeyServerBatchLogic::Normalise(Key("AAAA"), std::nullopt);
  ASSERT_EQ(one.size(), 1);
  EXPECT_EQ(one.front().fingerprint, "AAAA");

  const auto many = KeyServerBatchLogic::Normalise(
      std::nullopt, QList<BatchKey>{Key("AAAA"), Key("BBBB")});
  ASSERT_EQ(many.size(), 2);
  EXPECT_EQ(many[1].fingerprint, "BBBB");

  EXPECT_TRUE(
      KeyServerBatchLogic::Normalise(std::nullopt, std::nullopt).isEmpty());
}

TEST(KeyServerBatchLogicTest, AKeyNamedTwiceIsActedOnOnceInFirstOrder) {
  // Case and surrounding space do not make a fingerprint a different key.
  const auto keys = KeyServerBatchLogic::Normalise(
      Key("bbbb"), QList<BatchKey>{Key("AAAA"), Key(" BBBB "), Key("AAAA")});
  ASSERT_EQ(keys.size(), 2);
  EXPECT_EQ(keys[0].fingerprint, "bbbb");
  EXPECT_EQ(keys[1].fingerprint, "AAAA");
}

TEST(KeyServerBatchLogicTest, AKeyWithoutAFingerprintIsDropped) {
  const auto keys = KeyServerBatchLogic::Normalise(
      std::nullopt, QList<BatchKey>{Key(""), Key("  "), Key("CCCC")});
  ASSERT_EQ(keys.size(), 1);
  EXPECT_EQ(keys.front().fingerprint, "CCCC");
}

TEST(KeyServerBatchLogicTest, BlocksJoinIntoOneImportOnSeparateLines) {
  const QByteArray a =
      "-----BEGIN PGP PUBLIC KEY BLOCK-----\nA\n"
      "-----END PGP PUBLIC KEY BLOCK-----";
  const QByteArray b =
      "-----BEGIN PGP PUBLIC KEY BLOCK-----\nB\n"
      "-----END PGP PUBLIC KEY BLOCK-----\n";
  const auto joined = KeyServerBatchLogic::JoinForImport({a, "", "  \n", b});
  EXPECT_EQ(joined, a + "\n" + b);
  EXPECT_TRUE(KeyServerBatchLogic::JoinForImport({}).isEmpty());
}

TEST(KeyServerBatchLogicTest, SummariesCountAndListFailures) {
  const auto clean = KeyServerBatchLogic::RefreshSummary(3, 3, {});
  EXPECT_TRUE(clean.contains("3 of 3"));
  EXPECT_FALSE(clean.contains("Failed"));

  const auto partial =
      KeyServerBatchLogic::RefreshSummary(1, 2, {"BBBB: not found"});
  EXPECT_TRUE(partial.contains("1 of 2"));
  EXPECT_TRUE(partial.contains("BBBB: not found"));

  const auto published =
      KeyServerBatchLogic::PublishSummary(2, 2, "keys.openpgp.org", {});
  EXPECT_TRUE(published.contains("2 of 2"));
  EXPECT_TRUE(published.contains("keys.openpgp.org"));
}

// "The server does not have this key" is an answer, told apart from a real
// failure by the caller, and never shown in the transport's own wording.
TEST(KeyServerBatchLogicTest, NotFoundIsItsOwnPlainAnswer) {
  EXPECT_TRUE(
      KeyServerBatchLogic::IsNotFound(KeyServerBatchLogic::NotFoundText()));
  EXPECT_FALSE(KeyServerBatchLogic::IsNotFound(
      "Error transferring https://keys.openpgp.org/vks/v1/by-fingerprint/X - "
      "server replied: "));
  EXPECT_FALSE(KeyServerBatchLogic::NotFoundText().contains("http"));
}
