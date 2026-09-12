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

#include <QList>
#include <QString>
#include <cstdint>

#include "EMailAccountModel.h"

/**
 * @brief The configured mail accounts, as stored in application settings.
 *
 * Mirrors KeyServerList: the list is one JSON string in the app-wide QSettings
 * beside a schema version, every access is serialized, and the store is read
 * and written whole rather than field by field.
 *
 * Credentials are deliberately not here. See EMailCredentialStore.
 */
namespace EMailAccountStore {

/// Why Load() came back with nothing, when it did.
enum class LoadOutcome : uint8_t {
  kOK = 0,      ///< the list was read; it may still be empty
  kNEWER,       ///< written by a newer build and deliberately not read
  kUNREADABLE,  ///< present but not parsable as this schema
};

/// An account list together with whether it is the whole truth.
struct LoadResult {
  QList<MailAccountConfig> accounts;
  LoadOutcome outcome{LoadOutcome::kOK};

  /// Whether these accounts may be written back.
  ///
  /// They may not when the load was refused: an empty list stored over a
  /// newer-schema one destroys settings this build could not read, which is
  /// the very thing refusing to read them was meant to prevent.
  [[nodiscard]] auto MayStore() const -> bool {
    return outcome == LoadOutcome::kOK;
  }
};

/**
 * @brief Read the configured accounts.
 *
 * Never throws and never fails: an unreadable or corrupt list reads as no
 * accounts, which disables the features rather than breaking the application.
 *
 * @return the accounts in display order, possibly empty
 */
auto Load() -> QList<MailAccountConfig>;

/**
 * @brief Read the configured accounts, and say whether they are complete.
 *
 * The same read as Load(), for callers that may WRITE the result back. An
 * empty list means "there are no accounts" and "this build refused to read
 * them" indistinguishably, and storing the second one over a newer build's
 * settings erases them.
 */
auto LoadChecked() -> LoadResult;

/**
 * @brief Persist the accounts and which one is preferred.
 *
 * @param accounts the full list, in display order
 * @param default_id account to prefer; ignored when it is not in @p accounts
 */
void Store(const QList<MailAccountConfig>& accounts, const QString& default_id);

/**
 * @brief The account to offer first.
 *
 * @return the default account, or the first usable one, or a default-
 *         constructed account when nothing is configured
 */
auto DefaultAccount() -> MailAccountConfig;

/**
 * @brief Look one account up by its stable id.
 *
 * @param id account id
 * @param out receives the account when found
 * @return true when @p id named a configured account
 */
auto FindById(const QString& id, MailAccountConfig& out) -> bool;

/// Accounts that can be browsed. Empty means "hide Import from IMAP".
auto ImapAccounts() -> QList<MailAccountConfig>;

/// Accounts that can send. Empty means "hide Send via SMTP".
auto SmtpAccounts() -> QList<MailAccountConfig>;

/**
 * @brief Mint an id for a new account.
 *
 * @return a fresh, stable identifier
 */
auto NewAccountId() -> QString;

}  // namespace EMailAccountStore
