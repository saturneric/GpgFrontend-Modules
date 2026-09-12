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
#include <QList>

/// What the fake crypto SDK was actually handed.
///
/// The whole point of the crypto-boundary target: a signature covers OCTETS,
/// so the only question that matters is whether the bytes the module parsed,
/// sliced, hashed and displayed are the same bytes GPG was given. Every entry
/// point records its inputs verbatim here and the tests compare.
namespace crypto_recorder {

struct VerifyCall {
  QByteArray data;
  QByteArray signature;
};

struct SignCall {
  QByteArray data;
  int sign_mode{};
};

struct EncryptCall {
  QByteArray data;
};

struct DecryptCall {
  QByteArray data;
};

struct Recording {
  QList<VerifyCall> verify;
  QList<SignCall> sign;
  QList<EncryptCall> encrypt;
  QList<DecryptCall> decrypt;

  /// Handed back as the decrypted plaintext. Lets a test drive the inner
  /// re-parse without a real key.
  QByteArray decrypt_output;
  /// Handed back as signed/encrypted output.
  QByteArray sign_output{
      "-----BEGIN PGP SIGNATURE-----\nstub\n"
      "-----END PGP SIGNATURE-----\n"};
  QByteArray encrypt_output{
      "-----BEGIN PGP MESSAGE-----\nstub\n"
      "-----END PGP MESSAGE-----\n"};
};

void Reset();
auto Get() -> Recording&;

}  // namespace crypto_recorder
