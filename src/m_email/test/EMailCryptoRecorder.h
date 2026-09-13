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

  /// Makes the next crypto call fail the way the REAL SDK fails: it allocates
  /// its result struct before it can know whether the operation will work, so
  /// a non-zero return still comes back with a live struct -- and, on most of
  /// those paths, with the engine's own account of what went wrong in it. A
  /// caller that only checks the return code leaks the struct and loses the
  /// explanation. See src/sdk/GFSDKGpg.cpp.
  bool fail_next{false};
  /// What the engine "said". Empty models the paths that set no message.
  QByteArray fail_error_string;

  /// What the analysis reports, one entry per verify call, in order.
  ///
  /// A message can carry several signature regions and they do not have to
  /// agree -- a good outer signature over a forged inner one is the whole
  /// reason the aggregation rules exist. Leave it empty for the default single
  /// good-shaped signature; queue entries to give each region its own answer.
  /// Running out falls back to the default rather than failing, so a test only
  /// has to describe the regions it cares about.
  QList<QByteArray> verify_info_json;
};

/// Buffers the fake SDK has handed out and not had back. A result struct that
/// a failure path forgot to reclaim shows up here.
auto OutstandingAllocations() -> int;

void Reset();
auto Get() -> Recording&;

}  // namespace crypto_recorder
