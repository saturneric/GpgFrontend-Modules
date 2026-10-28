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

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QString>

/**
 * @file NoteBridge.h
 * @brief Requests to the editor page and their replies: a reply counts only
 * for a request still pending in the current epoch.
 *
 * SDK-free. Lock, close or a new editor bump the epoch; whatever arrives for
 * an older one -- a late getMarkdown, a crashed page's last word -- is
 * dropped.
 */

namespace gf::notes {

class NoteBridge {
 public:
  static constexpr int kMaxPending = 8;
  static constexpr qint64 kDeadlineMs = 5000;

  /// A request of @p type, as JSON to post; empty when too many are pending.
  auto Request(const QString& type, QJsonObject fields, qint64 now)
      -> QByteArray;
  /// The pending request @p reply answers, by type; empty when it is stale,
  /// unknown, of another epoch, or already answered.
  auto Accept(const QJsonObject& reply) -> QString;
  /// Forget requests past their deadline; how many went.
  auto Expire(qint64 now) -> int;
  /// Everything pending is void from here on.
  void NewEpoch();

  [[nodiscard]] auto Epoch() const -> quint64 { return epoch_; }
  [[nodiscard]] auto Pending() const -> int {
    return static_cast<int>(pending_.size());
  }

 private:
  struct Entry {
    QString type;
    qint64 deadline = 0;
  };
  quint64 epoch_ = 1;
  quint64 next_id_ = 1;
  QHash<quint64, Entry> pending_;
};

/// One page message as an object, or empty if it is not one.
auto ParseMessage(const QByteArray& json) -> QJsonObject;

}  // namespace gf::notes
