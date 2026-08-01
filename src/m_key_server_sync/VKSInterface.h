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

#include <QNetworkReply>

#include "KeyInfo.h"

class QNetworkAccessManager;

class VKSInterface : public QObject {
  Q_OBJECT

 public:
  /**
   * @brief Talk VKS to @p target_key_server.
   *
   * No default: the server is configurable, and a default here would let a call
   * site quietly ignore the user's choice just by leaving the argument out.
   */
  explicit VKSInterface(QString target_key_server, QObject* parent = nullptr);

  [[nodiscard]] auto TargetKeyServer() const -> QString {
    return target_key_server_;
  }

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
  /**
   * @brief A request carrying this application's user agent and timeout.
   *
   * Every request has to identify itself and give up eventually; building them
   * in one place is what stops a new call site from forgetting either.
   */
  [[nodiscard]] static auto make_request(const QUrl& url) -> QNetworkRequest;

  QString cache_key_;
  QString target_key_server_;
  QNetworkAccessManager* network_manager_;
};
