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
 * @brief
 *
 * @param channel
 * @param keys
 * @param meta_data
 * @param body_data
 * @param eml_data
 * @return int
 */
auto EncryptEMLData(int channel, const QStringList& keys,
                    const EMailMetaData& meta_data, const QByteArray& body_data,
                    QString& eml_data) -> int;

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
auto SignEMLData(int channel, const QString& key,
                 const EMailMetaData& meta_data, const QByteArray& body_data,
                 QString& eml_data) -> int;

/**
 * @brief
 *
 * @param channel
 * @param key
 * @param message
 * @param eml_data
 * @return int
 */
auto AppendSignToEMLData(int channel, const QString& key,
                         const vmime::shared_ptr<vmime::message>& message,
                         QString& eml_data) -> int;