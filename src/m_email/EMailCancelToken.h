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

#include <QElapsedTimer>
#include <atomic>
#include <limits>
#include <memory>

// The test target defines this on the command line; the module build does
// not, so it is set here and guarded rather than assumed either way.
#ifndef VMIME_STATIC
#define VMIME_STATIC
#endif
#include <vmime/vmime.hpp>

/**
 * @brief A stop request that can safely cross threads.
 *
 * Deliberately a bare atomic rather than a QObject. It is set from the GUI
 * thread while a worker thread is blocked inside a socket call, and that is
 * only safe because there is no Qt machinery involved -- no event loop to
 * deliver to, no affinity to violate, nothing to destroy underneath the
 * reader.
 */
class EMailCancelToken {
 public:
  /// Cancels the operation numbered @p seq: the one running now, or one that
  /// has been queued to the worker and has not started yet.
  ///
  /// Scoped by sequence number rather than a bare flag because both ends of
  /// that range matter. A stop pressed while a request is still sitting in the
  /// worker's event queue has to survive until the slot runs -- the previous
  /// design cleared the flag at slot entry, so such a stop did nothing at all
  /// and the operation ran to its full timeout. Equally, a stop belonging to a
  /// finished operation must not cancel the next one, which a latch that was
  /// never cleared would do.
  void Cancel(quint64 seq) {
    auto previous = cancelled_through_.load(std::memory_order_relaxed);
    while (seq > previous &&
           !cancelled_through_.compare_exchange_weak(
               previous, seq, std::memory_order_relaxed)) {
    }
  }

  /// Cancels the current operation and everything queued behind it, whatever
  /// their numbers. For teardown, where no further work is wanted at all.
  void CancelAll() {
    cancelled_through_.store(std::numeric_limits<quint64>::max(),
                             std::memory_order_relaxed);
  }

  /// Announces the operation the worker is about to run.
  ///
  /// Deliberately does NOT clear a cancel already issued for @p seq: that
  /// stop was aimed at this very operation.
  void Begin(quint64 seq) {
    current_.store(seq, std::memory_order_relaxed);
  }

  [[nodiscard]] auto IsCancelled() const -> bool {
    const auto current = current_.load(std::memory_order_relaxed);
    return current != 0 &&
           current <= cancelled_through_.load(std::memory_order_relaxed);
  }

 private:
  /// The operation the worker is running. 0 means none has begun.
  std::atomic<quint64> current_{0};
  /// Every operation numbered at or below this is cancelled.
  std::atomic<quint64> cancelled_through_{0};
};

using EMailCancelTokenPtr = std::shared_ptr<EMailCancelToken>;

/**
 * @brief How a cancellation actually reaches a blocked vmime call.
 *
 * vmime has no cancel of its own, but it polls its timeout handler from inside
 * every blocking read, write and connect loop -- and inside the TLS handshake.
 * Answering "yes, time is up" is therefore the one way to make a blocked
 * socket call return, which is why cancellation is expressed as a timeout.
 *
 * The cost of that trick is that a cancelled operation and a genuinely
 * unresponsive server raise the same exception, so this records which it was.
 * Reporting a user's own stop as "the server stopped responding" would be a
 * lie, and the classifier consults @ref WasCancelled to avoid telling it.
 */
class EMailTimeoutHandler : public vmime::net::timeoutHandler {
 public:
  explicit EMailTimeoutHandler(EMailCancelTokenPtr token, int timeout_seconds);

  auto isTimeOut() -> bool override;
  void resetTimeOut() override;
  auto handleTimeOut() -> bool override;

  /// Whether the last firing was the user stopping rather than a timeout.
  [[nodiscard]] auto WasCancelled() const -> bool { return cancelled_; }

  /// Forgets a recorded stop once the operation it belonged to has ended, so
  /// a later timeout is not reported as the user's own cancellation.
  void ClearCancelled() { cancelled_ = false; }

 private:
  EMailCancelTokenPtr token_;
  int timeout_seconds_;
  /// Monotonic: a clock change mid-transfer must not look like a timeout.
  QElapsedTimer elapsed_;
  bool cancelled_{false};
};

/**
 * @brief Supplies the handler above to every connection vmime opens.
 */
class EMailTimeoutHandlerFactory : public vmime::net::timeoutHandlerFactory {
 public:
  explicit EMailTimeoutHandlerFactory(EMailCancelTokenPtr token,
                                      int timeout_seconds = 30);

  auto create() -> vmime::shared_ptr<vmime::net::timeoutHandler> override;

  /// The handler most recently created, so a caller unwinding an exception can
  /// ask whether the stop was deliberate.
  [[nodiscard]] auto LastWasCancelled() const -> bool;

  /// Clears the recorded stop on the most recent handler. See
  /// EMailTimeoutHandler::ClearCancelled.
  void ClearLastCancelled();

 private:
  EMailCancelTokenPtr token_;
  int timeout_seconds_;
  vmime::shared_ptr<EMailTimeoutHandler> last_;
};
