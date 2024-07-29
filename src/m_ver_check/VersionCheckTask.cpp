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
  version_.current_version = current_version_;
}

auto VersionCheckTask::Run() -> int {
  QString latest_version_url =
      "https://api.github.com/repos/saturneric/gpgfrontend/releases/latest";

  QNetworkRequest latest_request(latest_version_url);
  latest_request.setHeader(QNetworkRequest::UserAgentHeader,
                           GFHttpRequestUserAgent());

  latest_reply_ = network_manager_->get(latest_request);
  connect(latest_reply_, &QNetworkReply::finished, this,
          &VersionCheckTask::slot_parse_latest_version_info);
  return 0;
}

void VersionCheckTask::slot_parse_latest_version_info() {
  if (latest_reply_ == nullptr) {
    version_.latest_version = current_version_;
    version_.loading_done = false;
  } else if (latest_reply_->error() != QNetworkReply::NoError) {
    MLogError(QString("latest version request error: %1")
                  .arg(latest_reply_->errorString()));
    version_.latest_version = current_version_;
  } else {
    latest_reply_bytes_ = latest_reply_->readAll();
    auto latest_reply_json = QJsonDocument::fromJson(latest_reply_bytes_);

    if (latest_reply_json.isObject()) {
      QString latest_version = latest_reply_json["tag_name"].toString();

      QRegularExpression re(R"(^[vV](\d+\.)?(\d+\.)?(\*|\d+))");
      auto version_match = re.match(latest_version);
      if (version_match.hasMatch()) {
        latest_version = version_match.captured(0);
      } else {
        latest_version = current_version_;
        MLogWarn(QString("latest version unknown, set to current version: %1")
                     .arg(current_version_));
      }

      bool prerelease = latest_reply_json["prerelease"].toBool();
      bool draft = latest_reply_json["draft"].toBool();
      auto publish_date = latest_reply_json["published_at"].toString();
      auto release_note = latest_reply_json["body"].toString();
      version_.latest_version = latest_version;
      version_.latest_prerelease_version_from_remote = prerelease;
      version_.latest_draft_from_remote = draft;
      version_.publish_date = publish_date;
      version_.release_note = release_note;
    } else {
      MLogWarn(QString("cannot parse data got from github: %1")
                   .arg(latest_reply_bytes_));
    }
  }

  if (latest_reply_ != nullptr) {
    latest_reply_->deleteLater();
  }

  try {
    QString current_version_url =
        "https://api.github.com/repos/saturneric/gpgfrontend/releases/tags/" +
        current_version_;

    QNetworkRequest current_request(current_version_url);
    current_request.setHeader(QNetworkRequest::UserAgentHeader,
                              GFHttpRequestUserAgent());

    current_reply_ = network_manager_->get(current_request);

    connect(current_reply_, &QNetworkReply::finished, this,
            &VersionCheckTask::slot_parse_current_version_info);
  } catch (...) {
    GFModuleLogError("current version request create error");
  }
}

void VersionCheckTask::slot_parse_current_version_info() {
  if (current_reply_ == nullptr) {
    // loading done
    version_.loading_done = false;

  } else if (current_reply_->error() != QNetworkReply::NoError) {
    MLogError(QString("current version request network error: {}")
                  .arg(current_reply_->errorString()));

    // loading done
    version_.loading_done = true;
    version_.current_version_publish_in_remote = false;
  } else {
    version_.current_version_publish_in_remote = true;
    current_reply_bytes_ = current_reply_->readAll();
    auto current_reply_json = QJsonDocument::fromJson(current_reply_bytes_);

    if (current_reply_json.isObject()) {
      bool current_prerelease = current_reply_json["prerelease"].toBool();
      bool current_draft = current_reply_json["draft"].toBool();
      version_.latest_prerelease_version_from_remote = current_prerelease;
      version_.latest_draft_from_remote = current_draft;
      // loading done
      version_.loading_done = true;
    } else {
      MLogWarn(QString("cannot parse data got from github: %1")
                   .arg(current_reply_bytes_));
    }
  }

  if (current_reply_ != nullptr) current_reply_->deleteLater();

  slot_fill_grt_with_version_info(version_);
  emit SignalUpgradeVersion(version_);
}

void VersionCheckTask::slot_fill_grt_with_version_info(
    const SoftwareVersion &version) {
  GFModuleLogDebug("filling software information info in rt...");

  GFModuleUpsertRTValue(GFGetModuleID(),
                        GFModuleStrDup("version.current_version"),
                        GFModuleStrDup(version.current_version.toUtf8()));
  GFModuleUpsertRTValue(GFGetModuleID(),
                        GFModuleStrDup("version.latest_version"),
                        GFModuleStrDup(version.latest_version.toUtf8()));
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
                            GFModuleStrDup("version.loading_done"),
                            version.loading_done ? 1 : 0);

  GFModuleLogDebug("software information filled in rt");
}
