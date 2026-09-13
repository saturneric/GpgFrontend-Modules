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

#include <QString>

#include "EMailSecret.h"

/**
 * @brief Where mail passwords live, and whether they may live there at all.
 *
 * Passwords go to the durable cache's secure tier, which is encrypted at rest
 * under the application secure key. That makes the store exactly as strong as
 * whatever protects that key -- so this is also the one place that decides
 * whether persisting a password is acceptable in the first place.
 *
 * Nothing here ever touches QSettings. An account record carries only its id.
 */
namespace EMailCredentialStore {

/**
 * @brief How well anything stored here would actually be protected.
 */
enum class ProtectionLevel : uint8_t {
  kUNKNOWN = 0,  ///< the host did not say; treat as unprotected
  kNONE,         ///< the app key is plaintext on disk
  kKEYCHAIN,     ///< the app key is sealed with the system credential store
  kPIN,          ///< the app key is sealed with a PIN the user types
};

/**
 * @brief Read the current protection level from the host.
 */
auto CurrentProtection() -> ProtectionLevel;

/**
 * @brief Whether a password may be persisted without the user insisting.
 *
 * False when the application secure key is itself unprotected: writing a mail
 * password there would put it behind encryption whose key sits in the clear
 * beside it, which is protection in name only. The user may still choose to do
 * it (@ref Save honours the request), but it must be their explicit decision
 * and must not be the default.
 */
auto MayPersistSilently() -> bool;

/**
 * @brief Store a password for an account.
 *
 * Takes the secret itself rather than a QString: a QString cannot be erased
 * once anything shares its buffer, so rendering a password back into one on
 * the way to storage undoes the reason EMailSecret exists.
 *
 * @param account_id stable account id
 * @param password the secret; the caller keeps ownership
 * @return true on success
 */
auto Save(const QString& account_id, const EMailSecret& password) -> bool;

/**
 * @brief Read a stored password.
 *
 * @param account_id stable account id
 * @return the password, or an empty string when none is stored
 */
auto Load(const QString& account_id) -> EMailSecretPtr;

/**
 * @brief Whether a password is stored for an account.
 */
auto Has(const QString& account_id) -> bool;

/**
 * @brief Forget an account's password.
 *
 * Called when the account is deleted and when the user turns remembering off,
 * so a secret never outlives the reason it was kept.
 *
 * @param account_id stable account id
 */
void Remove(const QString& account_id);

}  // namespace EMailCredentialStore
