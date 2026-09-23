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

#include "KeyServerSyncModule.h"

#include <GFSDKGpg.h>

#include <QtCore>
#include <QtNetwork>
#include <QtWidgets>

#include "GFModule.h"
#include "GFModuleIdentity.h"
#include "GFSDKHostCommands.hpp"
#include "GFSDKUI.h"
#include "KeyServerList.h"
#include "KeyServerSettingsPage.h"
#include "PKSInterface.h"
#include "SearchKeyDialog.h"
#include "VKSInterface.h"

namespace {

using Severity = gf::cmd::host::AppMessage::Severity;

/// A result for the user, shown by the Host from its own window: the module
/// has no business holding one of the Host's windows to parent a box to.
void Tell(Severity severity, const QString& title, const QString& text) {
  Commands().Invoke<gf::cmd::host::AppMessage>({severity, title, text});
}

/// A fingerprint to open the search dialog with, handed from whoever asked
/// to the widget factory the Host calls on the GUI thread.
auto SearchPreset() -> QString& {
  static QString preset;
  return preset;
}
QMutex g_search_preset_mutex;

void OpenSearch(const QString& fingerprint) {
  {
    QMutexLocker locker(&g_search_preset_mutex);
    SearchPreset() = fingerprint;
  }
  Commands().Invoke<gf::cmd::host::ViewOpen>(
      {gf::cmd::ViewRef{QStringLiteral(GF_MODULE_ID ".search")}});
}

using KeyCallback = std::function<void(const QString&)>;
using ErrorCallback = std::function<void(const QString&, const QString&)>;

/**
 * @brief Fetch one public key over whichever protocol @p route names.
 *
 * The two interfaces report in different shapes; normalising them here keeps
 * every caller free of the question, which is the only reason a server that
 * speaks just one of them can be used at all.
 *
 * @param by_fingerprint VKS has separate endpoints for the two handle kinds;
 *        HKP does not care.
 */
void FetchKey(const KeyServerList::Route& route, const QString& handle,
              bool by_fingerprint, const KeyCallback& on_key,
              const ErrorCallback& on_error) {
  if (route.vks) {
    auto* vks = new VKSInterface(route.url);
    QObject::connect(vks, &VKSInterface::SignalKeyRetrieved,
                     QThread::currentThread(),
                     [on_key](const QString& key) { on_key(key); });
    QObject::connect(vks, &VKSInterface::SignalErrorOccurred,
                     QThread::currentThread(),
                     [on_error](const QString& error, const QString& data) {
                       on_error(error, data);
                     });
    QObject::connect(vks, &VKSInterface::SignalKeyRetrieved, vks,
                     &VKSInterface::deleteLater);
    QObject::connect(vks, &VKSInterface::SignalErrorOccurred, vks,
                     &VKSInterface::deleteLater);

    if (by_fingerprint) {
      vks->GetByFingerprint(handle);
    } else {
      vks->GetByKeyId(handle);
    }
    return;
  }

  auto* pks = new PKSInterface();
  QObject::connect(
      pks, &PKSInterface::SignalKeyServerKeyLookupResult,
      QThread::currentThread(),
      [on_key, on_error](QNetworkReply::NetworkError error,
                         const QString& error_string, const QByteArray& data) {
        if (error != QNetworkReply::NoError) {
          on_error(error_string, QString::fromUtf8(data));
          return;
        }
        // An empty 200 is how some HKP servers say "no such key"; importing it
        // would report success while changing nothing.
        if (data.trimmed().isEmpty()) {
          on_error(QCoreApplication::translate(
                       "GTrC", "The key server did not return a key."),
                   {});
          return;
        }
        on_key(QString::fromUtf8(data));
      });
  QObject::connect(pks, &PKSInterface::SignalKeyServerKeyLookupResult, pks,
                   &PKSInterface::deleteLater);

  pks->LookupKeyById(route.url, handle);
}

/**
 * @brief Ask before publishing over HKP.
 *
 * VKS publishing is a different bargain: the server checks the address by mail
 * and the key can be taken down again. HKP offers neither, so the one place
 * where the protocol choice is not the caller's business is here.
 *
 * @return bool false when the user declined
 */
auto ConfirmHkpPublish(const QString& url) -> bool {
  const auto host = QUrl(url).host();
  return QMessageBox::warning(
             nullptr,
             QCoreApplication::translate("GTrC",
                                         "Publish Without Verification?"),
             QCoreApplication::translate(
                 "GTrC",
                 "%1 does not support verified publishing (VKS), so the key "
                 "would be uploaded over HKP instead.\n\n"
                 "The server will not confirm your email address, and the "
                 "upload cannot be undone: HKP key servers do not allow keys "
                 "to be removed.\n\n"
                 "Publish to %1 anyway?")
                 .arg(host),
             QMessageBox::Ok | QMessageBox::Cancel,
             QMessageBox::Cancel) == QMessageBox::Ok;
}
}  // namespace

auto OnActivate() -> GFResult {
  LOG_INFO("key server sync module registering");

  // Presentation is registered untranslated: the module translators are not
  // installed yet, so anything translated here would be stuck at the source
  // text for the rest of the session. The Host translates it when shown.
  const bool search = gf::ui::RegisterNativeWidget<SearchKeyDialog>(
      "search", {GC_TR("Key Server"), "", "", "", "", 0, 0},
      [](const QCborMap& /*args*/) {
        auto* dialog = new SearchKeyDialog();
        QString preset;
        {
          QMutexLocker locker(&g_search_preset_mutex);
          std::swap(preset, SearchPreset());
        }
        // An empty preset opens a blank search; only a fingerprint that was
        // actually supplied (the verify-failure flow) seeds the field.
        if (!preset.isEmpty()) dialog->SetPresetFingerprint(preset);
        return dialog;
      });
  const bool settings = gf::ui::RegisterNativeWidget<KeyServerSettingsPage>(
      "settings",
      {GC_TR("Key Servers"),
       GC_TR("keyserver,key server,hkp,vks,publish,search"), "", "", "", 0, 0},
      [](const QCborMap& /*args*/) { return new KeyServerSettingsPage(); });

  return search && settings
             ? GFResult::Ok()
             : GFResult::Fail("the key server widgets were not registered");
}

namespace {

auto UploadKeyToServer(int channel, const QString& key_id) -> int {
  const auto exported =
      gf::sdk::ExportKey(GFModuleSdkContext(), channel, key_id, true);
  if (exported.isEmpty()) {
    Tell(Severity::kError, QCoreApplication::translate("GTrC", "Key Upload Failed"),
        QCoreApplication::translate(
            "GTrC",
            "Failed to export the public key before uploading.\n"
            "Key: %1")
            .arg(key_id));
    return -1;
  }

  const auto key_text = QString::fromUtf8(exported);

  const auto route = KeyServerList::SyncRoute();
  const auto server = route.url;

  if (!route.vks) {
    if (!ConfirmHkpPublish(server)) return 0;

    auto* pks = new PKSInterface();
    QObject::connect(
        pks, &PKSInterface::SignalKeyServerKeyUploadResult,
        QThread::currentThread(),
        [server, key_id](QNetworkReply::NetworkError error,
                                 const QString& error_string) {
          if (error != QNetworkReply::NoError) {
            Tell(Severity::kError, QCoreApplication::translate("GTrC", "Key Upload Failed"),
                QCoreApplication::translate(
                    "GTrC",
                    "Failed to upload public key to the server.\n"
                    "Fingerprint: %1\n"
                    "Error: %2")
                    .arg(key_id, error_string));
            return;
          }

          // No verification mail follows an HKP upload, so do not promise one.
          Tell(Severity::kInfo, QCoreApplication::translate("GTrC",
                                          "Public Key Upload Successful"),
              QCoreApplication::translate(
                  "GTrC",
                  "The public key was uploaded to the key server %2 over "
                  "HKP.\n"
                  "Fingerprint: %1")
                  .arg(key_id, QUrl(server).host()));
        });
    QObject::connect(pks, &PKSInterface::SignalKeyServerKeyUploadResult, pks,
                     &PKSInterface::deleteLater);

    pks->UploadKey(server, key_text.toUtf8());
    return 0;
  }

  auto* vks = new VKSInterface(server);
  QObject::connect(
      vks, &VKSInterface::SignalKeyUploaded, QThread::currentThread(),
      [server](const QString& fpr, const QJsonObject& status,
                       const QString& token) {
        // Handle successful response
        QString status_message = QCoreApplication::translate(
            "GTrC", "The following email addresses have status:\n");
        QStringList email_list;
        if (!status.isEmpty()) {
          for (auto it = status.constBegin(); it != status.constEnd(); ++it) {
            status_message +=
                QString("%1: %2\n").arg(it.key(), it.value().toString());
            email_list.append(it.key());
          }
        } else {
          status_message += QCoreApplication::translate(
              "GTrC", "Could not parse status information.");
        }

        // Name the server that was actually used: it is configurable now, so a
        // hard-coded host in this message could simply be wrong.
        const auto host = QUrl(server).host();

        // Notify user of successful upload and status details
        Tell(Severity::kInfo, QCoreApplication::translate("GTrC", "Public Key Upload Successful"),
            QCoreApplication::translate(
                "GTrC",
                "The public key was successfully uploaded to the "
                "key server %4.\n"
                "Fingerprint: %1\n\n"
                "%2\n"
                "Please check your email (%3) for further "
                "verification from %4.")
                .arg(fpr, status_message, email_list.join(", "), host));
      });

  QObject::connect(
      vks, &VKSInterface::SignalErrorOccurred, QThread::currentThread(),
      [key_id](const QString& error, const QString& data) {
        Tell(Severity::kError, QCoreApplication::translate("GTrC", "Key Upload Failed"),
            QCoreApplication::translate(
                "GTrC",
                "Failed to upload public key to the server.\n"
                "Fingerprint: %1\n"
                "Error: %2")
                .arg(key_id, error));
      });

  QObject::connect(vks, &VKSInterface::SignalKeyUploaded, vks,
                   &VKSInterface::deleteLater);
  QObject::connect(vks, &VKSInterface::SignalErrorOccurred, vks,
                   &VKSInterface::deleteLater);

  vks->UploadKey(key_text);

  return 0;
}

auto UpdateKeyFromKeyServer(int channel, const QString& fpr) -> int {
  const auto route = KeyServerList::SyncRoute();
  const auto host = QUrl(route.url).host();

  FetchKey(
      route, fpr, true,
      [channel](const QString& key_data) {
        gf::sdk::ImportKeys(GFModuleSdkContext(), channel, nullptr,
                            key_data.toUtf8());
      },
      [fpr, host](const QString& error, const QString& data) {
        Q_UNUSED(data);
        // Name the server: it is the user's choice now, and a failure they
        // cannot attribute to a host is one they cannot fix.
        Tell(Severity::kError, QCoreApplication::translate("GTrC", "Key Update Failed"),
            QCoreApplication::translate(
                "GTrC",
                "Failed to retrieve public key from %3.\n"
                "Key ID: %1\n"
                "Error: %2")
                .arg(fpr, error, host));
      });
  return 0;
}

}  // namespace

namespace {

/// Both key commands act on one key, named by the Host's context.
struct KeyArgs {
  gf::cmd::KeyRef key;
  static constexpr auto Fields() {
    return std::make_tuple(gf::cmd::F("key", &KeyArgs::key));
  }
};

struct PublishKey {
  static constexpr gf::cmd::Meta kMeta{
      GF_MODULE_ID ".publish_key", GC_TR("Publish Public Key to Key Server"),
      GC_TR("Upload the public key to the key server used for syncing"),
      GC_TR("Key Server Operations"), 0, gf::cmd::kNeedsGuiThread};
  using Args = KeyArgs;
  using Result = gf::cmd::Unit;
};

struct RefreshKey {
  static constexpr gf::cmd::Meta kMeta{
      GF_MODULE_ID ".refresh_key", GC_TR("Refresh Public Key From Key Server"),
      GC_TR("Import the latest copy of the public key from the key server"),
      GC_TR("Key Server Operations"), 0, gf::cmd::kNeedsGuiThread};
  using Args = KeyArgs;
  using Result = gf::cmd::Unit;
};

struct SearchKey {
  static constexpr gf::cmd::Meta kMeta{
      GF_MODULE_ID ".search_key", GC_TR("Key Server"),
      GC_TR("Import public keys from a trusted key server."), "", 0,
      gf::cmd::kNeedsGuiThread};
  struct Args {
    std::optional<QString> fingerprint;
    static constexpr auto Fields() {
      return std::make_tuple(gf::cmd::F("fingerprint", &Args::fingerprint));
    }
  };
  using Result = gf::cmd::Unit;
};

auto DoPublishKey(const gf::cmd::CommandContext& /*ctx*/, const KeyArgs& a)
    -> gf::cmd::Outcome<gf::cmd::Unit> {
  // Any key with a public part can be published, mirroring the Key
  // Management "Publish Key to Keyserver" action.
  UploadKeyToServer(static_cast<int>(a.key.channel), a.key.key_id);
  return gf::cmd::Outcome<gf::cmd::Unit>::Success({});
}

auto DoRefreshKey(const gf::cmd::CommandContext& /*ctx*/, const KeyArgs& a)
    -> gf::cmd::Outcome<gf::cmd::Unit> {
  // Refresh re-imports the latest public key from the server; it is valid
  // for any key, including your own.
  UpdateKeyFromKeyServer(static_cast<int>(a.key.channel), a.key.fingerprint);
  return gf::cmd::Outcome<gf::cmd::Unit>::Success({});
}

auto DoSearchKey(const gf::cmd::CommandContext& /*ctx*/,
                 const SearchKey::Args& a) -> gf::cmd::Outcome<gf::cmd::Unit> {
  OpenSearch(a.fingerprint.value_or(QString()));
  return gf::cmd::Outcome<gf::cmd::Unit>::Success({});
}

}  // namespace

auto OnRequestGetPublicKeyByFingerprint(const GFEvent& event) -> GFEventResult {
  if (event.Str("fingerprint").isEmpty())
    return GFEventResult::Bad("fingerprint is empty");

  QString fingerprint = event.Str("fingerprint");
  FLOG_DEBUG("try to get key info of fingerprint: %1", fingerprint);

  const auto route = KeyServerList::SyncRoute();
  const auto server = route.url;

  FetchKey(
      route, fingerprint, true,
      [event, server](const QString& key) {
        event.Answer().Ok({{"key_data", key}, {"key_server", server}});
      },
      [event, server](const QString& error, const QString& data) {
        event.Answer().Fail(error,
                            {{"reply_data", data}, {"key_server", server}});
      });
  return GFEventResult::Deferred();
}

auto OnRequestGetPublicKeyByKeyId(const GFEvent& event) -> GFEventResult {
  if (event.Str("key_id").isEmpty())
    return GFEventResult::Bad("key_id is empty");

  QString key_id = event.Str("key_id");
  FLOG_DEBUG("try to get key info of key id: %1", key_id);

  const auto route = KeyServerList::SyncRoute();
  const auto server = route.url;

  FetchKey(
      route, key_id, false,
      [event, server](const QString& key) {
        event.Answer().Ok({{"key_data", key}, {"key_server", server}});
      },
      [event, server](const QString& error, const QString& data) {
        event.Answer().Fail(error,
                            {{"reply_data", data}, {"key_server", server}});
      });

  return GFEventResult::Deferred();
}

auto OnRequestUploadPublicKey(const GFEvent& event) -> GFEventResult {
  if (event.Str("key_text").isEmpty())
    return GFEventResult::Bad("key_text is empty");

  QByteArray key_text = event.Str("key_text").toLatin1();
  FLOG_DEBUG("try to get key info of key id: %1", key_text);

  const auto route = KeyServerList::SyncRoute();
  const auto server = route.url;

  if (!route.vks) {
    // No widget to ask through here, and the callers of this event already
    // confirm that publishing is permanent. Report the protocol so the
    // caller can say what actually happened rather than promise a
    // verification mail that is never coming.
    auto* pks = new PKSInterface();
    QObject::connect(
        pks, &PKSInterface::SignalKeyServerKeyUploadResult,
        QThread::currentThread(),
        [event, server](QNetworkReply::NetworkError error,
                        const QString& error_string) {
          if (error != QNetworkReply::NoError) {
            event.Answer().Fail(error_string,
                                {{"key_server", server}, {"protocol", "hkp"}});
            return;
          }

          event.Answer().Ok({{"key_server", server}, {"protocol", "hkp"}});
        });
    QObject::connect(pks, &PKSInterface::SignalKeyServerKeyUploadResult, pks,
                     &PKSInterface::deleteLater);

    pks->UploadKey(server, key_text);
    return GFEventResult::Deferred();
  }

  auto* vks = new VKSInterface(server);
  QObject::connect(
      vks, &VKSInterface::SignalKeyUploaded, QThread::currentThread(),
      [event, server](const QString& fpr, const QJsonObject& status,
                      const QString& token) {
        event.Answer().Ok(
            {{"fingerprint", fpr},
             {"status", QString::fromUtf8(QJsonDocument(status).toJson())},
             {"token", token},
             {"key_server", server},
             {"protocol", "vks"}});
      });
  QObject::connect(vks, &VKSInterface::SignalErrorOccurred,
                   QThread::currentThread(),
                   [event, server](const QString& error, const QString& data) {
                     event.Answer().Fail(error, {{"reply_data", data},
                                                 {"key_server", server},
                                                 {"protocol", "vks"}});
                   });
  // Released on the signals an upload actually sends: it never retrieves a
  // key, so connecting to that one left every upload task behind.
  QObject::connect(vks, &VKSInterface::SignalKeyUploaded, vks,
                   &VKSInterface::deleteLater);
  QObject::connect(vks, &VKSInterface::SignalErrorOccurred, vks,
                   &VKSInterface::deleteLater);
  vks->UploadKey(key_text);
  return GFEventResult::Deferred();
}

auto OnRequestSearchPublicKeyByFingerprint(const GFEvent& event)
    -> GFEventResult {
  const auto fingerprint = event.Str("fingerprint").trimmed();
  FLOG_DEBUG("open key server search dialog with fingerprint: %1", fingerprint);
  // The module's own dialog, opened by the Host in a frame of its own.
  OpenSearch(fingerprint);
  return GFEventResult::Ok();
}

auto OnUnload() -> void {
  // Said "paper key module" until now, copied from a module that no longer
  // exists -- so the one line naming who was shutting down named the wrong one.
  LOG_INFO("key server sync module unregistering");
}

// The module's whole framework surface.
constexpr std::array<GFEventBinding, 4> kEvents = {{
    {"REQUEST_GET_PUBLIC_KEY_BY_FINGERPRINT",
     &OnRequestGetPublicKeyByFingerprint},
    {"REQUEST_GET_PUBLIC_KEY_BY_KEY_ID", &OnRequestGetPublicKeyByKeyId},
    {"REQUEST_SEARCH_PUBLIC_KEY_BY_FINGERPRINT",
     &OnRequestSearchPublicKeyByFingerprint},
    {"REQUEST_UPLOAD_PUBLIC_KEY", &OnRequestUploadPublicKey},
}};

const std::array<gf::cmd::Binding, 3> kCommands = {
    gf::cmd::Bind<PublishKey, &DoPublishKey>(),
    gf::cmd::Bind<RefreshKey, &DoRefreshKey>(),
    gf::cmd::Bind<SearchKey, &DoSearchKey>(),
};

const GFModuleHooks kHooks = {
    sizeof(GFModuleHooks),
    GF_MODULE_ID,
    GF_MODULE_VERSION,
    GF_MODULE_TRANSLATION_CONTEXT,
    &OnActivate,
    nullptr,  // the Host withdraws the commands, widgets and script itself
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
