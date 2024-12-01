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

#include <QtWidgets/QtWidgets>

/**
 * @brief Class containing the main tab of about dialog
 *
 */
class UpdateTab : public QWidget {
  Q_OBJECT

  QLabel* current_version_label_;   ///<
  QLabel* latest_version_label_;    ///<
  QLabel* upgrade_label_;           ///<
  QProgressBar* pb_;                ///<
  QTextEdit* release_note_viewer_;  ///<
  QString current_version_;         ///<
  QGroupBox* release_note_box_;
  QGroupBox* upgrade_info_box_;
  QGroupBox* current_version_box_;

 public:
  /**
   * @brief Construct a new Update Tab object
   *
   * @param parent
   */
  explicit UpdateTab(QWidget* parent = nullptr);

 protected:
  void showEvent(QShowEvent* event) override;

 private slots:
  /**
   * @brief
   *
   * @param version
   */
  void slot_show_version_status();

 signals:
  /**
   * @brief
   *
   * @param data
   */
  void SignalReplyFromUpdateServer(QByteArray data);
};

auto UpdateTabFactory(void* id) -> void*;