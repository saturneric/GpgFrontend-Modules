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
                      const QByteArray& body_data, QString& eml_data,
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
                    const QByteArray& body_data, QString& eml_data,
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
                   QString& eml_data, gpgme_error_t& err, QString& capsule_id)
    -> int;

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
                 QString& eml_data, gpgme_error_t& err, QString& capsule_id)
    -> int;

/**
 * @brief Verifies every signature region in a parsed message.
 *
 * VerifyEMLData() checks the message against RFC 3156 and verifies its
 * outermost signature; that stays the primary path and the source of the
 * operation's status. This walks the regions the tree found and verifies each
 * one on ITS OWN bytes, which is the only way a nested or countersigned
 * message reports more than its outer signature.
 *
 * Each result is stamped with the region_id whose bytes produced it, at the
 * call site that passed those bytes in -- results are never matched to regions
 * by position, since one region can yield several signatures or none.
 *
 * Results whose hash algorithm disagrees with the region's declared micalg are
 * flagged rather than reconciled.
 *
 * @param channel GPG context channel
 * @param raw the ORIGINAL message bytes the tree was parsed from
 * @param root the parsed tree
 * @param regions the regions found in @p root
 * @param results receives every signature reported, across all regions
 * @return the number of regions that could be verified, or -1 on a bad argument
 */
auto VerifyEMLRegions(int channel, const QByteArray& raw, const EMailPart& root,
                      const QList<EMailSignatureRegion>& regions,
                      QList<EMailSignatureResult>& results) -> int;

/**
 * @brief
 *
 * @param data
 * @param error_string
 * @return int
 */
auto VerifyEMLData(int channel, const QByteArray& data,
                   EMailMetaData& meta_data, QString& error_string,
                   gpgme_error_t& err, QString& capsule_id) -> int;

/**
 * @brief
 *
 * @param data
 * @param error_string
 * @return int
 */
auto DecryptEMLData(int channel, const QByteArray& data,
                    EMailMetaData& meta_data, QString& eml_data,
                    gpgme_error_t& err, QString& capsule_id) -> int;