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
#include <QSharedPointer>

#include "GFModuleCommonUtils.hpp"

class QWidget;
class Ui_EMailMetaDataDialog;

class EMailMetaDataDialog : public QDialog {
  Q_OBJECT
 public:
  explicit EMailMetaDataDialog(QByteArray body_data, QWidget *parent);

  /**
   * @brief Set the Channel object
   *
   */
  void SetChannel(int c);

  /**
   * @brief Set the Sign Keys object
   *
   */
  void SetSignKey(QString k);

 signals:

  void SignalEMLDataGenerateSuccess(QString);

  void SignalEMLDataGenerateFailed(QString);

 private slots:

  void slot_export_eml_data();

  void slot_export_encrypted_data();

  void slot_set_from_field_by_sign_key();

 private:
  QSharedPointer<Ui_EMailMetaDataDialog> ui_;
  QByteArray body_data_;
  int channel_;
  QString sign_key_;
  QString from_name_;
  QString from_email_;
};

Q_VARIANT_Q_OBJECT_FACTORY_DECLARE(CreateEMailMetaDataDialog);