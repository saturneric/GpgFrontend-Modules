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

/**
 * @file ImCodec.h
 * @brief What the module answers the Host's codec calls with, without the SDK
 *        in the way, so it can be tested on its own.
 */
namespace ImCodec {

/// Mirrors gf::cmd::host::CodecOutcome.
enum class Outcome { kNotHandled = 0, kHandled = 1, kFailed = 2 };

struct Answer {
  Outcome outcome = Outcome::kNotHandled;
  QByteArray output;
  QString error;
  QString cards;  ///< info-board card JSON, the Host's result_cards format
};

/**
 * @brief The pre-decrypt decoder: claims @p input only when it is one of our
 *        tokens under the book @p phrase names.
 *
 * Anything else -- armored OpenPGP, ordinary text, a token of another book --
 * is not ours, and the Host decrypts it as it is.
 */
auto DecodeInput(const QByteArray& input, const QString& phrase) -> Answer;

/**
 * @brief The encoder: wraps the binary OpenPGP message @p pgp as a token.
 *
 * Fails, with a message for the user, when the message is too long for the
 * format or the token cannot be made.
 */
auto EncodeOutput(const QByteArray& pgp, const QString& phrase) -> Answer;

/**
 * @brief The phrase as the secure cache holds it.
 *
 * The cache flushes only non-empty values, so "no phrase" cannot be an empty
 * value or clearing it would not survive a restart: a one-character version
 * prefix comes first, and a cleared phrase is the prefix alone. The Host's
 * core stored it the same way, which is what lets the migrated value be read
 * unchanged.
 */
auto EncodePhraseBlob(const QString& phrase) -> QByteArray;
auto DecodePhraseBlob(const QByteArray& blob) -> QString;

}  // namespace ImCodec
