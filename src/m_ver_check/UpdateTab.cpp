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
#include "GFSDKLog.h"
#include "GFSDKModule.h"
#include "VersionCheckTask.h"
#include "VersionCheckingModule.h"

UpdateTab::UpdateTab(QWidget* parent) : QWidget(parent) {
  auto* layout = new QGridLayout();

  current_version_ = GFProjectVersion();

  auto* tips_label = new QLabel();
  tips_label->setText(
      "<center>" +
      tr("It is recommended that you always check the version "
         "of GpgFrontend and upgrade to the latest version.") +
      "</center><center>" +
      tr("New versions not only represent new features, but "
         "also often represent functional and security fixes.") +
      "</center>");
  tips_label->setWordWrap(true);

  current_version_label_ = new QLabel();
  current_version_label_->setText("<center>" + tr("Current Version") +
                                  tr(": ") + "<b>" + current_version_ +
                                  "</b></center>");
  current_version_label_->setWordWrap(true);

  latest_version_label_ = new QLabel();
  latest_version_label_->setWordWrap(true);

  upgrade_label_ = new QLabel();
  upgrade_label_->setWordWrap(true);
  upgrade_label_->setOpenExternalLinks(true);
  upgrade_label_->setHidden(true);

  pb_ = new QProgressBar();
  pb_->setRange(0, 0);
  pb_->setTextVisible(false);

  layout->addWidget(tips_label, 1, 0, 1, -1);
  layout->addWidget(current_version_label_, 2, 0, 1, -1);
  layout->addWidget(latest_version_label_, 3, 0, 1, -1);
  layout->addWidget(upgrade_label_, 4, 0, 1, -1);
  layout->addWidget(pb_, 5, 0, 1, -1);
  layout->addItem(
      new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed), 2, 1,
      1, 1);

  setLayout(layout);
}

void UpdateTab::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  MLogDebug("loading version loading info from rt");

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
  MLogDebug("loading version info from rt");
  this->pb_->setHidden(true);

  auto is_loading_done = GFModuleRetrieveRTValueOrDefaultBool(
      GFGetModuleID(), GFModuleStrDup("version.loading_done"), 0);

  if (is_loading_done == 0) {
    MLogDebug("version info loading haven't been done yet.");
    return;
  }

  auto is_need_upgrade = GFModuleRetrieveRTValueOrDefaultBool(
      GFGetModuleID(), GFModuleStrDup("version.need_upgrade"), 0);

  auto is_current_a_withdrawn_version = GFModuleRetrieveRTValueOrDefaultBool(
      GFGetModuleID(), GFModuleStrDup("version.current_a_withdrawn_version"),
      0);

  auto is_current_version_released = GFModuleRetrieveRTValueOrDefaultBool(
      GFGetModuleID(), GFModuleStrDup("version.current_version_released"), 0);

  QString const latest_version = UDUP(GFModuleRetrieveRTValueOrDefault(
      GFGetModuleID(), GFModuleStrDup("version.latest_version"),
      GFModuleStrDup("")));

  latest_version_label_->setText("<center><b>" +
                                 tr("Latest Version From Github") + ": " +
                                 latest_version + "</b></center>");

  if (is_need_upgrade != 0) {
    upgrade_label_->setText(
        "<center>" +
        tr("The current version is less than the latest version on "
           "github.") +
        "</center><center>" + tr("Please click") +
        " <a "
        "href=\"https://www.gpgfrontend.bktus.com/#/downloads\">" +
        tr("Here") + "</a> " + tr("to download the latest stable version.") +
        "</center>");
    upgrade_label_->show();
  } else if (is_current_a_withdrawn_version != 0) {
    upgrade_label_->setText(
        "<center>" +
        tr("This version has serious problems and has been withdrawn. "
           "Please stop using it immediately.") +
        "</center><center>" + tr("Please click") +
        " <a "
        "href=\"https://github.com/saturneric/GpgFrontend/releases\">" +
        tr("Here") + "</a> " + tr("to download the latest stable version.") +
        "</center>");
    upgrade_label_->show();
  } else if (is_current_version_released == 0) {
    upgrade_label_->setText(
        "<center>" +
        tr("This version has not been released yet, it may be a beta "
           "version. If you are not a tester and care about version "
           "stability, please do not use this version.") +
        "</center><center>" + tr("Please click") +
        " <a "
        "href=\"https://www.gpgfrontend.bktus.com/#/downloads\">" +
        tr("Here") + "</a> " + tr("to download the latest stable version.") +
        "</center>");
    upgrade_label_->show();
  }
}

auto UpdateTabFactory(const char* id) -> void* { return new UpdateTab(); }