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

//
#include "GFSDKGpg.h"

enum EMailGpgOperaResult : int8_t {
  kSUCCESS = 0,
  kFAILED = -1,
  kEML_FAILED = -2,
  kGPG_FAILED = -3,
};

/**
 * @brief
 *
 * @param channel
 * @param keys
 * @param meta_data
 * @param body_data
 * @param eml_data
 * @return int
 */
auto EncryptPlainText(int channel, const QStringList& keys,
                      const EMailMetaData& meta_data,
                      const QByteArray& body_data, QByteArray& eml_data,
                      gpgme_error_t& err, QString& capsule_id) -> int;

/**
 * @brief
 *
 * @param channel
 * @param keys
 * @param message
 * @param eml_data
 * @return int
 */
auto EncryptEMLData(int channel, const QStringList& keys,
                    const vmime::shared_ptr<vmime::message>& message,
                    const QByteArray& body_data, QByteArray& eml_data,
                    gpgme_error_t& err, QString& capsule_id) -> int;

/**
 * @brief
 *
 * @param channel
 * @param key
 * @param meta_data
 * @param body_data
 * @param eml_data
 * @return int
 */
auto SignPlainText(int channel, const QString& key,
                   const EMailMetaData& meta_data, const QByteArray& body_data,
                   QByteArray& eml_data, gpgme_error_t& err,
                   QString& capsule_id) -> int;

/**
 * @brief
 *
 * @param channel
 * @param key
 * @param message
 * @param eml_data
 * @return int
 */
auto SignEMLData(int channel, const QString& key,
                 const vmime::shared_ptr<vmime::message>& message,
                 QByteArray& eml_data, gpgme_error_t& err, QString& capsule_id)
    -> int;

/**
 * @brief Verifies a message. The only verification there is.
 *
 * Replaces the pair this used to be -- an RFC 3156 check of the outermost
 * signature that produced the operation's status, and a separate per-region
 * walk that produced the security surface's state. Two verifiers over two
 * byte sequences meant the same message could be reported as verified in one
 * place and forged in another, which is precisely what happened.
 *
 * One pass, one answer: parses @p raw once, walks EVERY signature region the
 * tree yields, verifies each on its own byte slice, and fills @p out with the
 * per-region verdicts, every signature reported, the message's headers, and
 * the one aggregated summary every consumer quotes.
 *
 * The bytes are judged AS THEY STAND. Each region is handed to the engine as
 * the exact slice of @p raw it occupies -- never canonicalized, re-encoded or
 * repaired first. Bytes whose line endings were rewritten after signing
 * cannot verify, and a verifier that quietly repaired them would report a
 * good signature over a document that does not have one. The rewrite is
 * recorded instead, as EMailRegionVerdict::signed_bytes_non_canonical, so the
 * failure can be EXPLAINED without being excused.
 *
 * Each signature is stamped with the region_id whose bytes produced it -- they
 * are never matched to regions by position, since one region can yield several
 * or none -- and results whose hash algorithm disagrees with the region's
 * declared micalg are flagged rather than reconciled.
 *
 * @param channel GPG context channel
 * @param raw the message, exactly as it is to be judged
 * @param out receives everything this pass found
 * @return kSUCCESS, or one of the EMailGpgOperaResult failures
 */
auto VerifyEMLMessage(int channel, const QByteArray& raw,
                      EMailVerificationResult& out, QString& error_string)
    -> int;

/**
 * @brief
 *
 * @param data
 * @param error_string
 * @return int
 */
auto DecryptEMLData(int channel, const QByteArray& data,
                    EMailMetaData& meta_data, QByteArray& eml_data,
                    gpgme_error_t& err, QString& capsule_id) -> int;