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

#include "VersionCheckingModule.h"

#include <GFSDKBasic.h>
#include <GFSDKBuildInfo.h>
#include <GFSDKExtra.h>
#include <GFSDKLog.h>
#include <GFSDKUI.h>

#include <QMetaType>
#include <QtNetwork>

#include "BKTUSVersionCheckTask.h"
#include "GFModuleCommonUtils.hpp"
#include "GFModuleDefine.h"
#include "GitHubVersionCheckTask.h"
#include "SoftwareVersion.h"
#include "UpdateTab.h"
#include "Utils.h"

GF_MODULE_API_DEFINE_V2("com.bktus.gpgfrontend.module.version_checking",
                        "VersionChecking", "1.4.1",
                        "Try checking GpgFrontend version.", "Saturneric");

DEFINE_TRANSLATIONS_STRUCTURE(ModuleVersionChecking);

auto GFRegisterModule() -> int {
  MLogInfo("version checking module registering");

  REGISTER_TRANS_READER();
  return 0;
}

auto GFActiveModule() -> int {
  MLogInfo("version checking module activating");

  LISTEN("MAINWINDOW_MENU_MOUNTED");
  LISTEN("APPLICATION_LOADED");
  LISTEN("NETWORK_SETTINGS_TAB_UI_CREATED");
  LISTEN("NETWORK_SETTINGS_TAB_LOAD_SETTINGS");
  LISTEN("NETWORK_SETTINGS_TAB_APPLY_SETTINGS");

  return 0;
}

namespace {

auto CheckUpdate(const QMap<QString, QString>& event) -> int {
  if (event["api"] == "bktus") {
    MLogInfo("checking updating using api of bktus.com");
    auto* task = new BKTUSVersionCheckTask();
    QObject::connect(
        task, &BKTUSVersionCheckTask::SignalUpgradeVersion,
        QThread::currentThread(), [event](const SoftwareVersion& sv) {
          GFDurableCacheSave(DUP("update_checking_cache"),
                             DUP(QJsonDocument(sv.ToJson()).toJson()));
          CB_SUCC(event);
        });
    QObject::connect(task, &BKTUSVersionCheckTask::SignalUpgradeVersion, task,
                     &QObject::deleteLater);
    task->Run();
  } else {
    MLogInfo("checking updating using api of github.com");
    auto* task = new GitHubVersionCheckTask();
    QObject::connect(
        task, &GitHubVersionCheckTask::SignalUpgradeVersion,
        QThread::currentThread(), [event](const SoftwareVersion& sv) {
          GFDurableCacheSave(DUP("update_checking_cache"),
                             DUP(QJsonDocument(sv.ToJson()).toJson()));
          CB_SUCC(event);
        });
    QObject::connect(task, &GitHubVersionCheckTask::SignalUpgradeVersion, task,
                     &QObject::deleteLater);
    task->Run();
  }
  return 0;
}

auto RaiseUpdateDialog(QWidget* parent) -> QDialog* {
  auto* dialog = new QDialog(parent);
  dialog->setWindowTitle(
      QCoreApplication::translate("GTrC", "Check for Updates"));
  dialog->setModal(true);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  auto* layout = new QVBoxLayout();
  auto* update_tab = new UpdateTab(dialog);
  layout->addWidget(update_tab);
  dialog->setLayout(layout);
  dialog->resize(500, 600);
  dialog->show();
  return dialog;
}

}  // namespace

REGISTER_EVENT_HANDLER(MAINWINDOW_MENU_MOUNTED, [](const MEvent& event) -> int {
  LOG_DEBUG("main window menu mounted event: processing");

  if (!event.contains("main_window")) {
    LOG_DEBUG("main window menu mounted event: no main_window found");
    CB_ERR(event, -1, "no main_window found");
  }

  auto* main_window = GFUIGetGUIObjectAs<QMainWindow>(event["main_window"]);
  if (!main_window) {
    LOG_ERROR(
        "main window menu mounted: main_window handle invalid or not "
        "QMainWindow");
    CB_ERR(event, -1, "main_window handle invalid or not QMainWindow");
  }

  if (!event.contains("help_menu")) {
    LOG_DEBUG("main window menu mounted event: no help_menu found");
    CB_ERR(event, -1, "no help_menu found");
  }

  auto* help_menu = GFUIGetGUIObjectAs<QMenu>(event["help_menu"]);
  if (!help_menu) {
    LOG_ERROR(
        "main window menu mounted: help_menu handle invalid or not "
        "QMenu");
    CB_ERR(event, -1, "help_menu handle invalid or not QMenu");
  }

  LOG_DEBUG("adding check update action to help menu");

  QMetaObject::invokeMethod(
      QApplication::instance(),
      [&]() -> void {
        QWidget* parent =
            qobject_cast<QWidget*>(static_cast<QObject*>(main_window));
        auto* action = new QAction(
            QCoreApplication::translate("GTrC", "Check for Updates"), nullptr);
        action->setToolTip(QCoreApplication::translate(
            "GTrC", "Check for updates from the Internet."));
        action->setIcon(QIcon(":/icons/update.png"));
        QObject::connect(action, &QAction::triggered, parent,
                         [=]() { RaiseUpdateDialog(parent); });
        help_menu->addAction(action);
      },
      Qt::BlockingQueuedConnection);
  CB_SUCC(event);
});

REGISTER_EVENT_HANDLER(APPLICATION_LOADED, [](const MEvent& event) -> int {
  LOG_DEBUG("application starting completed event: processing");

  auto* parent = GFUIGetGUIObjectAs<QWidget>(
      event.contains("main_window") ? event["main_window"] : "");
  if (!parent) {
    LOG_ERROR("application loaded: main_window handle invalid or not QWidget");
    CB_ERR(event, -1, "main_window handle invalid or not QWidget");
  }

  // check version information
  auto settings =
      qobject_cast<QSettings*>(static_cast<QObject*>(GFUIGlobalSettings()));
  if (!settings) {
    LOG_ERROR("application loaded: global settings handle invalid");
    CB_ERR(event, -1, "global settings handle invalid");
  }

  // ensure that it will not perform at the first startup before wizard is done
  if (!settings->contains("network/prohibit_update_checking")) {
    LOG_DEBUG(
        "application loaded: prohibit_update_checking setting "
        "not found");
    CB_SUCC(event);
  }

  auto update_checking_api =
      settings->value("network/update_checking_api", "github").toString();
  FLOG_DEBUG("application loaded: update checking api: %1",
             update_checking_api);

  // we only check for update if the user did set the option to allow it
  auto prohibit_update_checking =
      settings->value("network/prohibit_update_checking", true).toBool();
  FLOG_DEBUG("application loaded: prohibit update checking: %1",
             prohibit_update_checking);

  if (prohibit_update_checking) {
    LOG_DEBUG("application loaded: update checking is prohibited");
    CB_SUCC(event);
  }

  auto cache = UDUP(GFDurableCacheGet(DUP("update_checking_cache")));
  auto json = QJsonDocument::fromJson(cache.toUtf8());

  if (json.isEmpty() || !json.isObject()) {
    LOG_DEBUG(
        "application loaded: no valid cached version info found, "
        "checking update");
    return CheckUpdate(event);
  }

  SoftwareVersion sv;
  sv.FromJson(json.object());

  FLOG_DEBUG("got software version meta data: %1", json.toJson());
  if (sv.timestamp.addDays(1) < QDateTime::currentDateTime()) {
    return CheckUpdate(event);
  }

  FillGrtWithVersionInfo(sv);

  if (sv.NeedUpgrade() || !sv.CurrentVersionReleased() ||
      !sv.current_commit_hash_publish_in_remote) {
    LOG_INFO(
        "software version is outdated or not fully released, notifying user");

    QMetaObject::invokeMethod(
        QApplication::instance(),
        [=]() {
          auto* dialog = RaiseUpdateDialog(parent);
          Q_UNUSED(dialog);
        },
        Qt::QueuedConnection);
  }

  CB_SUCC(event);
});

REGISTER_EVENT_HANDLER(
    NETWORK_SETTINGS_TAB_UI_CREATED, [](const MEvent& event) -> int {
      LOG_DEBUG("network settings tab ui created event: processing");

      auto* tab = GFUIGetGUIObjectAs<QWidget>(
          event.contains("network_settings_tab") ? event["network_settings_tab"]
                                                 : "");
      if (!tab) {
        LOG_ERROR(
            "network settings tab ui created: network_settings_tab handle "
            "invalid or not QWidget");
        CB_ERR(event, -1, "network_settings_tab handle invalid or not QWidget");
      }

      auto* capability_group_box = GFUIGetGUIObjectAs<QGroupBox>(
          event.contains("capability_group_box") ? event["capability_group_box"]
                                                 : "");

      if (!capability_group_box) {
        LOG_ERROR(
            "network settings tab ui created: capability_group_box handle "
            "invalid or not QGroupBox");
        CB_ERR(event, -1,
               "capability_group_box handle invalid or not QGroupBox");
      }

      QMetaObject::invokeMethod(
          QApplication::instance(),
          [=]() {
            auto* update_checking_check_box =
                new QCheckBox(QCoreApplication::translate(
                                  "GTrC",
                                  "Checking for version updates when the "
                                  "application starts."),
                              capability_group_box);
            update_checking_check_box->setObjectName(
                "update_checking_check_box");

            capability_group_box->layout()->addWidget(
                update_checking_check_box);

            auto* github_radio_button =
                new QRadioButton(QCoreApplication::translate("GTrC", "GitHub"),
                                 capability_group_box);
            auto* bktus_radio_button = new QRadioButton(
                QCoreApplication::translate("GTrC", "BKTUS.com"),
                capability_group_box);

            auto layout = new QHBoxLayout();
            layout->addWidget(new QLabel(
                QCoreApplication::translate("GTrC", "Update Checking API:"),
                capability_group_box));
            layout->addWidget(github_radio_button);
            layout->addWidget(bktus_radio_button);

            capability_group_box->layout()->addItem(layout);

            auto* update_api_group = new QButtonGroup(tab);
            update_api_group->setObjectName("update_api_group");
            update_api_group->addButton(github_radio_button);
            update_api_group->addButton(bktus_radio_button);
          },
          Qt::QueuedConnection);

      CB_SUCC(event);
    });

REGISTER_EVENT_HANDLER(
    NETWORK_SETTINGS_TAB_APPLY_SETTINGS, [](const MEvent& event) -> int {
      LOG_DEBUG("network settings tab apply settings event: processing");
      auto* settings =
          qobject_cast<QSettings*>(static_cast<QObject*>(GFUIGlobalSettings()));
      if (!settings) {
        LOG_ERROR(
            "network settings tab apply settings: global settings handle "
            "invalid");
        CB_ERR(event, -1, "global settings handle invalid");
      }
      auto* tab = GFUIGetGUIObjectAs<QWidget>(
          event.contains("network_settings_tab") ? event["network_settings_tab"]
                                                 : "");
      if (!tab) {
        LOG_ERROR(
            "network settings tab apply settings: network_settings_tab "
            "handle invalid or not QWidget");
        CB_ERR(event, -1, "network_settings_tab handle invalid or not QWidget");
      }

      QMetaObject::invokeMethod(
          QApplication::instance(),
          [=]() {
            auto* update_checking_check_box =
                tab->findChild<QCheckBox*>("update_checking_check_box");
            if (update_checking_check_box) {
              settings->setValue(
                  "network/version_checking/"
                  "check_for_updates_on_startup",
                  update_checking_check_box->isChecked());
            }

            auto* update_api_group =
                tab->findChild<QButtonGroup*>("update_api_group");
            if (update_api_group) {
              QString api = "github";
              if (update_api_group->buttons().at(1)->isChecked()) {
                api = "bktus";
              }
              settings->setValue("network/version_checking/update_checking_api",
                                 api);
            }
          },
          Qt::BlockingQueuedConnection);

      CB_SUCC(event);
    });

REGISTER_EVENT_HANDLER(
    NETWORK_SETTINGS_TAB_LOAD_SETTINGS, [](const MEvent& event) -> int {
      LOG_DEBUG("network settings tab load settings event: processing");

      auto* settings =
          qobject_cast<QSettings*>(static_cast<QObject*>(GFUIGlobalSettings()));

      if (!settings) {
        LOG_ERROR(
            "network settings tab load settings: global settings handle "
            "invalid");
        CB_ERR(event, -1, "global settings handle invalid");
      }

      auto* tab = GFUIGetGUIObjectAs<QWidget>(
          event.contains("network_settings_tab") ? event["network_settings_tab"]
                                                 : "");
      if (!tab) {
        LOG_ERROR(
            "network settings tab load settings: network_settings_tab "
            "handle invalid or not QWidget");
        CB_ERR(event, -1, "network_settings_tab handle invalid or not QWidget");
      }

      QMetaObject::invokeMethod(
          QApplication::instance(),
          [=]() {
            auto* update_checking_check_box =
                tab->findChild<QCheckBox*>("update_checking_check_box");
            if (update_checking_check_box) {
              auto check_for_updates_on_startup =
                  settings
                      ->value(
                          "network/version_checking/"
                          "check_for_updates_on_startup",
                          false)
                      .toBool();
              update_checking_check_box->setChecked(
                  check_for_updates_on_startup);
            }

            auto* update_api_group =
                tab->findChild<QButtonGroup*>("update_api_group");
            if (update_api_group) {
              auto update_checking_api =
                  settings
                      ->value("network/version_checking/update_checking_api",
                              "github")
                      .toString();
              if (update_checking_api == "github") {
                update_api_group->buttons().at(0)->setChecked(true);
              } else {
                update_api_group->buttons().at(1)->setChecked(true);
              }
            }
          },
          Qt::BlockingQueuedConnection);

      CB_SUCC(event);
    });

auto GFDeactivateModule() -> int { return 0; }

auto GFUnregisterModule() -> int { return 0; }