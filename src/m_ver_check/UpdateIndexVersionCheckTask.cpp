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

#include "UpdateIndexVersionCheckTask.h"

#include <GFSDKBasic.h>
#include <GFSDKExtra.h>
#include <GFSDKLog.h>

#include <QMetaType>
#include <QtNetwork>

#include "GFModuleCommonUtils.hpp"
#include "Utils.h"

namespace {

/// The signed update index the new update pipeline publishes. Only its
/// stable channel is read here.
constexpr const char* kUpdateIndexUrl =
    "https://update.bktus.com/gpgfrontend/v1/index.json";

/// The largest index this reads; the real one is a few kilobytes.
constexpr qint64 kMaxIndexSize = 4 * 1024 * 1024;

/// "v2.1.14" or "2.1.14" as the index spells it: no "v", core only.
auto IndexSpelling(QString version) -> QString {
  if (version.startsWith('v') || version.startsWith('V')) version.remove(0, 1);
  return version;
}

}  // namespace

UpdateIndexVersionCheckTask::UpdateIndexVersionCheckTask()
    : network_manager_(new QNetworkAccessManager(this)) {
  qRegisterMetaType<SoftwareVersion>("SoftwareVersion");
  meta_.api = "update.bktus.com";
  meta_.current_version = GFProjectVersion();
  meta_.local_commit_hash = GFProjectGitCommitHash();
  // The index lists releases, not commits: an official build is one whose
  // version is listed, which current_version_publish_in_remote says.
  meta_.current_commit_hash_publish_in_remote = true;
}

void UpdateIndexVersionCheckTask::Run() {
  connect(network_manager_, &QNetworkAccessManager::finished, this,
          &UpdateIndexVersionCheckTask::slot_parse_reply);

  QNetworkRequest request(QUrl(QString::fromLatin1(kUpdateIndexUrl)));
  request.setHeader(QNetworkRequest::UserAgentHeader,
                    GFHttpRequestUserAgent());
  request.setTransferTimeout(30000);
  network_manager_->get(request);
}

auto UpdateIndexVersionCheckTask::ParseIndex(const QByteArray& index,
                                             SoftwareVersion& meta) -> bool {
  const auto doc = QJsonDocument::fromJson(index);
  if (!doc.isObject()) return false;
  const auto root = doc.object();
  if (root.value("schema").toInt() != 1 ||
      root.value("product").toString() != "gpgfrontend") {
    return false;
  }

  // Always a final release: the newest of the stable series. Prereleases and
  // nightlies are never read, so the plain numeric comparison of
  // GFCompareSoftwareVersion() holds.
  const auto stable =
      root.value("channels").toObject().value("stable").toString();
  static const QRegularExpression kFinal(R"(^\d+\.\d+\.\d+$)");
  if (!kFinal.match(stable).hasMatch()) return false;
  meta.latest_version = "v" + stable;

  const auto current = IndexSpelling(meta.current_version);
  auto listed = root.value("releases").toObject().contains(current);
  for (const auto& entry : root.value("withdrawn").toArray()) {
    if (entry.toObject().value("version").toString() == current) {
      listed = false;
    }
  }
  meta.current_version_publish_in_remote = listed;
  return true;
}

void UpdateIndexVersionCheckTask::slot_parse_reply(QNetworkReply* reply) {
  if (reply->error() == QNetworkReply::NoError) {
    const auto body = reply->read(kMaxIndexSize + 1);
    if (body.size() > kMaxIndexSize || !ParseIndex(body, meta_)) {
      FLOG_WARN("the update index from %1 could not be read",
                reply->url().toString());
      meta_.latest_version.clear();
    }
  } else {
    FLOG_WARN("cannot fetch the update index from %1: %2",
              reply->url().toString(), reply->errorString());
  }
  reply->deleteLater();

  FLOG_INFO("update index: latest stable %1, %2 listed: %3",
            meta_.latest_version, meta_.current_version,
            meta_.current_version_publish_in_remote ? "yes" : "no");
  meta_.timestamp = QDateTime::currentDateTime();
  FillGrtWithVersionInfo(meta_);
  emit SignalUpgradeVersion(meta_);
}
