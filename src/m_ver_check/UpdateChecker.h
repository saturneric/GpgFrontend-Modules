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
#include <QDateTime>
#include <QObject>
#include <QUrl>
#include <functional>
#include <optional>

#include "SoftwareVersion.h"

/**
 * @brief Everything the update UI needs to know, readable at any moment.
 *
 * A failed attempt never replaces what was known: `last_good` is the last
 * result whose release list came back, and the attempt fields say how the
 * most recent try went.
 */
struct UpdateSnapshot {
  bool checking = false;
  std::optional<SoftwareVersion> last_good;
  QDateTime last_attempt;
  bool last_attempt_failed = false;
};

enum class CheckMode {
  kIfStale,  ///< skip when a recent authoritative result for this build exists
  kForce,    ///< always ask the server (the user pressed the button)
};

/**
 * @brief The one update check of the process.
 *
 * Every caller shares it: a Start() while a check is running joins that check
 * instead of starting another. The transport, the store and the clock are
 * handed in, so the whole state machine runs in a test without a network, a
 * Host or an event loop.
 */
class UpdateChecker : public QObject {
  Q_OBJECT

 public:
  /// Answers a GET: the HTTP status (0 when there was none) and the body.
  using Reply = std::function<void(int http_status, const QByteArray& body)>;
  using Fetcher = std::function<void(const QUrl& url, Reply reply)>;
  using Clock = std::function<QDateTime()>;

  struct Store {
    std::function<QByteArray()> load;
    std::function<void(const QByteArray&)> save;
  };

  struct Build {
    QString version;
    QString commit;
  };

  static constexpr qint64 kFreshSeconds = 24 * 60 * 60;
  static constexpr auto kRepoApi =
      "https://api.github.com/repos/saturneric/gpgfrontend";

  UpdateChecker(Build build, Fetcher fetch, Store store, Clock clock = {},
                QObject* parent = nullptr);

  [[nodiscard]] auto State() const -> const UpdateSnapshot&;

  /// Starts a check, joins the running one, or (kIfStale) does nothing.
  void Start(CheckMode mode);

  /// A recent authoritative result for this build: no startup check needed.
  [[nodiscard]] auto IsFresh() const -> bool;

  /// The state as stored; FromStorage() reads it back for @p build only.
  [[nodiscard]] static auto ToStorage(const UpdateSnapshot& s) -> QByteArray;
  [[nodiscard]] static auto FromStorage(const QByteArray& data,
                                        const Build& build) -> UpdateSnapshot;

 signals:
  /// Any change of State(), including the start of a check.
  void Changed();

 private:
  void finish();

  Build build_;
  Fetcher fetch_;
  Store store_;
  Clock clock_;

  UpdateSnapshot state_;
  SoftwareVersion fresh_;
  int pending_ = 0;
};

/// Whether the startup check should open the update dialog now.
auto ShouldPromptOnStartup(const UpdateSnapshot& s) -> bool;
