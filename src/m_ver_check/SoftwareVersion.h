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

#include <QJsonDocument>
#include <QString>

/**
 * @brief
 *
 */
struct SoftwareVersion {
  QString api;
  QString latest_version;   ///<
  QString current_version;  ///<

  bool current_version_publish_in_remote = false;      ///<
  bool current_commit_hash_publish_in_remote = false;  ///<

  QString publish_date;  ///<
  QString release_note;  ///<
  QString remote_commit_hash_by_tag;
  QString local_commit_hash;

  QDateTime timestamp;

  /**
   * @brief
   *
   * @return true
   * @return false
   */
  [[nodiscard]] auto IsInfoValid() const -> bool {
    return !latest_version.isEmpty();
  }

  /**
   * @brief
   *
   * @return true
   * @return false
   */
  [[nodiscard]] auto NeedUpgrade() const -> bool;

  /**
   * @brief
   *
   * @return true
   * @return false
   */
  [[nodiscard]] auto VersionWithdrawn() const -> bool;

  /**
   * @brief
   *
   * @return true
   * @return false
   */
  [[nodiscard]] auto GitCommitHashMismatch() const -> bool;

  /**
   * @brief
   *
   * @return true
   * @return false
   */
  [[nodiscard]] auto CurrentVersionReleased() const -> bool;

  /**
   * @brief
   *
   * @return QJsonDocument
   */
  [[nodiscard]] auto ToJson() const -> QJsonObject;

  /**
   * @brief
   *
   * @param obj
   * @return auto
   */
  void FromJson(const QJsonObject& obj);

 private:
  static auto version_compare(const QString& a, const QString& b) -> int;
};
