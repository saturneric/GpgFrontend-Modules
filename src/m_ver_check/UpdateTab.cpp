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

#include "UpdateTab.h"

#include "GFModuleCommonUtils.hpp"
#include "GFSDKBasic.h"
#include "GFSDKModule.h"
#include "VersionCheckingModule.h"

//
#include "BKTUSVersionCheckTask.h"
#include "GitHubVersionCheckTask.h"

UpdateTab::UpdateTab(QWidget* parent)
    : QWidget(parent), current_version_(GFProjectVersion()) {
  auto* layout = new QVBoxLayout();

  current_version_box_ = new QGroupBox(tr("Current Version Information"));
  auto* current_version_layout = new QVBoxLayout();
  current_version_label_ = new QLabel();
  current_version_label_->setText("<center>" + tr("Current Version") +
                                  tr(": ") + "<b>" + current_version_ +
                                  "</b></center>");
  current_version_label_->setWordWrap(true);
  latest_version_label_ = new QLabel();
  current_version_layout->addWidget(current_version_label_);
  current_version_layout->addWidget(latest_version_label_);
  current_version_box_->setLayout(current_version_layout);

  upgrade_info_box_ = new QGroupBox(tr("Upgrade Information"));
  auto* upgrade_info_layout = new QVBoxLayout();
  upgrade_label_ = new QLabel();
  upgrade_label_->setWordWrap(true);
  upgrade_label_->setOpenExternalLinks(true);
  upgrade_label_->setHidden(true);

  pb_ = new QProgressBar();
  pb_->setRange(0, 0);
  pb_->setTextVisible(false);

  upgrade_info_layout->addWidget(upgrade_label_);
  upgrade_info_box_->setLayout(upgrade_info_layout);

  check_update_btn_ = new QPushButton(tr("Check for Updates"));
  check_update_btn_->setIcon(QIcon::fromTheme("view-refresh"));
  check_update_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  release_note_box_ = new QGroupBox(tr("Release Notes"));
  auto* release_note_layout = new QVBoxLayout();
  release_note_viewer_ = new QTextEdit();
  release_note_viewer_->setReadOnly(true);
  release_note_viewer_->setAcceptRichText(true);
  release_note_viewer_->hide();
  release_note_layout->addWidget(release_note_viewer_);
  release_note_box_->setLayout(release_note_layout);

  current_version_box_->hide();
  release_note_box_->hide();
  upgrade_info_box_->hide();

  layout->addWidget(current_version_box_);
  layout->addWidget(upgrade_info_box_);
  layout->addWidget(release_note_box_);

  auto* hbox = new QHBoxLayout();
  hbox->addWidget(pb_, 1);
  hbox->addWidget(check_update_btn_, 0, Qt::AlignRight);
  layout->addLayout(hbox);

  connect(check_update_btn_, &QPushButton::clicked, this,
          &UpdateTab::slot_check_version_update);

  setLayout(layout);

  slot_show_version_status();
}

void UpdateTab::slot_show_version_status() {
  check_update_btn_->setEnabled(true);
  this->pb_->setHidden(true);

  auto is_loading_done = GFModuleRetrieveRTValueOrDefaultBool(
      GFGetModuleID(), DUP("version.loading_done"), 0);

  if (is_loading_done == 0) {
    MLogDebug("version info loading haven't been done yet.");

    upgrade_label_->setText(
        "<center>" +
        tr("Unable to retrieve the latest version information. This may be "
           "due to a network issue or the server being unavailable.") +
        "</center><center>" +
        tr("Please check your internet connection or try again later.") +
        "</center><center>" + tr("Alternatively, you can visit the") +
        " <a "
        "href=\"https://www.gpgfrontend.bktus.com/overview/downloads/\">" +
        tr("official download page") + "</a> " +
        tr("to check for the latest stable version.") + "</center>");
    upgrade_label_->show();
    upgrade_info_box_->show();
    return;
  }

  auto is_need_upgrade = GFModuleRetrieveRTValueOrDefaultBool(
      GFGetModuleID(), DUP("version.need_upgrade"), 0);

  auto is_current_version_publish_in_remote =
      GFModuleRetrieveRTValueOrDefaultBool(
          GFGetModuleID(), DUP("version.current_version_publish_in_remote"), 0);

  auto is_current_commit_hash_publish_in_remote =
      GFModuleRetrieveRTValueOrDefaultBool(
          GFGetModuleID(), DUP("version.current_commit_hash_publish_in_remote"),
          0);

  QString const latest_version = UDUP(GFModuleRetrieveRTValueOrDefault(
      GFGetModuleID(), DUP("version.latest_version"), DUP("")));

  QString const release_note = UDUP(GFModuleRetrieveRTValueOrDefault(
      GFGetModuleID(), DUP("version.release_note"), DUP("")));

  QString const api = UDUP(GFModuleRetrieveRTValueOrDefault(
      GFGetModuleID(), DUP("version.api"), DUP("Unknown")));

  FLOG_INFO("latest version from remote: %1", latest_version);

  latest_version_label_->setText("<center><b>" +
                                 tr("Latest Version From %1").arg(api) + ": " +
                                 latest_version + "</b></center>");
  current_version_box_->show();

  if (is_need_upgrade != 0) {
    upgrade_label_->setText(
        "<center>" + tr("Your current version is outdated.") +
        "</center><center>" + tr("Click") +
        " <a "
        "href=\"https://www.gpgfrontend.bktus.com/overview/downloads/\">" +
        tr("here") + "</a> " + tr("to download the latest stable version.") +
        "</center>");
    upgrade_label_->show();
    upgrade_info_box_->show();
  } else if ((!latest_version.trimmed().isEmpty() &&
              is_current_version_publish_in_remote == 0)) {
    upgrade_label_->setText(
        "<center>" +
        tr("This version is either withdrawn due to critical issues or is an "
           "unreleased build. "
           "Please stop using it and download the latest stable version.") +
        "</center><center>" + tr("Click") +
        " <a href=\"https://www.gpgfrontend.bktus.com/overview/downloads/\">" +
        tr("here") + "</a> " + tr("to download the latest stable version.") +
        "</center>");
    upgrade_label_->show();
    upgrade_info_box_->show();
  } else if (is_current_commit_hash_publish_in_remote == 0) {
    upgrade_label_->setText(
        "<center>" +
        tr("The commit hash for this build was not found in the official "
           "repository. This may indicate a modified or unofficial version.") +
        "</center><center>" + tr("Click") +
        " <a "
        "href=\"https://www.gpgfrontend.bktus.com/overview/downloads/\">" +
        tr("here") + "</a> " +
        tr("to verify your installation or download the official build.") +
        "</center>");
    upgrade_label_->show();
    upgrade_info_box_->show();
  } else {
    upgrade_label_->setText("<center>" +
                            tr("You are using the latest stable version. No "
                               "action is required.") +
                            "</center>");
    upgrade_label_->show();
    upgrade_info_box_->show();
  }

  if (!release_note.trimmed().isEmpty()) {
    release_note_viewer_->clear();
    release_note_viewer_->setMarkdown(release_note);
    release_note_viewer_->show();
    release_note_box_->show();
  }
}

auto UpdateTabFactory(void*) -> void* { return new UpdateTab(); }

void UpdateTab::slot_check_version_update() {
  check_update_btn_->setEnabled(false);
  pb_->show();

  auto api = UDUP(GFModuleRetrieveRTValueOrDefault(
      DUP("ui"), DUP("settings.network.update_checking_api"), DUP("github")));

  if (api == "bktus") {
    auto* task = new BKTUSVersionCheckTask();
    connect(task, &BKTUSVersionCheckTask::SignalUpgradeVersion,
            QThread::currentThread(),
            [this](const SoftwareVersion&) { slot_show_version_status(); });
    connect(task, &BKTUSVersionCheckTask::SignalUpgradeVersion, task,
            &QObject::deleteLater);
    task->Run();
  } else {
    auto* task = new GitHubVersionCheckTask();
    connect(task, &GitHubVersionCheckTask::SignalUpgradeVersion,
            QThread::currentThread(),
            [this](const SoftwareVersion&) { slot_show_version_status(); });
    connect(task, &GitHubVersionCheckTask::SignalUpgradeVersion, task,
            &QObject::deleteLater);
    task->Run();
  }
}

void UpdateTab::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);

  auto is_loading_done = GFModuleRetrieveRTValueOrDefaultBool(
      GFGetModuleID(), DUP("version.loading_done"), 0);

  auto prohibit = GFModuleRetrieveRTValueOrDefaultBool(
      DUP("ui"), DUP("settings.network.prohibit_update_checking"), 0);

  if ((prohibit == 0) && is_loading_done == 0) {
    slot_check_version_update();
  } else {
    slot_show_version_status();
  }
}
