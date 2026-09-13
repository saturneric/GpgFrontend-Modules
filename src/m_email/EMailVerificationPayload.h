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

#include "EMailModel.h"

/**
 * @file
 * @brief The wire form of a verification result.
 *
 * A verification is performed in one place and rendered in another, with the
 * host between them, so the result has to travel. This is the ONLY contract it
 * travels under -- deliberately its own thing rather than "whatever the struct
 * happens to contain today", so that rearranging EMailVerificationResult
 * cannot silently change what two sides of an event agree on.
 *
 * Nothing here is a raw cast of an enum. Every enumerated value is written as
 * a stable name and read back through the same table, because the numbering
 * changes whenever a state is inserted in severity order -- as kSIGNED_ERROR
 * just was -- and a payload written by one build must never be read as a
 * different verdict by another.
 */

/// Bumped when the meaning of any field changes. A payload announcing a
/// version this build does not know is refused, not guessed at.
constexpr int kEMailVerificationPayloadVersion = 1;

/**
 * @brief Serializes @p result for the trip back to the page.
 *
 * Carries what a consumer renders from: the summary, the regions, the
 * per-region verdicts, the signatures, the headers, and the source anchor the
 * staleness check needs. The engine's report text is NOT carried: it is the
 * status report's material, and the page does not render it.
 */
auto EncodeVerificationPayload(const EMailVerificationResult& result)
    -> QByteArray;

/**
 * @brief Reads a payload back.
 *
 * Refuses rather than guesses: an unknown schema version, an unknown enum
 * name, or malformed JSON all leave @p result untouched and return false. A
 * partially understood verdict is worse than no verdict, because the surface
 * would present it as though it were complete.
 *
 * @return true when @p result was filled from a payload this build fully
 *         understands.
 */
auto DecodeVerificationPayload(const QByteArray& payload,
                               EMailVerificationResult& result) -> bool;

/**
 * @brief Whether two results say the same thing.
 *
 * Semantic, not bitwise: list order within a region, the precision a timestamp
 * survived a round trip with, and the report material a payload deliberately
 * does not carry are all differences that do not change what either result
 * claims. Exists for the tests that pin the round trip, and for anyone
 * comparing a fresh answer against a cached one.
 */
auto VerificationResultsEquivalent(const EMailVerificationResult& a,
                                   const EMailVerificationResult& b) -> bool;
