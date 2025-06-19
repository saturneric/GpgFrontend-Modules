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

#include "GitHubVersionCheckTask.h"

#include <GFSDKBasic.h>
#include <GFSDKExtra.h>
#include <GFSDKLog.h>
#include <qobject.h>

#include <QMetaType>
#include <QtNetwork>

#include "GFModuleCommonUtils.hpp"
#include "SoftwareVersion.h"
#include "Utils.h"

GitHubVersionCheckTask::GitHubVersionCheckTask()
    : network_manager_(new QNetworkAccessManager(this)),
      current_version_(GFProjectVersion()) {
  qRegisterMetaType<SoftwareVersion>("SoftwareVersion");
  meta_.api = "GitHub";
  meta_.current_version = current_version_;
  meta_.local_commit_hash = GFProjectGitCommitHash();

  connect(this, &GitHubVersionCheckTask::SignalUpgradeVersion, this,
          [](const SoftwareVersion& sv) {
            GFDurableCacheSave(DUP("update_checking_cache"),
                               DUP(QJsonDocument(sv.ToJson()).toJson()));
          });
}

auto GitHubVersionCheckTask::Run() -> int {
  QString base_url = "https://api.github.com/repos/saturneric/gpgfrontend";
  QList<QUrl> urls = {
      {base_url + "/releases/latest"},
      {base_url + "/releases/tags/" + current_version_},
      {base_url + "/git/ref/tags/" + current_version_},
      {base_url + "/commits/" + meta_.local_commit_hash},
  };

  connect(network_manager_, &QNetworkAccessManager::finished, this,
          &GitHubVersionCheckTask::slot_parse_reply);

  int index = 0;
  for (const QUrl& url : urls) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      GFHttpRequestUserAgent());
    QNetworkReply* reply = network_manager_->get(request);
    reply->setProperty("GFCheckIndex", index++);
    replies_.append(reply);
  }

  return 0;
}

void GitHubVersionCheckTask::slot_parse_reply(QNetworkReply* reply) {
  if (reply->error() == QNetworkReply::NoError) {
    auto index = reply->property("GFCheckIndex").toInt();
    FLOG_DEBUG("get reply from url: %1, index: %2", reply->url().toString(),
               index);
    switch (index) {
      case 0:
        slot_parse_latest_version_info(reply);
        break;
      case 1:
        slot_parse_current_version_info(reply);
        break;
      case 2:
        slot_parse_current_tag_info(reply);
        break;
      case 3:
        slot_parse_current_commit_info(reply);
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
    meta_.timestamp = QDateTime::currentDateTime();
    FillGrtWithVersionInfo(meta_);
    emit SignalUpgradeVersion(meta_);
  }
}

void GitHubVersionCheckTask::slot_parse_latest_version_info(
    QNetworkReply* reply) {
  if (reply == nullptr || reply->error() != QNetworkReply::NoError) {
    return;
  }

  auto reply_bytes = reply->readAll();
  auto latest_reply_json = QJsonDocument::fromJson(reply_bytes);

  if (!latest_reply_json.isObject()) {
    FLOG_WARN("cannot parse data from github: %1", reply_bytes);
    return;
  }

  QString latest_version = latest_reply_json["tag_name"].toString();
  FLOG_DEBUG("raw tag name from github: %1", latest_version);

  QRegularExpression re(R"(^[vV](\d+\.)?(\d+\.)?(\*|\d+))");
  auto version_match = re.match(latest_version);
  if (version_match.hasMatch()) {
    latest_version = version_match.captured(0);
  } else {
    latest_version = "";
    FLOG_WARN("the raw tag name from github: %1 cannot match regex rules",
              latest_version);
  }

  auto publish_date = latest_reply_json["published_at"].toString();
  auto release_note = latest_reply_json["body"].toString();

  meta_.latest_version = latest_version;
  meta_.publish_date = publish_date;
  meta_.release_note = release_note;
}

void GitHubVersionCheckTask::slot_parse_current_version_info(
    QNetworkReply* reply) {
  if (reply == nullptr || reply->error() != QNetworkReply::NoError) return;

  auto reply_bytes = reply->readAll();
  auto current_reply_json = QJsonDocument::fromJson(reply_bytes);

  if (!current_reply_json.isObject()) {
    FLOG_WARN("cannot parse data from github: %1", reply_bytes);
    return;
  }

  meta_.current_version_publish_in_remote = true;
}

void GitHubVersionCheckTask::slot_parse_current_tag_info(QNetworkReply* reply) {
  if (reply == nullptr || reply->error() != QNetworkReply::NoError) {
    meta_.current_version_publish_in_remote = false;
    return;
  }

  meta_.current_version_publish_in_remote = true;
  auto reply_bytes = reply->readAll();
  auto current_reply_json = QJsonDocument::fromJson(reply_bytes);

  if (!current_reply_json.isObject()) {
    FLOG_WARN("cannot parse data from github: %1", reply_bytes);
    return;
  }

  auto object = current_reply_json["object"].toObject();
  if (object["type"].toString() != "tag" &&
      object["type"].toString() != "commit") {
    FLOG_WARN("remote tag: %1 is not a ref: %2", meta_.current_version,
              object["type"].toString());
    return;
  }

  auto sha = object["sha"].toString();
  meta_.remote_commit_hash_by_tag = sha.trimmed();
  FLOG_DEBUG("got remote commit hash: %1", meta_.remote_commit_hash_by_tag);
}

void GitHubVersionCheckTask::slot_parse_current_commit_info(
    QNetworkReply* reply) {
  if (reply == nullptr || reply->error() != QNetworkReply::NoError) {
    meta_.current_version_publish_in_remote = false;
    return;
  }

  meta_.current_version_publish_in_remote = true;
  auto reply_bytes = reply->readAll();
  auto current_reply_json = QJsonDocument::fromJson(reply_bytes);

  if (!current_reply_json.isObject()) {
    FLOG_WARN("cannot parse data from github: %1", reply_bytes);
    return;
  }

  auto sha = current_reply_json["sha"].toString();
  FLOG_DEBUG("got remote commit hash from github: %1",
             meta_.remote_commit_hash_by_tag);

  meta_.current_commit_hash_publish_in_remote = sha == meta_.local_commit_hash;
}
