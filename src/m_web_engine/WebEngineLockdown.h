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
#include <QString>

class QObject;
class QWebEnginePage;
class QWebEngineProfile;
class QWebEngineSettings;
class QWebEngineUrlRequestJob;

/**
 * @file WebEngineLockdown.h
 * @brief What every page this module shows is denied, whatever it shows.
 *
 * An HTML view and a web editor differ in what they load and whether script
 * runs. Everything else -- the profile, the request filter, every request a
 * page can make of the user -- is the same refusal, kept in one place so the
 * two cannot drift apart.
 */

namespace web_engine {

/// Everything that could act, fetch or reach outside the page: off. Script
/// too; an editor turns that one back on for its own page.
void LockDown(QWebEngineSettings* s);

/// Decides from the URL's scheme alone, on the engine's IO thread.
using SchemeFilter = bool (*)(const QString& scheme);

/**
 * @brief A profile of @p owner's that keeps nothing.
 *
 * No storage name, so off the record: an in-memory cache, no cookies or
 * permissions kept, no spell check, every download cancelled, and every
 * request whose scheme @p allowed refuses blocked before Chromium's network
 * stack. Its settings are locked down.
 */
auto NewLockedProfile(QObject* owner, SchemeFilter allowed)
    -> QWebEngineProfile*;

/// Every request a page can put to the user -- a permission, full screen,
/// the file system, a protocol handler, screen sharing, WebAuthn -- refused.
void RefuseUserRequests(QWebEnginePage* page);

/// Answer @p job with @p data; the bytes are the job's from here.
void ReplyBytes(QWebEngineUrlRequestJob* job, const QByteArray& mime,
                const QByteArray& data);

}  // namespace web_engine
