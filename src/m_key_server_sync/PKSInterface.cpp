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

#include "PKSInterface.h"

#include <GFSDKExtra.h>

#include "GFModuleCommonUtils.hpp"

namespace {

auto GetAlgorithmName(const QString& algo_id) -> QString {
  static const QMap<QString, QString> algo_map = {
      {"1", "RSA"},
      {"2", "RSA (Encrypt-Only)"},
      {"3", "RSA (Sign-Only)"},
      {"16", "ElGamal (Encrypt-Only)"},
      {"17", "DSA"},
      {"18", "ECDH"},
      {"19", "ECDSA"},
      {"20", "ElGamal"},
      {"22", "EdDSA"},
      {"23", "AEDH"},
      {"24", "AEDSA"}};

  return algo_map.value(algo_id, QString("Unknown (%1)").arg(algo_id));
}

auto GetKeySizeDescription(const QString& algo_id, const QString& key_size)
    -> QString {
  if (key_size.isEmpty()) return "Unknown";

  // For ECC algorithms, show curve instead of size
  if (algo_id == "18" || algo_id == "19" || algo_id == "22") {
    static const QMap<QString, QString> curve_map = {
        {"256", "NIST P-256"},      {"384", "NIST P-384"},
        {"521", "NIST P-521"},      {"255", "Curve25519"},
        {"448", "Curve448"},        {"nistp256", "NIST P-256"},
        {"nistp384", "NIST P-384"}, {"nistp521", "NIST P-521"},
        {"cv25519", "Curve25519"},  {"ed25519", "Ed25519"}};

    return curve_map.value(key_size, QString("%1 bits").arg(key_size));
  }

  return QString("%1 bits").arg(key_size);
}

auto GetFlagsDescription(const QString& flags) -> QString {
  QStringList descriptions;

  if (flags.contains('r')) descriptions << "Revoked";
  if (flags.contains('d')) descriptions << "Disabled";
  if (flags.contains('e')) descriptions << "Expired";

  return descriptions.isEmpty() ? "Valid" : descriptions.join(", ");
}

auto Parse(const QByteArray& response) -> QList<KeyServerKeyInfo> {
  QList<KeyServerKeyInfo> keys;

  QString response_str = QString::fromUtf8(response);
  QStringList lines = response_str.split('\n', Qt::SkipEmptyParts);

  KeyServerKeyInfo current_key;
  bool has_current_key = false;

  for (const auto& line : lines) {
    if (line.startsWith("info:")) {
      // Info line, ignore for now
      continue;
    } else if (line.startsWith("pub:")) {
      // Save previous key if exists
      if (has_current_key) {
        keys.append(current_key);
      }

      // Parse public key line:
      // pub:keyid:algo:keylen:creationdate:expirationdate:flags
      QStringList parts = line.split(':');
      current_key = KeyServerKeyInfo();
      current_key.uids.clear();

      if (parts.size() >= 7) {
        current_key.keyid = parts[1];
        current_key.algorithm = parts[2];
        current_key.key_size = parts[3];
        current_key.creation_date = parts[4];
        current_key.expiration_date = parts[5];
        current_key.flags = parts[6];

        // Add human-readable descriptions
        current_key.algorithm_desc = GetAlgorithmName(parts[2]);
        current_key.key_size_desc = GetKeySizeDescription(parts[2], parts[3]);
        current_key.flags_desc = GetFlagsDescription(parts[6]);
      }
      has_current_key = true;

    } else if (line.startsWith("uid:")) {
      // Parse UID line: uid:escaped uid
      // string:creationdate:expirationdate:flags
      if (has_current_key) {
        QStringList parts = line.split(':');
        KeyServerUID uid;

        if (parts.size() >= 5) {
          // Decode URL-encoded UID
          uid.uid = QUrl::fromPercentEncoding(parts[1].toUtf8());
          uid.creation_date = parts[2];
          uid.expiration_date = parts[3];
          uid.flags = parts[4];

          // Add human-readable descriptions
          uid.flags_desc = GetFlagsDescription(parts[4]);

          current_key.uids.append(uid);
        }
      }
    }
  }

  // Add last key
  if (has_current_key) {
    keys.append(current_key);
  }

  FLOG_DEBUG("parsed %1 keys from keyserver response", keys.size());

  return keys;
}
}  // namespace

PKSInterface::PKSInterface(QObject* parent)
    : QObject(parent), manager_(new QNetworkAccessManager(this)) {}

auto PKSInterface::Search(const QString& url, const QString& type,
                          const QString& value) -> void {
  FLOG_DEBUG("searching keyserver %1 for type %2 value %3", url, type, value);

  QUrl url_from_remote =
      url + "/pks/lookup?search=" + QUrl::toPercentEncoding(value) +
      "&op=index&options=mr";

  if (type == "fpr" || type == "keyid") {
    url_from_remote =
        url + "/pks/lookup?search=0x" + value + "&op=index&options=mr";
  }

  auto request = QNetworkRequest(url_from_remote);
  // set timeout and user agent
  request.setHeader(QNetworkRequest::UserAgentHeader, GFHttpRequestUserAgent());
  request.setTransferTimeout(15000);  // 15 seconds

  reply_ = manager_->get(request);
  connect(reply_, &QNetworkReply::finished, this,
          &PKSInterface::dealing_reply_from_server);
}

void PKSInterface::dealing_reply_from_server() {
  QByteArray buffer;
  QNetworkReply::NetworkError network_reply = reply_->error();
  if (network_reply == QNetworkReply::NoError) {
    buffer = reply_->readAll();
  }

  FLOG_DEBUG("reply from key server: %1, err string: %2, reply: %3",
             static_cast<int>(network_reply), reply_->errorString(),
             QString::fromLatin1(buffer));

  // Parse the response
  auto parsed_keys = Parse(buffer);
  emit SignalKeyServerSearchResultParsed(network_reply, reply_->errorString(),
                                         parsed_keys);
}

void PKSInterface::LookupKeyById(const QString& url, const QString& keyid) {
  FLOG_DEBUG("looking up keyid %1 from keyserver %2", keyid, url);

  QUrl url_from_remote =
      url + "/pks/lookup?search=0x" + keyid + "&op=get&options=mr";

  auto request = QNetworkRequest(url_from_remote);
  request.setHeader(QNetworkRequest::UserAgentHeader, GFHttpRequestUserAgent());
  request.setTransferTimeout(15000);

  auto* reply = manager_->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    QByteArray buffer;
    QNetworkReply::NetworkError network_reply = reply->error();

    if (network_reply == QNetworkReply::NoError) {
      buffer = reply->readAll();
    }

    FLOG_DEBUG("key lookup reply from server: %1, err string: %2",
               static_cast<int>(network_reply), reply->errorString());

    emit SignalKeyServerKeyLookupResult(network_reply, reply->errorString(),
                                        buffer);
    reply->deleteLater();
  });
}

void PKSInterface::UploadKey(const QString& url, const QByteArray& key_data) {
  QUrl req_url(url + "/pks/add");
  auto* q_nam = new QNetworkAccessManager(this);

  // Building Post Data
  QByteArray post_data;

  auto data = key_data;

  data.replace("\n", "%0A");
  data.replace("\r", "%0D");
  data.replace("(", "%28");
  data.replace(")", "%29");
  data.replace("/", "%2F");
  data.replace(":", "%3A");
  data.replace("+", "%2B");
  data.replace("=", "%3D");
  data.replace(" ", "+");

  QNetworkRequest request(req_url);
  request.setHeader(QNetworkRequest::UserAgentHeader, GFHttpRequestUserAgent());
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    "application/x-www-form-urlencoded");

  post_data.append("keytext").append("=").append(data);

  // Send Post Data
  QNetworkReply* reply = q_nam->post(request, post_data);

  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    QNetworkReply::NetworkError network_reply = reply->error();

    FLOG_DEBUG("key upload reply from server: %1, err string: %2",
               static_cast<int>(network_reply), reply->errorString());

    emit SignalKeyServerKeyUploadResult(network_reply, reply->errorString());
    reply->deleteLater();
  });
}
