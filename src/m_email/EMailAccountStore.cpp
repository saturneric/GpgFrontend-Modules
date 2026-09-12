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

#include "EMailAccountStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QSettings>
#include <QUuid>

#include "GFModuleCommonUtils.hpp"
#include "GFSDKUI.h"

namespace {

constexpr auto kSchemaVersionKey = "email/schema_version";
constexpr auto kAccountsKey = "email/accounts";
constexpr auto kDefaultAccountKey = "email/default_account";
constexpr auto kSchemaVersion = 1;

/// The settings object is shared with the host, and QSettings is only
/// reentrant, so every read-modify-write of our keys has to be serialized.
Q_GLOBAL_STATIC(QMutex, settings_mutex)

auto GlobalSettings() -> QSettings* {
  return qobject_cast<QSettings*>(static_cast<QObject*>(GFUIGlobalSettings()));
}

}  // namespace

namespace EMailAccountStore {

auto Load() -> QList<MailAccountConfig> { return LoadChecked().accounts; }

auto LoadChecked() -> LoadResult {
  QMutexLocker locker(settings_mutex());

  auto* settings = GlobalSettings();
  if (settings == nullptr) {
    LOG_ERROR("global settings unavailable, no mail accounts loaded");
    // Not kOK: nothing was read, so nothing may be written back over whatever
    // is actually there.
    return {{}, LoadOutcome::kUNREADABLE};
  }

  // The host writes through its own short-lived QSettings objects, and this one
  // outlives them all; without a sync it would keep serving whatever it read at
  // startup.
  settings->sync();

  const auto version = settings->value(kSchemaVersionKey, 0).toInt();
  if (version > kSchemaVersion) {
    // Written by a newer build. Refusing to read it is the honest answer:
    // silently reinterpreting fields we do not understand could downgrade a
    // transport's security, and writing it back would destroy the newer data.
    LOG_WARN("mail account schema is newer than this build understands");
    return {{}, LoadOutcome::kNEWER};
  }

  const auto raw = settings->value(kAccountsKey).toString();
  const auto document = QJsonDocument::fromJson(raw.toUtf8());
  if (!document.isArray()) {
    // An absent value is not a corrupt one: a profile that has never had a
    // mail account has no key here, and that must stay writable.
    if (raw.trimmed().isEmpty()) return {};
    LOG_WARN("mail account list is not readable as this schema");
    return {{}, LoadOutcome::kUNREADABLE};
  }

  QList<MailAccountConfig> accounts;
  for (const auto value : document.array()) {
    if (!value.isObject()) continue;
    auto account = MailAccountConfig::FromJson(value.toObject());
    if (account.id.isEmpty()) continue;
    accounts.append(account);
  }
  return {accounts, LoadOutcome::kOK};
}

void Store(const QList<MailAccountConfig>& accounts,
           const QString& default_id) {
  QMutexLocker locker(settings_mutex());

  auto* settings = GlobalSettings();

  // Refused here, not only in the settings page. A newer build may have
  // written a schema this one cannot represent, and stamping kSchemaVersion
  // over it would discard those accounts irreversibly. The page checks too and
  // gives the user a reason; this is what makes the rule hold for any future
  // caller that forgets to.
  if (settings != nullptr &&
      settings->value(kSchemaVersionKey, 0).toInt() > kSchemaVersion) {
    LOG_WARN("refusing to overwrite mail accounts written by a newer version");
    return;
  }
  if (settings == nullptr) {
    LOG_ERROR("global settings unavailable, mail accounts not stored");
    return;
  }

  QJsonArray array;
  QStringList ids;
  for (const auto& account : accounts) {
    if (account.id.isEmpty()) continue;
    array.append(account.ToJson());
    ids.append(account.id);
  }

  // A default naming an account that is no longer listed would leave every
  // operation pointing at something the user thought they had removed.
  auto resolved = default_id;
  if (!ids.contains(resolved))
    resolved = ids.isEmpty() ? QString() : ids.first();

  settings->setValue(kSchemaVersionKey, kSchemaVersion);
  settings->setValue(
      kAccountsKey,
      QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
  settings->setValue(kDefaultAccountKey, resolved);

  // Our QSettings is a different object from the host's, so without this the
  // host would keep reading the previous values.
  settings->sync();
}

auto DefaultAccount() -> MailAccountConfig {
  const auto accounts = Load();
  if (accounts.isEmpty()) return {};

  QString default_id;
  {
    QMutexLocker locker(settings_mutex());
    if (auto* settings = GlobalSettings(); settings != nullptr) {
      default_id = settings->value(kDefaultAccountKey).toString();
    }
  }

  for (const auto& account : accounts) {
    if (account.id == default_id) return account;
  }
  for (const auto& account : accounts) {
    if (account.IsUsable()) return account;
  }
  return accounts.first();
}

auto FindById(const QString& id, MailAccountConfig& out) -> bool {
  if (id.isEmpty()) return false;
  for (const auto& account : Load()) {
    if (account.id != id) continue;
    out = account;
    return true;
  }
  return false;
}

auto ImapAccounts() -> QList<MailAccountConfig> {
  QList<MailAccountConfig> result;
  for (const auto& account : Load()) {
    if (account.imap.enabled && account.IsUsable()) result.append(account);
  }
  return result;
}

auto SmtpAccounts() -> QList<MailAccountConfig> {
  QList<MailAccountConfig> result;
  for (const auto& account : Load()) {
    if (account.smtp.enabled && account.IsUsable()) result.append(account);
  }
  return result;
}

auto NewAccountId() -> QString {
  return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

}  // namespace EMailAccountStore
