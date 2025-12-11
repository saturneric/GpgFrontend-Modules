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

#include <QDialog>

#include "PKSInterface.h"

class Ui_SearchKeyDialog;

class SearchKeyDialog : public QDialog {
  Q_OBJECT
 public:
  explicit SearchKeyDialog(QWidget* parent = nullptr);

 private slots:

  /**
   * @brief
   *
   */
  void slot_search();

  /**
   * @brief
   *
   * @param message
   */
  void slot_set_error_message(const QString& message);

  /**
   * @brief
   *
   * @param loading
   */
  void slot_set_loading(bool loading);

  /**
   * @brief
   *
   * @param error
   * @param error_string
   * @param keys
   */
  void slot_search_finished_pks(QNetworkReply::NetworkError error,
                                const QString& error_string,
                                const QList<KeyServerKeyInfo>& keys);

  /**
   * @brief
   *
   * @param error
   * @param error_string
   * @param key_data
   */
  void slot_lookup_finished_pks(QNetworkReply::NetworkError error,
                                const QString& error_string,
                                const QByteArray& key_data);

  /**
   * @brief
   */
  void slot_import(int row, int column);

 private:
  std::shared_ptr<Ui_SearchKeyDialog> ui_;
};
