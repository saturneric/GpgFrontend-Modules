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

#include "KeyServerList.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>

#include "GFModule.h"
#include "GFSDKUI.h"

namespace {

// Relative: the Host places them in this module's own settings group, which
// is where they always were ("key_server_sync/..."), so nothing moves.
constexpr auto kSchemaVersionKey = "schema_version";
constexpr auto kServersKey = "servers";
constexpr auto kDefaultServerKey = "default_server";
constexpr auto kSchemaVersion = 1;

constexpr auto kOpenPGPServer = "https://keys.openpgp.org";

/// A read-modify-write of the list is two calls; the mutex keeps two of this
/// module's own threads from interleaving them. Every call reaches the Host's
/// settings through the storage capability -- no settings object is shared.
Q_GLOBAL_STATIC(QMutex, settings_mutex)

auto Get(const char* key) -> QVariant {
  return gf::sdk::Setting(GFModuleSdkContext(), GF_SETTING_MODULE, key);
}

void Put(const char* key, const QVariant& value) {
  gf::sdk::SetSetting(GFModuleSdkContext(), GF_SETTING_MODULE, key, value);
}

/**
 * @brief The list to start from when nothing has been configured yet.
 *
 * These are the servers the module used to hard-code, with the capabilities we
 * already know them to have, so behaviour on the first run is unchanged. They
 * are left unverified regardless: claiming a probe happened when none did would
 * be a lie the user cannot check.
 */
auto SeedEntries() -> QList<KeyServerEntry> {
  QList<KeyServerEntry> entries;

  KeyServerEntry openpgp;
  openpgp.url = kOpenPGPServer;
  openpgp.hkp = true;
  openpgp.vks = true;
  entries.append(openpgp);

  KeyServerEntry ubuntu;
  ubuntu.url = "https://keyserver.ubuntu.com";
  ubuntu.hkp = true;
  entries.append(ubuntu);

  return entries;
}

/**
 * @brief The stored default, checked against @p entries.
 *
 * Takes the list it should agree with rather than reading it again, so a caller
 * that already has the entries cannot end up reconciling against a different
 * snapshot than the one it is about to use.
 */
auto ResolveDefaultUrl(const QList<KeyServerEntry>& entries) -> QString {
  QString stored;
  {
    QMutexLocker locker(settings_mutex());
    stored = Get(kDefaultServerKey).toString();
  }

  const auto listed = std::any_of(
      entries.cbegin(), entries.cend(),
      [&stored](const KeyServerEntry& e) { return e.url == stored; });
  if (listed) return stored;

  // The stored default was removed, or nothing was ever stored.
  return entries.isEmpty() ? QString(kOpenPGPServer) : entries.first().url;
}

auto HasCapability(const KeyServerEntry& entry,
                   KeyServerList::Capability capability) -> bool {
  switch (capability) {
    case KeyServerList::Capability::kHKP:
      return entry.hkp;
    case KeyServerList::Capability::kAny:
      return true;
  }
  return true;
}

}  // namespace

auto KeyServerEntry::ToJson() const -> QJsonObject {
  return QJsonObject{
      {"url", url},
      {"hkp", hkp},
      {"vks", vks},
      {"state",
       verified ? QStringLiteral("verified") : QStringLiteral("unverified")},
      {"last_tested", last_tested.isValid()
                          ? last_tested.toUTC().toString(Qt::ISODate)
                          : QString()},
      {"detail", detail},
  };
}

auto KeyServerEntry::FromJson(const QJsonObject& json) -> KeyServerEntry {
  KeyServerEntry entry;
  entry.url = json.value("url").toString().trimmed();
  entry.hkp = json.value("hkp").toBool();
  entry.vks = json.value("vks").toBool();
  entry.verified = json.value("state").toString() == "verified";
  entry.detail = json.value("detail").toString();

  const auto last_tested = json.value("last_tested").toString();
  if (!last_tested.isEmpty()) {
    entry.last_tested = QDateTime::fromString(last_tested, Qt::ISODate);
  }

  return entry;
}

namespace KeyServerList {

auto Load() -> QList<KeyServerEntry> {
  QMutexLocker locker(settings_mutex());

  const auto stored = Get(kServersKey);
  if (!stored.isValid()) {
    locker.unlock();
    auto seed = SeedEntries();
    Store(seed, kOpenPGPServer);
    return seed;
  }

  const auto document =
      QJsonDocument::fromJson(stored.toString().toUtf8());

  QList<KeyServerEntry> entries;
  if (document.isArray()) {
    for (const auto value : document.array()) {
      if (!value.isObject()) continue;
      auto entry = KeyServerEntry::FromJson(value.toObject());
      if (entry.url.isEmpty()) continue;
      entries.append(entry);
    }
  }

  // An empty or corrupted list would leave every key server operation with
  // nowhere to go, so fall back rather than hand back nothing.
  if (entries.isEmpty()) {
    LOG_DEBUG("stored key server list is empty or unreadable, reseeding");
    locker.unlock();
    auto seed = SeedEntries();
    Store(seed, kOpenPGPServer);
    return seed;
  }

  return entries;
}

void Store(const QList<KeyServerEntry>& entries, const QString& default_url) {
  QMutexLocker locker(settings_mutex());

  QJsonArray array;
  for (const auto& entry : entries) {
    if (entry.url.isEmpty()) continue;
    array.append(entry.ToJson());
  }

  // A default naming a server that is no longer listed would silently send
  // every operation to a host the user thought they had removed.
  auto resolved_default = default_url;
  const auto listed = std::any_of(entries.cbegin(), entries.cend(),
                                  [&resolved_default](const KeyServerEntry& e) {
                                    return e.url == resolved_default;
                                  });
  if (!listed) {
    resolved_default = entries.isEmpty() ? QString() : entries.first().url;
  }

  Put(kSchemaVersionKey, kSchemaVersion);
  Put(kServersKey, QString::fromUtf8(
                       QJsonDocument(array).toJson(QJsonDocument::Compact)));
  Put(kDefaultServerKey, resolved_default);
}

auto DefaultUrl() -> QString { return ResolveDefaultUrl(Load()); }

auto Urls() -> QStringList {
  QStringList urls;
  for (const auto& entry : Load()) urls << entry.url;
  return urls;
}

auto UrlFor(Capability capability) -> QString {
  const auto entries = Load();
  const auto default_url = ResolveDefaultUrl(entries);

  if (capability == Capability::kAny) return default_url;

  const auto find = [&entries](const auto& predicate) -> QString {
    const auto it = std::find_if(entries.cbegin(), entries.cend(), predicate);
    return it == entries.cend() ? QString() : it->url;
  };

  // The user's own choice comes first, and an unverified server still counts:
  // not having probed it yet is our gap, not a reason to route around them.
  for (const auto& entry : entries) {
    if (entry.url != default_url) continue;
    if (HasCapability(entry, capability) || !entry.verified) return entry.url;
    break;
  }

  auto url = find([capability](const KeyServerEntry& e) {
    return HasCapability(e, capability);
  });
  if (!url.isEmpty()) return url;

  url = find([](const KeyServerEntry& e) { return !e.verified; });
  if (!url.isEmpty()) return url;

  // Nothing matched. Going ahead with the default surfaces a real error the
  // user can act on, where doing nothing would just look broken.
  return default_url;
}

auto SyncRoute() -> Route {
  const auto entries = Load();
  const auto default_url = ResolveDefaultUrl(entries);

  for (const auto& entry : entries) {
    if (entry.url != default_url) continue;

    // VKS first where the server has it: it confirms the address by email and
    // keeps third-party signatures out. HKP is the fallback, not the
    // preference -- but it does carry both operations, so a server that only
    // speaks HKP is a reason to change protocol, never a reason to go
    // somewhere else.
    if (entry.vks) return {entry.url, true};
    if (entry.hkp) return {entry.url, false};

    // Never successfully probed, so nothing is known either way. Try the
    // better protocol rather than assume the worse one; a failure here names
    // the server the user picked, which is something they can act on.
    return {entry.url, true};
  }

  // Only reachable when settings are unavailable, in which case
  // ResolveDefaultUrl names a server that does speak VKS.
  return {default_url, true};
}

}  // namespace KeyServerList
