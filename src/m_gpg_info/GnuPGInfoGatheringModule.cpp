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

#include <GFSDKBasic.h>
#include <GFSDKBuildInfo.h>
#include <GFSDKLog.h>

// qt
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonDocument>
#include <QString>

// c++
#include <optional>

#include "GFModuleCommonUtils.hpp"
#include "GFModuleDefine.h"
#include "GpgInfo.h"

GF_MODULE_API_DEFINE_V2("com.bktus.gpgfrontend.module.gnupg_info_gathering",
                        "GatherGnupgInfo", "1.2.0",
                        "Try gathering gnupg information.", "Saturneric")

DEFINE_TRANSLATIONS_STRUCTURE(ModuleGnuPGInfoGathering);

extern auto CalculateBinaryChecksum(const QString &path)
    -> std::optional<QString>;

extern void GetGpgComponentInfos(void *, int, const char *, const char *);

extern void GetGpgDirectoryInfos(void *, int, const char *, const char *);

extern void GetGpgOptionInfos(void *, int, const char *, const char *);

extern auto StartGatheringAllGnuPGInfo() -> int;

extern auto StartStartGatheringGnuPGComponentsInfo(
    const QString &gpgme_version, const QString &gpgconf_path) -> int;

extern auto GnupgTabFactory(void *id) -> void *;

using Context = struct {
  QString gpgme_version;
  QString gpgconf_path;
  GpgComponentInfo component_info;
};

auto GFRegisterModule() -> int {
  MLogDebug("gnupg info gathering module registering...");

  REGISTER_TRANS_READER();

  GFUIMountEntry(DUP("AboutDialogTabs"),
                 QMapToMetaDataArray({
                     {"TabTitle", GC_TR("GnuPG")},
                 }),
                 1, GnupgTabFactory);

  return 0;
}

auto GFActiveModule() -> int {
  LISTEN("APPLICATION_LOADED");
  LISTEN("REQUEST_GATHERING_ALL_GNUPG_INFO");
  return 0;
}

REGISTER_EVENT_HANDLER(APPLICATION_LOADED, [](const MEvent &event) -> int {
  const auto gpgme_version = UDUP(GFModuleRetrieveRTValueOrDefault(
      DUP("core"), DUP("gpgme.version"), DUP("0.0.0")));
  MLogDebug(QString("got gpgme version from rt: %1").arg(gpgme_version));

  const auto gpgconf_path = UDUP(GFModuleRetrieveRTValueOrDefault(
      DUP("core"), DUP("gpgme.ctx.gpgconf_path"), DUP("")));
  MLogDebug(QString("got gpgconf path from rt: %1").arg(gpgconf_path));

  StartStartGatheringGnuPGComponentsInfo(gpgme_version, gpgconf_path);
  CB_SUCC(event);
});

REGISTER_EVENT_HANDLER(REQUEST_GATHERING_ALL_GNUPG_INFO,
                       [](const MEvent &event) -> int {
                         StartGatheringAllGnuPGInfo();

                         CB_SUCC(event);
                       });

auto GFDeactivateModule() -> int { return 0; }

auto GFUnregisterModule() -> int {
  MLogDebug("gnupg info gathering module unregistering...");

  return 0;
}

auto StartStartGatheringGnuPGComponentsInfo(
    const QString &gpgme_version, const QString &gpgconf_path) -> int {
  auto context = Context{gpgme_version, gpgconf_path};

  // get all components
  GFExecuteCommandSync(QDUP(gpgconf_path), 1,
                       QStringListToCharArray({"--list-components"}),
                       GetGpgComponentInfos, &context);
  MLogDebug("loading gnupg component info done.");
  return 0;
}

auto StartGatheringAllGnuPGInfo() -> int {
  const auto gpgme_version = UDUP(GFModuleRetrieveRTValueOrDefault(
      DUP("core"), DUP("gpgme.version"), DUP("0.0.0")));
  MLogDebug(QString("got gpgme version from rt: %1").arg(gpgme_version));

  const auto gpgconf_path = UDUP(GFModuleRetrieveRTValueOrDefault(
      DUP("core"), DUP("gpgme.ctx.gpgconf_path"), DUP("")));
  MLogDebug(QString("got gpgconf path from rt: %1").arg(gpgconf_path));

  // get components infos
  StartStartGatheringGnuPGComponentsInfo(gpgme_version, gpgconf_path);

  QList<GFCommandExecuteContext> exec_contexts;

  auto exec_context = GFCommandExecuteContext{
      QDUP(gpgconf_path), 1, QStringListToCharArray({"--list-dirs"}),
      GetGpgDirectoryInfos, nullptr};
  exec_contexts.push_back(exec_context);

  char **components_c_array;
  int ret = GFModuleListRTChildKeys(GFGetModuleID(), DUP("gnupg.components"),
                                    &components_c_array);
  if (components_c_array == nullptr || ret == 0) return -1;

  QStringList components;
  auto *p_a = components_c_array;
  for (int i = 0; i < ret; i++) components.append(QString::fromUtf8(p_a[i]));

  for (const auto &component : components) {
    const auto *component_info_json = GFModuleRetrieveRTValueOrDefault(
        GFGetModuleID(),
        DUP(QString("gnupg.components.%1").arg(component).toUtf8()), nullptr);

    if (component_info_json == nullptr) continue;

    auto jsonlized_component_info =
        QJsonDocument::fromJson(component_info_json);
    assert(jsonlized_component_info.isObject());

    auto component_info = GpgComponentInfo(jsonlized_component_info.object());
    MLogDebug(QString("gpgconf check options ready, component: %1")
                  .arg(component_info.name));

    if (component_info.name == "gpgme" || component_info.name == "gpgconf") {
      continue;
    }

    auto *context = new (GFAllocateMemory(sizeof(Context)))
        Context{gpgme_version, gpgconf_path, component_info};

    auto exec_context = GFCommandExecuteContext{
        QDUP(gpgconf_path), 2,
        QStringListToCharArray({"--list-options", component_info.name}),
        GetGpgOptionInfos, context};
    exec_contexts.push_back(exec_context);
  }

  GFExecuteCommandBatchSync(QListToArray(exec_contexts),
                            static_cast<int>(exec_contexts.size()));
  GFModuleUpsertRTValueBool(GFGetModuleID(), DUP("gnupg.gathering_done"), 1);

  return 0;
}

auto CalculateBinaryChecksum(const QString &path) -> std::optional<QString> {
  // check file info and access rights
  QFileInfo const info(path);
  if (!info.exists() || !info.isFile() || !info.isReadable()) {
    MLogDebug(QString("get info for file %1 error, exists: %2")
                  .arg(info.filePath())
                  .arg(info.exists()));
    return {};
  }

  // open and read file
  QFile f(info.filePath());
  if (!f.open(QIODevice::ReadOnly)) {
    MLogDebug(QString("open %1 to calculate checksum error: %2")
                  .arg(path)
                  .arg(f.errorString()));
    return {};
  }

  QCryptographicHash hash_sha(QCryptographicHash::Sha256);

  // read data by chunks
  const qint64 buffer_size = 8192;  // Define a suitable buffer size
  while (!f.atEnd()) {
    QByteArray const buffer = f.read(buffer_size);
    if (buffer.isEmpty()) {
      MLogDebug(QString("error reading file %1 during checksum calculation")
                    .arg(path));
      return {};
    }
    hash_sha.addData(buffer);
  }

  // close the file
  f.close();

  // return the first 6 characters of the SHA-256 hash
  // of the file
  return QString(hash_sha.result().toHex()).left(6);
}

void GetGpgComponentInfos(void *data, int exit_code, const char *out,
                          const char *err) {
  auto *context = reinterpret_cast<Context *>(data);
  auto p_out = QString::fromUtf8(out);
  auto p_err = QString::fromUtf8(err);

  MLogDebug(QString("gpgconf components exit_code: %1 process stdout size: %2")
                .arg(exit_code)
                .arg(p_out.size()));

  if (exit_code != 0) {
    MLogDebug(
        QString("gpgconf execute error, process stderr: %1, process stdout: %2")
            .arg(p_err)
            .arg(p_out));
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
  GFModuleUpsertRTValue(
      GFGetModuleID(), DUP("gnupg.components.gpgme"),
      DUP(QJsonDocument(jsonlized_gpgme_component_info).toJson()));
  GFModuleUpsertRTValue(
      GFGetModuleID(), DUP("gnupg.components.gpgconf"),
      DUP(QJsonDocument(jsonlized_gpgconf_component_info).toJson()));

  auto line_split_list = p_out.split("\n");

  for (const auto &line : line_split_list) {
    auto info_split_list = line.split(":");

    if (info_split_list.size() != 3) continue;

    auto component_name = info_split_list[0].trimmed();
    auto component_desc = info_split_list[1].trimmed();
    auto component_path = info_split_list[2].trimmed();

#ifdef __MINGW32__
    // replace some special substrings on windows
    // platform
    component_path.replace("%3a", ":");
#endif

    auto binary_checksum = CalculateBinaryChecksum(component_path);

    MLogDebug(
        QString("gnupg component name: %1 desc: %2 checksum: %3 path: %4")
            .arg(component_name)
            .arg(component_desc)
            .arg(binary_checksum.has_value() ? binary_checksum.value() : "/",
                 component_path));

    QString version = "/";

    if (component_name == "gpg") {
      version = GFModuleRetrieveRTValueOrDefault(
          DUP("core"), DUP("gpgme.ctx.gnupg_version"), DUP("2.0.0"));
    }
    if (component_name == "gpg-agent") {
      GFModuleUpsertRTValue(GFGetModuleID(), DUP("gnupg.gpg_agent_path"),
                            DUP(QString(component_path).toUtf8()));
    }
    if (component_name == "dirmngr") {
      GFModuleUpsertRTValue(GFGetModuleID(), DUP("gnupg.dirmngr_path"),
                            DUP(QString(component_path).toUtf8()));
    }
    if (component_name == "keyboxd") {
      GFModuleUpsertRTValue(GFGetModuleID(), DUP("gnupg.keyboxd_path"),
                            DUP(QString(component_path).toUtf8()));
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
      GFModuleUpsertRTValue(
          GFGetModuleID(),
          DUP(QString("gnupg.components.%1").arg(component_name).toUtf8()),
          DUP(QJsonDocument(jsonlized_component_info).toJson()));

      component_infos.push_back(c_i);
    }

    MLogDebug("load gnupg component info actually done.");
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
    MLogDebug(QString("gpgconf direcrotries info line: %1 info size: %2")
                  .arg(line)
                  .arg(info_split_list.size()));

    if (info_split_list.size() != 2) continue;

    auto configuration_name = info_split_list[0].trimmed();
    auto configuration_value = info_split_list[1].trimmed();

#ifdef __MINGW32__
    // replace some special substrings on windows
    // platform
    configuration_value.replace("%3a", ":");
#endif

    // record gnupg home path
    if (configuration_name == "homedir") {
      GFModuleUpsertRTValue(GFGetModuleID(), DUP("gnupg.home_path"),
                            DUP(configuration_value.toUtf8()));
    }

    GFModuleUpsertRTValue(
        GFGetModuleID(),
        DUP(QString("gnupg.dirs.%1").arg(configuration_name).toUtf8()),
        DUP(configuration_value.toUtf8()));
  }
}

void GetGpgOptionInfos(void *data, int exit_code, const char *out,
                       const char *err) {
  if (exit_code != 0) return;

  auto p_out = QString::fromUtf8(out);
  auto p_err = QString::fromUtf8(err);
  auto *context = reinterpret_cast<Context *>(data);
  auto component_name = context->component_info.name;

  MLogDebug(
      QString(
          "gpgconf %1 avaliable options exit_code: %2 process stdout size: %3")
          .arg(component_name)
          .arg(exit_code)
          .arg(p_out.size()));

  std::vector<GpgOptionsInfo> options_infos;

  auto line_split_list = p_out.split("\n");

  for (const auto &line : line_split_list) {
    auto info_split_list = line.split(":");

    MLogDebug(QString("component %1 available options line: %2 info size: %3")
                  .arg(component_name)
                  .arg(line)
                  .arg(info_split_list.size()));

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
    GFModuleUpsertRTValue(GFGetModuleID(),
                          DUP(QString("gnupg.components.%1.options.%2")
                                  .arg(component_name)
                                  .arg(option_name)
                                  .toUtf8()),
                          DUP(QJsonDocument(jsonlized_option_info).toJson()));
    options_infos.push_back(info);
  }

  context->~Context();
  GFFreeMemory(context);
}