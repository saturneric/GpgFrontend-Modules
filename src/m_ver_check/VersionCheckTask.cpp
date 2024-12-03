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

#include "VersionCheckTask.h"

#include <GFSDKBasic.h>
#include <GFSDKExtra.h>
#include <GFSDKLog.h>
#include <qobject.h>

#include <QMetaType>
#include <QtNetwork>

#include "GFModuleCommonUtils.hpp"
#include "SoftwareVersion.h"
#include "VersionCheckingModule.h"

VersionCheckTask::VersionCheckTask()
    : network_manager_(new QNetworkAccessManager(this)),
      current_version_(GFProjectVersion()) {
  qRegisterMetaType<SoftwareVersion>("SoftwareVersion");
  version_meta_data_.current_version = current_version_;
  version_meta_data_.local_commit_hash = GFProjectGitCommitHash();
}

auto VersionCheckTask::Run() -> int {
  QString base_url = "https://api.github.com/repos/saturneric/gpgfrontend";
  QList<QUrl> urls = {
      {base_url + "/releases/latest"},
      {base_url + "/releases/tags/" + current_version_},
      {base_url + "/git/ref/tags/" + current_version_},
  };

  connect(network_manager_, &QNetworkAccessManager::finished, this,
          &VersionCheckTask::slot_parse_reply);

  for (const QUrl& url : urls) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      GFHttpRequestUserAgent());
    QNetworkReply* reply = network_manager_->get(request);
    replies_.append(reply);
  }

  return 0;
}

void VersionCheckTask::slot_parse_reply(QNetworkReply* reply) {
  if (reply->error() == QNetworkReply::NoError) {
    FLOG_DEBUG("get reply from url: %1", reply->url().toString());
    switch (replies_.indexOf(reply)) {
      case 0:
        slot_parse_latest_version_info(reply);
        break;
      case 1:
        slot_parse_current_version_info(reply);
        break;
      case 2:
        slot_parse_current_tag_info(reply);
        break;
    }
  } else {
    FLOG_DEBUG("get reply from url: %1, error: %2 %3", reply->url().toString(),
               reply->errorString(), reply->readAll());
  }

  replies_.removeAll(reply);
  reply->deleteLater();

  if (replies_.isEmpty()) {
    slot_fill_grt_with_version_info(version_meta_data_);
    emit SignalUpgradeVersion(version_meta_data_);
  }
}

void VersionCheckTask::slot_parse_latest_version_info(QNetworkReply* reply) {
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

  bool prerelease = latest_reply_json["prerelease"].toBool();
  bool draft = latest_reply_json["draft"].toBool();
  auto publish_date = latest_reply_json["published_at"].toString();
  auto release_note = latest_reply_json["body"].toString();
  version_meta_data_.latest_version = latest_version;
  version_meta_data_.latest_prerelease_version_from_remote = prerelease;
  version_meta_data_.latest_draft_from_remote = draft;
  version_meta_data_.publish_date = publish_date;
  version_meta_data_.release_note = release_note;
}

void VersionCheckTask::slot_parse_current_version_info(QNetworkReply* reply) {
  if (reply == nullptr || reply->error() != QNetworkReply::NoError) {
    version_meta_data_.current_version_publish_in_remote = false;
    return;
  }

  version_meta_data_.current_version_publish_in_remote = true;
  auto reply_bytes = reply->readAll();
  auto current_reply_json = QJsonDocument::fromJson(reply_bytes);

  if (!current_reply_json.isObject()) {
    FLOG_WARN("cannot parse data from github: %1", reply_bytes);
    return;
  }

  bool current_prerelease = current_reply_json["prerelease"].toBool();
  bool current_draft = current_reply_json["draft"].toBool();
  version_meta_data_.latest_prerelease_version_from_remote = current_prerelease;
  version_meta_data_.latest_draft_from_remote = current_draft;
}

void VersionCheckTask::slot_parse_current_tag_info(QNetworkReply* reply) {
  if (reply == nullptr || reply->error() != QNetworkReply::NoError) {
    version_meta_data_.current_version_publish_in_remote = false;
    return;
  }

  version_meta_data_.current_version_publish_in_remote = true;
  auto reply_bytes = reply->readAll();
  auto current_reply_json = QJsonDocument::fromJson(reply_bytes);

  if (!current_reply_json.isObject()) {
    FLOG_WARN("cannot parse data from github: %1", reply_bytes);
    return;
  }

  auto object = current_reply_json["object"].toObject();
  if (object["type"].toString() != "commit") {
    FLOG_WARN("remote tag: %1 is not a ref: %2",
              version_meta_data_.current_version, object["type"].toString());
    return;
  }

  auto sha = object["sha"].toString();
  version_meta_data_.remote_commit_hash_by_tag = sha.trimmed();
  FLOG_DEBUG("got remote commit hash: %1",
             version_meta_data_.remote_commit_hash_by_tag);
}

void VersionCheckTask::slot_fill_grt_with_version_info(
    const SoftwareVersion& version) {
  GFModuleLogDebug("filling software information info in rt...");

  GFModuleUpsertRTValue(GFGetModuleID(),
                        GFModuleStrDup("version.current_version"),
                        GFModuleStrDup(version.current_version.toUtf8()));
  GFModuleUpsertRTValue(GFGetModuleID(),
                        GFModuleStrDup("version.latest_version"),
                        GFModuleStrDup(version.latest_version.toUtf8()));
  GFModuleUpsertRTValue(
      GFGetModuleID(), GFModuleStrDup("version.remote_commit_hash_by_tag"),
      GFModuleStrDup(version.remote_commit_hash_by_tag.toUtf8()));
  GFModuleUpsertRTValue(GFGetModuleID(),
                        GFModuleStrDup("version.local_commit_hash"),
                        GFModuleStrDup(version.local_commit_hash.toUtf8()));

  GFModuleUpsertRTValueBool(
      GFGetModuleID(), GFModuleStrDup("version.current_version_is_drafted"),
      version.current_version_is_drafted ? 1 : 0);
  GFModuleUpsertRTValueBool(
      GFGetModuleID(),
      GFModuleStrDup("version.current_version_is_a_prerelease"),
      version.current_version_is_a_prerelease ? 1 : 0);
  GFModuleUpsertRTValueBool(
      GFGetModuleID(),
      GFModuleStrDup("version.current_version_publish_in_remote"),
      version.current_version_publish_in_remote ? 1 : 0);
  GFModuleUpsertRTValueBool(
      GFGetModuleID(),
      GFModuleStrDup("version.latest_prerelease_version_from_remote"),
      version.latest_prerelease_version_from_remote ? 1 : 0);
  GFModuleUpsertRTValueBool(GFGetModuleID(),
                            GFModuleStrDup("version.need_upgrade"),
                            version.NeedUpgrade() ? 1 : 0);
  GFModuleUpsertRTValueBool(GFGetModuleID(),
                            GFModuleStrDup("version.current_version_released"),
                            version.CurrentVersionReleased() ? 1 : 0);
  GFModuleUpsertRTValueBool(
      GFGetModuleID(), GFModuleStrDup("version.current_a_withdrawn_version"),
      version.VersionWithdrawn() ? 1 : 0);
  GFModuleUpsertRTValueBool(GFGetModuleID(),
                            GFModuleStrDup("version.git_commit_hash_mismatch"),
                            version.GitCommitHashMismatch() ? 1 : 0);

  GFModuleUpsertRTValue(GFGetModuleID(), GFModuleStrDup("version.release_note"),
                        GFModuleStrDup(version.release_note.toUtf8()));
  GFModuleUpsertRTValueBool(GFGetModuleID(),
                            GFModuleStrDup("version.loading_done"),
                            version.IsInfoValid() ? 1 : 0);

  GFModuleLogDebug("software information filled in rt");
}
