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

#include "GFModule.h"
#include "GFSDKUI.h"

namespace {

// Relative: the Host places them in this module's own settings group, which
// is where they always were ("email/..."), so no account moves.
constexpr auto kSchemaVersionKey = "schema_version";
constexpr auto kAccountsKey = "accounts";
constexpr auto kDefaultAccountKey = "default_account";
constexpr auto kSchemaVersion = 1;

/// A read-modify-write of the account list is several calls; the mutex keeps
/// this module's own threads from interleaving them. No settings object is
/// shared: every call reaches the Host's settings through the storage
/// capability.
Q_GLOBAL_STATIC(QMutex, settings_mutex)

auto Get(const char* key, const QVariant& fallback = {}) -> QVariant {
  return gf::sdk::Setting(GFModuleSdkContext(), GF_SETTING_MODULE, key,
                          fallback);
}

void Put(const char* key, const QVariant& value) {
  gf::sdk::SetSetting(GFModuleSdkContext(), GF_SETTING_MODULE, key, value);
}

}  // namespace

namespace EMailAccountStore {

auto Load() -> QList<MailAccountConfig> { return LoadChecked().accounts; }

auto LoadChecked() -> LoadResult {
  QMutexLocker locker(settings_mutex());

  const auto version = Get(kSchemaVersionKey, 0).toInt();
  if (version > kSchemaVersion) {
    // Written by a newer build. Refusing to read it is the honest answer:
    // silently reinterpreting fields we do not understand could downgrade a
    // transport's security, and writing it back would destroy the newer data.
    LOG_WARN("mail account schema is newer than this build understands");
    return {{}, LoadOutcome::kNEWER};
  }

  const auto raw = Get(kAccountsKey).toString();
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

  // Refused here, not only in the settings page. A newer build may have
  // written a schema this one cannot represent, and stamping kSchemaVersion
  // over it would discard those accounts irreversibly. The page checks too and
  // gives the user a reason; this is what makes the rule hold for any future
  // caller that forgets to.
  if (Get(kSchemaVersionKey, 0).toInt() > kSchemaVersion) {
    LOG_WARN("refusing to overwrite mail accounts written by a newer version");
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

  Put(kSchemaVersionKey, kSchemaVersion);
  Put(kAccountsKey, QString::fromUtf8(
                        QJsonDocument(array).toJson(QJsonDocument::Compact)));
  Put(kDefaultAccountKey, resolved);
}

auto DefaultAccount() -> MailAccountConfig {
  const auto accounts = Load();
  if (accounts.isEmpty()) return {};

  QString default_id;
  {
    QMutexLocker locker(settings_mutex());
    default_id = Get(kDefaultAccountKey).toString();
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
