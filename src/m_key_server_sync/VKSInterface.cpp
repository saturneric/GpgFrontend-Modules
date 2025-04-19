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
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>
#include <QUrl>
#include <QUrlQuery>

//
#include <GFModuleCommonUtils.hpp>

VKSInterface::VKSInterface(QString key_server, QObject* parent)
    : QObject(parent),
      target_key_server_(std::move(key_server)),
      network_manager_(new QNetworkAccessManager(this)) {
  connect(network_manager_, &QNetworkAccessManager::finished, this,
          &VKSInterface::on_reply_finished);
}

void VKSInterface::GetByFingerprint(const QString& fingerprint) {
  // search cache by first
  cache_key_ =
      QString("module:key-server-sync:key-data:fpr:%1").arg(fingerprint);
  auto value = UDUP(GFCacheGet(QDUP(cache_key_)));
  if (!value.isEmpty()) {
    emit SignalKeyRetrieved(value);
    return;
  }

  QUrl url(QString("%1/vks/v1/by-fingerprint/%2")
               .arg(target_key_server_)
               .arg(fingerprint));
  QNetworkRequest request(url);
  network_manager_->get(request);
}

void VKSInterface::GetByKeyId(const QString& key_id) {
  // search cache by first
  cache_key_ = QString("module:key-server-sync:key-data:id:%1").arg(key_id);
  auto value = UDUP(GFCacheGet(QDUP(cache_key_)));
  if (!value.isEmpty()) {
    emit SignalKeyRetrieved(value);
    return;
  }

  QUrl url(
      QString("%1/vks/v1/by-keyid/%2").arg(target_key_server_).arg(key_id));
  QNetworkRequest request(url);
  network_manager_->get(request);
}

void VKSInterface::GetByEmail(const QString& email) {
  QUrl url(QString("%1/vks/v1/by-email/%2")
               .arg(target_key_server_)
               .arg(QUrl::toPercentEncoding(email)));
  QNetworkRequest request(url);
  network_manager_->get(request);
}

void VKSInterface::UploadKey(const QString& key_text) {
  QUrl url(QString("%1/vks/v1/upload").arg(target_key_server_));
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QJsonObject json;
  json["keytext"] = key_text;

  network_manager_->post(request, QJsonDocument(json).toJson());
}

void VKSInterface::RequestVerify(const QString& token,
                                 const QStringList& addresses,
                                 const QStringList& locale) {
  QUrl url(QString("%1/vks/v1/request-verify").arg(target_key_server_));
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

  QJsonObject json;
  json["token"] = token;
  QJsonArray addresses_array;
  for (const QString& address : addresses) {
    addresses_array.append(address);
  }
  json["addresses"] = addresses_array;

  if (!locale.isEmpty()) {
    QJsonArray locale_array;
    for (const QString& loc : locale) {
      locale_array.append(loc);
    }
    json["locale"] = locale_array;
  }

  network_manager_->post(request, QJsonDocument(json).toJson());
}

void VKSInterface::on_reply_finished(QNetworkReply* reply) {
  if (reply->error() != QNetworkReply::NoError) {
    emit SignalErrorOccurred(reply->errorString(), reply->readAll());
    reply->deleteLater();
    return;
  }

  QUrl url = reply->url();
  QByteArray response_data = reply->readAll();
  QJsonDocument json_response = QJsonDocument::fromJson(response_data);

  if (url.path().contains("/vks/v1/by-fingerprint") ||
      url.path().contains("/vks/v1/by-keyid") ||
      url.path().contains("/vks/v1/by-email")) {
    GFCacheSaveWithTTL(QDUP(cache_key_), QDUP(QString(response_data)), 300);
    emit SignalKeyRetrieved(QString(response_data));
  } else if (url.path().contains("/vks/v1/upload")) {
    if (json_response.isObject()) {
      QJsonObject response_object = json_response.object();
      emit SignalKeyUploaded(response_object["key_fpr"].toString(),
                             response_object["status"].toObject(),
                             response_object["token"].toString());
    }
  } else if (url.path().contains("/vks/v1/request-verify")) {
    if (json_response.isObject()) {
      QJsonObject response_object = json_response.object();
      emit SignalVerificationRequested(response_object["key_fpr"].toString(),
                                       response_object["status"].toObject());
    }
  }

  reply->deleteLater();
}
