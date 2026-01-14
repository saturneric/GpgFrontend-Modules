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

BKTUSVersionCheckTask::BKTUSVersionCheckTask()
    : network_manager_(new QNetworkAccessManager(this)),
      current_version_(GFProjectVersion()) {
  qRegisterMetaType<SoftwareVersion>("SoftwareVersion");
  meta_.api = "BKTUS.com";
  meta_.current_version = current_version_;
  meta_.local_commit_hash = GFProjectGitCommitHash();

  connect(this, &BKTUSVersionCheckTask::SignalUpgradeVersion, this,
          [](const SoftwareVersion& sv) {
            GFDurableCacheSave(DUP("update_checking_cache"),
                               DUP(QJsonDocument(sv.ToJson()).toJson()));
          });
}

auto BKTUSVersionCheckTask::Run() -> int {
  QString base_url = "";

  QList<QUrl> urls = {
      {"https://ftp.bktus.com/GpgFrontend/appcast.xml"},
      {"https://git.bktus.com/gpgfrontend/gpgfrontend/atom/?h=" +
       current_version_},
      {"https://git.bktus.com/gpgfrontend/gpgfrontend/atom/?h=" +
       meta_.local_commit_hash},
  };

  connect(network_manager_, &QNetworkAccessManager::finished, this,
          &BKTUSVersionCheckTask::slot_parse_reply);

  int index = 0;
  for (const QUrl& url : urls) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      GFHttpRequestUserAgent());
    auto* reply = network_manager_->get(request);
    reply->setProperty("GFCheckIndex", index++);
    replies_.append(reply);
  }

  return 0;
}

void BKTUSVersionCheckTask::slot_parse_reply(QNetworkReply* reply) {
  if (reply->error() == QNetworkReply::NoError) {
    auto index = reply->property("GFCheckIndex").toInt();
    FLOG_DEBUG("get reply from url: %1, index: %2", reply->url().toString(),
               index);
    switch (index) {
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
    meta_.timestamp = QDateTime::currentDateTime();
    FillGrtWithVersionInfo(meta_);
    emit SignalUpgradeVersion(meta_);
  }
}

void BKTUSVersionCheckTask::slot_parse_latest_version_info(
    QNetworkReply* reply) {
  if (reply == nullptr || reply->error() != QNetworkReply::NoError) {
    return;
  }

  auto reply_bytes = reply->readAll();

  QDomDocument doc;
  auto xml_text = QString::fromUtf8(reply_bytes);
  auto result = doc.setContent(xml_text);
  if (!result) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    FLOG_WARN("xml parse failed: %1, line: %2, column: %3", result.errorMessage,
              result.errorLine, result.errorColumn);
#else
    LOG_WARN("xml parse failed");
#endif
    return;
  }

  auto root = doc.documentElement();
  auto channel = root.firstChildElement("channel");
  auto items = channel.elementsByTagName("item");

  FLOG_DEBUG("appcast.xml items: %1", items.size());
  if (items.size() == 0) {
    LOG_WARN("no xml entry of latest version");
    return;
  }

  auto item = items.at(0).toElement();
  auto title = item.firstChildElement("title").text();
  auto pub_date = item.firstChildElement("pubDate").text();
  auto desc = item.firstChildElement("description").text();

  auto latest_version = title;
  FLOG_DEBUG("raw tag name from bktus: %1", latest_version);

  QRegularExpression re(R"(^[vV](\d+\.)?(\d+\.)?(\*|\d+))");
  auto version_match = re.match(latest_version);
  if (version_match.hasMatch()) {
    latest_version = version_match.captured(0);
  } else {
    latest_version = "";
    FLOG_WARN("fail to match regex rules for release name from bktus: %1",
              latest_version);
  }

  const auto& publish_date = pub_date;
  const auto& release_note = desc;

  meta_.latest_version = latest_version;
  meta_.publish_date = publish_date;
  meta_.release_note = release_note;
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
  QString xml_text = QString::fromUtf8(reply_bytes);
  auto result = doc.setContent(xml_text);
  if (!result) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    FLOG_WARN("xml parse failed: %1, line: %2, column: %3", result.errorMessage,
              result.errorLine, result.errorColumn);
#else
    LOG_WARN("xml parse failed");
#endif
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

  FLOG_DEBUG("got tag info from bktus: %1, %2, %3", title_text, id_text,
             published_text);
  meta_.current_version_publish_in_remote = true;
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
  QString xml_text = QString::fromUtf8(reply_bytes);
  auto result = doc.setContent(xml_text);
  if (!result) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    FLOG_WARN("xml parse failed: %1, line: %2, column: %3", result.errorMessage,
              result.errorLine, result.errorColumn);
#else
    LOG_WARN("xml parse failed");
#endif
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

  // the new form of id_text is like: urn:sha1:abcdef1234567890...
  meta_.current_commit_hash_publish_in_remote =
      id_text.trimmed().contains(meta_.local_commit_hash.trimmed());
}
