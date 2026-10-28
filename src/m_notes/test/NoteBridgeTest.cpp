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

#include <QJsonDocument>

#include "NoteBridge.h"

using namespace Qt::StringLiterals;
using namespace gf::notes;

namespace {

auto Reply(const QByteArray& request) -> QJsonObject {
  const auto r = QJsonDocument::fromJson(request).object();
  return {{u"re"_s, r.value(u"id"_s)}, {u"epoch"_s, r.value(u"epoch"_s)}};
}

}  // namespace

TEST(NoteBridgeTest, AReplyCountsOnceForItsOwnRequest) {
  NoteBridge b;
  const auto req = b.Request(u"get"_s, {}, 0);
  ASSERT_FALSE(req.isEmpty());
  EXPECT_EQ(b.Accept(Reply(req)), u"get"_s);
  EXPECT_EQ(b.Accept(Reply(req)), QString()) << "already answered";
}

TEST(NoteBridgeTest, ALateReplyAfterLockIsDropped) {
  NoteBridge b;
  const auto req = b.Request(u"get"_s, {}, 0);
  b.NewEpoch();  // locked
  EXPECT_EQ(b.Accept(Reply(req)), QString());
  EXPECT_EQ(b.Pending(), 0);
}

TEST(NoteBridgeTest, RequestsExpireAndAreBounded) {
  NoteBridge b;
  for (int i = 0; i < NoteBridge::kMaxPending; ++i) {
    EXPECT_FALSE(b.Request(u"get"_s, {}, 0).isEmpty());
  }
  EXPECT_TRUE(b.Request(u"get"_s, {}, 0).isEmpty()) << "too many outstanding";
  EXPECT_EQ(b.Expire(NoteBridge::kDeadlineMs), NoteBridge::kMaxPending);
  EXPECT_FALSE(b.Request(u"get"_s, {}, 0).isEmpty());
}

TEST(NoteBridgeTest, OnlyObjectsAreMessages) {
  EXPECT_TRUE(ParseMessage("[1]").isEmpty());
  EXPECT_TRUE(ParseMessage("{bad").isEmpty());
  EXPECT_EQ(ParseMessage(R"({"type":"dirty"})").value(u"type"_s).toString(),
            u"dirty"_s);
}
