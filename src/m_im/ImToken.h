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
#include <QString>
#include <cstddef>

/**
 * @file ImToken.h
 *
 * @brief Compact, instant-messaging-friendly container for an OpenPGP message,
 * emitted as a single Base58 word so it survives pasting into any chat app.
 *
 * A normal ASCII-armored PGP message is verbose and multi-line; pasted into a
 * messenger its boilerplate, line-wraps and CRC footer get mangled. This codec
 * emits one unbreakable Base58 word (the Bitcoin/IPFS alphabet, no ambiguous
 * 0 O I l and nothing a messenger turns into markdown, a link or a soft-wrap).
 *
 * There is deliberately no marker, prefix or magic on the wire: the whole
 * container is "whitened" with the shared password book. A per-message random
 * seed and the book derive — via a memory-hard KDF — a set of secret bytes;
 * part of them re-shuffles the byte order, the rest XORs the payload, and a
 * secret-controlled amount of random padding is interleaved to hide the true
 * length. The GpgFrontend recognition tag is a secret check value derived from
 * the book (not a fixed constant), so there is no known-plaintext to search
 * for; it lives inside this whitened blob, invisible on the wire and revealed
 * only after a receiver un-whitens with the same book.
 *
 * The consequence: without the book, a token is indistinguishable from random
 * Base58. Even *detecting* whether a token is one of ours costs the adversary a
 * memory-hard derivation per candidate book, since there is no cheap
 * pre-filter.
 *
 * Decode() does apply a shape test before that derivation — length bounds and
 * Base58 alphabet membership — but only over information an observer already
 * has for free, so it does not lower the adversary's per-candidate cost. It
 * exists so that ordinary text a user hits Decrypt on stops at its first space
 * or punctuation mark rather than paying the KDF.
 *
 * The shared "password book" is a 256-byte table derived from an optional
 * phrase (Argon2id) or a fixed default. The empty-phrase default only hides the
 * format from naive scanners; real indistinguishability against a GpgFrontend-
 * aware adversary requires both sides to share a secret phrase.
 */
namespace ImToken {

/// The result of decoding a token.
struct DecodeResult {
  bool ok{false};          ///< the token was one of ours and un-whitened
  QByteArray pgp_message;  ///< the wrapped OpenPGP message to decrypt
};

/**
 * @brief Wrap an already-OpenPGP-encrypted message as one whitened Base58
 * token, with the book @p phrase names (empty: the default book).
 *
 * @return the Base58 token, or empty on failure
 */
auto Encode(const QByteArray& pgp_message, const QString& phrase) -> QString;

/**
 * @brief Un-whiten @p text with the book @p phrase names and recover the
 * wrapped OpenPGP message. @c ok is false when @p text is not one of our
 * tokens (plain chat, or a different book).
 */
auto Decode(const QString& text, const QString& phrase) -> DecodeResult;

/// The current container format version (the inner tag's version).
auto FormatVersion() -> int;

/**
 * @brief The largest OpenPGP message Encode() will wrap, in bytes.
 *
 * Derived from the token-length bound Decode() enforces, so any token this
 * codec produces is one this codec accepts.
 */
auto MaxPayloadBytes() -> int;

/**
 * @brief A fresh book phrase from the platform CSPRNG: 256 characters over the
 * Base58 alphabet, or empty if the CSPRNG was unavailable.
 */
auto GeneratePhrase() -> QString;

/**
 * @brief A short, domain-separated digest (e.g. "3F9A-1C4E") of the book
 * @p phrase produces, for peers to confirm by eye that they share a book.
 *
 * @warning Identifies the book: anyone who learns it can test candidate
 * phrases offline. Display it locally; never put it on the wire. Costs one
 * Argon2id (64 MiB) per distinct phrase; call it off the UI thread.
 */
auto BookFingerprintOf(const QString& phrase) -> QString;

/// The per-message Argon2id cost, as (ops, memory in bytes).
struct KdfCost {
  unsigned long long ops;
  size_t mem;
};

/// The per-message Argon2id cost the format ships with.
auto DefaultKdfCost() -> KdfCost;

/// The per-message Argon2id cost currently in effect.
auto CurrentKdfCost() -> KdfCost;

/**
 * @brief Override the per-message Argon2id cost. **Tests only.**
 *
 * The cost is a protocol constant rather than something carried in the token,
 * so Encode() and Decode() must agree on it, and it is the feature's whole
 * security argument: lowering it anywhere but a test removes the protection.
 */
void SetKdfCostForTesting(KdfCost cost);

}  // namespace ImToken
