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

#include "VKSInterface.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QUrl>
#include <QUrlQuery>

#include "GFModule.h"
#include "GFModuleTr.h"
#include "KeyServerBatchLogic.h"

namespace {
/// Matches the HKP side; without it a stalled server hangs the caller forever.
constexpr int kTransferTimeoutMs = 15000;
}  // namespace

auto VKSInterface::make_request(const QUrl& url) -> QNetworkRequest {
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::UserAgentHeader,
                    QString::fromUtf8(GFAppUserAgent(GFModuleSdkContext())));
  request.setTransferTimeout(kTransferTimeoutMs);
  return request;
}

VKSInterface::VKSInterface(QString key_server, QObject* parent)
    : QObject(parent),
      target_key_server_(std::move(key_server)),
      network_manager_(new QNetworkAccessManager(this)) {
  connect(network_manager_, &QNetworkAccessManager::finished, this,
          &VKSInterface::on_reply_finished);
}

void VKSInterface::GetByFingerprint(const QString& fingerprint) {
  // search cache by first
  // Keyed by server too: which server answered is now the user's choice, and
  // two of them can hold different versions of the same key.
  cache_key_ = QString("module:key-server-sync:key-data:%1:fpr:%2")
                   .arg(QUrl(target_key_server_).host(), fingerprint);
  auto value = gf::sdk::CacheText(GFModuleSdkContext(), GF_STORE_SESSION,
                                  (cache_key_).toUtf8().constData());
  if (!value.isEmpty()) {
    emit SignalKeyRetrieved(value);
    return;
  }

  QUrl url(QString("%1/vks/v1/by-fingerprint/%2")
               .arg(target_key_server_)
               .arg(fingerprint));
  network_manager_->get(make_request(url));
}

void VKSInterface::GetByKeyId(const QString& key_id) {
  // search cache by first
  cache_key_ = QString("module:key-server-sync:key-data:%1:id:%2")
                   .arg(QUrl(target_key_server_).host(), key_id);
  auto value = gf::sdk::CacheText(GFModuleSdkContext(), GF_STORE_SESSION,
                                  (cache_key_).toUtf8().constData());
  if (!value.isEmpty()) {
    emit SignalKeyRetrieved(value);
    return;
  }

  QUrl url(
      QString("%1/vks/v1/by-keyid/%2").arg(target_key_server_).arg(key_id));
  network_manager_->get(make_request(url));
}

void VKSInterface::UploadKey(const QString& key_text) {
  QUrl url(QString("%1/vks/v1/upload").arg(target_key_server_));
  auto request = make_request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QJsonObject json;
  json["keytext"] = key_text;

  network_manager_->post(request, QJsonDocument(json).toJson());
}

void VKSInterface::on_reply_finished(QNetworkReply* reply) {
  if (reply->error() != QNetworkReply::NoError) {
    // A 404 from a lookup is an answer, not a failure: the server has no
    // such key. Said plainly, rather than as Qt's transfer error.
    const auto error = reply->error() == QNetworkReply::ContentNotFoundError
                           ? KeyServerBatchLogic::NotFoundText()
                           : reply->errorString();
    emit SignalErrorOccurred(error, reply->readAll());
    reply->deleteLater();
    return;
  }

  QUrl url = reply->url();
  QByteArray response_data = reply->readAll();
  QJsonDocument json_response = QJsonDocument::fromJson(response_data);

  if (url.path().contains("/vks/v1/by-fingerprint") ||
      url.path().contains("/vks/v1/by-keyid")) {
    gf::sdk::SetCacheText(GFModuleSdkContext(), GF_STORE_SESSION,
                          (cache_key_).toUtf8().constData(),
                          (QString(response_data)).toUtf8().constData(), 300);
    emit SignalKeyRetrieved(QString(response_data));
  } else if (url.path().contains("/vks/v1/upload")) {
    // Every reply must end in one of the two signals: the caller releases
    // this object, and a batch moves on, only when one of them arrives.
    if (json_response.isObject()) {
      QJsonObject response_object = json_response.object();
      emit SignalKeyUploaded(response_object["key_fpr"].toString(),
                             response_object["status"].toObject(),
                             response_object["token"].toString());
    } else {
      emit SignalErrorOccurred(
          GTrC::tr("The key server sent a reply that could not be read."),
          QString::fromUtf8(response_data));
    }
  }

  reply->deleteLater();
}
