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

#include "GFModuleCommonUtils.hpp"
#include "GFModuleDefine.h"
#include "SoftwareVersion.h"
#include "UpdateIndexVersionCheckTask.h"
#include "UpdateTab.h"
#include "Utils.h"

GF_MODULE_API_DEFINE_V2("com.bktus.gpgfrontend.module.version_checking",
                        "VersionChecking", "1.5.0",
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

/// Where the last check's answer is kept, for a day. Not the 2.1.13 key: what
/// that version cached came from the GitHub and BKTUS checks.
constexpr const char* kCacheKey = "update_checking_cache_v2";
/// The stable version the user was last told about at startup.
constexpr const char* kNotifiedKey = "update_checking_notified_stable";

/// The host's setting, "network/prohibit_update_check", is the one the first
/// start wizard writes; the network settings tab of this module writes it
/// too. Checking at startup is off unless the user turned it on.
auto StartupCheckEnabled(QSettings* settings) -> bool {
  return !settings->value("network/prohibit_update_check", true).toBool();
}

auto CheckUpdate(const QMap<QString, QString>& event,
                 const std::function<void(const SoftwareVersion&)>& done = {})
    -> int {
  MLogInfo("checking for updates in the update index");
  auto* task = new UpdateIndexVersionCheckTask();
  QObject::connect(
      task, &UpdateIndexVersionCheckTask::SignalUpgradeVersion,
      QCoreApplication::instance(), [event, done](const SoftwareVersion& sv) {
        if (!sv.latest_version.isEmpty()) {
          GFDurableCacheSave(DUP(kCacheKey),
                             DUP(QJsonDocument(sv.ToJson()).toJson()));
        }
        if (done) done(sv);
        CB_SUCC(event);
      });
  QObject::connect(task, &UpdateIndexVersionCheckTask::SignalUpgradeVersion,
                   task, &QObject::deleteLater);
  task->Run();
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

  auto p_mw = QPointer<QMainWindow>(main_window);

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

  auto p_help_menu = QPointer<QMenu>(help_menu);

  LOG_DEBUG("adding check update action to help menu");

  QMetaObject::invokeMethod(
      QApplication::instance(),
      [&]() -> void {
        if (!p_mw || !p_help_menu) {
          LOG_ERROR(
              "main window menu mounted: main_window or help_menu was deleted "
              "before invoking the check update action");
          return;
        }

        QWidget* parent =
            qobject_cast<QWidget*>(static_cast<QObject*>(main_window));
        auto* action = new QAction(
            QCoreApplication::translate("GTrC", "Check for Updates"), nullptr);
        action->setToolTip(QCoreApplication::translate(
            "GTrC", "Check for updates from the Internet."));
        action->setIcon(QIcon(":/icons/update.png"));
        QObject::connect(action, &QAction::triggered, parent,
                         [=]() -> void { RaiseUpdateDialog(parent); });
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

  // We only check for updates if the user did set the option to allow it;
  // the wizard asks at the first start.
  if (!StartupCheckEnabled(settings)) {
    LOG_DEBUG("application loaded: update checking is not enabled");
    CB_SUCC(event);
  }

  // Told once per stable release, not at every start: a 2.1 user stays behind
  // the stable series on purpose, and may not be able to move.
  auto p_parent = QPointer<QWidget>(parent);
  auto notify = [p_parent](const SoftwareVersion& sv) {
    if (!sv.NeedUpgrade() && sv.CurrentVersionReleased()) return;
    // The durable cache holds JSON documents only.
    const auto notified =
        QJsonDocument::fromJson(
            UDUP(GFDurableCacheGet(DUP(kNotifiedKey))).toUtf8())
            .object()
            .value("stable")
            .toString();
    if (notified == sv.latest_version) return;
    GFDurableCacheSave(
        DUP(kNotifiedKey),
        DUP(QJsonDocument(QJsonObject{{"stable", sv.latest_version}})
                .toJson()));

    LOG_INFO("a newer stable release is available, notifying user");
    QMetaObject::invokeMethod(
        QApplication::instance(),
        [p_parent]() {
          if (p_parent.isNull()) return;
          RaiseUpdateDialog(p_parent);
        },
        Qt::QueuedConnection);
  };

  auto cache = UDUP(GFDurableCacheGet(DUP(kCacheKey)));
  auto json = QJsonDocument::fromJson(cache.toUtf8());

  if (json.isEmpty() || !json.isObject()) {
    LOG_DEBUG(
        "application loaded: no valid cached version info found, "
        "checking update");
    return CheckUpdate(event, notify);
  }

  SoftwareVersion sv;
  sv.FromJson(json.object());

  FLOG_DEBUG("got software version meta data: %1", json.toJson());
  if (sv.timestamp.addDays(1) < QDateTime::currentDateTime()) {
    return CheckUpdate(event, notify);
  }

  FillGrtWithVersionInfo(sv);
  notify(sv);

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

      // we need to apply the settings in the main thread to avoid some
      // potential racing conditions
      auto p_tab = QPointer<QWidget>(tab);

      QMetaObject::invokeMethod(
          QApplication::instance(),
          [=]() {
            if (!p_tab) {
              LOG_ERROR(
                  "network settings tab apply settings: network_settings_tab "
                  "already deleted when applying settings");
              return;
            }
            auto* update_checking_check_box =
                p_tab->findChild<QCheckBox*>("update_checking_check_box");
            if (update_checking_check_box) {
              // The setting the wizard writes and the startup check reads.
              settings->setValue("network/prohibit_update_check",
                                 !update_checking_check_box->isChecked());
            }

            FLOG_DEBUG(
                "network settings tab apply settings: check for updates on "
                "startup: %1",
                update_checking_check_box
                    ? QString::number(update_checking_check_box->isChecked())
                    : "null");
          },
          Qt::QueuedConnection);

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
              update_checking_check_box->setChecked(
                  StartupCheckEnabled(settings));
            }
          },
          Qt::BlockingQueuedConnection);

      CB_SUCC(event);
    });

auto GFDeactivateModule() -> int { return 0; }

auto GFUnregisterModule() -> int { return 0; }