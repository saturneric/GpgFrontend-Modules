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

#include "NoteBridge.h"

#include <QJsonDocument>

using namespace Qt::StringLiterals;

namespace gf::notes {

auto NoteBridge::Request(const QString& type, QJsonObject fields, qint64 now)
    -> QByteArray {
  if (pending_.size() >= kMaxPending) return {};
  const auto id = next_id_++;
  pending_.insert(id, {type, now + kDeadlineMs});
  fields.insert(u"type"_s, type);
  fields.insert(u"id"_s, static_cast<qint64>(id));
  fields.insert(u"epoch"_s, static_cast<qint64>(epoch_));
  return QJsonDocument(fields).toJson(QJsonDocument::Compact);
}

auto NoteBridge::Accept(const QJsonObject& reply) -> QString {
  if (reply.value(u"epoch"_s).toInteger(-1) != static_cast<qint64>(epoch_)) {
    return {};
  }
  const auto id = static_cast<quint64>(reply.value(u"re"_s).toInteger(0));
  const auto it = pending_.find(id);
  if (it == pending_.end()) return {};
  const auto type = it->type;
  pending_.erase(it);
  return type;
}

auto NoteBridge::Expire(qint64 now) -> int {
  int gone = 0;
  for (auto it = pending_.begin(); it != pending_.end();) {
    if (it->deadline <= now) {
      it = pending_.erase(it);
      ++gone;
    } else {
      ++it;
    }
  }
  return gone;
}

void NoteBridge::NewEpoch() {
  ++epoch_;
  pending_.clear();
}

auto ParseMessage(const QByteArray& json) -> QJsonObject {
  QJsonParseError error{};
  const auto doc = QJsonDocument::fromJson(json, &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject()) return {};
  return doc.object();
}

}  // namespace gf::notes
