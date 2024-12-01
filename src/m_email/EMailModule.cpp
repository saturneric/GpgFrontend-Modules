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

#include "EMailModule.h"

#include <GFSDKBasic.h>
#include <GFSDKBuildInfo.h>
#include <GFSDKLog.h>

// qt
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonDocument>
#include <QString>

#include "EMailMetaDataDialog.h"

// vmime
#define VMIME_STATIC
#include <vmime/vmime.hpp>

// vmime extend
#include <vmime/contentTypeField.hpp>

#include "GFModuleCommonUtils.hpp"
#include "GFModuleDefine.h"

//
#include "EMailBasicGpgOpera.h"
#include "EMailHelper.h"
#include "EMailMetaDataDialog.h"

GF_MODULE_API_DEFINE_V2("com.bktus.gpgfrontend.module.email", "Email", "1.0.0",
                        "Everything related to E-Mails.", "Saturneric")

DEFINE_TRANSLATIONS_STRUCTURE(ModuleEMail);

auto GFRegisterModule() -> int {
  MLogDebug("email module registering...");

  REGISTER_TRANS_READER();

  LISTEN("EMAIL_VERIFY_EML_DATA");
  LISTEN("EMAIL_DECRYPT_EML_DATA");
  LISTEN("EMAIL_SIGN_EML_DATA");
  LISTEN("EMAIL_ENCRYPT_EML_DATA");
  LISTEN("EMAIL_ENCRYPT_SIGN_EML_DATA");
  LISTEN("EMAIL_DECRYPT_VERIFY_EML_DATA");
  return 0;
}

auto GFActiveModule() -> int { return 0; }

REGISTER_EVENT_HANDLER(EMAIL_VERIFY_EML_DATA, [](const MEvent& event) -> int {
  if (event["channel"].isEmpty()) CB_ERR(event, -1, "channel is empty");
  if (event["eml_data"].isEmpty()) CB_ERR(event, -1, "eml_data is empty");

  auto channel = event.value("channel", "0").toInt();
  auto data = QByteArray::fromBase64(QString(event["eml_data"]).toLatin1());

  EMailMetaData meta_data;
  QString error_string;
  QString capsule_id;
  auto ret = VerifyEMLData(channel, data, meta_data, error_string, capsule_id);
  if (ret != 0) CB_ERR(event, ret, error_string);

  // callback
  CB(event, GFGetModuleID(),
     {
         {"ret", QString::number(0)},
         {"mime", QString::fromLatin1(meta_data.mime.toBase64())},
         {"mime_hash", meta_data.mime_hash},
         {"signature", QString::fromLatin1(meta_data.signature.toBase64())},
         {"from", meta_data.from},
         {"to", meta_data.to.join("; ")},
         {"cc", meta_data.cc.join("; ")},
         {"bcc", meta_data.bcc.join("; ")},
         {"subject", meta_data.subject},
         {"datetime", QString::number(meta_data.datetime.toMSecsSinceEpoch())},
         {"micalg", meta_data.micalg},
         {"public_keys", meta_data.public_keys},
         {"capsule_id", capsule_id},
     });

  return 0;
});

REGISTER_EVENT_HANDLER(EMAIL_DECRYPT_EML_DATA, [](const MEvent& event) -> int {
  if (event["channel"].isEmpty()) CB_ERR(event, -1, "channel is empty");
  if (event["eml_data"].isEmpty()) CB_ERR(event, -1, "eml_data is empty");

  auto channel = event.value("channel", "0").toInt();
  auto data = QByteArray::fromBase64(QString(event["eml_data"]).toLatin1());

  EMailMetaData meta_data;
  QString eml_data;
  QString capsule_id;
  auto ret = DecryptEMLData(channel, data, meta_data, eml_data, capsule_id);
  if (ret != 0) CB_ERR(event, ret, eml_data);

  // callback
  CB(event, GFGetModuleID(),
     {
         {"ret", QString::number(0)},
         {"eml_data", QString::fromLatin1(eml_data.toLatin1().toBase64())},
         {"from", meta_data.from},
         {"to", meta_data.to.join("; ")},
         {"cc", meta_data.cc.join("; ")},
         {"bcc", meta_data.bcc.join("; ")},
         {"subject", meta_data.subject},
         {"datetime", QString::number(meta_data.datetime.toMSecsSinceEpoch())},
         {"capsule_id", capsule_id},
     });

  return 0;
});

REGISTER_EVENT_HANDLER(EMAIL_SIGN_EML_DATA, [](const MEvent& event) -> int {
  if (event["body_data"].isEmpty()) CB_ERR(event, -1, "body_data is empty");
  if (event["channel"].isEmpty()) CB_ERR(event, -1, "channel is empty");
  if (event["sign_key"].isEmpty()) CB_ERR(event, -1, "sign_key is empty");

  auto channel = event.value("channel", "0").toInt();
  auto sign_key = event.value("sign_key", "");

  FLOG_DEBUG("eml sign key: %1", sign_key);

  auto body_data =
      QByteArray::fromBase64(QString(event["body_data"]).toLatin1());

  auto* dialog = GUI_OBJECT(CreateEMailMetaDataDialog, 0);
  auto* r_dialog =
      qobject_cast<EMailMetaDataDialog*>(static_cast<QObject*>(dialog));
  if (r_dialog == nullptr)
    CB_ERR(event, -1, "convert dialog to r_dialog failed");

  r_dialog->SetChannel(channel);
  r_dialog->SetKeys({sign_key});
  r_dialog->SetBodyData({body_data});

  vmime::shared_ptr<vmime::message> message;
  if (CheckIfEMLMessage(body_data, message)) {
    EMailMetaData meta_data;
    auto ret = GetEMLMetaData(message, meta_data);

    if (ret != 0) {
      CB_ERR(event, -1, "Get MetaData From EML Data Failed");
    }

    QString eml_data;
    QString capsule_id;
    ret = SignEMLData(channel, sign_key, message, eml_data, capsule_id);
    if (ret != 0) {
      CB_ERR(event, -2, eml_data);
    }

    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"eml_data", eml_data},
           {"capsule_id", capsule_id},
       });
    return 0;
  }

  GFUIShowDialog(dialog, nullptr);
  QObject::connect(r_dialog, &EMailMetaDataDialog::SignalEMLMetaData, r_dialog,
                   [=](const EMailMetaData& meta_data) {
                     QString eml_data;
                     QString capsule_id;
                     auto ret = SignPlainText(channel, sign_key, meta_data,
                                              body_data, eml_data, capsule_id);
                     if (ret != 0) {
                       CB_ERR(event, -2, eml_data);
                     }

                     CB(event, GFGetModuleID(),
                        {
                            {"ret", QString::number(0)},
                            {"eml_data", eml_data},
                            {"capsule_id", capsule_id},
                        });
                     return 0;
                   });

  QObject::connect(
      r_dialog, &EMailMetaDataDialog::SignalNoEMLMetaData, r_dialog,
      [=](const QString& error_string) { CB_ERR(event, -1, error_string); });

  return 0;
});

REGISTER_EVENT_HANDLER(EMAIL_ENCRYPT_EML_DATA, [](const MEvent& event) -> int {
  if (event["body_data"].isEmpty()) CB_ERR(event, -1, "body_data is empty");
  if (event["channel"].isEmpty()) CB_ERR(event, -1, "channel is empty");
  if (event["encrypt_keys"].isEmpty())
    CB_ERR(event, -1, "encrypt_keys is empty");

  auto channel = event.value("channel", "0").toInt();
  auto encrypt_keys = event.value("encrypt_keys", "").split(';');

  FLOG_DEBUG("eml encrypt keys: %1", encrypt_keys.join(';'));

  auto body_data =
      QByteArray::fromBase64(QString(event["body_data"]).toLatin1());

  vmime::shared_ptr<vmime::message> message;
  if (CheckIfEMLMessage(body_data, message)) {
    QString eml_data;
    QString capsule_id;
    auto ret = EncryptEMLData(channel, encrypt_keys, message, body_data,
                              eml_data, capsule_id);
    if (ret != 0) {
      CB_ERR(event, -2, eml_data);
    }

    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"eml_data", eml_data},
           {"capsule_id", capsule_id},
       });
    return 0;
  }

  auto* dialog = GUI_OBJECT(CreateEMailMetaDataDialog, 1);
  auto* r_dialog =
      qobject_cast<EMailMetaDataDialog*>(static_cast<QObject*>(dialog));
  if (r_dialog == nullptr)
    CB_ERR(event, -1, "convert dialog to r_dialog failed");

  r_dialog->SetChannel(channel);
  r_dialog->SetKeys(encrypt_keys);
  r_dialog->SetBodyData({body_data});

  GFUIShowDialog(dialog, nullptr);

  QObject::connect(r_dialog, &EMailMetaDataDialog::SignalEMLMetaData, r_dialog,
                   [=](const EMailMetaData& meta_data) {
                     QString eml_data;
                     QString capsule_id;
                     QString plain_text_eml_data;

                     auto ret = BuildPlainTextEML(meta_data, body_data,
                                                  plain_text_eml_data);

                     if (ret != 0) {
                       CB_ERR(event, -1, "Build PlainText EML Data Failed");
                     }

                     ret = EncryptPlainText(channel, encrypt_keys, meta_data,
                                            plain_text_eml_data.toLatin1(),
                                            eml_data, capsule_id);
                     if (ret != 0) {
                       CB_ERR(event, -2, eml_data);
                     }

                     CB(event, GFGetModuleID(),
                        {
                            {"ret", QString::number(0)},
                            {"eml_data", eml_data},
                            {"capsule_id", capsule_id},
                        });
                     return 0;
                   });

  QObject::connect(
      r_dialog, &EMailMetaDataDialog::SignalNoEMLMetaData, r_dialog,
      [=](const QString& error_string) { CB_ERR(event, -1, error_string); });

  return 0;
});

REGISTER_EVENT_HANDLER(
    EMAIL_ENCRYPT_SIGN_EML_DATA, [](const MEvent& event) -> int {
      if (event["body_data"].isEmpty()) CB_ERR(event, -1, "body_data is empty");
      if (event["channel"].isEmpty()) CB_ERR(event, -1, "channel is empty");
      if (event["encrypt_keys"].isEmpty())
        CB_ERR(event, -1, "encrypt_keys is empty");
      if (event["sign_key"].isEmpty()) CB_ERR(event, -1, "sign_key is empty");

      auto channel = event.value("channel", "0").toInt();
      auto sign_key = event.value("sign_key", "");
      auto encrypt_keys = event.value("encrypt_keys", "").split(';');

      FLOG_DEBUG("eml encrypt keys: %1", encrypt_keys.join(';'));

      auto body_data =
          QByteArray::fromBase64(QString(event["body_data"]).toLatin1());

      vmime::shared_ptr<vmime::message> message;
      if (CheckIfEMLMessage(body_data, message)) {
        QString eml_data;
        QString sign_capsule_id;
        auto ret =
            SignEMLData(channel, sign_key, message, eml_data, sign_capsule_id);
        if (ret != 0) {
          CB_ERR(event, -2, eml_data);
        }

        QByteArray body_data = eml_data.toLatin1();
        eml_data.clear();

        vmime::shared_ptr<vmime::message> signed_message;
        bool r = CheckIfEMLMessage(body_data, signed_message);
        if (!r) {
          CB_ERR(event, -1, "Parse Signed Message Failed");
        }

        QString encr_capsule_id;
        ret = EncryptEMLData(channel, encrypt_keys, signed_message, body_data,
                             eml_data, encr_capsule_id);
        if (ret != 0) {
          CB_ERR(event, -2, eml_data);
        }

        CB(event, GFGetModuleID(),
           {
               {"ret", QString::number(0)},
               {"eml_data", eml_data},
               {"sign_capsule_id", sign_capsule_id},
               {"encr_capsule_id", encr_capsule_id},
           });
        return 0;
      }

      auto* dialog = GUI_OBJECT(CreateEMailMetaDataDialog, 1);
      auto* r_dialog =
          qobject_cast<EMailMetaDataDialog*>(static_cast<QObject*>(dialog));
      if (r_dialog == nullptr)
        CB_ERR(event, -1, "convert dialog to r_dialog failed");

      r_dialog->SetChannel(channel);
      r_dialog->SetKeys(encrypt_keys);
      r_dialog->SetBodyData({body_data});

      GFUIShowDialog(dialog, nullptr);

      QObject::connect(
          r_dialog, &EMailMetaDataDialog::SignalEMLMetaData, r_dialog,
          [=](const EMailMetaData& meta_data) {
            QString eml_data;
            QString sign_capsule_id;
            QString encr_capsule_id;
            auto ret = SignPlainText(channel, sign_key, meta_data, body_data,
                                     eml_data, sign_capsule_id);
            if (ret != 0) {
              CB_ERR(event, -2, eml_data);
            }

            QByteArray body_data = eml_data.toLatin1();
            eml_data.clear();

            ret = EncryptPlainText(channel, encrypt_keys, meta_data, body_data,
                                   eml_data, encr_capsule_id);
            if (ret != 0) {
              CB_ERR(event, -2, eml_data);
            }

            CB(event, GFGetModuleID(),
               {
                   {"ret", QString::number(0)},
                   {"eml_data", eml_data},
                   {"sign_capsule_id", sign_capsule_id},
                   {"encr_capsule_id", encr_capsule_id},
               });
            return 0;
          });

      QObject::connect(r_dialog, &EMailMetaDataDialog::SignalNoEMLMetaData,
                       r_dialog, [=](const QString& error_string) {
                         CB_ERR(event, -1, error_string);
                       });

      return 0;
    });

REGISTER_EVENT_HANDLER(
    EMAIL_DECRYPT_VERIFY_EML_DATA, [](const MEvent& event) -> int {
      if (event["channel"].isEmpty()) CB_ERR(event, -1, "channel is empty");
      if (event["eml_data"].isEmpty()) CB_ERR(event, -1, "eml_data is empty");

      auto channel = event.value("channel", "0").toInt();
      auto data = QByteArray::fromBase64(QString(event["eml_data"]).toLatin1());

      auto body_data =
          QByteArray::fromBase64(QString(event["body_data"]).toLatin1());

      QString eml_data;
      QString decr_capsule_id;
      EMailMetaData meta_data;
      auto ret =
          DecryptEMLData(channel, data, meta_data, eml_data, decr_capsule_id);
      if (ret != 0) {
        CB_ERR(event, -2, eml_data);
      }

      QString verify_capsule_id;
      ret = VerifyEMLData(channel, eml_data.toLatin1(), meta_data, eml_data,
                          verify_capsule_id);
      if (ret != 0) {
        CB_ERR(event, -2, eml_data);
      }

      CB(event, GFGetModuleID(),
         {
             {"ret", QString::number(0)},
             {"eml_data", eml_data},
             {"decr_capsule_id", decr_capsule_id},
             {"verify_capsule_id", verify_capsule_id},
         });
      return 0;
    });

auto GFDeactivateModule() -> int { return 0; }

auto GFUnregisterModule() -> int {
  MLogDebug("email module unregistering...");

  return 0;
}
