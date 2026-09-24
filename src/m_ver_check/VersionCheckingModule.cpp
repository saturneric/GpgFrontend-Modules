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
#include <GFSDKLog.h>
#include <GFSDKUI.h>

#include <GFSDKHostCommands.hpp>
#include <QCheckBox>
#include <QGroupBox>
#include <QPointer>
#include <QVBoxLayout>
#include <QtNetwork>
#include <atomic>

#include "GFModule.h"
#include "GFModuleIdentity.h"
#include "UpdateChecker.h"
#include "UpdateTab.h"
#include "Utils.h"

namespace {

/// Whether to check at startup. The Host's own setting: the setup wizard
/// asks it, and this module's settings page shows the same switch rather
/// than a second one that could disagree.
constexpr auto kProhibitKey = "network/prohibit_update_check";

/// The checker's state, last known good result included.
constexpr auto kStateKey = "update_checking_state";

std::atomic<bool> g_stopped{false};
QPointer<UpdateChecker> g_checker;

/// A GitHub API GET through Qt. Every request answers, success or not: a
/// transport failure is status 0, and the transfer timeout bounds a server
/// that never replies.
auto MakeFetcher(QNetworkAccessManager* nam) -> UpdateChecker::Fetcher {
  return [nam = QPointer<QNetworkAccessManager>(nam)](
             const QUrl& url, const UpdateChecker::Reply& reply) {
    if (nam.isNull()) {
      reply(0, {});
      return;
    }

    QNetworkRequest request(url);
    request.setTransferTimeout(30000);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      GFAppUserAgent(GFModuleSdkContext()));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");

    auto* r = nam->get(request);
    QObject::connect(r, &QNetworkReply::finished, r, [r, reply] {
      const int status =
          r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      FLOG_DEBUG("update check reply: %1, http status: %2, error: %3",
                 r->url().toString(), status, r->errorString());
      const auto body = r->readAll();
      r->deleteLater();
      reply(status, body);
    });
  };
}

auto MakeStore() -> UpdateChecker::Store {
  return {
      [] {
        return gf::sdk::CacheText(GFModuleSdkContext(), GF_STORE_DURABLE,
                                  kStateKey)
            .toUtf8();
      },
      [](const QByteArray& data) {
        gf::sdk::SetCacheText(GFModuleSdkContext(), GF_STORE_DURABLE, kStateKey,
                              QString::fromUtf8(data));
      },
  };
}

/// On the GUI thread, where the checker and its requests live.
template <typename F>
void OnGuiThread(F&& f) {
  QMetaObject::invokeMethod(QCoreApplication::instance(), std::forward<F>(f));
}

void OpenUpdateDialog() {
  Commands().Invoke<gf::cmd::host::ViewOpen>(
      {gf::cmd::ViewRef{QStringLiteral(GF_MODULE_ID ".update")}});
}

/// Once per run: the startup check's answer, or a fresh stored one.
void CheckAtStartup() {
  auto* checker = VersionChecker();
  if (checker == nullptr) return;

  auto watch = std::make_shared<QMetaObject::Connection>();
  const auto decide = [checker, watch] {
    if (checker->State().checking) return;
    QObject::disconnect(*watch);
    if (ShouldPromptOnStartup(checker->State())) {
      LOG_INFO("a newer release is available, notifying user");
      OpenUpdateDialog();
    }
  };

  *watch = QObject::connect(checker, &UpdateChecker::Changed, checker, decide);
  checker->Start(CheckMode::kIfStale);
  decide();
}

/// The update dialog, in a frame the Host owns.
class UpdateWidget : public QWidget, public gf::ui::DialogWidget {
 public:
  UpdateWidget() {
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new UpdateTab(this));
  }
};

/// Settings > Updates: whether to check at startup. Nothing else is worth a
/// setting: there is one release server, and a day is the right interval.
class UpdateSettingsWidget : public QWidget, public gf::ui::SettingsWidget {
 public:
  UpdateSettingsWidget() {
    auto* box = new QGroupBox(
        QCoreApplication::translate("GTrC", "Update Checking"), this);
    auto* column = new QVBoxLayout(box);

    check_ =
        new QCheckBox(QCoreApplication::translate(
                          "GTrC",
                          "Checking for version updates when the application "
                          "starts."),
                      box);
    column->addWidget(check_);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(box);
    layout->addStretch();
  }

  void LoadSettings() override {
    check_->setChecked(!gf::sdk::Setting(GFModuleSdkContext(), GF_SETTING_HOST,
                                         kProhibitKey, true)
                            .toBool());
  }

  auto ApplySettings() -> bool override {
    return gf::sdk::SetSetting(GFModuleSdkContext(), GF_SETTING_HOST,
                               kProhibitKey, !check_->isChecked());
  }

 private:
  QCheckBox* check_;
};

/// Help > Check for Updates.
struct CheckForUpdates {
  static constexpr gf::cmd::Meta kMeta{
      GF_MODULE_ID ".check_for_updates",
      GC_TR("Check for Updates"),
      GC_TR("See whether a newer GpgFrontend is available"),
      "",
      0,
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

auto VersionChecker() -> UpdateChecker* {
  if (g_stopped.load()) return nullptr;
  if (g_checker.isNull()) {
    auto* ctx = GFModuleSdkContext();
    auto* nam = new QNetworkAccessManager();
    g_checker = new UpdateChecker({QString::fromUtf8(GFAppVersion(ctx)),
                                   QString::fromUtf8(GFAppGitCommitHash(ctx))},
                                  MakeFetcher(nam), MakeStore());
    nam->setParent(g_checker);

    QObject::connect(g_checker, &UpdateChecker::Changed, g_checker,
                     [checker = g_checker.data()] {
                       FillGrtWithVersionInfo(checker->State());
                     });
    FillGrtWithVersionInfo(g_checker->State());
  }
  return g_checker;
}

auto OnActivate() -> GFResult {
  LOG_INFO("version checking module activating");
  g_stopped.store(false);

  // Left behind by 1.5 and earlier: a cache in the old format, and the choice
  // of a check service that no longer exists.
  gf::sdk::RemoveCache(GFModuleSdkContext(), GF_STORE_DURABLE,
                       "update_checking_cache");
  gf::sdk::RemoveSetting(GFModuleSdkContext(), GF_SETTING_MODULE,
                         "update_checking_api");

  const bool dialog = gf::ui::RegisterNativeWidget<UpdateWidget>(
      "update", {GC_TR("Check for Updates"), "", "", "", "", 500, 600},
      [](const QCborMap& /*args*/) { return new UpdateWidget(); });
  const bool settings = gf::ui::RegisterNativeWidget<UpdateSettingsWidget>(
      "settings",
      {GC_TR("Updates"), GC_TR("update,version,check,github"), "", "", "", 0,
       0},
      [](const QCborMap& /*args*/) { return new UpdateSettingsWidget(); });
  return dialog && settings
             ? GFResult::Ok()
             : GFResult::Fail("the update widgets were not registered");
}

auto OnDeactivate() -> GFResult {
  // No check starts from here on; the checker and its requests go with the
  // event loop's next turn, on the thread that owns them.
  g_stopped.store(true);
  if (!g_checker.isNull()) {
    QMetaObject::invokeMethod(g_checker.data(), &QObject::deleteLater,
                              Qt::QueuedConnection);
  }
  return GFResult::Ok();
}

auto OnApplicationLoaded(const GFEvent& /*event*/) -> GFEventResult {
  LOG_DEBUG("application starting completed event: processing");

  // Absent until the setup wizard has run: no check before the user has been
  // asked whether to check at all.
  const auto prohibited =
      gf::sdk::Setting(GFModuleSdkContext(), GF_SETTING_HOST, kProhibitKey);
  if (!prohibited.isValid()) {
    LOG_DEBUG("application loaded: the setup wizard has not asked yet");
    return GFEventResult::Ok();
  }
  if (prohibited.toBool()) {
    LOG_DEBUG("application loaded: update checking is prohibited");
    return GFEventResult::Ok();
  }

  OnGuiThread(&CheckAtStartup);

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
    &OnDeactivate,  // stops a running check; the Host withdraws the rest
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
