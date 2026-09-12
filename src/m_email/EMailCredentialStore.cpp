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

#include "GFModuleCommonUtils.hpp"
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

auto Save(const QString& account_id, const QString& password) -> bool {
  if (account_id.isEmpty()) return false;

  // Deliberately no MayPersistSilently() check here. This function does what
  // it is told; deciding whether to call it is the caller's job, because only
  // the caller knows whether the user was asked. Refusing here would mean an
  // informed user could not opt in at all.
  //
  // Both arguments are handed over owned, from the allocators the SDK will
  // release them through: the key ordinary, the secret secure. Passing a
  // QByteArray's internal pointer here would have the SDK free memory Qt owns.
  const auto result =
      GFSecDurableCacheSave(QDUP(CredentialKey(account_id)), QSECDUP(password));

  if (result != 0) LOG_ERROR("failed to store mail credential");
  return result == 0;
}

auto Load(const QString& account_id) -> QString {
  if (account_id.isEmpty()) return {};

  auto* raw = GFSecDurableCacheGet(QDUP(CredentialKey(account_id)));
  if (raw == nullptr) return {};

  // UnSecStrDup takes ownership and wipes the SDK's buffer, so the secret
  // exists in exactly one place from here on -- the QString we return, which
  // the caller is expected to clear once it has been handed to the transport.
  return UnSecStrDup(raw);
}

auto Has(const QString& account_id) -> bool {
  auto password = Load(account_id);
  const auto present = !password.isEmpty();
  password.fill(QChar('\0'));
  return present;
}

void Remove(const QString& account_id) {
  if (account_id.isEmpty()) return;
  GFSecDurableCacheRemove(QDUP(CredentialKey(account_id)));
}

}  // namespace EMailCredentialStore
