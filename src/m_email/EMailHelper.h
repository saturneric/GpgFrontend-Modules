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

#include <QDateTime>
#include <QString>

#include "EMailModel.h"

auto inline Q_SC(const std::string& s) -> QString {
  return QString::fromStdString(s);
}

/**
 * @brief
 *
 * @param prm_micalg_value
 * @return true
 * @return false
 */
auto IsValidMicalgFormat(const QString& prm_micalg_value) -> bool;

/**
 * @brief
 *
 * @param header
 * @param field_name
 * @return QString
 */
auto ExtractFieldValue(const vmime::shared_ptr<vmime::header>& header,
                       const QString& field_name) -> QString;

/**
 * @brief
 *
 * @param header
 * @param field_name
 * @return QString
 */
auto ExtractFieldValueText(const vmime::shared_ptr<vmime::header>& header,
                           const QString& field_name) -> QString;

/**
 * @brief
 *
 * @param header
 * @param field_name
 * @return QString
 */
auto ExtractFieldValueMailBox(const vmime::shared_ptr<vmime::header>& header,
                              const QString& field_name) -> QString;

/**
 * @brief
 *
 * @param header
 * @param field_name
 * @return QString
 */
auto ExtractFieldValueAddressList(
    const vmime::shared_ptr<vmime::header>& header,
    const QString& field_name) -> QString;

/**
 * @brief
 *
 * @param header
 * @param field_name
 * @return QDateTime
 */
auto ExtractFieldValueDateTime(const vmime::shared_ptr<vmime::header>& header,
                               const QString& field_name) -> QDateTime;

/**
 * @brief
 *
 * @param input
 * @param name
 * @param email
 * @return true
 * @return false
 */
auto ParseEmailString(const QString& input, QString& name,
                      QString& email) -> bool;

/**
 * @brief
 *
 * @param data
 * @param lineLength
 * @return QString
 */
auto EncodeBase64WithLineBreaks(const QByteArray& data,
                                int lineLength = 76) -> QString;

/**
 * @brief
 *
 * @param data
 * @return true
 * @return false
 */
auto CheckIfEMLMessage(const QByteArray& data,
                       vmime::shared_ptr<vmime::message>& message) -> bool;

/**
 * @brief
 *
 * @param meta_data
 * @param eml_data
 * @return int
 */
auto BuildPlainTextEML(const EMailMetaData& meta_data,
                       const QByteArray& body_data, QString& eml_data) -> int;

/**
 * @brief
 *
 * @param body_data
 * @param meta_data
 * @return int
 */
auto GetEMLMetaData(vmime::shared_ptr<vmime::message>& message,
                    EMailMetaData& meta_data) -> int;