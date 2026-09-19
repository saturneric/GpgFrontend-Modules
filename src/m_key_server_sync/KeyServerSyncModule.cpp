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
#include "GFModuleBootstrap.h"
#include "GFSDKUI.h"
#include "KeyServerList.h"
#include "KeyServerSettingsPage.h"
#include "PKSInterface.h"
#include "SearchKeyDialog.h"
#include "VKSInterface.h"

namespace {
constexpr auto kSettingsPageId =
    "com.bktus.gpgfrontend.module.key_server_sync.settings";

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
auto ConfirmHkpPublish(QWidget* parent, const QString& url) -> bool {
  const auto host = QUrl(url).host();
  return QMessageBox::warning(
             parent,
             QCoreApplication::translate("GTrC",
                                         "Publish Without Verification?"),
             QCoreApplication::translate(
                 "GTrC",
                 "%1 does not support verified publishing (VKS), so the key "
                 "would be uploaded over HKP instead.\n\n"
                 "The server will not confirm your email address, and the "
                 "upload cannot be undone — HKP key servers do not let keys be "
                 "removed.\n\n"
                 "Publish to %1 anyway?")
                 .arg(host),
             QMessageBox::Ok | QMessageBox::Cancel,
             QMessageBox::Cancel) == QMessageBox::Ok;
}
}  // namespace

GF_MODULE_BOOTSTRAP()

DEFINE_TRANSLATIONS_STRUCTURE();

auto GFRegisterModule() -> int {
  LOG_INFO("key server sync module registering");

  REGISTER_TRANS_READER();

  return 0;
}

auto GFActiveModule() -> int {
  LISTEN("REQUEST_GET_PUBLIC_KEY_BY_FINGERPRINT");
  LISTEN("REQUEST_GET_PUBLIC_KEY_BY_KEY_ID");
  LISTEN("REQUEST_UPLOAD_PUBLIC_KEY");
  LISTEN("REQUEST_SEARCH_PUBLIC_KEY_BY_FINGERPRINT");
  LISTEN("MAINWINDOW_MENU_MOUNTED");
  LISTEN("KEY_PAIR_OPERA_MENU_CREATED");

  // Registered untranslated: the module translators are not installed yet, so
  // anything translated here would be stuck at the source text for the rest of
  // the session. The host translates these when it builds the dialog.
  const auto keywords =
      QStringList{GC_TR("keyserver"), GC_TR("key server"), GC_TR("hkp"),
                  GC_TR("vks"),       GC_TR("publish"),    GC_TR("search")}
          .join('\n');
  GFUIRegisterSettingsPage(
      kSettingsPageId, "keys_engines", GC_TR("Key Servers"),
      (keywords).toUtf8().constData(), KeyServerSettingsPageFactory, nullptr);

  return 0;
}

namespace {

auto UploadKeyToServer(QWidget* parent, int channel, const QString& key_id)
    -> int {
  char* key_data = nullptr;
  int size = 0;
  auto ret = GFGpgExportKey(channel, (key_id).toUtf8().constData(), 1,
                            &key_data, &size);
  if (ret != 0 || key_data == nullptr || size <= 0) {
    QMessageBox::critical(
        parent, QCoreApplication::translate("GTrC", "Key Upload Failed"),
        QCoreApplication::translate(
            "GTrC",
            "Failed to export the public key before uploading.\n"
            "Key: %1")
            .arg(key_id));
    return -1;
  }

  // UnStrDup takes ownership of key_data and frees it; key_data must not be
  // used afterwards.
  auto key_text = UDUP(key_data);

  const auto route = KeyServerList::SyncRoute();
  const auto server = route.url;

  if (!route.vks) {
    if (!ConfirmHkpPublish(parent, server)) return 0;

    auto* pks = new PKSInterface();
    QObject::connect(
        pks, &PKSInterface::SignalKeyServerKeyUploadResult,
        QThread::currentThread(),
        [parent, server, key_id](QNetworkReply::NetworkError error,
                                 const QString& error_string) {
          if (error != QNetworkReply::NoError) {
            QMessageBox::critical(
                parent,
                QCoreApplication::translate("GTrC", "Key Upload Failed"),
                QCoreApplication::translate(
                    "GTrC",
                    "Failed to upload public key to the server.\n"
                    "Fingerprint: %1\n"
                    "Error: %2")
                    .arg(key_id, error_string));
            return;
          }

          // No verification mail follows an HKP upload, so do not promise one.
          QMessageBox::information(
              parent,
              QCoreApplication::translate("GTrC",
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
      [parent, server](const QString& fpr, const QJsonObject& status,
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
        QMessageBox::information(
            parent,
            QCoreApplication::translate("GTrC", "Public Key Upload Successful"),
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
      [parent, key_id](const QString& error, const QString& data) {
        QMessageBox::critical(
            parent, QCoreApplication::translate("GTrC", "Key Upload Failed"),
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

auto UpdateKeyFromKeyServer(QWidget* parent, int channel, const QString& fpr)
    -> int {
  const auto route = KeyServerList::SyncRoute();
  const auto host = QUrl(route.url).host();

  FetchKey(
      route, fpr, true,
      [parent, channel](const QString& key_data) {
        auto data = key_data.toUtf8();
        GFGpgImportKeys(channel, parent, data.constData(),
                        static_cast<int>(data.size()));
      },
      [parent, fpr, host](const QString& error, const QString& data) {
        Q_UNUSED(data);
        // Name the server: it is the user's choice now, and a failure they
        // cannot attribute to a host is one they cannot fix.
        QMessageBox::critical(
            parent, QCoreApplication::translate("GTrC", "Key Update Failed"),
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

  if (!event.contains("import_key_menu")) {
    LOG_DEBUG("main window menu mounted event: no import_key_menu found");
    CB_ERR(event, -1, "no import_key_menu found");
  }

  auto* import_key_menu = GFUIGetGUIObjectAs<QMenu>(event["import_key_menu"]);
  if (!import_key_menu) {
    LOG_ERROR(
        "main window menu mounted: import_key_menu handle invalid or not "
        "QMenu");
    CB_ERR(event, -1, "import_key_menu handle invalid or not QMenu");
  }

  LOG_DEBUG("adding key server sync actions to import key menu");

  QMetaObject::invokeMethod(
      QApplication::instance(),
      [&]() -> void {
        QWidget* parent =
            qobject_cast<QWidget*>(static_cast<QObject*>(main_window));
        auto* action = new QAction(
            QCoreApplication::translate("GTrC", "Key Server"), parent);
        action->setToolTip(QCoreApplication::translate(
            "GTrC", "Import public keys from a trusted key server."));
        action->setIcon(QIcon(":/icons/import_key_from_server.png"));
        QObject::connect(action, &QAction::triggered, parent, [=]() {
          auto* dialog = new SearchKeyDialog(parent);
          dialog->show();
        });
        import_key_menu->addAction(action);
      },
      Qt::BlockingQueuedConnection);
  CB_SUCC(event);
});

REGISTER_EVENT_HANDLER(
    KEY_PAIR_OPERA_MENU_CREATED, [](const MEvent& event) -> int {
      auto* tab = GFUIGetGUIObjectAs<QWidget>(event["tab"]);
      if (!tab) {
        LOG_ERROR(
            "key pair opera menu created: tab handle "
            "invalid or not KeyPairOperaTab");
        CB_ERR(event, -1, "tab handle invalid or not KeyPairOperaTab");
      }

      auto* layout = GFUIGetGUIObjectAs<QVBoxLayout>(event["opera_layout"]);
      if (!layout) {
        LOG_ERROR(
            "key pair opera menu created: opera_menu handle "
            "invalid or not QMenu");
        CB_ERR(event, -1, "opera_menu handle invalid or not QMenu");
      }

      auto is_private_key = event["is_private_key"].toInt() != 0;
      auto has_master_key = event["has_master_key"].toInt() != 0;

      auto channel = event["channel"].toInt();
      auto key_id = event["key_id"];
      auto fpr = event["fpr"];

      FLOG_DEBUG(
          "adding key server sync actions: key id: %1, channel: %2, is "
          "private key: %3, has master key: %4",
          key_id, channel, static_cast<int>(is_private_key),
          static_cast<int>(has_master_key));

      QMetaObject::invokeMethod(QApplication::instance(), [=]() -> void {
        auto* menu = new QMenu(tab);

        auto* key_server_opera_button = new QPushButton(
            QCoreApplication::translate("GTrC", "Key Server Operations"));
        key_server_opera_button->setStyleSheet("text-align:center;");
        key_server_opera_button->setMenu(menu);

        // add upload / update key actions
        auto* upload_key_pair = new QAction(QCoreApplication::translate(
            "GTrC", "Publish Public Key to Key Server"));
        QObject::connect(upload_key_pair, &QAction::triggered, tab,
                         [=]() { UploadKeyToServer(tab, channel, key_id); });
        // Any key with a public part can be published, mirroring the Key
        // Management "Publish Key to Keyserver" action.

        auto* update_key_pair = new QAction(QCoreApplication::translate(
            "GTrC", "Refresh Public Key From Key Server"));
        QObject::connect(update_key_pair, &QAction::triggered, tab,
                         [=]() { UpdateKeyFromKeyServer(tab, channel, fpr); });

        // Refresh re-imports the latest public key from the server; it is
        // valid for any key, including your own.

        menu->addAction(upload_key_pair);
        menu->addAction(update_key_pair);

        layout->addWidget(key_server_opera_button);
      });

      CB_SUCC(event);
    });

REGISTER_EVENT_HANDLER(
    REQUEST_GET_PUBLIC_KEY_BY_FINGERPRINT, [](const MEvent& event) -> int {
      if (event["fingerprint"].isEmpty())
        CB_ERR(event, -1, "fingerprint is empty");

      QString fingerprint = event["fingerprint"];
      FLOG_DEBUG("try to get key info of fingerprint: %1", fingerprint);

      const auto route = KeyServerList::SyncRoute();
      const auto server = route.url;

      FetchKey(
          route, fingerprint, true,
          [event, server](const QString& key) {
            CB(event, GFGetModuleID(),
               {
                   {"ret", QString::number(0)},
                   {"key_data", key},
                   {"key_server", server},
               });
          },
          [event, server](const QString& error, const QString& data) {
            CB(event, GFGetModuleID(),
               {
                   {"ret", QString::number(-1)},
                   {"error_msg", error},
                   {"reply_data", data},
                   {"key_server", server},
               });
          });
      return 0;
    });

REGISTER_EVENT_HANDLER(
    REQUEST_GET_PUBLIC_KEY_BY_KEY_ID, [](const MEvent& event) -> int {
      if (event["key_id"].isEmpty()) CB_ERR(event, -1, "key_id is empty");

      QString key_id = event["key_id"];
      FLOG_DEBUG("try to get key info of key id: %1", key_id);

      const auto route = KeyServerList::SyncRoute();
      const auto server = route.url;

      FetchKey(
          route, key_id, false,
          [event, server](const QString& key) {
            CB(event, GFGetModuleID(),
               {
                   {"ret", QString::number(0)},
                   {"key_data", key},
                   {"key_server", server},
               });
          },
          [event, server](const QString& error, const QString& data) {
            CB(event, GFGetModuleID(),
               {
                   {"ret", QString::number(-1)},
                   {"error_msg", error},
                   {"reply_data", data},
                   {"key_server", server},
               });
          });

      return 0;
    });

REGISTER_EVENT_HANDLER(
    REQUEST_UPLOAD_PUBLIC_KEY, [](const MEvent& event) -> int {
      if (event["key_text"].isEmpty()) CB_ERR(event, -1, "key_text is empty");

      QByteArray key_text = event["key_text"].toLatin1();
      FLOG_DEBUG("try to get key info of key id: %1", key_text);

      const auto route = KeyServerList::SyncRoute();
      const auto server = route.url;

      if (!route.vks) {
        // No widget to ask through here, and the callers of this event already
        // confirm that publishing is permanent. Report the protocol so the
        // caller can say what actually happened rather than promise a
        // verification mail that is never coming.
        auto* pks = new PKSInterface();
        QObject::connect(pks, &PKSInterface::SignalKeyServerKeyUploadResult,
                         QThread::currentThread(),
                         [event, server](QNetworkReply::NetworkError error,
                                         const QString& error_string) {
                           if (error != QNetworkReply::NoError) {
                             CB(event, GFGetModuleID(),
                                {
                                    {"ret", QString::number(-1)},
                                    {"error_msg", error_string},
                                    {"key_server", server},
                                    {"protocol", "hkp"},
                                });
                             return;
                           }

                           CB(event, GFGetModuleID(),
                              {
                                  {"ret", QString::number(0)},
                                  {"key_server", server},
                                  {"protocol", "hkp"},
                              });
                         });
        QObject::connect(pks, &PKSInterface::SignalKeyServerKeyUploadResult,
                         pks, &PKSInterface::deleteLater);

        pks->UploadKey(server, key_text);
        return 0;
      }

      auto* vks = new VKSInterface(server);
      QObject::connect(
          vks, &VKSInterface::SignalKeyUploaded, QThread::currentThread(),
          [event, server](const QString& fpr, const QJsonObject& status,
                          const QString& token) {
            CB(event, GFGetModuleID(),
               {
                   {"ret", QString::number(0)},
                   {"fingerprint", fpr},
                   {"status",
                    QString::fromUtf8(QJsonDocument(status).toJson())},
                   {"token", token},
                   {"key_server", server},
                   {"protocol", "vks"},
               });
          });
      QObject::connect(
          vks, &VKSInterface::SignalErrorOccurred, QThread::currentThread(),
          [event, server](const QString& error, const QString& data) {
            CB(event, GFGetModuleID(),
               {
                   {"ret", QString::number(-1)},
                   {"error_msg", error},
                   {"reply_data", data},
                   {"key_server", server},
                   {"protocol", "vks"},
               });
          });
      QObject::connect(vks, &VKSInterface::SignalKeyRetrieved, vks,
                       &VKSInterface::deleteLater);
      vks->UploadKey(key_text);
      return 0;
    });

REGISTER_EVENT_HANDLER(
    REQUEST_SEARCH_PUBLIC_KEY_BY_FINGERPRINT, [](const MEvent& event) -> int {
      auto fingerprint = event["fingerprint"].trimmed();

      QWidget* parent = nullptr;

      if (event.contains("parent")) {
        parent = GFUIGetGUIObjectAs<QWidget>(event["parent"]);
      }

      if (parent == nullptr) {
        parent = QApplication::activeWindow();
      }

      FLOG_DEBUG("open key server search dialog with fingerprint: %1",
                 fingerprint);

      QMetaObject::invokeMethod(
          QApplication::instance(),
          [parent, fingerprint]() {
            auto* dialog = new SearchKeyDialog(parent);
            // An empty preset opens a blank search dialog; only seed the field
            // when a fingerprint was actually supplied (verify-failure flow).
            if (!fingerprint.isEmpty()) {
              dialog->SetPresetFingerprint(fingerprint);
            }
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->show();
            dialog->raise();
            dialog->activateWindow();
          },
          Qt::QueuedConnection);

      CB_SUCC(event);
    });

auto GFDeactivateModule() -> int {
  // The registry holds a function pointer into this shared object; leaving it
  // behind would crash the next time the Settings dialog is built.
  GFUIUnregisterSettingsPage(kSettingsPageId);
  return 0;
}

auto GFUnregisterModule() -> int {
  // Said "paper key module" until now, copied from a module that no longer
  // exists -- so the one line naming who was shutting down named the wrong one.
  LOG_INFO("key server sync module unregistering");

  return 0;
}