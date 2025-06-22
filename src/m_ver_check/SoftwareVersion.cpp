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

#include "SoftwareVersion.h"

#include <GFSDKBasic.h>
#include <GFSDKExtra.h>
#include <GFSDKLog.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

auto SoftwareVersion::NeedUpgrade() const -> bool {
  return !latest_version.isEmpty() &&
         GFCompareSoftwareVersion(GFModuleStrDup(current_version.toUtf8()),
                                  GFModuleStrDup(latest_version.toUtf8())) < 0;
}

auto SoftwareVersion::VersionWithdrawn() const -> bool {
  return !latest_version.isEmpty() && !current_version_publish_in_remote;
}

auto SoftwareVersion::CurrentVersionReleased() const -> bool {
  return !latest_version.isEmpty() && current_version_publish_in_remote;
}

auto SoftwareVersion::ToJson() const -> QJsonObject {
  QJsonObject obj;
  obj["api"] = api;
  obj["latest_version"] = latest_version;
  obj["current_version"] = current_version;
  obj["current_version_publish_in_remote"] = current_version_publish_in_remote;
  obj["current_commit_hash_publish_in_remote"] =
      current_commit_hash_publish_in_remote;
  obj["publish_date"] = publish_date;
  obj["release_note"] = release_note;
  obj["local_commit_hash"] = local_commit_hash;
  obj["timestamp"] = timestamp.toSecsSinceEpoch();
  return obj;
}

void SoftwareVersion::FromJson(const QJsonObject& obj) {
  api = obj.value("api").toString();
  latest_version = obj.value("latest_version").toString();
  current_version = obj.value("current_version").toString();
  current_version_publish_in_remote =
      obj.value("current_version_publish_in_remote").toBool();
  current_commit_hash_publish_in_remote =
      obj.value("current_commit_hash_publish_in_remote").toBool();
  publish_date = obj.value("publish_date").toString();
  release_note = obj.value("release_note").toString();
  local_commit_hash = obj.value("local_commit_hash").toString();
  timestamp = QDateTime::fromSecsSinceEpoch(obj.value("timestamp").toInt());
}
