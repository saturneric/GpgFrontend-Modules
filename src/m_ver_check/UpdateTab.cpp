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
#include "VersionCheckTask.h"
#include "VersionCheckingModule.h"

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
  upgrade_info_layout->addWidget(pb_);
  upgrade_info_box_->setLayout(upgrade_info_layout);

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

  setLayout(layout);
}

void UpdateTab::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);

  auto is_loading_done = GFModuleRetrieveRTValueOrDefaultBool(
      GFGetModuleID(), GFModuleStrDup("version.loading_done"), 0);

  if (is_loading_done == 0) {
    auto* task = new VersionCheckTask();
    QObject::connect(
        task, &VersionCheckTask::SignalUpgradeVersion, QThread::currentThread(),
        [this](const SoftwareVersion&) { slot_show_version_status(); });
    QObject::connect(task, &VersionCheckTask::SignalUpgradeVersion, task,
                     &QObject::deleteLater);
    task->Run();

  } else {
    slot_show_version_status();
  }
}

void UpdateTab::slot_show_version_status() {
  this->pb_->setHidden(true);

  auto is_loading_done = GFModuleRetrieveRTValueOrDefaultBool(
      GFGetModuleID(), GFModuleStrDup("version.loading_done"), 0);

  if (is_loading_done == 0) {
    MLogDebug("version info loading haven't been done yet.");

    upgrade_label_->setText(
        "<center>" +
        tr("Unable to retrieve the latest version information. This may be "
           "due "
           "to a network issue or the server being unavailable.") +
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
      GFGetModuleID(), GFModuleStrDup("version.need_upgrade"), 0);

  auto is_current_a_withdrawn_version = GFModuleRetrieveRTValueOrDefaultBool(
      GFGetModuleID(), GFModuleStrDup("version.current_a_withdrawn_version"),
      0);

  auto is_current_version_released = GFModuleRetrieveRTValueOrDefaultBool(
      GFGetModuleID(), GFModuleStrDup("version.current_version_released"), 0);

  auto is_git_commit_hash_mismatch = GFModuleRetrieveRTValueOrDefaultBool(
      GFGetModuleID(), GFModuleStrDup("version.git_commit_hash_mismatch"), 0);

  QString const latest_version = UDUP(GFModuleRetrieveRTValueOrDefault(
      GFGetModuleID(), GFModuleStrDup("version.latest_version"),
      GFModuleStrDup("")));

  QString const release_note = UDUP(GFModuleRetrieveRTValueOrDefault(
      GFGetModuleID(), GFModuleStrDup("version.release_note"),
      GFModuleStrDup("")));

  FLOG_INFO("latest version from GitHub: %1", latest_version);

  latest_version_label_->setText("<center><b>" +
                                 tr("Latest Version From Github") + ": " +
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
  } else if (is_current_a_withdrawn_version != 0) {
    upgrade_label_->setText(
        "<center>" +
        tr("This version has critical issues and has been withdrawn. Please "
           "stop using it immediately.") +
        "</center><center>" + tr("Click") +
        " <a "
        "href=\"https://www.gpgfrontend.bktus.com/overview/downloads/\">" +
        tr("here") + "</a> " + tr("to download the latest stable version.") +
        "</center>");
    upgrade_label_->show();
    upgrade_info_box_->show();
  } else if (!latest_version.trimmed().isEmpty() &&
             is_current_version_released == 0) {
    upgrade_label_->setText(
        "<center>" +
        tr("This is an unreleased version, possibly a beta. If stability is "
           "important to you, please avoid using this version.") +
        "</center><center>" + tr("Click") +
        " <a "
        "href=\"https://www.gpgfrontend.bktus.com/overview/downloads/\">" +
        tr("here") + "</a> " + tr("to download the latest stable version.") +
        "</center>");
    upgrade_label_->show();
    upgrade_info_box_->show();
  } else if (is_git_commit_hash_mismatch != 0 && !GFIsFlatpakENV()) {
    upgrade_label_->setText(
        "<center>" +
        tr("The current version's commit hash does not match the official "
           "release. This may indicate a modified or unofficial build.") +
        "</center><center>" + tr("Click") +
        " <a "
        "href=\"https://www.gpgfrontend.bktus.com/overview/downloads/\">" +
        tr("here") + "</a> " +
        tr("to verify your installation or download the official version.") +
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