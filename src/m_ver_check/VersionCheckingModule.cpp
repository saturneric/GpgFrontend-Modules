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

#include <GFSDKBuildInfo.h>
#include <GFSDKHostCommands.hpp>
#include <GFSDKLog.h>
#include <GFSDKUI.h>

#include <QButtonGroup>
#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaType>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QtNetwork>

#include "BKTUSVersionCheckTask.h"
#include "GFModule.h"
#include "GFModuleIdentity.h"
#include "GitHubVersionCheckTask.h"
#include "SoftwareVersion.h"
#include "UpdateTab.h"
#include "Utils.h"

namespace {

/// Whether to check at startup. The Host's own setting: the setup wizard
/// asks it, and this module's settings page shows the same switch rather
/// than a second one that could disagree.
constexpr auto kProhibitKey = "network/prohibit_update_check";

/// Which service to ask. The module's own setting.
constexpr auto kApiKey = "update_checking_api";

auto Api() -> QString {
  return gf::sdk::Setting(GFModuleSdkContext(), GF_SETTING_MODULE, kApiKey,
                          "github")
      .toString();
}

void StoreResult(const SoftwareVersion& sv) {
  gf::sdk::SetCacheText(GFModuleSdkContext(), GF_STORE_DURABLE,
                        "update_checking_cache",
                        (QJsonDocument(sv.ToJson()).toJson()).constData());
}

/// Ask the service; cache what it says for the next start. The task always
/// reports, success or not, so it is always released.
void CheckUpdate() {
  if (Api() == "bktus") {
    MLogInfo("checking updating using api of bktus.com");
    auto* task = new BKTUSVersionCheckTask();
    QObject::connect(task, &BKTUSVersionCheckTask::SignalUpgradeVersion,
                     QCoreApplication::instance(), &StoreResult);
    QObject::connect(task, &BKTUSVersionCheckTask::SignalUpgradeVersion, task,
                     &QObject::deleteLater);
    task->Run();
  } else {
    MLogInfo("checking updating using api of github.com");
    auto* task = new GitHubVersionCheckTask();
    QObject::connect(task, &GitHubVersionCheckTask::SignalUpgradeVersion,
                     QCoreApplication::instance(), &StoreResult);
    QObject::connect(task, &GitHubVersionCheckTask::SignalUpgradeVersion, task,
                     &QObject::deleteLater);
    task->Run();
  }
}

void OpenUpdateDialog() {
  Commands().Invoke<gf::cmd::host::ViewOpen>(
      {gf::cmd::ViewRef{QStringLiteral(GF_MODULE_ID ".update")}});
}

/// The update dialog, in a frame the Host owns.
class UpdateWidget : public QWidget, public gf::ui::DialogWidget {
 public:
  UpdateWidget() {
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new UpdateTab(this));
  }
};

/// Settings > Updates: whether to check at startup, and where.
class UpdateSettingsWidget : public QWidget, public gf::ui::SettingsWidget {
 public:
  UpdateSettingsWidget() {
    auto* box = new QGroupBox(
        QCoreApplication::translate("GTrC", "Update Checking"), this);
    auto* column = new QVBoxLayout(box);

    check_ = new QCheckBox(
        QCoreApplication::translate(
            "GTrC", "Checking for version updates when the application "
                    "starts."),
        box);
    column->addWidget(check_);

    auto* row = new QHBoxLayout();
    row->addWidget(new QLabel(
        QCoreApplication::translate("GTrC", "Update Checking API:"), box));
    github_ = new QRadioButton(QCoreApplication::translate("GTrC", "GitHub"),
                               box);
    bktus_ = new QRadioButton(QCoreApplication::translate("GTrC", "BKTUS.com"),
                              box);
    auto* group = new QButtonGroup(this);
    group->addButton(github_);
    group->addButton(bktus_);
    row->addWidget(github_);
    row->addWidget(bktus_);
    row->addStretch();
    column->addLayout(row);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(box);
    layout->addStretch();
  }

  void LoadSettings() override {
    check_->setChecked(!gf::sdk::Setting(GFModuleSdkContext(), GF_SETTING_HOST,
                                         kProhibitKey, true)
                            .toBool());
    const bool bktus = Api() == "bktus";
    bktus_->setChecked(bktus);
    github_->setChecked(!bktus);
  }

  auto ApplySettings() -> bool override {
    const bool host_ok =
        gf::sdk::SetSetting(GFModuleSdkContext(), GF_SETTING_HOST,
                            kProhibitKey, !check_->isChecked());
    const bool module_ok = gf::sdk::SetSetting(
        GFModuleSdkContext(), GF_SETTING_MODULE, kApiKey,
        bktus_->isChecked() ? QStringLiteral("bktus")
                            : QStringLiteral("github"));
    return host_ok && module_ok;
  }

 private:
  QCheckBox* check_;
  QRadioButton* github_;
  QRadioButton* bktus_;
};

/// Help > Check for Updates.
struct CheckForUpdates {
  static constexpr gf::cmd::Meta kMeta{
      GF_MODULE_ID ".check_for_updates", GC_TR("Check for Updates"),
      GC_TR("See whether a newer GpgFrontend is available"), "", 0,
      gf::cmd::kNeedsGuiThread};
  using Args = gf::cmd::Unit;
  using Result = gf::cmd::Unit;
};

auto DoCheckForUpdates(const gf::cmd::CommandContext& /*ctx*/,
                       const gf::cmd::Unit& /*args*/)
    -> gf::cmd::Outcome<gf::cmd::Unit> {
  OpenUpdateDialog();
  return gf::cmd::Outcome<gf::cmd::Unit>::Success({});
}

}  // namespace

auto OnActivate() -> GFResult {
  LOG_INFO("version checking module activating");
  const bool dialog = gf::ui::RegisterNativeWidget<UpdateWidget>(
      "update", {GC_TR("Check for Updates"), "", "", "", "", 500, 600},
      [](const QCborMap& /*args*/) { return new UpdateWidget(); });
  const bool settings = gf::ui::RegisterNativeWidget<UpdateSettingsWidget>(
      "settings",
      {GC_TR("Updates"), GC_TR("update,version,check,github,bktus"), "", "",
       "", 0, 0},
      [](const QCborMap& /*args*/) { return new UpdateSettingsWidget(); });
  return dialog && settings
             ? GFResult::Ok()
             : GFResult::Fail("the update widgets were not registered");
}

auto OnApplicationLoaded(const GFEvent& /*event*/) -> GFEventResult {
  LOG_DEBUG("application starting completed event: processing");

  // Absent until the setup wizard has run: no check before the user has been
  // asked whether to check at all.
  const auto prohibited = gf::sdk::Setting(GFModuleSdkContext(),
                                           GF_SETTING_HOST, kProhibitKey);
  if (!prohibited.isValid()) {
    LOG_DEBUG("application loaded: the setup wizard has not asked yet");
    return GFEventResult::Ok();
  }
  if (prohibited.toBool()) {
    LOG_DEBUG("application loaded: update checking is prohibited");
    return GFEventResult::Ok();
  }

  auto cache = gf::sdk::CacheText(GFModuleSdkContext(), GF_STORE_DURABLE,
                                  "update_checking_cache");
  auto json = QJsonDocument::fromJson(cache.toUtf8());

  if (json.isEmpty() || !json.isObject()) {
    LOG_DEBUG("application loaded: nothing cached, checking now");
    CheckUpdate();
    return GFEventResult::Ok();
  }

  SoftwareVersion sv;
  sv.FromJson(json.object());

  FLOG_DEBUG("got software version meta data: %1", json.toJson());
  if (sv.timestamp.addDays(1) < QDateTime::currentDateTime()) {
    CheckUpdate();
    return GFEventResult::Ok();
  }

  FillGrtWithVersionInfo(sv);

  if (sv.NeedUpgrade() || !sv.CurrentVersionReleased() ||
      !sv.current_commit_hash_publish_in_remote) {
    LOG_INFO(
        "software version is outdated or not fully released, notifying user");
    OpenUpdateDialog();
  }

  // An observation, answered at once: the check runs on its own.
  return GFEventResult::Ok();
}

auto OnUnload() -> void { LOG_INFO("version checking module unregistering"); }

// The module's whole framework surface.
constexpr std::array<GFEventBinding, 1> kEvents = {{
    {"APPLICATION_LOADED", &OnApplicationLoaded},
}};

const std::array<gf::cmd::Binding, 1> kCommands = {
    gf::cmd::Bind<CheckForUpdates, &DoCheckForUpdates>(),
};

const GFModuleHooks kHooks = {
    sizeof(GFModuleHooks),
    GF_MODULE_ID,
    GF_MODULE_VERSION,
    GF_MODULE_TRANSLATION_CONTEXT,
    &OnActivate,
    nullptr,  // the Host withdraws the command, widgets and script itself
    &OnUnload,
    kEvents.data(),
    kEvents.size(),
    kCommands.data(),
    kCommands.size(),
};

extern "C" GF_MODULE_EXPORT auto GFModuleGetApi(uint32_t abi)
    -> const GFModuleApi* {
  return GFModuleRuntimeGetApi(abi, &kHooks);
}
