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

#include <QObject>

#include "SoftwareVersion.h"

class QNetworkReply;
class QNetworkAccessManager;

/**
 * @brief Learns the latest stable release from the update index.
 *
 * Reads only `channels.stable` of https://update.bktus.com/gpgfrontend/v1/
 * index.json, which is always a final release, and whether the running
 * version is listed there (and not withdrawn). Nothing is downloaded or
 * installed: the user is pointed at the official download page, so the
 * index's signature is not checked here.
 */
class UpdateIndexVersionCheckTask : public QObject {
  Q_OBJECT
 public:
  UpdateIndexVersionCheckTask();

  /**
   * @brief Fetch the index; SignalUpgradeVersion() follows, also on failure
   * (then with an empty latest version).
   */
  void Run();

  /**
   * @brief What the index says about @p current_version: fills
   * latest_version and current_version_publish_in_remote of @p meta.
   *
   * @return false when @p index is not GpgFrontend's update index
   */
  static auto ParseIndex(const QByteArray& index, SoftwareVersion& meta)
      -> bool;

 signals:
  void SignalUpgradeVersion(SoftwareVersion version);

 private slots:
  void slot_parse_reply(QNetworkReply* reply);

 private:
  QNetworkAccessManager* network_manager_;  ///<
  SoftwareVersion meta_;
};
