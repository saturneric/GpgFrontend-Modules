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

#include "GnuPGInfoGatheringModule.h"

#include <GFSDKBuildInfo.h>
#include <GFSDKLog.h>

// qt
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QString>
#include <QVBoxLayout>

// c++
#include <optional>

#include "GFModule.h"
#include "GFModuleIdentity.h"
#include "GFSDKHostCommands.hpp"
#include "GnupgTab.h"
#include "GpgInfo.h"

extern auto CalculateBinaryChecksum(const QString &path)
    -> std::optional<QString>;

extern void GetGpgComponentInfos(void *, int, const char *, const char *);

extern void GetGpgDirectoryInfos(void *, int, const char *, const char *);

extern void GetGpgOptionInfos(void *, int, const char *, const char *);

extern auto StartGatheringAllGnuPGInfo() -> int;

extern auto StartStartGatheringGnuPGComponentsInfo(
    const QString &gpgme_version, const QString &gpgconf_path,
    const QString &default_home_path) -> int;

using Context = struct {
  QString gpgme_version;
  QString gpgconf_path;
  GpgComponentInfo component_info;
};

namespace {

/// What GnuPG is installed and how it is configured, in a dialog frame the
/// Host owns.
class GnupgInfoWidget : public QWidget, public gf::ui::DialogWidget {
 public:
  GnupgInfoWidget() {
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new GnupgTab(this));
  }
};

/// Help > GnuPG. Its title and whereabouts are the command's and the
/// script's; it only opens the module's own dialog.
struct ShowGnupgInfo {
  static constexpr gf::cmd::Meta kMeta{
      GF_MODULE_ID ".show_gnupg_info", GC_TR("GnuPG"),
      GC_TR("Information about GnuPG"), "", 0, gf::cmd::kNeedsGuiThread};
  using Args = gf::cmd::Unit;
  using Result = gf::cmd::Unit;
};

auto DoShowGnupgInfo(const gf::cmd::CommandContext& /*ctx*/,
                     const gf::cmd::Unit& /*args*/)
    -> gf::cmd::Outcome<gf::cmd::Unit> {
  Commands().Invoke<gf::cmd::host::ViewOpen>(
      {gf::cmd::ViewRef{QStringLiteral(GF_MODULE_ID ".info")}});
  return gf::cmd::Outcome<gf::cmd::Unit>::Success({});
}

}  // namespace

auto OnActivate() -> GFResult {
  LOG_INFO("gnupg info gathering module registering");
  const bool ok = gf::ui::RegisterNativeWidget<GnupgInfoWidget>(
      "info", {GC_TR("GnuPG"), "", "", "", ":/icons/key.png", 500, 600},
      [](const QCborMap& /*args*/) { return new GnupgInfoWidget(); });
  return ok ? GFResult::Ok()
            : GFResult::Fail("the GnuPG information widget was not registered");
}

auto OnRequestGatheringAllGnuPGInfo(const GFEvent & /*event*/)
    -> GFEventResult {
  StartGatheringAllGnuPGInfo();
  return GFEventResult::Ok();
}

auto OnUnload() -> void {
  LOG_INFO("gnupg info gathering module unregistering");
}

// The module's whole framework surface: which events it handles, what it
// provides, what runs at each lifecycle point, and one forwarder to the
// runtime that implements all of it. Everything above is business logic.
constexpr std::array<GFEventBinding, 1> kEvents = {{
    {"REQUEST_GATHERING_ALL_GNUPG_INFO", &OnRequestGatheringAllGnuPGInfo},
}};

const std::array<gf::cmd::Binding, 1> kCommands = {
    gf::cmd::Bind<ShowGnupgInfo, &DoShowGnupgInfo>(),
};

const GFModuleHooks kHooks = {
    sizeof(GFModuleHooks),
    GF_MODULE_ID,
    GF_MODULE_VERSION,
    GF_MODULE_TRANSLATION_CONTEXT,
    &OnActivate,
    nullptr,  // the Host withdraws the command, widget and script itself
    &OnUnload,
    kEvents.data(),
    kEvents.size(),
    kCommands.data(),
    kCommands.size(),
};

extern "C" GF_MODULE_EXPORT auto GFModuleGetApi(uint32_t abi)
    -> const GFModuleApi * {
  return GFModuleRuntimeGetApi(abi, &kHooks);
}

auto StartStartGatheringGnuPGComponentsInfo(const QString &gpgme_version,
                                            const QString &gpgconf_path,
                                            const QString &default_home_path)
    -> int {
  auto context = Context{gpgme_version, gpgconf_path};

  // get all components
  gf::sdk::RunCommand(GFModuleSdkContext(), gpgconf_path,
                      {"--homedir", default_home_path, "--list-components"},
                      GetGpgComponentInfos, &context);
  LOG_DEBUG("finished loading GnuPG component info");
  return 0;
}

auto StartGatheringAllGnuPGInfo() -> int {
  const auto gpgme_version = gf::sdk::StateText(GFModuleSdkContext(), "core",
                                                "gpgme.version", "0.0.0");
  LOG_D() << "got gpgme version from rt:" << gpgme_version;

  const auto gpgconf_path = gf::sdk::StateText(GFModuleSdkContext(), "core",
                                               "gpgme.ctx.gpgconf_path", "");
  LOG_D() << "got gpgconf path from rt:" << gpgconf_path;

  if (gpgconf_path.isEmpty()) {
    LOG_DEBUG("gpgconf path is empty; skipping GnuPG info gathering");
    return -1;
  }

  auto default_home_path = gf::sdk::StateText(
      GFModuleSdkContext(), "core", "gpgme.ctx.default_database_path", "");
  LOG_D() << "got default home path from rt:" << default_home_path;

  default_home_path = QDir::toNativeSeparators(
      QFileInfo(default_home_path).canonicalFilePath());
  LOG_D() << "final default home path:" << default_home_path;

  // gather component information
  StartStartGatheringGnuPGComponentsInfo(gpgme_version, gpgconf_path,
                                         default_home_path);

  // process.execute borrows everything it is given; RunCommands keeps the
  // strings alive for the call, so nothing here is allocated for the host.
  QList<gf::sdk::Command> commands;
  commands.push_back({gpgconf_path,
                      {"--homedir", default_home_path, "--list-dirs"},
                      GetGpgDirectoryInfos,
                      nullptr});

  // One call, one list, nothing to free by hand: the char** and count this
  // replaces had to be walked and released by the caller, and the hand-rolled
  // loop that preceded the helper leaked the whole array once per pass.
  const auto components = gf::sdk::StateChildren(
      GFModuleSdkContext(), GFModuleId(), "gnupg.components");
  if (components.isEmpty()) return -1;

  for (const auto &component : components) {
    // Absence is reported now, so "not set" and "set to nothing" are
    // different answers rather than the same empty string.
    const auto component_info_raw =
        gf::sdk::StateText(GFModuleSdkContext(), GFModuleId(),
                           QString("gnupg.components.%1").arg(component));
    if (component_info_raw.isEmpty()) continue;

    auto jsonlized_component_info =
        QJsonDocument::fromJson(component_info_raw.toUtf8());
    assert(jsonlized_component_info.isObject());

    auto component_info = GpgComponentInfo(jsonlized_component_info.object());
    LOG_T() << "gpgconf check options ready, component:" << component_info.name;

    if (component_info.name == "gpgme" || component_info.name == "gpgconf") {
      continue;
    }

    auto *context =
        new (GFMemAlloc(GFModuleSdkContext(), GF_ARENA_NORMAL, sizeof(Context)))
            Context{gpgme_version, gpgconf_path, component_info};

    // The context is freed by GetGpgOptionInfos once the command finishes.
    commands.push_back({gpgconf_path,
                        {"--homedir", default_home_path, "--list-options",
                         component_info.name},
                        GetGpgOptionInfos,
                        context});
  }

  gf::sdk::RunCommands(GFModuleSdkContext(), commands);
  gf::sdk::SetStateBool(GFModuleSdkContext(), GFModuleId(),
                        "gnupg.gathering_done", true);

  return 0;
}

auto CalculateBinaryChecksum(const QString &path) -> std::optional<QString> {
  // Check the file's existence and access rights.
  QFileInfo const info(path);
  if (!info.exists() || !info.isFile() || !info.isReadable()) {
    LOG_D() << "get info for file" << info.filePath()
            << "error, exists:" << info.exists();
    return {};
  }

  // Open and read the file.
  QFile f(info.filePath());
  if (!f.open(QIODevice::ReadOnly)) {
    LOG_D() << "cannot open" << path
            << "for checksum calculation:" << f.errorString();
    return {};
  }

  QCryptographicHash hash_sha(QCryptographicHash::Sha256);

  // Read the data in chunks.
  const qint64 buffer_size = 8192;
  while (!f.atEnd()) {
    QByteArray const buffer = f.read(buffer_size);
    if (buffer.isEmpty()) {
      LOG_D() << "error reading file" << path << "during checksum calculation";
      return {};
    }
    hash_sha.addData(buffer);
  }

  // Close the file.
  f.close();

  // Return the first 6 characters of the file's SHA-256 hash.
  return QString(hash_sha.result().toHex()).left(6);
}

void GetGpgComponentInfos(void *data, int exit_code, const char *out,
                          const char *err) {
  auto *context = reinterpret_cast<Context *>(data);
  auto p_out = QString::fromUtf8(out);
  auto p_err = QString::fromUtf8(err);

  LOG_D() << "gpgconf components exit_code:" << exit_code
          << "process stdout size:" << p_out.size();

  if (exit_code != 0) {
    LOG_D() << "gpgconf failed, stderr:" << p_err << "process stdout:" << p_out;
    return;
  }

  std::vector<GpgComponentInfo> component_infos;
  GpgComponentInfo c_i_gpgme;
  c_i_gpgme.name = "gpgme";
  c_i_gpgme.desc = "GPG Made Easy";
  c_i_gpgme.version = context->gpgme_version;
  c_i_gpgme.path = "Embedded In";
  c_i_gpgme.binary_checksum = "/";

  GpgComponentInfo c_i_gpgconf;
  c_i_gpgconf.name = "gpgconf";
  c_i_gpgconf.desc = "GPG Configure";
  c_i_gpgconf.version = "/";
  c_i_gpgconf.path = context->gpgconf_path;
  auto gpgconf_binary_checksum = CalculateBinaryChecksum(context->gpgconf_path);
  c_i_gpgconf.binary_checksum =
      (gpgconf_binary_checksum.has_value() ? gpgconf_binary_checksum.value()
                                           : QString("/"));

  component_infos.push_back(c_i_gpgme);
  component_infos.push_back(c_i_gpgconf);

  auto const jsonlized_gpgme_component_info = c_i_gpgme.Json();
  auto const jsonlized_gpgconf_component_info = c_i_gpgconf.Json();
  gf::sdk::SetStateText(
      GFModuleSdkContext(), GFGetModuleID(), "gnupg.components.gpgme",
      (QJsonDocument(jsonlized_gpgme_component_info).toJson()).constData());
  gf::sdk::SetStateText(
      GFModuleSdkContext(), GFGetModuleID(), "gnupg.components.gpgconf",
      (QJsonDocument(jsonlized_gpgconf_component_info).toJson()).constData());

  auto line_split_list = p_out.split("\n");

  for (const auto &line : line_split_list) {
    auto info_split_list = line.split(":");

    if (info_split_list.size() != 3) continue;

    auto component_name = info_split_list[0].trimmed();
    auto component_desc = info_split_list[1].trimmed();
    auto component_path = info_split_list[2].trimmed();

#if defined(_WIN32) || defined(WIN32)
    // replace some special substrings on windows
    // platform
    component_path.replace("%3a", ":");
#endif

    auto binary_checksum = CalculateBinaryChecksum(component_path);

    LOG_D() << "gnupg component name:" << component_name
            << "desc:" << component_desc << "checksum:"
            << (binary_checksum.has_value() ? binary_checksum.value() : "/")
            << "path:" << component_path;

    QString version = "/";

    if (component_name == "gpg") {
      version = gf::sdk::StateText(GFModuleSdkContext(), "core",
                                   "gpgme.ctx.gnupg_version", "2.0.0");
    }
    if (component_name == "gpg-agent") {
      gf::sdk::SetStateText(GFModuleSdkContext(), GFGetModuleID(),
                            "gnupg.gpg_agent_path",
                            (QString(component_path).toUtf8()).constData());
    }
    if (component_name == "dirmngr") {
      gf::sdk::SetStateText(GFModuleSdkContext(), GFGetModuleID(),
                            "gnupg.dirmngr_path",
                            (QString(component_path).toUtf8()).constData());
    }
    if (component_name == "keyboxd") {
      gf::sdk::SetStateText(GFModuleSdkContext(), GFGetModuleID(),
                            "gnupg.keyboxd_path",
                            (QString(component_path).toUtf8()).constData());
    }

    {
      GpgComponentInfo c_i;
      c_i.name = component_name;
      c_i.desc = component_desc;
      c_i.version = version;
      c_i.path = component_path;
      c_i.binary_checksum =
          (binary_checksum.has_value() ? binary_checksum.value()
                                       : QString("/"));

      auto const jsonlized_component_info = c_i.Json();
      gf::sdk::SetStateText(
          GFModuleSdkContext(), GFGetModuleID(),
          (QString("gnupg.components.%1").arg(component_name).toUtf8())
              .constData(),
          (QJsonDocument(jsonlized_component_info).toJson()).constData());

      component_infos.push_back(c_i);
    }

    LOG_DEBUG("finished loading GnuPG component info (all components)");
  }
}

void GetGpgDirectoryInfos(void *, int exit_code, const char *out,
                          const char *err) {
  if (exit_code != 0) return;

  auto p_out = QString::fromUtf8(out);
  auto p_err = QString::fromUtf8(err);
  auto line_split_list = p_out.split("\n");

  for (const auto &line : line_split_list) {
    auto info_split_list = line.split(":");
    LOG_T() << "gpgconf directories info line:" << line
            << "info size:" << info_split_list.size();

    if (info_split_list.size() != 2) continue;

    auto configuration_name = info_split_list[0].trimmed();
    auto configuration_value = info_split_list[1].trimmed();

#if defined(_WIN32) || defined(WIN32)
    // replace some special substrings on windows
    // platform
    configuration_value.replace("%3a", ":");
#endif

    // record gnupg home path
    if (configuration_name == "homedir") {
      gf::sdk::SetStateText(GFModuleSdkContext(), GFGetModuleID(),
                            "gnupg.home_path",
                            (configuration_value.toUtf8()).constData());
    }

    gf::sdk::SetStateText(
        GFModuleSdkContext(), GFGetModuleID(),
        (QString("gnupg.dirs.%1").arg(configuration_name).toUtf8()).constData(),
        (configuration_value.toUtf8()).constData());
  }
}

void GetGpgOptionInfos(void *data, int exit_code, const char *out,
                       const char *err) {
  if (exit_code != 0) return;

  auto p_out = QString::fromUtf8(out);
  auto p_err = QString::fromUtf8(err);
  auto *context = reinterpret_cast<Context *>(data);
  auto component_name = context->component_info.name;

  LOG_D() << "gpgconf" << component_name
          << "available options exit_code:" << exit_code
          << "process stdout size:" << p_out.size();

  std::vector<GpgOptionsInfo> options_infos;

  auto line_split_list = p_out.split("\n");

  for (const auto &line : line_split_list) {
    auto info_split_list = line.split(":");

    LOG_T() << "component" << component_name
            << "available options line:" << line
            << "info size:" << info_split_list.size();

    if (info_split_list.size() < 10) continue;

    // The format of each line is:
    // name:flags:level:description:type:alt-type:argname:default:argdef:value

    auto option_name = info_split_list[0].trimmed();
    auto option_flags = info_split_list[1].trimmed();
    auto option_level = info_split_list[2].trimmed();
    auto option_desc = info_split_list[3].trimmed();
    auto option_type = info_split_list[4].trimmed();
    auto option_alt_type = info_split_list[5].trimmed();
    auto option_argname = info_split_list[6].trimmed();
    auto option_default = info_split_list[7].trimmed();
    auto option_argdef = info_split_list[8].trimmed();
    auto option_value = info_split_list[9].trimmed();

    GpgOptionsInfo info;
    info.name = option_name;
    info.flags = option_flags;
    info.level = option_level;
    info.description = option_desc;
    info.type = option_type;
    info.alt_type = option_alt_type;
    info.argname = option_argname;
    info.default_value = option_default;
    info.argdef = option_argdef;
    info.value = option_value;

    auto const jsonlized_option_info = info.Json();
    gf::sdk::SetStateText(
        GFModuleSdkContext(), GFGetModuleID(),
        (QString("gnupg.components.%1.options.%2")
             .arg(component_name)
             .arg(option_name)
             .toUtf8())
            .constData(),
        (QJsonDocument(jsonlized_option_info).toJson()).constData());
    options_infos.push_back(info);
  }

  context->~Context();
  GFMemFree(GFModuleSdkContext(), GF_ARENA_NORMAL, context);
}