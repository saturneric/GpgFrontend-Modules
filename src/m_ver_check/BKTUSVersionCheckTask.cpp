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

#include "BKTUSVersionCheckTask.h"

#include <GFSDKBasic.h>
#include <GFSDKExtra.h>
#include <GFSDKLog.h>
#include <qobject.h>

#include <QDomDocument>
#include <QMetaType>
#include <QtNetwork>

#include "GFModuleCommonUtils.hpp"
#include "SoftwareVersion.h"
#include "Utils.h"
#include "VersionCheckingModule.h"

BKTUSVersionCheckTask::BKTUSVersionCheckTask()
    : network_manager_(new QNetworkAccessManager(this)),
      current_version_(GFProjectVersion()) {
  qRegisterMetaType<SoftwareVersion>("SoftwareVersion");
  version_meta_data_.current_version = current_version_;
  version_meta_data_.local_commit_hash = GFProjectGitCommitHash();
}

auto BKTUSVersionCheckTask::Run() -> int {
  QString base_url = "";
  QList<QUrl> urls = {
      {"https://ftp.bktus.com/GpgFrontend/LATEST"},
      {"https://git.bktus.com/GpgFrontend/GpgFrontend/atom/?h=" +
       current_version_},
      {"https://git.bktus.com/GpgFrontend/GpgFrontend/atom/?id=" +
       version_meta_data_.local_commit_hash},
  };

  connect(network_manager_, &QNetworkAccessManager::finished, this,
          &BKTUSVersionCheckTask::slot_parse_reply);

  for (const QUrl& url : urls) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      GFHttpRequestUserAgent());
    QNetworkReply* reply = network_manager_->get(request);
    replies_.append(reply);
  }

  return 0;
}

void BKTUSVersionCheckTask::slot_parse_reply(QNetworkReply* reply) {
  if (reply->error() == QNetworkReply::NoError) {
    FLOG_DEBUG("get reply from url: %1", reply->url().toString());
    switch (replies_.indexOf(reply)) {
      case 0:
        slot_parse_latest_version_info(reply);
        break;
      case 1:
        slot_parse_current_tag_info(reply);
        break;
      case 2:
        slot_parse_current_commit_hash_info(reply);
        break;
      default:
        break;
    }
  } else {
    FLOG_DEBUG("get reply from url: %1, error: %2 %3", reply->url().toString(),
               reply->errorString(), reply->readAll());
  }

  replies_.removeAll(reply);
  reply->deleteLater();

  if (replies_.isEmpty()) {
    FillGrtWithVersionInfo(version_meta_data_);
    emit SignalUpgradeVersion(version_meta_data_);
  }
}

void BKTUSVersionCheckTask::slot_parse_latest_version_info(
    QNetworkReply* reply) {
  if (reply == nullptr || reply->error() != QNetworkReply::NoError) {
    return;
  }

  auto reply_bytes = reply->readAll();
  auto latest_reply_json = QJsonDocument::fromJson(reply_bytes);

  if (!latest_reply_json.isObject()) {
    FLOG_WARN("cannot parse data from bktus: %1", reply_bytes);
    return;
  }

  QString latest_version = latest_reply_json["version"].toString();
  FLOG_DEBUG("raw tag name from bktus: %1", latest_version);

  QRegularExpression re(R"(^[vV](\d+\.)?(\d+\.)?(\*|\d+))");
  auto version_match = re.match(latest_version);
  if (version_match.hasMatch()) {
    latest_version = version_match.captured(0);
  } else {
    latest_version = "";
    FLOG_WARN("the raw release name from bktus: %1 cannot match regex rules",
              latest_version);
  }

  auto publish_date = latest_reply_json["release_date"].toString();
  auto release_note = latest_reply_json["release_notes"].toString();

  version_meta_data_.latest_version = latest_version;
  version_meta_data_.publish_date = publish_date;
  version_meta_data_.release_note = release_note;
}

void BKTUSVersionCheckTask::slot_parse_current_tag_info(QNetworkReply* reply) {
  if (reply == nullptr || reply->error() != QNetworkReply::NoError) return;

  QVariant status_code =
      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
  int status = status_code.toInt();

  if (status != 200) {
    FLOG_WARN("http status code from git.bktus.com is not 200: %1", status);
    return;
  }

  auto reply_bytes = reply->readAll();

  QDomDocument doc;
  QString err_msg;
  int err_line = 0;
  int err_column = 0;
  QString xml_text = QString::fromUtf8(reply_bytes);
  bool ok = doc.setContent(xml_text, &err_msg, &err_line, &err_column);
  if (!ok) {
    FLOG_WARN("xml parse failed: %1, line: %2, column: %3", err_msg, err_line,
              err_column);
    return;
  }

  auto entries = doc.elementsByTagName("entry");

  if (entries.size() == 0) {
    FLOG_WARN("no xml entry of current version: %1", current_version_);
    return;
  }

  QDomElement entry_elem = entries.at(0).toElement();
  if (entry_elem.isNull()) {
    FLOG_WARN("first xml entry of current version: %1 is null",
              current_version_);
    version_meta_data_.current_commit_hash_publish_in_remote = false;
    return;
  }

  auto title_elem = entry_elem.firstChildElement("title");
  auto id_elem = entry_elem.firstChildElement("id");
  auto published_elem = entry_elem.firstChildElement("published");

  if (title_elem.isNull() || id_elem.isNull() || published_elem.isNull()) {
    FLOG_WARN("illegal xml entry of structure of version: %1",
              current_version_);
    version_meta_data_.current_commit_hash_publish_in_remote = false;
    return;
  }

  auto title_text = title_elem.text();
  auto id_text = id_elem.text();
  auto published_text = published_elem.text();

  FLOG_DEBUG("got tag info from bktus: %1, %2, %3", title_text, id_text,
             published_text);

  const auto& sha = id_text;
  version_meta_data_.remote_commit_hash_by_tag = sha.trimmed();
  version_meta_data_.current_version_publish_in_remote = true;
}

void BKTUSVersionCheckTask::slot_parse_current_commit_hash_info(
    QNetworkReply* reply) {
  if (reply == nullptr || reply->error() != QNetworkReply::NoError) {
    return;
  }

  QVariant status_code =
      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
  int status = status_code.toInt();

  if (status != 200) {
    FLOG_WARN("http status code from git.bktus.com is not 200: %1", status);
    return;
  }

  auto reply_bytes = reply->readAll();

  QDomDocument doc;
  QString err_msg;
  int err_line = 0;
  int err_column = 0;
  QString xml_text = QString::fromUtf8(reply_bytes);
  bool ok = doc.setContent(xml_text, &err_msg, &err_line, &err_column);
  if (!ok) {
    FLOG_WARN("xml parse failed: %1, line: %2, column: %3", err_msg, err_line,
              err_column);
    return;
  }

  auto entries = doc.elementsByTagName("entry");

  if (entries.size() == 0) {
    FLOG_WARN("no xml entry of current version: %1", current_version_);
    return;
  }

  QDomElement entry_elem = entries.at(0).toElement();
  if (entry_elem.isNull()) {
    FLOG_WARN("first xml entry of current version: %1 is null",
              current_version_);
    return;
  }

  auto title_elem = entry_elem.firstChildElement("title");
  auto id_elem = entry_elem.firstChildElement("id");
  auto published_elem = entry_elem.firstChildElement("published");

  if (title_elem.isNull() || id_elem.isNull() || published_elem.isNull()) {
    FLOG_WARN("illegal xml entry of structure of version: %1",
              current_version_);
    return;
  }

  auto title_text = title_elem.text();
  auto id_text = id_elem.text();
  auto published_text = published_elem.text();

  FLOG_DEBUG("got commit info from bktus: %1, %2, %3", title_text, id_text,
             published_text);

  version_meta_data_.current_commit_hash_publish_in_remote =
      id_text.trimmed() == version_meta_data_.local_commit_hash.trimmed();
}
