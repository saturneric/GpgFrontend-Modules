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
#include <QtWidgets>

#include "GFModuleDefine.h"
#include "SearchKeyDialog.h"
#include "VKSInterface.h"

GF_MODULE_API_DEFINE_V2("com.bktus.gpgfrontend.module.key_server_sync",
                        "KeyServerSync", "1.0.0",
                        "Sync Information From Trusted Key Server.",
                        "Saturneric")

auto GFRegisterModule() -> int {
  LOG_DEBUG("key server sync module registering");

  return 0;
}

auto GFActiveModule() -> int {
  LISTEN("REQUEST_GET_PUBLIC_KEY_BY_FINGERPRINT");
  LISTEN("REQUEST_GET_PUBLIC_KEY_BY_KEY_ID");
  LISTEN("REQUEST_UPLOAD_PUBLIC_KEY");
  LISTEN("MAINWINDOW_MENU_MOUNTED");
  LISTEN("KEY_PAIR_OPERA_MENU_CREATED");
  return 0;
}

namespace {

auto UploadKeyToServer(QWidget* parent, int channel, const QString& key_id)
    -> int {
  char* key_data = nullptr;
  int size = 0;
  auto ret = GFGpgExportKey(channel, QDUP(key_id), 1, &key_data, &size);
  if (ret != 0 || key_data == nullptr || size <= 0) {
    return -1;
  }

  auto data = UDUP(key_data);

  if (data.size() != size) {
    FLOG_ERROR("uploaded key data size mismatch: expected %1, got %2", size,
               data.size());
    return -1;
  }

  QByteArray key_data_array = data.toUtf8();

  auto* vks = new VKSInterface();
  QObject::connect(
      vks, &VKSInterface::SignalKeyUploaded, QThread::currentThread(),
      [parent](const QString& fpr, const QJsonObject& status,
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

        // Notify user of successful upload and status details
        QMessageBox::information(
            parent,
            QCoreApplication::translate("GTrC", "Public Key Upload Successful"),
            QCoreApplication::translate(
                "GTrC",
                "The public key was successfully uploaded to the "
                "key server keys.openpgp.org.\n"
                "Fingerprint: %1\n\n"
                "%2\n"
                "Please check your email (%3) for further "
                "verification from keys.openpgp.org.\n\n"
                "Note: For verification, you can find more "
                "information here: "
                "https://keys.openpgp.org/about")
                .arg(fpr, status_message, email_list.join(", ")));
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

  QObject::connect(vks, &VKSInterface::SignalKeyRetrieved, vks,
                   &VKSInterface::deleteLater);

  vks->UploadKey(key_data);

  return 0;
}

auto UpdateKeyFromKeyServer(QWidget* parent, int channel, const QString& fpr)
    -> int {
  auto* vks = new VKSInterface();

  QObject::connect(vks, &VKSInterface::SignalKeyRetrieved,
                   QThread::currentThread(),
                   [parent, channel](const QString& key_data) {
                     auto data = key_data.toUtf8();
                     GFGpgImportKeys(channel, parent, data.constData(),
                                     static_cast<int>(data.size()));
                   });

  QObject::connect(
      vks, &VKSInterface::SignalErrorOccurred, QThread::currentThread(),
      [parent, fpr](const QString& error, const QString& data) {
        QMessageBox::critical(
            parent, QCoreApplication::translate("GTrC", "Key Update Failed"),
            QCoreApplication::translate(
                "GTrC",
                "Failed to retrieve public key from the server.\n"
                "Key ID: %1\n"
                "Error: %2")
                .arg(fpr, error));
      });
  QObject::connect(vks, &VKSInterface::SignalKeyRetrieved, vks,
                   &VKSInterface::deleteLater);
  vks->GetByFingerprint(fpr);
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
            QCoreApplication::translate("GTrC", "Key Server"), nullptr);
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
        if (!(is_private_key && has_master_key)) {
          upload_key_pair->setDisabled(true);
        }

        auto* update_key_pair = new QAction(QCoreApplication::translate(
            "GTrC", "Refresh Public Key From Key Server"));
        QObject::connect(update_key_pair, &QAction::triggered, tab,
                         [=]() { UpdateKeyFromKeyServer(tab, channel, fpr); });

        // when a key has primary key, it should always upload to keyserver.
        if (has_master_key) {
          update_key_pair->setDisabled(true);
        }

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

      QByteArray fingerprint = event["fingerprint"].toLatin1();
      FLOG_DEBUG("try to get key info of fingerprint: %1", fingerprint);

      auto* vks = new VKSInterface();
      QObject::connect(vks, &VKSInterface::SignalKeyRetrieved,
                       QThread::currentThread(), [event](const QString& key) {
                         // callback
                         CB(event, GFGetModuleID(),
                            {
                                {"ret", QString::number(0)},
                                {"key_data", key},
                            });
                       });
      QObject::connect(vks, &VKSInterface::SignalKeyRetrieved, vks,
                       &VKSInterface::deleteLater);

      QObject::connect(vks, &VKSInterface::SignalErrorOccurred,
                       QThread::currentThread(),
                       [event](const QString& error, const QString& data) {
                         CB(event, GFGetModuleID(),
                            {
                                {"ret", QString::number(-1)},
                                {"error_msg", error},
                                {"reply_data", data},
                            });
                       });
      QObject::connect(vks, &VKSInterface::SignalKeyRetrieved, vks,
                       &VKSInterface::deleteLater);
      vks->GetByFingerprint(fingerprint);
      return 0;
    });

REGISTER_EVENT_HANDLER(
    REQUEST_GET_PUBLIC_KEY_BY_KEY_ID, [](const MEvent& event) -> int {
      if (event["key_id"].isEmpty()) CB_ERR(event, -1, "key_id is empty");

      QByteArray key_id = event["key_id"].toLatin1();
      FLOG_DEBUG("try to get key info of key id: %1", key_id);

      auto* vks = new VKSInterface();
      QObject::connect(vks, &VKSInterface::SignalKeyRetrieved,
                       QThread::currentThread(), [event](const QString& key) {
                         // callback
                         CB(event, GFGetModuleID(),
                            {
                                {"ret", QString::number(0)},
                                {"key_data", key},
                            });
                       });
      QObject::connect(vks, &VKSInterface::SignalErrorOccurred,
                       QThread::currentThread(),
                       [event](const QString& error, const QString& data) {
                         CB(event, GFGetModuleID(),
                            {
                                {"ret", QString::number(-1)},
                                {"error_msg", error},
                                {"reply_data", data},
                            });
                       });
      QObject::connect(vks, &VKSInterface::SignalKeyRetrieved, vks,
                       &VKSInterface::deleteLater);
      vks->GetByKeyId(key_id);

      return 0;
    });

REGISTER_EVENT_HANDLER(
    REQUEST_UPLOAD_PUBLIC_KEY, [](const MEvent& event) -> int {
      if (event["key_text"].isEmpty()) CB_ERR(event, -1, "key_text is empty");

      QByteArray key_text = event["key_text"].toLatin1();
      FLOG_DEBUG("try to get key info of key id: %1", key_text);

      auto* vks = new VKSInterface();
      QObject::connect(
          vks, &VKSInterface::SignalKeyUploaded, QThread::currentThread(),
          [event](const QString& fpr, const QJsonObject& status,
                  const QString& token) {
            CB(event, GFGetModuleID(),
               {
                   {"ret", QString::number(0)},
                   {"fingerprint", fpr},
                   {"status",
                    QString::fromUtf8(QJsonDocument(status).toJson())},
                   {"token", token},
               });
          });
      QObject::connect(vks, &VKSInterface::SignalErrorOccurred,
                       QThread::currentThread(),
                       [event](const QString& error, const QString& data) {
                         CB(event, GFGetModuleID(),
                            {
                                {"ret", QString::number(-1)},
                                {"error_msg", error},
                                {"reply_data", data},
                            });
                       });
      QObject::connect(vks, &VKSInterface::SignalKeyRetrieved, vks,
                       &VKSInterface::deleteLater);
      vks->UploadKey(key_text);
      return 0;
    });

auto GFDeactivateModule() -> int { return 0; }

auto GFUnregisterModule() -> int {
  MLogDebug("paper key module unregistering");

  return 0;
}