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
#include <QOperatingSystemVersion>
#include <QPointer>
#include <QSysInfo>
#include <QTimer>
#include <QVBoxLayout>
#include <QtNetwork>
#include <atomic>

#include "GFModule.h"
#include "GFModuleIdentity.h"
#include "GFModuleTr.h"
#include "UpdateChecker.h"
#include "UpdateTab.h"
#include "Utils.h"

namespace {

/// Spelled unqualified: lupdate misreads `gf::cmd::Meta kMeta{` in a class
/// body and warns about an unknown namespace.
using CommandMeta = gf::cmd::Meta;

/// Whether to check at startup. The Host's own setting: the setup wizard
/// asks it, and this module's settings page shows the same switch rather
/// than a second one that could disagree.
constexpr auto kProhibitKey = "network/prohibit_update_check";

/// The checker's state, last known good result included.
constexpr auto kStateKey = "update_checking_state";

std::atomic<bool> g_stopped{false};
QPointer<UpdateChecker> g_checker;

/// A GET through Qt: the GitHub API, or the update index and its mirrors. Every
/// request answers, success or not: a transport failure is status 0, and the
/// transfer timeout bounds a server that never replies.
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

#ifndef GF_RELEASE_PRODUCT
#define GF_RELEASE_PRODUCT ""
#define GF_RELEASE_UPDATE_BASES ""
#define GF_RELEASE_BUILDS_BASES ""
#define GF_RELEASE_INDEX_KEYS ""
#endif

/// The update channel this build was made for (release/product.json, by way
/// of this module's CMakeLists.txt).
auto BuiltInChannel() -> ReleaseChannel {
  const auto split = [](const char* joined) {
    return QString::fromLatin1(joined).split(u'|', Qt::SkipEmptyParts);
  };
  return {QString::fromLatin1(GF_RELEASE_PRODUCT),
          split(GF_RELEASE_UPDATE_BASES), split(GF_RELEASE_BUILDS_BASES),
          split(GF_RELEASE_INDEX_KEYS)};
}

/// This machine, as release descriptors name it.
auto ThisPlatform() -> Platform {
  Platform p;
#if defined(Q_OS_WIN)
  p.os = QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
  p.os = QStringLiteral("macos");
  const auto v = QOperatingSystemVersion::current();
  p.os_version =
      QStringLiteral("%1.%2").arg(v.majorVersion()).arg(v.minorVersion());
#elif defined(Q_OS_LINUX)
  p.os = QStringLiteral("linux");
#endif
  p.arch = QSysInfo::currentCpuArchitecture();
  p.install_kind = QString::fromUtf8(GFAppInstallKind(GFModuleSdkContext()));
  return p;
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

/// The update dialog, in a frame the Host owns.
class UpdateWidget : public QWidget, public gf::ui::DialogWidget {
 public:
  UpdateWidget() {
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new UpdateTab(this));
  }
};

/// Settings > Updates: whether to check automatically, once a day. Nothing
/// else is worth a setting: there is one update channel per build, and a day
/// is the right interval.
class UpdateSettingsWidget : public QWidget, public gf::ui::SettingsWidget {
 public:
  UpdateSettingsWidget() {
    auto* box = new QGroupBox(GTrC::tr("Update Checking"), this);
    auto* column = new QVBoxLayout(box);

    check_ = new QCheckBox(
        GTrC::tr("Check for updates automatically, once a day."), box);
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
  static constexpr CommandMeta kMeta{
      GF_MODULE_ID ".check_for_updates",
      GTrC::Noop("Check for Updates"),
      GTrC::Noop("See whether a newer GpgFrontend is available"),
      "",
      0,
      gf::cmd::kNeedsGuiThread};
  using Args = gf::cmd::Unit;
  using Result = gf::cmd::Unit;

  /// The badge's tooltip on every entry placed for this command.
  static constexpr const char* kAttention =
      GTrC::Noop("A newer version is available");

  /// The badge on Help > Check for Updates: an update the user has not seen
  /// yet (for a nightly, at most once a day). Asked when a menu opens, on the
  /// GUI thread; it never starts a check, and with no checker yet there is
  /// nothing to say.
  static auto State(const gf::cmd::CommandContext& /*ctx*/) -> uint32_t {
    uint32_t bits = GF_CMD_STATE_ENABLED | GF_CMD_STATE_VISIBLE;
    if (!g_checker.isNull() &&
        g_checker->Attention() == UpdateAttention::kUpdateAvailable) {
      bits |= GF_CMD_STATE_ATTENTION;
    }
    return bits;
  }
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
    g_checker = new UpdateChecker(
        {QString::fromUtf8(GFAppVersion(ctx)),
         QString::fromUtf8(GFAppGitCommitHash(ctx)), BuiltInChannel(),
         ThisPlatform(), GFAppIsNightlyIdentity(ctx)},
        MakeFetcher(nam), MakeStore());
    nam->setParent(g_checker);

    QObject::connect(g_checker, &UpdateChecker::Changed, g_checker,
                     [checker = g_checker.data()] {
                       FillGrtWithVersionInfo(checker->State());
                       // The badge on Help > Check for Updates, and the one
                       // in the main window's status strip, ask again now
                       // rather than when a menu next opens.
                       GFCommandStateChanged(GFModuleSdkContext(),
                                             CheckForUpdates::kMeta.id);
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
      "update", {GTrC::Noop("Check for Updates"), "", "", "", "", 0, 0},
      [](const QCborMap& /*args*/) { return new UpdateWidget(); });
  const bool settings = gf::ui::RegisterNativeWidget<UpdateSettingsWidget>(
      "settings",
      {GTrC::Noop("Updates"), GTrC::Noop("update,version,check,github"), "", "",
       "", 0, 0},
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

/// How often the automatic check looks whether a check is due. A check is due
/// once the last good result is a day old (UpdateChecker::kFreshSeconds), so
/// the network is asked at most once a day after a success, and a failed
/// check is retried an hour later rather than a day.
constexpr int kAutoCheckTickMs = 60 * 60 * 1000;

/// Whether the user allows checking on their own: read anew each time, so a
/// change in Settings applies without a restart. Absent until the setup
/// wizard has run: no check before the user has been asked at all.
auto AutoCheckAllowed() -> bool {
  const auto prohibited =
      gf::sdk::Setting(GFModuleSdkContext(), GF_SETTING_HOST, kProhibitKey);
  return prohibited.isValid() && !prohibited.toBool();
}

/// In the background, and quietly: a newer release shows as a badge on
/// Help > Check for Updates (CheckForUpdates::State), never as a window.
/// Joins a running check; skips a fresh result.
void AutoCheck() {
  if (!AutoCheckAllowed()) return;
  if (auto* checker = VersionChecker()) checker->Start(CheckMode::kIfStale);
}

auto OnApplicationLoaded(const GFEvent& /*event*/) -> GFEventResult {
  LOG_DEBUG("application starting completed event: processing");

  OnGuiThread([] {
    AutoCheck();
    // For as long as the application runs, not only at its start: a session
    // left open for a week still learns of an update within a day. The timer
    // lives with the checker, so deactivating the module stops it too.
    auto* checker = VersionChecker();
    if (checker == nullptr) return;
    auto* timer = new QTimer(checker);
    timer->setInterval(kAutoCheckTickMs);
    QObject::connect(timer, &QTimer::timeout, checker, [] { AutoCheck(); });
    timer->start();
  });

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
