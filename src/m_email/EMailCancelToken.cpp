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

#include "EMailCancelToken.h"

EMailTimeoutHandler::EMailTimeoutHandler(EMailCancelTokenPtr token,
                                         int timeout_seconds)
    : token_(std::move(token)), timeout_seconds_(timeout_seconds) {
  elapsed_.start();
}

auto EMailTimeoutHandler::isTimeOut() -> bool {
  if (token_ && token_->IsCancelled()) {
    cancelled_ = true;
    return true;
  }

  return elapsed_.elapsed() >= static_cast<qint64>(timeout_seconds_) * 1000;
}

void EMailTimeoutHandler::resetTimeOut() {
  elapsed_.restart();
  cancelled_ = false;
}

auto EMailTimeoutHandler::handleTimeOut() -> bool {
  // false means "do not keep waiting", which makes vmime throw out of whatever
  // blocking call it was in. That unwinding is the entire mechanism: there is
  // no other way to interrupt a socket read it is sitting inside.
  return false;
}

EMailTimeoutHandlerFactory::EMailTimeoutHandlerFactory(
    EMailCancelTokenPtr token, int timeout_seconds)
    : token_(std::move(token)), timeout_seconds_(timeout_seconds) {}

auto EMailTimeoutHandlerFactory::create()
    -> vmime::shared_ptr<vmime::net::timeoutHandler> {
  last_ = vmime::make_shared<EMailTimeoutHandler>(token_, timeout_seconds_);
  return last_;
}

auto EMailTimeoutHandlerFactory::LastWasCancelled() const -> bool {
  // The token is the more trustworthy of the two: a cancellation that arrives
  // between the handler firing and the exception being caught still means the
  // user asked to stop.
  if (token_ && token_->IsCancelled()) return true;
  return last_ && last_->WasCancelled();
}
