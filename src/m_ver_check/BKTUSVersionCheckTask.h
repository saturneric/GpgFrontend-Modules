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
 * @brief
 *
 */
class BKTUSVersionCheckTask : public QObject {
  Q_OBJECT
 public:
  /**
   * @brief Construct a new Version Check Thread object
   *
   */
  BKTUSVersionCheckTask();

  /**
   * @brief
   *
   * @return int
   */
  auto Run() -> int;

 signals:

  /**
   * @brief
   *
   * @param version
   */
  void SignalUpgradeVersion(SoftwareVersion version);

 private slots:

  /**
   * @brief
   *
   */
  void slot_parse_reply(QNetworkReply* reply);

  /**
   * @brief
   *
   * @param reply
   */
  void slot_parse_latest_version_info(QNetworkReply* reply);

  /**
   * @brief
   *
   * @param reply
   */
  void slot_parse_current_tag_info(QNetworkReply* reply);

  /**
   * @brief
   *
   * @param reply
   */
  void slot_parse_current_commit_hash_info(QNetworkReply* reply);

 private:
  QList<QNetworkReply*> replies_;           ///<
  QNetworkAccessManager* network_manager_;  ///<
  QString current_version_;                 ///<
  SoftwareVersion meta_;
};
