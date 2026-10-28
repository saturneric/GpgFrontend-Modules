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
#include <QHash>
#include <QList>
#include <QString>
#include <optional>

#include "NoteModel.h"
#include "NoteProtection.h"

/**
 * @file NoteCrypto.h
 * @brief Everything the notes module asks of the Host's OpenPGP, through
 * its ordinary SDK: who a file is encrypted to, what keys are called,
 * encrypting a pack to keys or with a passphrase (which the Host asks for),
 * and decrypting it the way the Host's own Decrypt does. And the ZIP.
 *
 * Encrypt and Decrypt block, and may bring up the Host's passphrase prompt:
 * run them off the GUI thread.
 */

namespace gf::notes {

/// GPG errors a caller tells apart.
enum class CryptoError : uint32_t {
  kNONE = 0,
  kBAD_PASSPHRASE = 11,
  kNO_SECKEY = 17,
  kCANCELED = 99,
  kINV_DATA = 79,
  kNO_PASSPHRASE = 177,
};

struct CryptoResult {
  bool ok = false;
  uint32_t error = 0;  ///< gpg error code; see CryptoError
  QString why;
  QByteArray data;
};

auto CurrentChannel() -> int;

/// Who @p file is encrypted to, and whether a passphrase opens it; nullopt
/// when it is not an OpenPGP encrypted message at all.
auto SniffPackets(int channel, const QByteArray& file)
    -> std::optional<QList<DetectedPacket>>;

/// Facts for @p fingerprints, keyed by fingerprint (upper case).
auto DescribeKeys(int channel, const QStringList& fingerprints)
    -> QHash<QString, KeyFacts>;

/// Encrypt @p plain to @p config, binary: to its keys, every one of them or
/// nothing; or, for a passphrase, with one the Host asks the user for.
auto Encrypt(int channel, const ProtectionConfig& config,
             const QByteArray& plain) -> CryptoResult;

/// Decrypt as the Host's Decrypt does: a key here, or the passphrase, which
/// the Host asks for.
auto Decrypt(int channel, const QByteArray& cipher) -> CryptoResult;

/// The pack as a ZIP, in memory.
auto ZipWrite(const QList<PackEntry>& entries) -> std::optional<QByteArray>;
/// The entries of a ZIP, under @p budgets; nullopt and why on refusal.
auto ZipRead(const QByteArray& zip, const Budgets& budgets, QString* why)
    -> std::optional<QList<PackEntry>>;

}  // namespace gf::notes
