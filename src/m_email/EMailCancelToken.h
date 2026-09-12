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
  void Cancel() { cancelled_.store(true, std::memory_order_relaxed); }
  void Reset() { cancelled_.store(false, std::memory_order_relaxed); }

  [[nodiscard]] auto IsCancelled() const -> bool {
    return cancelled_.load(std::memory_order_relaxed);
  }

 private:
  std::atomic<bool> cancelled_{false};
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

 private:
  EMailCancelTokenPtr token_;
  int timeout_seconds_;
  vmime::shared_ptr<EMailTimeoutHandler> last_;
};
