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

#include "EMailAccountSettingsPage.h"
#include "EMailAccountStore.h"
#include "EMailImapController.h"
#include "EMailSecret.h"
#include "EMailSendDialog.h"

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
#include <QSaveFile>
#include <QStandardPaths>
#include <QString>
#include <QTextDocument>

#include "EMailPageView.h"

// vmime
// The test target defines this on the command line; the module build does
// not, so it is set here and guarded rather than assumed either way.
#ifndef VMIME_STATIC
#define VMIME_STATIC
#endif
#include <algorithm>
#include <vmime/vmime.hpp>

// vmime extend
#include <vmime/contentTypeField.hpp>

#include "GFModuleCommonUtils.hpp"
#include "GFModuleDefine.h"

//
#include "EMailBasicGpgOpera.h"
#include "EMailHelper.h"

GF_MODULE_API_DEFINE_V2("com.bktus.gpgfrontend.module.email", "Email", "2.0.0",
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
       {QApplication::translate("EMailModule", "BCC"), m.bcc_header.join("; ")},
       {QApplication::translate("EMailModule", "Date"),
        m.datetime.isValid() ? QLocale().toString(m.datetime) : QString()}});
}

// Structured OpenPGP/MIME metadata card from parsed metadata.
auto BuildOpenPGPMetaCard(const EMailMetaData& m) -> QJsonObject {
  return MakeCardJson(
      QApplication::translate("EMailModule", "OpenPGP"), "neutral",
      // Two different things, kept apart on purpose: the first is a digest we
      // computed over the bytes the signature covers, the second is only what
      // the message declares about the signature's hash algorithm.
      {{QApplication::translate("EMailModule",
                                "Digest of Signed MIME Entity (SHA-256)"),
        m.signed_entity_digest},
       {QApplication::translate("EMailModule",
                                "Declared Signature Hash (micalg)"),
        m.micalg}});
}

// Human-readable byte size for an attachment row.
auto FormatSize(qint64 bytes) -> QString {
  return QLocale().formattedDataSize(bytes);
}

// One card listing what the message carries besides its body.
//
// Attachments used to be enumerated and then dropped on the floor, so a signed
// or encrypted message with a document in it looked, to the user, exactly like
// one without. The unsigned marker matters: in PGP/MIME only the
// multipart/signed subtree is authenticated, so a part outside it arrived
// unverified however good the signature on the rest is.
auto BuildAttachmentCard(const EMailMetaData& m) -> QJsonObject {
  QList<QPair<QString, QString>> fields;

  for (const auto& att : m.attachments) {
    const auto name = SanitizeAttachmentFileName(att.filename, att.mime_type);

    auto description =
        QString("%1, %2").arg(att.mime_type, FormatSize(att.data.size()));
    if (att.is_openpgp_key) {
      description += QApplication::translate("EMailModule", ", an OpenPGP key");
    }
    if (!att.inside_signed_part) {
      description += QApplication::translate("EMailModule",
                                             ", not covered by the signature");
    }

    fields.append({name, description});
  }

  return MakeCardJson(QApplication::translate("EMailModule", "Attachments"),
                      "neutral", fields);
}

// Build "Encryption Recipient" cards from the selected recipient keys. The
// encrypt result itself does not carry recipient identities for every engine
// (GnuPG reports none), but the module always knows which keys it encrypted to,
// so resolve each to its primary UID here — mirroring the native operation's
// recipient card.
// The address a key stands for, as "Name <email>".
auto AddressOfKey(int channel, const QString& key_id) -> QString {
  if (key_id.isEmpty()) return {};

  GFGpgKeyUID* uid = nullptr;
  if (GFGpgKeyPrimaryUID(channel, QDUP(key_id), &uid) != 0 || uid == nullptr) {
    return {};
  }

  const auto name = UDUP(uid->name);
  const auto email = UDUP(uid->email);
  UDUP(uid->comment);  // free the unused field
  GFFreeMemory(uid);

  return email.isEmpty() ? name : QString("%1 <%2>").arg(name, email);
}

// Envelope for content that is not a message yet -- text typed straight into
// the raw source view and handed to an operation without going through the
// message view first.
//
// This used to be a modal dialog asking for From, To and Subject. Everything it
// really needed is already known: encryption is to a set of keys and signing is
// with one, and each of those keys carries the address it belongs to. So the
// envelope is derived rather than demanded, and the user is not interrupted to
// retype what they already chose in the key list.
auto EnvelopeFromKeys(int channel, const QString& sign_key,
                      const QStringList& encrypt_keys) -> EMailMetaData {
  EMailMetaData meta_data;

  meta_data.from = AddressOfKey(channel, sign_key);
  for (const auto& key : encrypt_keys) {
    const auto address = AddressOfKey(channel, key);
    if (!address.isEmpty()) meta_data.to.append(address);
  }

  // A signed-only message has no recipient key to derive an address from, so
  // it stays unaddressed -- which BuildMimeEML now handles rather than
  // refusing.
  if (meta_data.from.isEmpty() && !meta_data.to.isEmpty()) {
    meta_data.from = meta_data.to.front();
  }

  meta_data.datetime = QDateTime::currentDateTime();
  return meta_data;
}

auto BuildRecipientCards(int channel, const QStringList& encrypt_keys)
    -> QJsonArray {
  QJsonArray cards;
  for (const auto& key_id : encrypt_keys) {
    if (key_id.isEmpty()) continue;

    const auto recipient = AddressOfKey(channel, key_id);

    cards.append(MakeCardJson(
        QApplication::translate("EMailModule", "Encryption Recipient"), "ok",
        {{QApplication::translate("EMailModule", "Recipient"), recipient},
         {QApplication::translate("EMailModule", "Key ID"), key_id}}));
  }
  return cards;
}

// The metadata cards for a message that was read rather than composed.
auto BuildReadMetaCards(const EMailMetaData& m, bool with_openpgp)
    -> QJsonArray {
  QJsonArray cards{BuildEMailHeaderCard(m)};
  if (with_openpgp) cards.append(BuildOpenPGPMetaCard(m));
  // The board renders a limited number of cards, so only spend one on
  // attachments when there is something to list.
  if (!m.attachments.isEmpty()) cards.append(BuildAttachmentCard(m));
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

// The recipient cross-check, as a card.
//
// Asymmetric on purpose. Someone in To or Cc who was not encrypted to has a
// real problem: they were told the message is for them and cannot open it.
// The reverse -- a key that is not in the headers -- is ordinary (a blind
// copy, an archive key, the sender's own key, or a deliberately hidden
// recipient) and is reported without alarm. Treating it as suspicious would
// raise a warning on most correct messages.
auto BuildRecipientCheckCard(const EMailMetaData& m,
                             const QByteArray& decrypt_info_json)
    -> QJsonObject {
  const auto encrypted = ParseRecipientInfos(decrypt_info_json);
  if (encrypted.isEmpty()) return {};

  const auto rows = MatchRecipients(m.to, m.cc, m.bcc_header, encrypted);

  QList<QPair<QString, QString>> fields;
  bool any_warning = false;

  for (const auto& row : rows) {
    switch (row.match) {
      case RecipientMatch::kMATCHED:
        fields.append({row.address,
                       QApplication::translate("EMailModule", "encrypted to")});
        break;
      case RecipientMatch::kADDRESSED_NOT_ENCRYPTED:
        any_warning = true;
        fields.append(
            {row.address,
             QApplication::translate(
                 "EMailModule", "in %1, but not encrypted to it: cannot read")
                 .arg(row.header_field)});
        break;
      case RecipientMatch::kENCRYPTED_NOT_ADDRESSED:
        fields.append({row.info.uid.isEmpty() ? row.info.key_id : row.info.uid,
                       QApplication::translate(
                           "EMailModule", "encrypted to, not in the headers")});
        break;
      case RecipientMatch::kHIDDEN_RECIPIENT:
        fields.append(
            {QApplication::translate("EMailModule", "Hidden recipient"),
             QApplication::translate("EMailModule",
                                     "the sender withheld this key id")});
        break;
    }
  }

  if (fields.isEmpty()) return {};

  return MakeCardJson(QApplication::translate("EMailModule", "Recipient Check"),
                      any_warning ? "warn" : "ok", fields);
}

// Assemble the `result_cards` payload the UI decodes: the module's own metadata
// cards followed by the crypto-analysis cards produced by the SDK (which reuses
// the same converter the native operations use). `crypto_cards_json` is the
// JSON array string returned via the GFAnalyse*Result `cards` out-param.
auto BuildResultCardsParam(const QString& operation,
                           const QJsonArray& meta_cards,
                           const QString& crypto_cards_json,
                           const QByteArray& info_json = {}) -> QString {
  QJsonArray cards = meta_cards;
  if (!crypto_cards_json.isEmpty()) {
    const auto doc = QJsonDocument::fromJson(crypto_cards_json.toUtf8());
    if (doc.isArray()) {
      for (const auto& v : doc.array()) cards.append(v);
    }
  }

  QJsonObject obj{{"operation", operation}};

  // The structured analysis, if the caller had one. Without it the Info Board
  // has no description to show and falls back to "<operation> failed. See the
  // details for more information." -- which is how a failed e-mail decrypt came
  // to say less than the same failure on plain text, where the native path
  // hands over exactly these fields.
  //
  // Read from the JSON rather than from the rendered report: the Info Board
  // never parses report text, and neither does this.
  if (!info_json.isEmpty()) {
    const auto info = QJsonDocument::fromJson(info_json).object();

    // "Decrypt E-Mail · GnuPG v2.4.7" -- the engine belongs in the heading,
    // because which engine produced a verdict is part of the verdict.
    const auto engine = info.value("engine").toString();
    if (!engine.isEmpty()) {
      obj["operation"] = QString("%1 · %2").arg(operation, engine);
    }

    const auto description = info.value("description").toString();
    if (!description.isEmpty()) obj.insert("description", description);

    const auto details = info.value("details").toArray();
    if (!details.isEmpty()) {
      // The same titles the native path uses, chosen the same way, so one
      // failure does not get two different names depending on which door the
      // user came in through.
      QString title = QApplication::translate("EMailModule", "DETAILS");
      if (operation.contains(QApplication::translate("EMailModule", "Decrypt"),
                             Qt::CaseInsensitive)) {
        title = QApplication::translate("EMailModule", "RECIPIENT");
      } else if (operation.contains(
                     QApplication::translate("EMailModule", "Sign"),
                     Qt::CaseInsensitive) ||
                 operation.contains(
                     QApplication::translate("EMailModule", "Verify"),
                     Qt::CaseInsensitive)) {
        title = QApplication::translate("EMailModule", "SIGNER");
      }

      obj.insert("details_title", title);
      obj.insert("details_items", details);
    }
  }

  // A payload with neither cards nor a description has nothing the board could
  // render, so the caller falls back to the plain-text path.
  if (cards.isEmpty() && !obj.contains("description")) return {};

  obj.insert("cards", cards);
  return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

}  // namespace

/// Identifier of the settings page this module owns.
constexpr auto kMailSettingsPageId =
    "com.bktus.gpgfrontend.module.email.accounts";

auto GFRegisterModule() -> int {
  MLogDebug("email module registering...");

  // The MIME code carries no SDK symbol of its own so it can be unit-tested
  // without a module host; this is what gives it a logger at runtime.
  SetMimeLogSink([](const QString& m) { MLogDebug(m); });

  REGISTER_TRANS_READER();

  LISTEN("MAINWINDOW_MENU_MOUNTED");

  LISTEN("EDIT_TAB_TYPE_EMAIL_OP_ENCRYPT");
  LISTEN("EDIT_TAB_TYPE_EMAIL_OP_DECRYPT");
  LISTEN("EDIT_TAB_TYPE_EMAIL_OP_SIGN");
  LISTEN("EDIT_TAB_TYPE_EMAIL_OP_VERIFY");
  LISTEN("EDIT_TAB_TYPE_EMAIL_OP_ENCRYPT_SIGN");
  LISTEN("EDIT_TAB_TYPE_EMAIL_OP_DECRYPT_VERIFY");

  LISTEN("EDIT_TAB_TYPE_EMAIL_OP_SAVE_FILE");

  // The message view of an e-mail tab. The host still owns the page and its
  // document -- this only supplies the widget shown on top of it, with the raw
  // MIME still one click away.
  GFUIRegisterTabPageView(
      DUP("EMAIL"), [](void*) -> void* { return new EMailPageView(nullptr); },
      nullptr);

  // register file extension handler
  GFUIRegisterFileExtensionHandleEvent(DUP("eml"), DUP("EMAIL"));

  LISTEN("FILE_EXT_EMAIL_OP_OPEN_FILE");

  // These cross thread boundaries as queued signal arguments, so Qt has to
  // know how to copy them before the first connection is made.
  qRegisterMetaType<MailAccountConfig>("MailAccountConfig");
  // Shared rather than copied: a queued connection copies its arguments, and
  // copying the pointer is not copying the password. See EMailSecret.
  qRegisterMetaType<EMailSecretPtr>("EMailSecretPtr");
  qRegisterMetaType<MailError>("MailError");
  qRegisterMetaType<EMailFolderInfo>("EMailFolderInfo");
  qRegisterMetaType<QList<EMailFolderInfo>>("QList<EMailFolderInfo>");
  qRegisterMetaType<EMailMessageSummary>("EMailMessageSummary");
  qRegisterMetaType<EMailMessagePage>("EMailMessagePage");
  qRegisterMetaType<EMailOutgoingMessage>("EMailOutgoingMessage");
  qRegisterMetaType<EMailSendReceipt>("EMailSendReceipt");
  // Crosses a thread boundary as a queued signal argument, so it has to be
  // known to the metatype system by name before the first one is emitted.
  qRegisterMetaType<MailSentSaveOutcome>("MailSentSaveOutcome");

  // vmime's platform handler is assigned lazily with no synchronisation, so
  // two worker threads racing their first connection could construct it twice.
  // Touching it once here, on the thread that loads the module, settles it
  // before any worker exists.
  vmime::platform::getHandler();

  const auto keywords =
      QStringList{GC_TR("mail"), GC_TR("email"),   GC_TR("imap"),
                  GC_TR("smtp"), GC_TR("account"), GC_TR("send")}
          .join('\n');
  GFUIRegisterSettingsPage(DUP(kMailSettingsPageId), DUP("features"),
                           DUP(GC_TR("Mail Accounts")), QDUP(keywords),
                           EMailAccountSettingsPageFactory, nullptr);

  // An imported key changes what an already-open message can be told about
  // itself, so the views have to hear about it.
  LISTEN("KEY_DATABASE_REFRESH_DONE");
  return 0;
}

auto GFActiveModule() -> int { return 0; }

auto GFDeactivateModule() -> int {
  // A factory pointing into an unloaded shared object would crash the next
  // time an e-mail tab is opened.
  GFUIUnregisterTabPageView(DUP("EMAIL"));
  GFUIUnregisterSettingsPage(DUP(kMailSettingsPageId));
  return 0;
}

auto GFUnregisterModule() -> int {
  MLogDebug("email module unregistering...");

  return 0;
}

namespace {

// The preflight dialog: what the user should know before this message leaves.
//
// Shown only when there is something to say, and it never refuses the save --
// the decision is the user's, and a check that blocks gets worked around
// rather than read. Returns false when the user chooses not to write the file.
auto ConfirmExport(QWidget* parent, const QByteArray& source) -> bool {
  vmime::shared_ptr<vmime::message> parsed;
  if (!CheckIfEMLMessage(source, parsed)) return true;

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  if (ParseMimeTree(parsed, source, root, regions) != 0) return true;

  EMailMetaData meta;
  GetEMLMetaData(parsed, meta);

  const auto findings = PreflightMessage(meta, root, regions, {});

  QStringList risks;
  QStringList notes;
  for (const auto& finding : findings) {
    const auto line = QString("%1 - %2").arg(finding.title, finding.detail);
    if (finding.level == EMailFindingLevel::kRISK) {
      risks.append(line);
    } else if (finding.level == EMailFindingLevel::kWARN) {
      notes.append(line);
    }
  }

  // Notes alone are not worth interrupting a save for; the Security tab
  // already carries them.
  if (risks.isEmpty()) return true;

  QMessageBox box(parent);
  box.setIcon(QMessageBox::Warning);
  box.setWindowTitle(
      QApplication::translate("EMailModule", "Check before exporting"));
  box.setText(QApplication::translate(
      "EMailModule",
      "Something about this message is worth checking before you save it."));
  box.setInformativeText(risks.join("\n\n"));
  if (!notes.isEmpty()) box.setDetailedText(notes.join("\n\n"));
  box.setStandardButtons(QMessageBox::Save | QMessageBox::Cancel);
  box.setDefaultButton(QMessageBox::Cancel);

  return box.exec() == QMessageBox::Save;
}

// Where a Save dialog should open. Falls back to the home directory only if
// the host cannot answer, which it always can in practice.
auto default_save_dir() -> QString {
  auto path = UnStrDup(GFUIDefaultUserFilePath());
  return path.isEmpty() ? QDir::homePath() : path;
}

// Ceiling on the size of an .eml this module will open. Generous, because a
// message with attachments is legitimately large; the protection against a
// hostile *shape* is EMailParseLimits, not this number.
constexpr qint64 kMaxEMLFileSize = kMailMaxMessageSize;

/// Identifier of the settings page this module owns.
constexpr auto kMailSettingsPageId =
    "com.bktus.gpgfrontend.module.email.accounts";

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

REGISTER_EVENT_HANDLER(KEY_DATABASE_REFRESH_DONE,
                       [](const MEvent& event) -> int {
                         // A verification that reported a missing key stops
                         // being true once that key is imported; open messages
                         // work their signatures out again rather than keeping
                         // the stale verdict.
                         EMailNotifyKeyringChanged();
                         CB_SUCC(event);
                       });

namespace {

/**
 * @brief Open @p raw as a new e-mail tab, exactly as a file would be.
 *
 * The only thing the IMAP side is allowed to do with what it fetched. Nothing
 * about the account, folder or UID comes with it, so an imported message is
 * indistinguishable from the same bytes opened from disk -- which is the whole
 * point of treating IMAP as a file picker.
 *
 * Must run on the GUI thread.
 */
auto OpenRawAsEMailTab(const QByteArray& raw, const QString& title) -> bool {
  auto* edit = GFUIGetGUIObjectAs<QWidget>("main_window_edit");
  if (edit == nullptr) {
    LOG_ERROR("main_window_edit handle invalid or not QWidget");
    return false;
  }

  QWidget* page = nullptr;
  auto ok = QMetaObject::invokeMethod(
      edit, "SlotNewCustomTab", Qt::DirectConnection,
      Q_RETURN_ARG(QWidget*, page), Q_ARG(QString, "email"),
      Q_ARG(QString, title), Q_ARG(QIcon, QIcon(":/icons/email.png")),
      Q_ARG(QString, ":/icons/email.png"));

  if (!ok || page == nullptr) {
    LOG_ERROR("create new email tab page failed");
    return false;
  }

  // Bytes, not text: the page records which line endings the message arrived
  // with, and a PGP/MIME signature covers the exact octets. Going through
  // setPlainText would destroy every signature before anything could check
  // one.
  if (!QMetaObject::invokeMethod(page, "SetContentFromBytes",
                                 Qt::DirectConnection,
                                 Q_ARG(QByteArray, raw))) {
    LOG_ERROR("host does not support SetContentFromBytes");
    return false;
  }
  return true;
}

/// A tab title for an imported message: its subject, or a neutral fallback.
///
/// Sanitized, because this title also becomes the suggested file name when the
/// tab is saved, and the subject is written by whoever sent the message -- a
/// subject containing a path separator would otherwise produce a name that
/// resolves somewhere else entirely.
auto TitleForImported(const QByteArray& raw) -> QString {
  vmime::shared_ptr<vmime::message> parsed;
  if (CheckIfEMLMessage(raw, parsed)) {
    EMailMetaData meta;
    GetEMLMetaData(parsed, meta);
    const auto subject = meta.subject.trimmed();
    if (!subject.isEmpty()) return SuggestedEMailFileName(subject);
  }
  return "imported.eml";
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

  // Importing over the network is a different kind of act from opening a new
  // editor, so it belongs in Advanced rather than beside "Mail Editor" in the
  // workspace menu.
  auto* advance_menu = GFUIGetGUIObjectAs<QMenu>(event["advance_menu"]);
  if (advance_menu == nullptr) {
    LOG_ERROR("advance_menu handle invalid or not QMenu");
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
      [&, advance_menu]() -> void {
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

        // Import from IMAP. Shown only when an account could actually be
        // browsed: an entry that always fails teaches people to ignore it.
        // Named and dressed like its neighbours in this menu ("Open Smart
        // Card Controller", "Open Module Controller"): it opens a controller
        // rather than prompting for anything, so it takes no ellipsis.
        auto* import_action = new QAction(
            QCoreApplication::translate("GTrC", "Open IMAP Controller"),
            parent);
        import_action->setIcon(QIcon(":/icons/receive_email.png"));
        import_action->setToolTip(
            QCoreApplication::translate("GTrC", "Open IMAP Controller Dialog"));
        QObject::connect(
            import_action, &QAction::triggered, parent, [parent]() {
              if (!EMailImapController::HasUsableAccount()) {
                QMessageBox::information(
                    parent,
                    QCoreApplication::translate("GTrC", "No mail account"),
                    QCoreApplication::translate(
                        "GTrC",
                        "Configure a mail account with IMAP enabled in "
                        "Settings first."));
                return;
              }

              auto* controller = new EMailImapController(parent);
              controller->setAttribute(Qt::WA_DeleteOnClose);

              // The narrow boundary: raw bytes in, a tab out. The controller
              // never touches the tab widget itself.
              QObject::connect(controller,
                               &EMailImapController::SignalMessageChosen,
                               parent, [](const QByteArray& raw) {
                                 OpenRawAsEMailTab(raw, TitleForImported(raw));
                               });
              controller->show();
            });
        if (advance_menu != nullptr) advance_menu->addAction(import_action);
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

  QByteArray info_json;
  const char* tmp = nullptr;
  const char* cards_tmp = nullptr;
  const char* info_tmp = nullptr;
  // The Info variant, not the plain one: the structured description
  // and details are what let a FAILURE explain itself, and without
  // them the board can only fall back to "<operation> failed."
  result_status = GFAnalyseVerifyResultInfoByCapsule(
      channel, err, QDUP(capsule_id), &tmp, &cards_tmp, &info_tmp);
  result_detail = UnStrDup(tmp);
  result_cards = UnStrDup(cards_tmp);
  info_json = UnStrDup(info_tmp).toUtf8();

  if (ret == kGPG_FAILED) {
    // The operation failed, and the ANALYSIS of that failure is exactly what
    // the user needs -- which key was wanted, what the engine said. Those
    // cards were just built above; dropping them here is what left a failed
    // decrypt showing raw report text where every other outcome shows a
    // structured card.
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"result_status", QString::number(result_status)},
           {"result", result_detail},
           {"result_cards",
            BuildResultCardsParam(
                QApplication::translate("EMailModule", "Verify E-Mail"), {},
                result_cards, info_json)},
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
      int result_status = 0;
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
                            .arg(meta_data.bcc_header.join("; ")));
      email_info.append(QString("- %1: %2\n")
                            .arg(QApplication::translate("EMailModule", "Date"))
                            .arg(QLocale().toString(meta_data.datetime)));

      email_info.append("\n");

      email_info.append("# OpenPGP Information\n\n");

      email_info.append(
          QString("- %1: %2\n")
              .arg(QApplication::translate(
                  "EMailModule", "Digest of Signed MIME Entity (SHA-256)"))
              .arg(meta_data.signed_entity_digest));
      email_info.append(
          QString("- %1: %2\n")
              .arg(QApplication::translate("EMailModule",
                                           "Declared Signature Hash (micalg)"))
              .arg(meta_data.micalg));

      // Without this, a message whose line endings were rewritten after
      // signing reads exactly like a forged one, and the user has no way to
      // tell the difference or to know that importing a key cannot help.
      if (meta_data.signed_entity_non_canonical) {
        email_info.append(QString("- %1\n").arg(QApplication::translate(
            "EMailModule",
            "**Note**: the signed part is not in canonical form, "
            "because its line endings are not CRLF. Something rewrote "
            "this message after it was signed, which is usually a "
            "program that changed line endings while saving or "
            "copying it. A signature cannot verify against these "
            "bytes, and importing the sender's key will not change "
            "that.")));
      }

      email_info.append("\n");

      email_info.append("#" + result_detail + "\n");

      const auto result_cards_param = BuildResultCardsParam(
          QApplication::translate("EMailModule", "Verify E-Mail"),
          BuildReadMetaCards(meta_data, true), result_cards);

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
                      QString& result_cards, QByteArray& eml_data,
                      EMailMetaData& meta_data, QByteArray& decrypt_info_json)
    -> int {
  gpgme_error_t err;
  QString capsule_id;
  auto ret =
      DecryptEMLData(channel, data, meta_data, eml_data, err, capsule_id);

  if (ret == kFAILED || ret == kEML_FAILED) {
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", data},
           {"result_status", QString::number(-1)},
           {"result", ErrorHelper(ret, QString::fromUtf8(eml_data))},
       });
    return ret;
  }

  const char* tmp = nullptr;
  const char* cards_tmp = nullptr;
  const char* info_tmp = nullptr;
  // The Info variant, because the recipient cross-check needs the structured
  // recipients rather than the rendered report. The capsule is consumed by
  // whichever analyse call touches it, so everything has to come from this one.
  result_status = GFAnalyseDecryptResultInfoByCapsule(
      channel, err, QDUP(capsule_id), &tmp, &cards_tmp, &info_tmp);
  result_detail = UnStrDup(tmp);
  result_cards = UnStrDup(cards_tmp);
  decrypt_info_json = UnStrDup(info_tmp).toUtf8();

  if (ret == kGPG_FAILED) {
    // decrypt failed. The analysis cards built above travel with it: a failure
    // is the outcome that most needs explaining, not the one to explain least.
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", data},
           {"result_status", QString::number(result_status)},
           {"result", result_detail},
           {"result_cards",
            BuildResultCardsParam(
                QApplication::translate("EMailModule", "Decrypt E-Mail"), {},
                result_cards, decrypt_info_json)},
       });
    return ret;
  }

  if (ret != kSUCCESS) {
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", data},
           {"result_status", QString::number(-1)},
           {"result", ErrorHelper(ret, QString::fromUtf8(eml_data))},
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

      QByteArray eml_data;
      int result_status = 0;
      QString result_detail;
      QString result_cards;
      EMailMetaData meta_data;
      QByteArray decrypt_info_json;
      if (DoDecryptEMLData(channel, data, event, result_status, result_detail,
                           result_cards, eml_data, meta_data,
                           decrypt_info_json) != kSUCCESS) {
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
                            .arg(meta_data.bcc_header.join("; ")));
      email_info.append(QString("- %1: %2\n")
                            .arg(QApplication::translate("EMailModule", "Date"))
                            .arg(QLocale().toString(meta_data.datetime)));

      email_info.append("\n");

      email_info.append("# OpenPGP Information\n\n");
      email_info.append("#" + result_detail + "\n");

      auto decrypt_meta_cards = BuildReadMetaCards(meta_data, false);
      const auto recipient_card =
          BuildRecipientCheckCard(meta_data, decrypt_info_json);
      if (!recipient_card.isEmpty()) decrypt_meta_cards.append(recipient_card);

      const auto result_cards_param = BuildResultCardsParam(
          QApplication::translate("EMailModule", "Decrypt E-Mail"),
          decrypt_meta_cards, result_cards);

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
                   QString& result_cards, QByteArray& eml_data) -> int {
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
           {"result", ErrorHelper(ret, QString::fromUtf8(eml_data))},
       });
    return ret;
  }

  QByteArray info_json;
  const char* tmp = nullptr;
  const char* cards_tmp = nullptr;
  const char* info_tmp = nullptr;
  // The Info variant, not the plain one: the structured description
  // and details are what let a FAILURE explain itself, and without
  // them the board can only fall back to "<operation> failed."
  result_status = GFAnalyseSignResultInfoByCapsule(
      channel, err, QDUP(capsule_id), &tmp, &cards_tmp, &info_tmp);
  result_detail = UnStrDup(tmp);
  result_cards = UnStrDup(cards_tmp);
  info_json = UnStrDup(info_tmp).toUtf8();

  if (ret == kGPG_FAILED) {
    // decrypt failed. The analysis cards built above travel with it: a failure
    // is the outcome that most needs explaining, not the one to explain least.
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", body_data},
           {"result_status", QString::number(result_status)},
           {"result", result_detail},
           {"result_cards",
            BuildResultCardsParam(
                QApplication::translate("EMailModule", "Sign E-Mail"), {},
                result_cards, info_json)},
       });
    return ret;
  }

  if (ret != kSUCCESS) {
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", body_data},
           {"result_status", QString::number(-1)},
           {"result", ErrorHelper(ret, QString::fromUtf8(eml_data))},
       });
    return ret;
  }

  return kSUCCESS;
}

auto DoSignPlainText(int channel, const QString& sign_key,
                     const EMailMetaData& meta_data,
                     const QByteArray& body_data, const MEvent& event,
                     int& result_status, QString& result_detail,
                     QString& result_cards, QByteArray& eml_data) -> int {
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
           {"result", ErrorHelper(ret, QString::fromUtf8(eml_data))},
       });
    return ret;
  }

  QByteArray info_json;
  const char* tmp = nullptr;
  const char* cards_tmp = nullptr;
  const char* info_tmp = nullptr;
  // The Info variant, not the plain one: the structured description
  // and details are what let a FAILURE explain itself, and without
  // them the board can only fall back to "<operation> failed."
  result_status = GFAnalyseSignResultInfoByCapsule(
      channel, err, QDUP(capsule_id), &tmp, &cards_tmp, &info_tmp);
  result_detail = UnStrDup(tmp);
  result_cards = UnStrDup(cards_tmp);
  info_json = UnStrDup(info_tmp).toUtf8();

  if (ret == kGPG_FAILED) {
    // decrypt failed. The analysis cards built above travel with it: a failure
    // is the outcome that most needs explaining, not the one to explain least.
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", body_data},
           {"result_status", QString::number(result_status)},
           {"result", result_detail},
           {"result_cards",
            BuildResultCardsParam(
                QApplication::translate("EMailModule", "Sign E-Mail"), {},
                result_cards, info_json)},
       });
    return ret;
  }

  if (ret != kSUCCESS) {
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", body_data},
           {"result_status", QString::number(-1)},
           {"result", ErrorHelper(ret, QString::fromUtf8(eml_data))},
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

      vmime::shared_ptr<vmime::message> message;
      if (CheckIfEMLMessage(body_data, message)) {
        int result_status = 0;
        QString result_detail;
        QString result_cards;
        QByteArray eml_data;
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

      // Not a message yet: wrap the text in an envelope derived from the
      // signing key rather than stopping to ask for one.
      const auto meta_data = EnvelopeFromKeys(channel, sign_key, {});

      int result_status = 0;
      QString result_detail;
      QString result_cards;
      QByteArray eml_data;
      if (DoSignPlainText(channel, sign_key, meta_data, body_data, event,
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
                  QApplication::translate("EMailModule", "Sign E-Mail"),
                  {BuildEMailHeaderCard(meta_data)}, result_cards)},
         });
      return 0;
    });

namespace {

auto DoEncryptEMLData(int channel, const QStringList& encrypt_keys,
                      const vmime::shared_ptr<vmime::message>& message,
                      const QByteArray& body_data, const MEvent& event,
                      int& result_status, QString& result_detail,
                      QString& result_cards, QByteArray& eml_data) -> int {
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
           {"result", ErrorHelper(ret, QString::fromUtf8(eml_data))},
       });
    return ret;
  }

  QByteArray info_json;
  const char* tmp = nullptr;
  const char* cards_tmp = nullptr;
  const char* info_tmp = nullptr;
  // The Info variant, not the plain one: the structured description
  // and details are what let a FAILURE explain itself, and without
  // them the board can only fall back to "<operation> failed."
  result_status = GFAnalyseEncryptResultInfoByCapsule(
      channel, err, QDUP(capsule_id), &tmp, &cards_tmp, &info_tmp);
  result_detail = UnStrDup(tmp);
  result_cards = UnStrDup(cards_tmp);
  info_json = UnStrDup(info_tmp).toUtf8();

  if (ret == kGPG_FAILED) {
    // encrypt failed. The analysis cards built above travel with it: a failure
    // is the outcome that most needs explaining, not the one to explain least.
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", QString::fromLatin1(body_data.toBase64())},
           {"result_status", QString::number(result_status)},
           {"result", result_detail},
           {"result_cards",
            BuildResultCardsParam(
                QApplication::translate("EMailModule", "Encrypt E-Mail"), {},
                result_cards, info_json)},
       });
    return ret;
  }

  return kSUCCESS;
}

auto DoEncryptPlainText(int channel, const QStringList& encrypt_keys,
                        const EMailMetaData& meta_data,
                        const QByteArray& body_data, const MEvent& event,
                        int& result_status, QString& result_detail,
                        QString& result_cards, QByteArray& eml_data) -> int {
  gpgme_error_t err;
  QString capsule_id;
  QByteArray plain_text_eml_data;
  auto ret = BuildPlainTextEML(meta_data, body_data, plain_text_eml_data);

  if (ret != kSUCCESS) {
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", QString::fromLatin1(body_data.toBase64())},
           {"result_status", QString::number(-1)},
           {"result", ErrorHelper(ret, QString::fromUtf8(eml_data))},
       });
    return ret;
  }

  ret = EncryptPlainText(channel, encrypt_keys, meta_data, plain_text_eml_data,
                         eml_data, err, capsule_id);

  if (ret == kFAILED || ret == kEML_FAILED) {
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", QString::fromLatin1(body_data.toBase64())},
           {"result_status", QString::number(-1)},
           {"result", ErrorHelper(ret, QString::fromUtf8(eml_data))},
       });
    return ret;
  }

  QByteArray info_json;
  const char* tmp = nullptr;
  const char* cards_tmp = nullptr;
  const char* info_tmp = nullptr;
  // The Info variant, not the plain one: the structured description
  // and details are what let a FAILURE explain itself, and without
  // them the board can only fall back to "<operation> failed."
  result_status = GFAnalyseEncryptResultInfoByCapsule(
      channel, err, QDUP(capsule_id), &tmp, &cards_tmp, &info_tmp);
  result_detail = UnStrDup(tmp);
  result_cards = UnStrDup(cards_tmp);
  info_json = UnStrDup(info_tmp).toUtf8();

  if (ret == kGPG_FAILED) {
    // encrypt failed. The analysis cards built above travel with it: a failure
    // is the outcome that most needs explaining, not the one to explain least.
    CB(event, GFGetModuleID(),
       {
           {"ret", QString::number(0)},
           {"data", QString::fromLatin1(body_data.toBase64())},
           {"result_status", QString::number(result_status)},
           {"result", result_detail},
           {"result_cards",
            BuildResultCardsParam(
                QApplication::translate("EMailModule", "Encrypt E-Mail"), {},
                result_cards, info_json)},
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
        QByteArray eml_data;
        int result_status = 0;
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

      // Not a message yet: address it to the keys the user picked to encrypt
      // to, rather than stopping to ask for addresses they have just chosen.
      const auto meta_data = EnvelopeFromKeys(channel, {}, encrypt_keys);

      QByteArray eml_data;
      int result_status = 0;
      QString result_detail;
      QString result_cards;
      if (DoEncryptPlainText(channel, encrypt_keys, meta_data, body_data, event,
                             result_status, result_detail, result_cards,
                             eml_data) != kSUCCESS) {
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
                  QApplication::translate("EMailModule", "Encrypt E-Mail"),
                  meta_cards, result_cards)},
         });
      return 0;
    });

namespace {

auto DoEncryptSignEMLData(int channel, const QStringList& encrypt_keys,
                          const QString& sign_key,
                          vmime::shared_ptr<vmime::message>& message,
                          QByteArray& body_data, const MEvent& event,
                          int& result_status, QString& result_detail,
                          QString& result_cards, QByteArray& eml_data) -> int {
  QString sign_cards;
  if (DoSignEMLData(channel, sign_key, message, body_data, event, result_status,
                    result_detail, sign_cards, eml_data) != kSUCCESS) {
    return -1;
  }

  // UTF-8, not Latin-1: these are the bytes the signature was just taken
  // over, and they are about to be re-parsed and encrypted. Latin-1 collapses
  // every two-byte sequence to one byte and replaces anything above U+00FF, so
  // the recipient would verify a different message than the one that was
  // signed. The module converts through UTF-8 everywhere else -- Q_SC, QDUP
  // and UDUP are all UTF-8 -- and corpus fixture 18 pins it.
  body_data = eml_data;
  eml_data.clear();

  int t_result_status = 0;
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

  // The callee has already reported every failure of its own through CB, and
  // leaves t_result_status untouched on the earliest of those paths.
  // Aggregating there would read an uninitialised value and overwrite a status
  // that has already been sent.
  if (ret != kSUCCESS) return ret;

  result_status = WorseStatus(t_result_status, result_status);
  result_detail = t_result_detail + "\n\n" + result_detail;
  result_cards = MergeCardArrays(encrypt_cards, sign_cards);
  return ret;
}

auto DoEncryptSignPlainText(int channel, const QStringList& encrypt_keys,
                            const QString& sign_key,
                            const EMailMetaData& meta_data,
                            QByteArray& body_data, const MEvent& event,
                            int& result_status, QString& result_detail,
                            QString& result_cards, QByteArray& eml_data)
    -> int {
  QString sign_cards;
  if (DoSignPlainText(channel, sign_key, meta_data, body_data, event,
                      result_status, result_detail, sign_cards,
                      eml_data) != kSUCCESS) {
    return -1;
  }

  // UTF-8, not Latin-1: these are the bytes the signature was just taken
  // over, and they are about to be re-parsed and encrypted. Latin-1 collapses
  // every two-byte sequence to one byte and replaces anything above U+00FF, so
  // the recipient would verify a different message than the one that was
  // signed. The module converts through UTF-8 everywhere else -- Q_SC, QDUP
  // and UDUP are all UTF-8 -- and corpus fixture 18 pins it.
  body_data = eml_data;
  eml_data.clear();

  int t_result_status = 0;
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

  // The callee has already reported every failure of its own through CB, and
  // leaves t_result_status untouched on the earliest of those paths.
  // Aggregating there would read an uninitialised value and overwrite a status
  // that has already been sent.
  if (ret != kSUCCESS) return ret;

  result_status = WorseStatus(t_result_status, result_status);
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
        QByteArray eml_data;
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

      // Not a message yet: address it from the signing key to the encryption
      // keys, rather than stopping to ask for what the key list already says.
      const auto meta_data = EnvelopeFromKeys(channel, sign_key, encrypt_keys);

      QByteArray eml_data;
      int result_status = 0;
      QString result_detail;
      QString result_cards;
      QByteArray body_data_copy = body_data;

      if (DoEncryptSignPlainText(channel, encrypt_keys, sign_key, meta_data,
                                 body_data_copy, event, result_status,
                                 result_detail, result_cards,
                                 eml_data) != kSUCCESS) {
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
             {"result_cards", BuildResultCardsParam(
                                  QApplication::translate(
                                      "EMailModule", "Encrypt and Sign E-Mail"),
                                  meta_cards, result_cards)},
         });
      return 0;
    });

namespace {

auto DoDecryptVerifyEMLData(int channel, const QByteArray& data,
                            const MEvent& event, int& result_status,
                            QString& result_detail, QString& result_cards,
                            QByteArray& eml_data, QString& error_string,
                            EMailMetaData& meta_data,
                            QByteArray& decrypt_info_json,
                            EMailPostDecryptPlan& plan) -> int {
  QString decrypt_cards;
  if (DoDecryptEMLData(channel, data, event, result_status, result_detail,
                       decrypt_cards, eml_data, meta_data,
                       decrypt_info_json) != kSUCCESS) {
    return -1;
  }

  // The decrypt has succeeded and the plaintext is in hand. Whether there is
  // anything to VERIFY is a separate question, and answering it "no" must not
  // cost the user the plaintext they have just paid a passphrase for.
  //
  // VerifyEMLData() refuses any message whose outermost layer is not
  // multipart/signed, and that refusal used to be treated as a failure of the
  // whole operation: the callback carried no data, so the host -- which writes
  // the editor only when data is present -- discarded the plaintext and showed
  // an RFC 3156 lecture instead. An encrypted message that is not signed is
  // the ordinary case, not a fault.
  plan = PlanVerifyAfterDecrypt(eml_data);
  if (plan != EMailPostDecryptPlan::kVERIFY) {
    result_cards = decrypt_cards;
    const auto note =
        plan == EMailPostDecryptPlan::kNOT_SIGNED
            ? QApplication::translate(
                  "EMailModule",
                  "This message is not signed, so there is no signature to "
                  "check. It was decrypted successfully.")
            : QApplication::translate(
                  "EMailModule",
                  "The decrypted content is not a MIME message, so there is no "
                  "signature to check. It was decrypted successfully.");
    result_detail = note + "\n" + result_detail;
    return kSUCCESS;
  }

  int t_result_status = 0;
  QString t_result_detail;
  QString verify_cards;

  // Its OWN metadata object, not the one the decrypt filled. Both walk the
  // same plaintext and ExtractParts() only ever appends, so sharing one object
  // listed every attachment twice.
  EMailMetaData verified_meta;

  // UTF-8, not Latin-1: this is the decrypted plaintext and the signature
  // inside it is checked against these exact bytes. See DoEncryptSignEMLData.
  if (DoVerifyEMLData(channel, eml_data, event, t_result_status,
                      t_result_detail, verify_cards, error_string,
                      verified_meta) != kSUCCESS) {
    return -1;
  }

  MergeVerifiedMetaData(meta_data, verified_meta);

  result_status = WorseStatus(t_result_status, result_status);
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

      QByteArray eml_data;
      EMailMetaData meta_data;
      QString error_string;
      int result_status = 0;
      QString result_detail;
      QString result_cards;

      QByteArray decrypt_info_json;
      auto plan = EMailPostDecryptPlan::kVERIFY;
      if (DoDecryptVerifyEMLData(channel, data, event, result_status,
                                 result_detail, result_cards, eml_data,
                                 error_string, meta_data, decrypt_info_json,
                                 plan) != kSUCCESS) {
        return -1;
      }
      const bool verified = plan == EMailPostDecryptPlan::kVERIFY;

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
                            .arg(meta_data.bcc_header.join("; ")));
      email_info.append(QString("- %1: %2\n")
                            .arg(QApplication::translate("EMailModule", "Date"))
                            .arg(QLocale().toString(meta_data.datetime)));

      email_info.append("\n");

      email_info.append("# OpenPGP Information\n\n");

      // Only when something actually verified. An unsigned message has no
      // signed entity and no declared hash, and printing those labels with
      // nothing after them reads as a missing answer rather than as the
      // absence of a question.
      if (!verified) {
        email_info.append(QString("- %1\n").arg(QApplication::translate(
            "EMailModule",
            "This message was decrypted. It carries no signature, so nothing "
            "here says who sent it.")));
      } else {
        email_info.append(
            QString("- %1: %2\n")
                .arg(QApplication::translate(
                    "EMailModule", "Digest of Signed MIME Entity (SHA-256)"))
                .arg(meta_data.signed_entity_digest));
        email_info.append(
            QString("- %1: %2\n")
                .arg(QApplication::translate(
                    "EMailModule", "Declared Signature Hash (micalg)"))
                .arg(meta_data.micalg));
      }

      // Without this, a message whose line endings were rewritten after
      // signing reads exactly like a forged one, and the user has no way to
      // tell the difference or to know that importing a key cannot help.
      if (meta_data.signed_entity_non_canonical) {
        email_info.append(QString("- %1\n").arg(QApplication::translate(
            "EMailModule",
            "**Note**: the signed part is not in canonical form, "
            "because its line endings are not CRLF. Something rewrote "
            "this message after it was signed, which is usually a "
            "program that changed line endings while saving or "
            "copying it. A signature cannot verify against these "
            "bytes, and importing the sender's key will not change "
            "that.")));
      }

      email_info.append("\n");

      email_info.append("#" + result_detail + "\n");

      auto dv_meta_cards = BuildReadMetaCards(meta_data, verified);
      const auto dv_recipient_card =
          BuildRecipientCheckCard(meta_data, decrypt_info_json);
      if (!dv_recipient_card.isEmpty()) dv_meta_cards.append(dv_recipient_card);

      const auto result_cards_param = BuildResultCardsParam(
          QApplication::translate("EMailModule", "Decrypt and Verify E-Mail"),
          dv_meta_cards, result_cards);

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
              // Named after the message rather than left blank. The view is
              // the only thing that knows the subject, and it is reached the
              // same way the rest of this module reaches it.
              QString suggested;
              if (auto* view = page->findChild<EMailPageView*>();
                  view != nullptr) {
                suggested = view->SuggestedFileName();
              }
              if (suggested.isEmpty())
                suggested = QStringLiteral("untitled.eml");

              filename = QFileDialog::getSaveFileName(
                  page, QApplication::translate("EMailModule", "Save file"),
                  QDir(default_save_dir()).filePath(suggested),
                  QApplication::translate("EMailModule",
                                          "E-Mail Message (*.eml);;All Files "
                                          "(*)"));
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

      QPlainTextEdit* text_edit = nullptr;
      ok = QMetaObject::invokeMethod(page, "GetTextPage",
                                     Qt::BlockingQueuedConnection,
                                     Q_RETURN_ARG(QPlainTextEdit*, text_edit));
      if (!ok || text_edit == nullptr) {
        LOG_ERROR("invoke GetTextPage failed");
        CB_ERR(event, -1, "invoke GetTextPage failed");
      }

      // Normalize to LF first: the editor may already hold CRLF, and blindly
      // expanding every "\n" then turns each of those into CRCRLF.
      auto text = text_edit->toPlainText();
      text.replace("\r\n", "\n");
      text.replace("\n", "\r\n");

      // Last look before the bytes leave. Deliberately after the content is
      // assembled and before anything is written, so what is checked is
      // exactly what would be saved.
      if (!ConfirmExport(page, text.toUtf8())) {
        LOG_INFO("user cancelled the save after the export check");
        CB_SUCC(event);
      }

      // Written binary and through QSaveFile: QIODevice::Text would translate
      // the line endings a second time on Windows, and a plain QFile leaves a
      // truncated .eml behind if the write fails halfway.
      QSaveFile file(filename);
      if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(
            page, QApplication::translate("EMailModule", "Warning"),
            QApplication::translate("EMailModule", "Cannot write file %1:\n%2.")
                .arg(filename)
                .arg(file.errorString()));
        CB_ERR(event, -1, "cannot open file for writing");
      }

      QApplication::setOverrideCursor(Qt::WaitCursor);
      const auto bytes = text.toUtf8();
      const bool written = file.write(bytes) == bytes.size() && file.commit();
      QApplication::restoreOverrideCursor();

      if (!written) {
        QMessageBox::warning(
            page, QApplication::translate("EMailModule", "Warning"),
            QApplication::translate("EMailModule", "Cannot write file %1:\n%2.")
                .arg(filename)
                .arg(file.errorString()));
        CB_ERR(event, -1, "writing file failed");
      }

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

      // A 1 MB ceiling used to stand here, which refused any message with a
      // real attachment. What it was actually protecting was the synchronous
      // parse of untrusted input, and that is now bounded properly by the
      // depth, part-count and decoded-size limits in ExtractParts -- so the
      // ceiling can be about memory alone.
      // A FIFO, a device node or /proc entry reports size 0 and would sail
      // through the ceiling below, then block or grow without bound inside
      // readAll() -- on the GUI thread, where the read actually happens.
      if (!file_info.isFile()) {
        QMessageBox::warning(
            nullptr, QApplication::translate("EMailModule", "Warning"),
            QApplication::translate(
                "EMailModule",
                "%1 is not an ordinary file, so it cannot be opened as a "
                "message.")
                .arg(file_path));
        CB_ERR(event, -1, "not a regular file");
      }

      if (file_info.size() > kMaxEMLFileSize) {
        QMessageBox::warning(
            nullptr, QApplication::translate("EMailModule", "Warning"),
            QApplication::translate(
                "EMailModule",
                "The file %1 is too large (%2) to be opened. The maximum "
                "allowed size is %3.")
                .arg(file_path)
                .arg(QLocale().formattedDataSize(file_info.size()))
                .arg(QLocale().formattedDataSize(kMaxEMLFileSize)));
        CB_ERR(event, -1, "file too large");
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
        // NOT QIODevice::Text. Text mode translates CRLF to LF on the way
        // in, and a PGP/MIME signature covers the exact octets of the message
        // in canonical CRLF form -- so reading it that way silently destroys
        // every signature in the file before anything has a chance to check
        // one.
        if (!file.open(QIODevice::ReadOnly)) {
          QMessageBox::warning(
              nullptr, QApplication::translate("EMailModule", "Warning"),
              QApplication::translate("EMailModule",
                                      "Cannot read file %1:\n%2.")
                  .arg(file_path)
                  .arg(file.errorString()));
          return;
        }

        // Checked again on the OPEN handle. The size test above ran on the
        // module thread against a path; this runs against the file actually
        // opened, which is not necessarily the same one and not necessarily
        // the same length.
        if (file.size() > kMaxEMLFileSize) {
          QMessageBox::warning(
              nullptr, QApplication::translate("EMailModule", "Warning"),
              QApplication::translate(
                  "EMailModule",
                  "The file %1 is too large (%2) to be opened. The maximum "
                  "allowed size is %3.")
                  .arg(file_path)
                  .arg(QLocale().formattedDataSize(file.size()))
                  .arg(QLocale().formattedDataSize(kMaxEMLFileSize)));
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

        const auto raw = file.readAll();

        // Handed over as bytes so the page can record which line endings the
        // message arrived with and reproduce them when it is read back or
        // saved. Falls back to the old path on a host that does not offer
        // this, where the endings are lost exactly as they were before.
        if (!QMetaObject::invokeMethod(page, "SetContentFromBytes",
                                       Qt::DirectConnection,
                                       Q_ARG(QByteArray, raw))) {
          text_edit->setPlainText(QString::fromUtf8(raw));
          text_edit->document()->setModified(false);
        }

        QMetaObject::invokeMethod(page, "SetFilePath", Qt::DirectConnection,
                                  Q_ARG(QString, file_path));
      });
      return 0;
    })