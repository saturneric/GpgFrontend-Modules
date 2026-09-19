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

#include "EMailCredentialStore.h"

#include "GFModule.h"
#include "GFSDKBasic.h"

namespace {

/// Credentials are keyed by account id, not by host or username: re-hosting an
/// account or correcting its username must not orphan the stored secret.
auto CredentialKey(const QString& account_id) -> QString {
  return QString("mail/cred/%1").arg(account_id);
}

}  // namespace

namespace EMailCredentialStore {

auto CurrentProtection() -> ProtectionLevel {
  switch (GFAppKeyProtectionLevel()) {
    case 0:
      return ProtectionLevel::kNONE;
    case 1:
      return ProtectionLevel::kKEYCHAIN;
    case 2:
      return ProtectionLevel::kPIN;
    default:
      return ProtectionLevel::kUNKNOWN;
  }
}

auto MayPersistSilently() -> bool {
  const auto protection = CurrentProtection();

  // kUNKNOWN is treated as kNONE rather than given the benefit of the doubt:
  // if we cannot establish that the key is protected, we have to assume it is
  // not. Guessing the other way stores a password on a promise nobody made.
  return protection == ProtectionLevel::kKEYCHAIN ||
         protection == ProtectionLevel::kPIN;
}

auto Save(const QString& account_id, const EMailSecret& password) -> bool {
  if (account_id.isEmpty()) return false;

  // Storing is not optional: there is no password prompt anywhere, so an
  // account without a stored password simply cannot be used. Refusing here
  // would not protect anyone, it would just break the feature.
  //
  // The protection level still matters though, so it is recorded rather than
  // shown. Everything here is encrypted under the application secure key, and
  // when that key is itself unprotected this is protection in name only --
  // worth having in a log when someone is working out how exposed a profile
  // is, even though it is not worth interrupting them over.
  if (!MayPersistSilently()) {
    LOG_WARN(
        "storing a mail credential while the application key is unprotected; "
        "it is only as protected as the profile itself");
  }

  //
  // Both arguments are handed over owned, from the allocators the SDK will
  // release them through: the key ordinary, the secret secure. Passing a
  // QByteArray's internal pointer here would have the SDK free memory Qt owns.
  auto* secret = password.ToSecureCString();
  if (secret == nullptr) {
    LOG_ERROR("could not allocate a secure buffer for a mail credential");
    return false;
  }

  const auto result = GFSecDurableCacheSave(
      (CredentialKey(account_id)).toUtf8().constData(), secret);

  if (result != 0) LOG_ERROR("failed to store mail credential");
  return result == 0;
}

auto Load(const QString& account_id) -> EMailSecretPtr {
  if (account_id.isEmpty()) return std::make_shared<EMailSecret>();

  auto* raw =
      GFSecDurableCacheGet((CredentialKey(account_id)).toUtf8().constData());
  if (raw == nullptr) return std::make_shared<EMailSecret>();

  // AdoptCString copies the bytes out, wipes the SDK's buffer and frees it to
  // the secure allocator it came from, so the secret exists in exactly one
  // place from here on -- and that place can actually be erased, which a
  // QString could not be. See EMailSecret.
  return EMailSecret::AdoptCString(raw);
}

auto Has(const QString& account_id) -> bool {
  return !Load(account_id)->IsEmpty();
}

void Remove(const QString& account_id) {
  if (account_id.isEmpty()) return;
  GFSecDurableCacheRemove((CredentialKey(account_id)).toUtf8().constData());
}

}  // namespace EMailCredentialStore
