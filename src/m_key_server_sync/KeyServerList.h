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

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

/**
 * @brief One configured key server.
 *
 * The capability flags say which protocols the server was observed to speak,
 * so a lookup can be sent somewhere that actually answers it rather than to
 * whichever host happens to be first.
 */
struct KeyServerEntry {
  QString url;
  bool hkp{false};        ///< answers /pks/lookup
  bool vks{false};        ///< answers /vks/v1
  bool verified{false};   ///< a probe confirmed at least one protocol
  QDateTime last_tested;  ///< invalid until the first probe
  QString detail;         ///< why the last probe failed, empty when verified

  [[nodiscard]] auto ToJson() const -> QJsonObject;
  static auto FromJson(const QJsonObject& json) -> KeyServerEntry;
};

namespace KeyServerList {

/**
 * @brief What an operation needs from a server.
 */
enum class Capability {
  kAny,  ///< no protocol requirement, use the default
  kHKP,  ///< /pks/lookup, used for search and import
};

/**
 * @brief Which server to talk to, and which protocol to talk to it with.
 */
struct Route {
  QString url;
  bool vks{false};  ///< speak /vks/v1; speak /pks when false
};

/**
 * @brief Read the configured servers.
 *
 * Seeds the list on first run, so the module behaves the same as it did when
 * the servers were hard-coded.
 *
 * @return QList<KeyServerEntry> never empty
 */
auto Load() -> QList<KeyServerEntry>;

/**
 * @brief Persist the servers and which one is the default.
 *
 * @param entries the full list, in display order
 * @param default_url the entry to prefer; ignored if it is not in @p entries
 */
void Store(const QList<KeyServerEntry>& entries, const QString& default_url);

/**
 * @brief The default server's URL, whatever its capabilities.
 *
 * @return QString
 */
auto DefaultUrl() -> QString;

/**
 * @brief Every configured URL, in display order.
 *
 * @return QStringList
 */
auto Urls() -> QStringList;

/**
 * @brief The server to use for an operation needing @p capability.
 *
 * Prefers the user's default, and only looks past it when that server is known
 * not to speak the protocol — a server nobody has probed yet is still worth
 * trying, since an explicit choice should not be quietly overridden by a gap in
 * our knowledge. Falls back to the default rather than returning nothing: a
 * real network error tells the user far more than an operation that silently
 * does nothing.
 *
 * Only for operations that genuinely cannot be carried out any other way —
 * searching parses HKP output. Publish and refresh work over either protocol
 * and must use @ref SyncRoute instead, so that they stay on the server the
 * user chose.
 *
 * @param capability what the caller is about to do
 * @return QString never empty
 */
auto UrlFor(Capability capability) -> QString;

/**
 * @brief Where publish and refresh should go, and how.
 *
 * Always the user's default server. Both operations can be carried out over
 * either protocol, so there is never a reason to send them somewhere the user
 * did not ask for: VKS is preferred where the server speaks it, HKP is used
 * where it does not. Sending a publish to a different host than the one that
 * was chosen is not something the user can undo afterwards.
 *
 * @return Route url is never empty
 */
auto SyncRoute() -> Route;

}  // namespace KeyServerList
