/**
 * Copyright (C) 2021-2026 Saturneric <eric@bktus.com>
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
#include <QApplication>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QString>
#include <QTextDocument>

#include "EMailMetaDataDialog.h"

// vmime
#define VMIME_STATIC
#include <algorithm>
#include <vmime/vmime.hpp>

// vmime extend
#include <vmime/contentTypeField.hpp>

#include "GFModuleCommonUtils.hpp"
#include "GFModuleDefine.h"

//
#include "EMailBasicGpgOpera.h"
#include "EMailHelper.h"

GF_MODULE_API_DEFINE_V2("com.bktus.gpgfrontend.module.email", "Email", "1.2.2",
                        "Everything related to E-Mails.", "Saturneric")

DEFINE_TRANSLATIONS_STRUCTURE(ModuleEMail);

namespace {

// Build one Info Board card object (title/status/fields) in the JSON shape the
// UI's decode_info_board_cards() expects. Empty values are dropped so the card
// stays compact, matching how the native operations render.
auto MakeCardJson(const QString& title, const QString& status,
                  const QList<QPair<QString, QString>>& fields) -> QJsonObject {
  QJsonArray field_array;
  for (const auto& f : fields) {
    if (f.second.isEmpty()) continue;
    field_array.append(QJsonArray{f.first, f.second});
  }
  return QJsonObject{
      {"title", title}, {"status", status}, {"fields", field_array}};
}

// Structured E-Mail header card from parsed metadata.
auto BuildEMailHeaderCard(const EMailMetaData& m) -> QJsonObject {
  return MakeCardJson(
      QApplication::translate("EMailModule", "E-Mail"), "neutral",
      {{QApplication::translate("EMailModule", "From"), m.from},
       {QApplication::translate("EMailModule", "To"), m.to.join("; ")},
       {QApplication::translate("EMailModule", "Subject"), m.subject},
       {QApplication::translate("EMailModule", "CC"), m.cc.join("; ")},
       {QApplication::translate("EMailModule", "BCC"), m.bcc.join("; ")},
       {QApplication::translate("EMailModule", "Date"),
        m.datetime.isValid() ? QLocale().toString(m.datetime) : QString()}});
}

// Structured OpenPGP/MIME metadata card from parsed metadata.
auto BuildOpenPGPMetaCard(const EMailMetaData& m) -> QJsonObject {
  return MakeCardJson(
      QApplication::translate("EMailModule", "OpenPGP"), "neutral",
      {{QApplication::translate("EMailModule", "Signed EML Data Hash (SHA1)"),
        m.mime_hash},
       {QApplication::translate("EMailModule",
                                "Message Integrity Check Algorithm"),
        m.micalg}});
}

// Build "Encryption Recipient" cards from the selected recipient keys. The
// encrypt result itself does not carry recipient identities for every engine
// (GnuPG reports none), but the module always knows which keys it encrypted to,
// so resolve each to its primary UID here — mirroring the native operation's
// recipient card.
auto BuildRecipientCards(int channel, const QStringList& encrypt_keys)
    -> QJsonArray {
  QJsonArray cards;
  for (const auto& key_id : encrypt_keys) {
    if (key_id.isEmpty()) continue;

    QString recipient;
    GFGpgKeyUID* uid = nullptr;
    if (GFGpgKeyPrimaryUID(channel, QDUP(key_id), &uid) == 0 &&
        uid != nullptr) {
      const auto name = UDUP(uid->name);
      const auto email = UDUP(uid->email);
      UDUP(uid->comment);  // free the unused field
      GFFreeMemory(uid);
      recipient = email.isEmpty() ? name : QString("%1 <%2>").arg(name, email);
    }

    cards.append(MakeCardJson(
        QApplication::translate("EMailModule", "Encryption Recipient"), "ok",
        {{QApplication::translate("EMailModule", "Recipient"), recipient},
         {QApplication::translate("EMailModule", "Key ID"), key_id}}));
  }
  return cards;
}

// Concatenate two crypto card JSON arrays (as returned by GFAnalyse*Result)
// into one, for combined operations like Encrypt+Sign and Decrypt+Verify.
auto MergeCardArrays(const QString& a, const QString& b) -> QString {
  QJsonArray merged;
  for (const auto& s : {a, b}) {
    if (s.isEmpty()) continue;
    const auto doc = QJsonDocument::fromJson(s.toUtf8());
    if (doc.isArray()) {
      for (const auto& v : doc.array()) merged.append(v);
    }
  }
  return QString::fromUtf8(
      QJsonDocument(merged).toJson(QJsonDocument::Compact));
}

// Assemble the `result_cards` payload the UI decodes: the module's own metadata
// cards followed by the crypto-analysis cards produced by the SDK (which reuses
// the same converter the native operations use). `crypto_cards_json` is the
// JSON array string returned via the GFAnalyse*Result `cards` out-param.
auto BuildResultCardsParam(const QString& operation,
                           const QJsonArray& meta_cards,
                           const QString& crypto_cards_json) -> QString {
  QJsonArray cards = meta_cards;
  if (!crypto_cards_json.isEmpty()) {
    const auto doc = QJsonDocument::fromJson(crypto_cards_json.toUtf8());
    if (doc.isArray()) {
      for (const auto& v : doc.array()) cards.append(v);
    }
  }

  if (cards.isEmpty()) return {};

  QJsonObject obj{{"operation", operation}, {"cards", cards}};
  return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

}  // namespace

auto GFRegisterModule() -> int {
  MLogDebug("email module registering...");

  REGISTER_TRANS_READER();

  LISTEN("MAINWINDOW_MENU_MOUNTED");

  LISTEN("EDIT_TAB_TYPE_EMAIL_OP_ENCRYPT");
  LISTEN("EDIT_TAB_TYPE_EMAIL_OP_DECRYPT");
  LISTEN("EDIT_TAB_TYPE_EMAIL_OP_SIGN");
  LISTEN("EDIT_TAB_TYPE_EMAIL_OP_VERIFY");
  LISTEN("EDIT_TAB_TYPE_EMAIL_OP_ENCRYPT_SIGN");
  LISTEN("EDIT_TAB_TYPE_EMAIL_OP_DECRYPT_VERIFY");

  LISTEN("EDIT_TAB_TYPE_EMAIL_OP_SAVE_FILE");

  // register file extension handler
  GFUIRegisterFileExtensionHandleEvent(DUP("eml"), DUP("EMAIL"));

  LISTEN("FILE_EXT_EMAIL_OP_OPEN_FILE");
  return 0;
}

auto GFActiveModule() -> int { return 0; }

auto GFDeactivateModule() -> int { return 0; }

auto GFUnregisterModule() -> int {
  MLogDebug("email module unregistering...");

  return 0;
}

namespace {

auto ErrorHelper(int ret, const QString& err) -> QString {
  if (ret == -2) {
    auto info =
        QApplication::translate(
            "EMailModule",
            "# EML Data Error\n\n"
            "The provided EML data does not conform to RFC 3156 standards "
            "and cannot be processed.\n\n"
            "**Details:** %1\n\n"
            "### What is EML Data?\n"
            "EML is a file format for representing email messages, "
            "typically "
            "including headers, body text, attachments, and metadata. "
            "Complete and properly structured EML data is required for "
            "validation.\n\n"
            "### Suggested Solutions\n"
            "1. Verify the EML data is complete and matches the structure "
            "outlined in RFC 3156.\n"
            "2. Refer to the official documentation for the EML structure: "
            "%2\n\n"
            "After correcting the EML data, try the operation again.")
            .arg(err)
            .arg("https://www.rfc-editor.org/rfc/rfc3156.txt");

    return info;
  }

  auto error_message =
      QApplication::translate(
          "EMailModule",
          "# Email Operation Error\n\n"
          "An error occurred during the email operation. The process "
          "could not be completed.\n\n"
          "**Details:**\n"
          "- **Error Code:** %1\n"
          "- **Error Message:** %2\n\n"
          "### Possible Causes\n"
          "1. The email data may be incomplete or corrupted.\n"
          "2. The selected GPG key does not have the necessary "
          "permissions.\n"
          "3. Issues in the GPG environment or configuration.\n\n"
          "### Suggested Solutions\n"
          "1. Ensure the email data is complete and follows the expected "
          "format.\n"
          "2. Verify the GPG key has the required access permissions.\n"
          "3. Check your GPG environment and configuration settings.\n"
          "4. Review the error details above or application logs for "
          "further troubleshooting.\n\n"
          "If the issue persists, consider seeking technical support or "
          "consulting the documentation.")
          .arg(ret)
          .arg(err);

  return error_message;
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

  auto* workspace_menu =
      GFUIGetGUIObjectAs<QMenu>(event["file_workspace_menu"]);
  if (!workspace_menu) {
    LOG_ERROR(
        "main window menu mounted: workspace_menu handle invalid or not "
        "QMenu");
    CB_ERR(event, -1, "workspace_menu handle invalid or not QMenu");
  }

  LOG_DEBUG("adding key server sync actions to import key menu");

  auto* edit = GFUIGetGUIObjectAs<QWidget>("main_window_edit");
  if (!edit) {
    LOG_ERROR(
        "main window menu mounted: main_window_edit handle invalid or not "
        "QWidget");
    CB_ERR(event, -1, "main_window_edit handle invalid or not QWidget");
  }

  QMetaObject::invokeMethod(
      QApplication::instance(),
      [&]() -> void {
        QWidget* parent =
            qobject_cast<QWidget*>(static_cast<QObject*>(main_window));
        auto* action = new QAction(
            QCoreApplication::translate("GTrC", "Mail Editor"), parent);

        action->setToolTip(QCoreApplication::translate(
            "GTrC", "Open a new text editor for email."));
        action->setIcon(QIcon(":/icons/email.png"));
        bool ok =
            QObject::connect(action, &QAction::triggered, parent, [edit]() {
              QMetaObject::invokeMethod(
                  edit, "SlotNewCustomTab", Qt::DirectConnection,
                  Q_ARG(QString, "email"), Q_ARG(QString, "untitled.eml"),
                  Q_ARG(QIcon, QIcon(":/icons/email.png")),
                  Q_ARG(QString, ":/icons/email.png"));
            });

        if (!ok) {
          LOG_ERROR("connecting mail editor action failed");
        }

        workspace_menu->addAction(action);
      },
      Qt::BlockingQueuedConnection);
  CB_SUCC(event);
});

namespace {

auto DoVerifyEMLData(int channel, const QByteArray& data, const MEvent& event,
                     int& result_status, QString& result_detail,
                     QString& result_cards, QString& error_string,
                     EMailMetaData& meta_data) -> int {
  gpg_error_t err;
  QString capsule_id;
  auto ret =
      VerifyEMLData(channel, data, meta_data, error_string, err, capsule_id);
  if (ret == kFAILED || ret == kEML_FAILED) {
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"result_status", QString::number(-1)},
           {"result", ErrorHelper(ret, error_string)},
       });
    return ret;
  }

  const char* tmp = nullptr;
  const char* cards_tmp = nullptr;
  result_status = GFAnalyseVerifyResultByCapsule(channel, err, QDUP(capsule_id),
                                                 &tmp, &cards_tmp);
  result_detail = UnStrDup(tmp);
  result_cards = UnStrDup(cards_tmp);

  if (ret == kGPG_FAILED) {
    // decrypt failed
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"result_status", QString::number(result_status)},
           {"result", result_detail},
       });
    return ret;
  }

  if (ret != kSUCCESS) {
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"result_status", QString::number(-1)},
           {"result", ErrorHelper(ret, error_string)},
       });
    return ret;
  }
  return kSUCCESS;
}
}  // namespace

REGISTER_EVENT_HANDLER(
    EDIT_TAB_TYPE_EMAIL_OP_VERIFY, [](const MEvent& event) -> int {
      if (event["channel"].isEmpty()) CB_ERR(event, -1, "channel is empty");
      if (event["data"].isEmpty()) CB_ERR(event, -1, "data is empty");

      auto channel = event.value("channel", "0").toInt();
      auto data = QByteArray::fromBase64(QString(event["data"]).toLatin1());

      EMailMetaData meta_data;
      QString error_string;
      int result_status;
      QString result_detail;
      QString result_cards;
      if (DoVerifyEMLData(channel, data, event, result_status, result_detail,
                          result_cards, error_string, meta_data) != kSUCCESS) {
        return -1;
      }

      QString email_info;
      email_info.append("# E-Mail Information\n\n");
      email_info.append(QString("- %1: %2\n")
                            .arg(QApplication::translate("EMailModule", "From"))
                            .arg(meta_data.from));
      email_info.append(QString("- %1: %2\n")
                            .arg(QApplication::translate("EMailModule", "To"))
                            .arg(meta_data.to.join("; ")));
      email_info.append(
          QString("- %1: %2\n")
              .arg(QApplication::translate("EMailModule", "Subject"))
              .arg(meta_data.subject));
      email_info.append(QString("- %1: %2\n")
                            .arg(QApplication::translate("EMailModule", "CC"))
                            .arg(meta_data.cc.join("; ")));
      email_info.append(QString("- %1: %2\n")
                            .arg(QApplication::translate("EMailModule", "BCC"))
                            .arg(meta_data.bcc.join("; ")));
      email_info.append(QString("- %1: %2\n")
                            .arg(QApplication::translate("EMailModule", "Date"))
                            .arg(QLocale().toString(meta_data.datetime)));

      email_info.append("\n");

      email_info.append("# OpenPGP Information\n\n");

      email_info.append(QString("- %1: %2\n")
                            .arg(QApplication::translate(
                                "EMailModule", "Signed EML Data Hash (SHA1)"))
                            .arg(meta_data.mime_hash));
      email_info.append(
          QString("- %1: %2\n")
              .arg(QApplication::translate("EMailModule",
                                           "Message Integrity Check Algorithm"))
              .arg(meta_data.micalg));

      email_info.append("\n");

      email_info.append("#" + result_detail + "\n");

      const auto result_cards_param = BuildResultCardsParam(
          QApplication::translate("EMailModule", "Verify E-Mail"),
          {BuildEMailHeaderCard(meta_data), BuildOpenPGPMetaCard(meta_data)},
          result_cards);

      // callback
      CB(event, GFGetModuleID(),
         {
             {"ret", QString::number(0)},
             {"result_status", QString::number(result_status)},
             {"result", email_info},
             {"result_cards", result_cards_param},
         });
      return 0;
    });

namespace {

auto DoDecryptEMLData(int channel, const QByteArray& data, const MEvent& event,
                      int& result_status, QString& result_detail,
                      QString& result_cards, QString& eml_data,
                      EMailMetaData& meta_data) -> int {
  gpgme_error_t err;
  QString capsule_id;
  auto ret = DecryptEMLData(channel, data, meta_data, eml_data, err, capsule_id);

  if (ret == kFAILED || ret == kEML_FAILED) {
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", data},
           {"result_status", QString::number(-1)},
           {"result", ErrorHelper(ret, eml_data)},
       });
    return ret;
  }

  const char* tmp = nullptr;
  const char* cards_tmp = nullptr;
  result_status = GFAnalyseDecryptResultByCapsule(channel, err, QDUP(capsule_id),
                                                  &tmp, &cards_tmp);
  result_detail = UnStrDup(tmp);
  result_cards = UnStrDup(cards_tmp);

  if (ret == kGPG_FAILED) {
    // decrypt failed
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", data},
           {"result_status", QString::number(result_status)},
           {"result", result_detail},
       });
    return ret;
  }

  if (ret != kSUCCESS) {
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", data},
           {"result_status", QString::number(-1)},
           {"result", ErrorHelper(ret, eml_data)},
       });
    return ret;
  }

  return 0;
}

}  // namespace

REGISTER_EVENT_HANDLER(
    EDIT_TAB_TYPE_EMAIL_OP_DECRYPT, [](const MEvent& event) -> int {
      if (event["channel"].isEmpty()) CB_ERR(event, -1, "channel is empty");
      if (event["data"].isEmpty()) CB_ERR(event, -1, "data is empty");

      auto channel = event.value("channel", "0").toInt();
      auto data = QByteArray::fromBase64(QString(event["data"]).toLatin1());

      QString eml_data;
      int result_status;
      QString result_detail;
      QString result_cards;
      EMailMetaData meta_data;
      if (DoDecryptEMLData(channel, data, event, result_status, result_detail,
                           result_cards, eml_data, meta_data) != kSUCCESS) {
        return -1;
      }

      QString email_info;
      email_info.append("# E-Mail Information\n\n");
      email_info.append(QString("- %1: %2\n")
                            .arg(QApplication::translate("EMailModule", "From"))
                            .arg(meta_data.from));
      email_info.append(QString("- %1: %2\n")
                            .arg(QApplication::translate("EMailModule", "To"))
                            .arg(meta_data.to.join("; ")));
      email_info.append(
          QString("- %1: %2\n")
              .arg(QApplication::translate("EMailModule", "Subject"))
              .arg(meta_data.subject));
      email_info.append(QString("- %1: %2\n")
                            .arg(QApplication::translate("EMailModule", "CC"))
                            .arg(meta_data.cc.join("; ")));
      email_info.append(QString("- %1: %2\n")
                            .arg(QApplication::translate("EMailModule", "BCC"))
                            .arg(meta_data.bcc.join("; ")));
      email_info.append(QString("- %1: %2\n")
                            .arg(QApplication::translate("EMailModule", "Date"))
                            .arg(QLocale().toString(meta_data.datetime)));

      email_info.append("\n");

      email_info.append("# OpenPGP Information\n\n");
      email_info.append("#" + result_detail + "\n");

      const auto result_cards_param = BuildResultCardsParam(
          QApplication::translate("EMailModule", "Decrypt E-Mail"),
          {BuildEMailHeaderCard(meta_data)}, result_cards);

      // callback
      CB(event, GFGetModuleID(),
         {
             {"ret", QString::number(0)},
             {"data", eml_data},
             {"result_status", QString::number(result_status)},
             {"result", email_info},
             {"result_cards", result_cards_param},
         });
      return kSUCCESS;
    });

namespace {

auto DoSignEMLData(int channel, const QString& sign_key,
                   vmime::shared_ptr<vmime::message>& message,
                   const QByteArray& body_data, const MEvent& event,
                   int& result_status, QString& result_detail,
                   QString& result_cards, QString& eml_data) -> int {
  EMailMetaData meta_data;
  auto ret = GetEMLMetaData(message, meta_data);

  if (ret != 0) {
    CB_ERR(event, -1, "Get MetaData From EML Data Failed");
  }

  gpg_error_t err;
  QString capsule_id;
  ret = SignEMLData(channel, sign_key, message, eml_data, err, capsule_id);

  if (ret == kFAILED || ret == kEML_FAILED) {
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", body_data},
           {"result_status", QString::number(-1)},
           {"result", ErrorHelper(ret, eml_data)},
       });
    return ret;
  }

  const char* tmp = nullptr;
  const char* cards_tmp = nullptr;
  result_status = GFAnalyseSignResultByCapsule(channel, err, QDUP(capsule_id),
                                               &tmp, &cards_tmp);
  result_detail = UnStrDup(tmp);
  result_cards = UnStrDup(cards_tmp);

  if (ret == kGPG_FAILED) {
    // decrypt failed
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", body_data},
           {"result_status", QString::number(result_status)},
           {"result", result_detail},
       });
    return ret;
  }

  if (ret != kSUCCESS) {
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", body_data},
           {"result_status", QString::number(-1)},
           {"result", ErrorHelper(ret, eml_data)},
       });
    return ret;
  }

  return kSUCCESS;
}

auto DoSignPlainText(int channel, const QString& sign_key,
                     const EMailMetaData& meta_data,
                     const QByteArray& body_data, const MEvent& event,
                     int& result_status, QString& result_detail,
                     QString& result_cards, QString& eml_data) -> int {
  gpg_error_t err;
  QString capsule_id;

  auto ret = SignPlainText(channel, sign_key, meta_data, body_data, eml_data,
                           err, capsule_id);
  if (ret == kFAILED || ret == kEML_FAILED) {
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", body_data},
           {"result_status", QString::number(-1)},
           {"result", ErrorHelper(ret, eml_data)},
       });
    return ret;
  }

  const char* tmp = nullptr;
  const char* cards_tmp = nullptr;
  result_status = GFAnalyseSignResultByCapsule(channel, err, QDUP(capsule_id),
                                               &tmp, &cards_tmp);
  result_detail = UnStrDup(tmp);
  result_cards = UnStrDup(cards_tmp);

  if (ret == kGPG_FAILED) {
    // decrypt failed
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", body_data},
           {"result_status", QString::number(result_status)},
           {"result", result_detail},
       });
    return ret;
  }

  if (ret != kSUCCESS) {
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", body_data},
           {"result_status", QString::number(-1)},
           {"result", ErrorHelper(ret, eml_data)},
       });
    return ret;
  }

  return kSUCCESS;
}

}  // namespace

REGISTER_EVENT_HANDLER(
    EDIT_TAB_TYPE_EMAIL_OP_SIGN, [](const MEvent& event) -> int {
      if (event["body_data"].isEmpty()) CB_ERR(event, -1, "body_data is empty");
      if (event["channel"].isEmpty()) CB_ERR(event, -1, "channel is empty");
      if (event["sign_key"].isEmpty()) CB_ERR(event, -1, "sign_key is empty");

      auto channel = event.value("channel", "0").toInt();
      auto sign_key = event.value("sign_key", "");

      FLOG_DEBUG("eml sign key: %1", sign_key);

      auto body_data =
          QByteArray::fromBase64(QString(event["body_data"]).toLatin1());

      auto* dialog = GUI_OBJECT(CreateEMailMetaDataDialog, {});
      auto* r_dialog =
          qobject_cast<EMailMetaDataDialog*>(static_cast<QObject*>(dialog));
      if (r_dialog == nullptr)
        CB_ERR(event, -1, "convert dialog to r_dialog failed");

      r_dialog->SetChannel(channel);
      r_dialog->SetFromKeys({sign_key});
      r_dialog->SetBodyData({body_data});

      vmime::shared_ptr<vmime::message> message;
      if (CheckIfEMLMessage(body_data, message)) {
        int result_status = 0;
        QString result_detail;
        QString result_cards;
        QString eml_data;
        if (DoSignEMLData(channel, sign_key, message, body_data, event,
                          result_status, result_detail, result_cards,
                          eml_data) != kSUCCESS) {
          return -1;
        }

        CB(event, GFGetModuleID(),
           {
               {"ret", QString::number(0)},
               {"data", eml_data},
               {"result_status", QString::number(result_status)},
               {"result", result_detail},
               {"result_cards",
                BuildResultCardsParam(
                    QApplication::translate("EMailModule", "Sign E-Mail"), {},
                    result_cards)},
           });
        return 0;
      }

      GFUIShowDialog(dialog, nullptr);
      QObject::connect(
          r_dialog, &EMailMetaDataDialog::SignalEMLMetaData, r_dialog,
          [=](const EMailMetaData& meta_data) {
            int result_status = 0;
            QString result_detail;
            QString result_cards;
            QString eml_data;
            if (DoSignPlainText(channel, sign_key, meta_data, body_data, event,
                                result_status, result_detail, result_cards,
                                eml_data) == kSUCCESS) {
              CB(event, GFGetModuleID(),
                 {
                     {"ret", QString::number(0)},
                     {"data", eml_data},
                     {"result_status", QString::number(result_status)},
                     {"result", result_detail},
                     {"result_cards",
                      BuildResultCardsParam(
                          QApplication::translate("EMailModule", "Sign E-Mail"),
                          {BuildEMailHeaderCard(meta_data)}, result_cards)},
                 });
            }
          });

      QObject::connect(r_dialog, &EMailMetaDataDialog::SignalNoEMLMetaData,
                       r_dialog, [=](const QString& error_string) {
                         CB(event, GFGetModuleID(),
                            {
                                {"ret", QString::number(0)},
                                {"data", body_data},
                                {"result_status", QString::number(-1)},
                                {"result", ErrorHelper(-1, error_string)},
                            });
                       });

      return 0;
    });

namespace {

auto DoEncryptEMLData(int channel, const QStringList& encrypt_keys,
                      const vmime::shared_ptr<vmime::message>& message,
                      const QByteArray& body_data, const MEvent& event,
                      int& result_status, QString& result_detail,
                      QString& result_cards, QString& eml_data) -> int {
  gpgme_error_t err;
  QString capsule_id;
  auto ret = EncryptEMLData(channel, encrypt_keys, message, body_data, eml_data,
                            err, capsule_id);

  if (ret == kFAILED || ret == kEML_FAILED) {
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", QString::fromLatin1(body_data.toBase64())},
           {"result_status", QString::number(-1)},
           {"result", ErrorHelper(ret, eml_data)},
       });
    return ret;
  }

  const char* tmp = nullptr;
  const char* cards_tmp = nullptr;
  result_status = GFAnalyseEncryptResultByCapsule(channel, err, QDUP(capsule_id),
                                                  &tmp, &cards_tmp);
  result_detail = UnStrDup(tmp);
  result_cards = UnStrDup(cards_tmp);

  if (ret == kGPG_FAILED) {
    // encrypt failed
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", QString::fromLatin1(body_data.toBase64())},
           {"result_status", QString::number(result_status)},
           {"result", result_detail},
       });
    return ret;
  }

  return kSUCCESS;
}

auto DoEncryptPlainText(int channel, const QStringList& encrypt_keys,
                        const EMailMetaData& meta_data,
                        const QByteArray& body_data, const MEvent& event,
                        int& result_status, QString& result_detail,
                        QString& result_cards, QString& eml_data) -> int {
  gpgme_error_t err;
  QString capsule_id;
  QString plain_text_eml_data;
  auto ret = BuildPlainTextEML(meta_data, body_data, plain_text_eml_data);

  if (ret != kSUCCESS) {
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", QString::fromLatin1(body_data.toBase64())},
           {"result_status", QString::number(-1)},
           {"result", ErrorHelper(ret, eml_data)},
       });
    return ret;
  }

  ret = EncryptPlainText(channel, encrypt_keys, meta_data,
                         plain_text_eml_data.toLatin1(), eml_data, err,
                         capsule_id);

  if (ret == kFAILED || ret == kEML_FAILED) {
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", QString::fromLatin1(body_data.toBase64())},
           {"result_status", QString::number(-1)},
           {"result", ErrorHelper(ret, eml_data)},
       });
    return ret;
  }

  const char* tmp = nullptr;
  const char* cards_tmp = nullptr;
  result_status = GFAnalyseEncryptResultByCapsule(channel, err, QDUP(capsule_id),
                                                  &tmp, &cards_tmp);
  result_detail = UnStrDup(tmp);
  result_cards = UnStrDup(cards_tmp);

  if (ret == kGPG_FAILED) {
    // encrypt failed
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", QString::fromLatin1(body_data.toBase64())},
           {"result_status", QString::number(result_status)},
           {"result", result_detail},
       });
    return ret;
  }

  return kSUCCESS;
}
};  // namespace

REGISTER_EVENT_HANDLER(
    EDIT_TAB_TYPE_EMAIL_OP_ENCRYPT, [](const MEvent& event) -> int {
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
        int result_status;
        QString result_detail;
        QString result_cards;
        if (DoEncryptEMLData(channel, encrypt_keys, message, body_data, event,
                             result_status, result_detail, result_cards,
                             eml_data) != kSUCCESS) {
          return -1;
        }

        CB(event, GFGetModuleID(),
           {
               {"ret", QString::number(0)},
               {"data", eml_data},
               {"result", result_detail},
               {"result_status", QString::number(result_status)},
               {"result_cards",
                BuildResultCardsParam(
                    QApplication::translate("EMailModule", "Encrypt E-Mail"),
                    BuildRecipientCards(channel, encrypt_keys), result_cards)},
           });
        return 0;
      }

      auto* dialog = GUI_OBJECT(CreateEMailMetaDataDialog, {});
      auto* r_dialog =
          qobject_cast<EMailMetaDataDialog*>(static_cast<QObject*>(dialog));
      if (r_dialog == nullptr)
        CB_ERR(event, -1, "convert dialog to r_dialog failed");

      r_dialog->SetChannel(channel);
      r_dialog->SetToKeys(encrypt_keys);
      r_dialog->SetBodyData({body_data});

      GFUIShowDialog(dialog, nullptr);

      QObject::connect(
          r_dialog, &EMailMetaDataDialog::SignalEMLMetaData, r_dialog,
          [=](const EMailMetaData& meta_data) -> void {
            QString eml_data;
            int result_status;
            QString result_detail;
            QString result_cards;
            if (DoEncryptPlainText(channel, encrypt_keys, meta_data, body_data,
                                   event, result_status, result_detail,
                                   result_cards, eml_data) == kSUCCESS) {
              auto meta_cards = BuildRecipientCards(channel, encrypt_keys);
              meta_cards.prepend(BuildEMailHeaderCard(meta_data));
              CB(event, GFGetModuleID(),
                 {
                     {"ret", QString::number(0)},
                     {"data", eml_data},
                     {"result", result_detail},
                     {"result_status", QString::number(result_status)},
                     {"result_cards", BuildResultCardsParam(
                                          QApplication::translate(
                                              "EMailModule", "Encrypt E-Mail"),
                                          meta_cards, result_cards)},
                 });
            }
          });

      QObject::connect(
          r_dialog, &EMailMetaDataDialog::SignalNoEMLMetaData, r_dialog,
          [=](const QString& error_string) {
            CB(event, GFGetModuleID(),
               {
                   {"ret", QString::number(0)},
                   {"data", QString::fromLatin1(body_data.toBase64())},
                   {"result_status", QString::number(-1)},
                   {"result", ErrorHelper(-1, error_string)},
               });
          });

      return 0;
    });

namespace {

auto DoEncryptSignEMLData(int channel, const QStringList& encrypt_keys,
                          const QString& sign_key,
                          vmime::shared_ptr<vmime::message>& message,
                          QByteArray& body_data, const MEvent& event,
                          int result_status, QString& result_detail,
                          QString& result_cards, QString& eml_data) -> int {
  QString sign_cards;
  if (DoSignEMLData(channel, sign_key, message, body_data, event, result_status,
                    result_detail, sign_cards, eml_data) != kSUCCESS) {
    return -1;
  }

  body_data = eml_data.toLatin1();
  eml_data.clear();

  int t_result_status;
  QString t_result_detail;
  QString encrypt_cards;

  vmime::shared_ptr<vmime::message> signed_message;
  bool r = CheckIfEMLMessage(body_data, signed_message);
  if (!r) {
    CB_ERR(event, -1, "Parse Signed Message Failed");
  }

  auto ret = DoEncryptEMLData(channel, encrypt_keys, signed_message, body_data,
                              event, t_result_status, t_result_detail,
                              encrypt_cards, eml_data);

  result_status = std::min(t_result_status, result_status);
  result_detail = t_result_detail + "\n\n" + result_detail;
  result_cards = MergeCardArrays(encrypt_cards, sign_cards);
  return ret;
}

auto DoEncryptSignPlainText(int channel, const QStringList& encrypt_keys,
                            const QString& sign_key,
                            const EMailMetaData& meta_data,
                            QByteArray& body_data, const MEvent& event,
                            int& result_status, QString& result_detail,
                            QString& result_cards, QString& eml_data) -> int {
  QString sign_cards;
  if (DoSignPlainText(channel, sign_key, meta_data, body_data, event,
                      result_status, result_detail, sign_cards,
                      eml_data) != kSUCCESS) {
    return -1;
  }

  body_data = eml_data.toLatin1();
  eml_data.clear();

  int t_result_status;
  QString t_result_detail;
  QString encrypt_cards;

  vmime::shared_ptr<vmime::message> signed_message;
  bool r = CheckIfEMLMessage(body_data, signed_message);
  if (!r) {
    CB_ERR(event, -1, "Parse Signed Message Failed");
  }

  auto ret = DoEncryptEMLData(channel, encrypt_keys, signed_message, body_data,
                              event, t_result_status, t_result_detail,
                              encrypt_cards, eml_data);

  result_status = std::min(t_result_status, result_status);
  result_detail = t_result_detail + "\n" + result_detail;
  result_cards = MergeCardArrays(encrypt_cards, sign_cards);
  return ret;
}

}  // namespace

REGISTER_EVENT_HANDLER(
    EDIT_TAB_TYPE_EMAIL_OP_ENCRYPT_SIGN, [](const MEvent& event) -> int {
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
        int result_status = 0;
        QString result_detail;
        QString result_cards;
        if (DoEncryptSignEMLData(channel, encrypt_keys, sign_key, message,
                                 body_data, event, result_status, result_detail,
                                 result_cards, eml_data) != kSUCCESS) {
          return -1;
        }
        CB(event, GFGetModuleID(),
           {
               {"ret", QString::number(0)},
               {"data", eml_data},
               {"result", result_detail},
               {"result_status", QString::number(result_status)},
               {"result_cards",
                BuildResultCardsParam(
                    QApplication::translate("EMailModule",
                                            "Encrypt and Sign E-Mail"),
                    BuildRecipientCards(channel, encrypt_keys), result_cards)},
           });
        return 0;
      }

      auto* dialog = GUI_OBJECT(CreateEMailMetaDataDialog, {});
      auto* r_dialog =
          qobject_cast<EMailMetaDataDialog*>(static_cast<QObject*>(dialog));
      if (r_dialog == nullptr)
        CB_ERR(event, -1, "convert dialog to r_dialog failed");

      r_dialog->SetChannel(channel);
      r_dialog->SetToKeys(encrypt_keys);
      r_dialog->SetFromKeys({sign_key});
      r_dialog->SetBodyData({body_data});

      GFUIShowDialog(dialog, nullptr);

      QObject::connect(
          r_dialog, &EMailMetaDataDialog::SignalEMLMetaData, r_dialog,
          [=](const EMailMetaData& meta_data) -> int {
            QString eml_data;
            int result_status = 0;
            QString result_detail;
            QString result_cards;
            QByteArray body_data_copy = body_data;

            FLOG_DEBUG("meta data, from: %1", meta_data.from);

            if (DoEncryptSignPlainText(channel, encrypt_keys, sign_key,
                                       meta_data, body_data_copy, event,
                                       result_status, result_detail,
                                       result_cards, eml_data) != kSUCCESS) {
              return -1;
            }
            auto meta_cards = BuildRecipientCards(channel, encrypt_keys);
            meta_cards.prepend(BuildEMailHeaderCard(meta_data));
            CB(event, GFGetModuleID(),
               {
                   {"ret", QString::number(0)},
                   {"data", eml_data},
                   {"result", result_detail},
                   {"result_status", QString::number(result_status)},
                   {"result_cards",
                    BuildResultCardsParam(
                        QApplication::translate("EMailModule",
                                                "Encrypt and Sign E-Mail"),
                        meta_cards, result_cards)},
               });
            return 0;
          });

      QObject::connect(
          r_dialog, &EMailMetaDataDialog::SignalNoEMLMetaData, r_dialog,
          [=](const QString& error_string) {
            CB(event, GFGetModuleID(),
               {
                   {"ret", QString::number(0)},
                   {"data", QString::fromLatin1(body_data.toBase64())},
                   {"result_status", QString::number(-1)},
                   {"result", ErrorHelper(-1, error_string)},
               });
          });

      return 0;
    });

namespace {

auto DoDecryptVerifyEMLData(int channel, const QByteArray& data,
                            const MEvent& event, int& result_status,
                            QString& result_detail, QString& result_cards,
                            QString& eml_data, QString& error_string,
                            EMailMetaData& meta_data) -> int {
  QString decrypt_cards;
  if (DoDecryptEMLData(channel, data, event, result_status, result_detail,
                       decrypt_cards, eml_data, meta_data) != kSUCCESS) {
    return -1;
  }

  int t_result_status;
  QString t_result_detail;
  QString verify_cards;

  if (DoVerifyEMLData(channel, eml_data.toLatin1(), event, t_result_status,
                      t_result_detail, verify_cards, error_string,
                      meta_data) != kSUCCESS) {
    return -1;
  }

  result_status = std::min(t_result_status, result_status);
  result_detail = t_result_detail + "\n" + result_detail;
  result_cards = MergeCardArrays(verify_cards, decrypt_cards);

  return kSUCCESS;
}
}  // namespace

REGISTER_EVENT_HANDLER(
    EDIT_TAB_TYPE_EMAIL_OP_DECRYPT_VERIFY, [](const MEvent& event) -> int {
      if (event["channel"].isEmpty()) CB_ERR(event, -1, "channel is empty");
      if (event["data"].isEmpty()) CB_ERR(event, -1, "data is empty");

      auto channel = event.value("channel", "0").toInt();
      auto data = QByteArray::fromBase64(QString(event["data"]).toLatin1());

      auto body_data =
          QByteArray::fromBase64(QString(event["body_data"]).toLatin1());

      QString eml_data;
      EMailMetaData meta_data;
      QString error_string;
      int result_status;
      QString result_detail;
      QString result_cards;

      if (DoDecryptVerifyEMLData(channel, data, event, result_status,
                                 result_detail, result_cards, eml_data,
                                 error_string, meta_data) != kSUCCESS) {
        return -1;
      }

      QString email_info;
      email_info.append("# E-Mail Information\n\n");
      email_info.append(QString("- %1: %2\n")
                            .arg(QApplication::translate("EMailModule", "From"))
                            .arg(meta_data.from));
      email_info.append(QString("- %1: %2\n")
                            .arg(QApplication::translate("EMailModule", "To"))
                            .arg(meta_data.to.join("; ")));
      email_info.append(
          QString("- %1: %2\n")
              .arg(QApplication::translate("EMailModule", "Subject"))
              .arg(meta_data.subject));
      email_info.append(QString("- %1: %2\n")
                            .arg(QApplication::translate("EMailModule", "CC"))
                            .arg(meta_data.cc.join("; ")));
      email_info.append(QString("- %1: %2\n")
                            .arg(QApplication::translate("EMailModule", "BCC"))
                            .arg(meta_data.bcc.join("; ")));
      email_info.append(QString("- %1: %2\n")
                            .arg(QApplication::translate("EMailModule", "Date"))
                            .arg(QLocale().toString(meta_data.datetime)));

      email_info.append("\n");

      email_info.append("# OpenPGP Information\n\n");

      email_info.append(QString("- %1: %2\n")
                            .arg(QApplication::translate(
                                "EMailModule", "Signed EML Data Hash (SHA1)"))
                            .arg(meta_data.mime_hash));
      email_info.append(
          QString("- %1: %2\n")
              .arg(QApplication::translate("EMailModule",
                                           "Message Integrity Check Algorithm"))
              .arg(meta_data.micalg));

      email_info.append("\n");

      email_info.append("#" + result_detail + "\n");

      const auto result_cards_param = BuildResultCardsParam(
          QApplication::translate("EMailModule", "Decrypt and Verify E-Mail"),
          {BuildEMailHeaderCard(meta_data), BuildOpenPGPMetaCard(meta_data)},
          result_cards);

      // callback
      CB(event, GFGetModuleID(),
         {
             {"ret", QString::number(0)},
             {"data", eml_data},
             {"result_status", QString::number(result_status)},
             {"result", email_info},
             {"result_cards", result_cards_param},
         });
      return 0;
    });

REGISTER_EVENT_HANDLER(
    EDIT_TAB_TYPE_EMAIL_OP_SAVE_FILE, [](const MEvent& event) -> int {
      if (event["page"].isEmpty()) CB_ERR(event, -1, "page is empty");

      auto* page = GFUIGetGUIObjectAs<QWidget>(event["page"]);
      if (!page) {
        LOG_ERROR("page handler is not a QWidget");
        CB_ERR(event, -1, "page handle invalid or not QMainWindow");
      }

      auto* tab_widget = GFUIGetGUIObjectAs<QTabWidget>(event["tab_widget"]);
      if (!tab_widget) {
        LOG_ERROR("tab widget handler is not a QTabWidget");
        CB_ERR(event, -1, "main_window handle invalid or not QMainWindow");
      }

      QString filename;

      auto ok = QMetaObject::invokeMethod(page, "GetFilePath",
                                          Qt::BlockingQueuedConnection,
                                          Q_RETURN_ARG(QString, filename));

      if (!ok) {
        LOG_ERROR("invoke GetFilePath failed");
        CB_ERR(event, -1, "invoke GetFilePath failed");
      }

      if (filename.isEmpty()) {
        auto ok = QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [&]() -> void {
              filename = QFileDialog::getSaveFileName(
                  page, QApplication::translate("EMailModule", "Save file"),
                  QDir::currentPath());
            },
            Qt::BlockingQueuedConnection);

        if (!ok) {
          LOG_ERROR("invoke getSaveFileName failed");
          CB_ERR(event, -1, "invoke getSaveFileName failed");
        }
      }

      if (filename.isEmpty()) {
        LOG_INFO("user cancelled to select file to save");
        CB_SUCC(event);
      }

      QFileInfo file_info(filename);
      if (file_info.suffix().toLower() != "eml") {
        file_info.setFile(file_info.path(),
                          file_info.completeBaseName() + ".eml");
        filename = file_info.absoluteFilePath();
        FLOG_DEBUG("append .eml suffix to filename: %1", filename);
      }

      QFile file(filename);
      if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(
            page, QApplication::translate("EMailModule", "Warning"),
            QApplication::translate("EMailModule", "Cannot read file%1:\n%2.")
                .arg(filename)
                .arg(file.errorString()));
        return false;
      }

      QPlainTextEdit* text_edit = nullptr;
      ok = QMetaObject::invokeMethod(page, "GetTextPage",
                                     Qt::BlockingQueuedConnection,
                                     Q_RETURN_ARG(QPlainTextEdit*, text_edit));
      if (!ok || text_edit == nullptr) {
        LOG_ERROR("invoke GetTextPage failed");
        CB_ERR(event, -1, "invoke GetTextPage failed");
      }

      QTextStream output_stream(&file);
      QApplication::setOverrideCursor(Qt::WaitCursor);
      output_stream << text_edit->toPlainText().replace("\n", "\r\n").toUtf8();
      QApplication::restoreOverrideCursor();
      QTextDocument* document = text_edit->document();

      document->setModified(false);

      int cur_index = tab_widget->currentIndex();
      tab_widget->setTabText(cur_index, QFileInfo(filename).fileName());

      QMetaObject::invokeMethod(page, "SetFilePath",
                                Qt::BlockingQueuedConnection,
                                Q_ARG(QString, filename));
      QMetaObject::invokeMethod(page, "NotifyFileSaved",
                                Qt::BlockingQueuedConnection);
      return 0;
    });

REGISTER_EVENT_HANDLER(
    FILE_EXT_EMAIL_OP_OPEN_FILE, [](const MEvent& event) -> int {
      if (event["file_path"].isEmpty()) CB_ERR(event, -1, "file_path is empty");

      auto file_path = event.value("file_path", "");
      QFileInfo file_info(file_path);

      // stop here if file is too large (> 1mb)
      if (file_info.size() > static_cast<qint64>(1 * 1024 * 1024)) {
        QMessageBox::warning(
            nullptr, QApplication::translate("EMailModule", "Warning"),
            QApplication::translate(
                "EMailModule",
                "The file %1 is too large (%2 bytes) to be opened. The "
                "maximum allowed size is 1 MB.")
                .arg(file_path)
                .arg(file_info.size()));
        return -1;
      }

      auto* edit = GFUIGetGUIObjectAs<QWidget>("main_window_edit");
      if (!edit) {
        LOG_ERROR(
            "main window menu mounted: main_window_edit "
            "handle invalid or not "
            "QWidget");
        CB_ERR(event, -1,
               "main_window_edit handle invalid or not "
               "QWidget");
      }

      // Run in GUI thread to avoid blocking the main thread
      QMetaObject::invokeMethod(QCoreApplication::instance(), [=]() -> void {
        QFileInfo file_info(file_path);
        QFile file(file_path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
          QMessageBox::warning(
              nullptr, QApplication::translate("EMailModule", "Warning"),
              QApplication::translate("EMailModule",
                                      "Cannot read file %1:\n%2.")
                  .arg(file_path)
                  .arg(file.errorString()));
          return;
        }

        QWidget* page = nullptr;

        auto ok = QMetaObject::invokeMethod(
            edit, "SlotNewCustomTab", Qt::DirectConnection,
            Q_RETURN_ARG(QWidget*, page), Q_ARG(QString, "email"),
            Q_ARG(QString, file_info.fileName()),
            Q_ARG(QIcon, QIcon(":/icons/email.png")),
            Q_ARG(QString, ":/icons/email.png"));

        if (!ok || !page) {
          LOG_ERROR("create new email tab page failed");
          CB_ERR_NO_RET(event, -1, "create new email tab page failed");
          return;
        }

        QPlainTextEdit* text_edit = nullptr;
        ok =
            QMetaObject::invokeMethod(page, "GetTextPage", Qt::DirectConnection,
                                      Q_RETURN_ARG(QPlainTextEdit*, text_edit));
        if (!ok || text_edit == nullptr) {
          LOG_ERROR("invoke GetTextPage failed");
          CB_ERR_NO_RET(event, -1, "invoke GetTextPage failed");
          return;
        }

        text_edit->setPlainText(
            QString::fromUtf8(file.readAll()).replace("\r\n", "\n"));
        text_edit->document()->setModified(false);

        QMetaObject::invokeMethod(page, "SetFilePath", Qt::DirectConnection,
                                  Q_ARG(QString, file_path));
      });
      return 0;
    })