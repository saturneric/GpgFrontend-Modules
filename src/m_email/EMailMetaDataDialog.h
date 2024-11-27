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
  explicit EMailMetaDataDialog(int mode, QWidget* parent);

  /**
   * @brief Set the Channel object
   *
   */
  void SetChannel(int c);

  /**
   * @brief Set the Sign Keys object
   *
   */
  void SetKeys(QStringList ks);

  /**
   * @brief Set the Body Data object
   *
   * @param b
   */
  void SetBodyData(QByteArray b);

 signals:

  void SignalEMLDataGenerateSuccess(QString);

  void SignalEMLDataGenerateFailed(QString);

 private slots:

  void slot_sign_eml_data();

  void slot_encrypt_eml_data();

  void slot_export_encrypted_data();

  void slot_set_from_field_by_sign_key();

  void slot_set_to_field_by_encrypt_keys();

  void slot_validate_inputs_and_show_errors();

 private:
  /**
   * @brief
   *
   * @param email
   * @return true
   * @return false
   */
  static auto is_valid_email(const QString& email) -> bool;

  /**
   * @brief
   *
   * @param emails
   * @return true
   * @return false
   */
  static auto are_valid_emails(const QString& emails) -> bool;

  QSharedPointer<Ui_EMailMetaDataDialog> ui_;
  int mode_;
  QByteArray body_data_;
  int channel_;
  QStringList keys_;
  QString from_name_;
  QString from_email_;
};

Q_VARIANT_Q_OBJECT_FACTORY_DECLARE(CreateEMailMetaDataDialog);