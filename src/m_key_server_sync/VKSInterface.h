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

#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;

class VKSInterface : public QObject {
  Q_OBJECT

 public:
  explicit VKSInterface(QString target_key_server = "https://keys.openpgp.org",
                        QObject* parent = nullptr);

  void GetByFingerprint(const QString& fingerprint);
  void GetByKeyId(const QString& keyId);
  void GetByEmail(const QString& email);
  void UploadKey(const QString& key_text);
  void RequestVerify(const QString& token, const QStringList& addresses,
                     const QStringList& locale = QStringList());

 signals:
  void SignalKeyRetrieved(const QString& key);
  void SignalKeyUploaded(const QString& key_fingerprint,
                         const QJsonObject& status, const QString& token);
  void SignalVerificationRequested(const QString& key_fingerprint,
                                   const QJsonObject& status);
  void SignalErrorOccurred(const QString& error_string,
                           const QString& reply_data);

 private slots:
  void on_reply_finished(QNetworkReply* reply);

 private:
  QString target_key_server_;
  QNetworkAccessManager* network_manager_;
};
