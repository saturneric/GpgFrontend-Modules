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

#include <GFSDKBuildInfo.h>
#include <GFSDKLog.h>

#include "EMailAccountSettingsPage.h"
#include "EMailAccountStore.h"
#include "EMailImapController.h"
#include "EMailSecret.h"
#include "EMailSendDialog.h"
#include "GFModuleIdentity.h"
#include "GFSDKHostCommands.hpp"

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
#include <QThread>

#include "EMailPageView.h"

// vmime
// The test target defines this on the command line; the module build does
// not, so it is set here and guarded rather than assumed either way.
#ifndef VMIME_STATIC
#define VMIME_STATIC
#endif
#include <algorithm>
#include <functional>
#include <vmime/vmime.hpp>

// vmime extend
#include <vmime/contentTypeField.hpp>

#include "GFModule.h"
#include "GFSDKHostCommands.hpp"

//
#include "EMailBasicGpgOpera.h"
#include "EMailHelper.h"
#include "EMailVerificationPayload.h"

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
// so resolve each to its primary UID here -- mirroring the native operation's
// recipient card.
// The address a key stands for, as "Name <email>".
auto AddressOfKey(int channel, const QString& key_id) -> QString {
  if (key_id.isEmpty()) return {};

  // Three out-parameters, and only the two that are wanted. The struct this
  // replaced handed back all three as char* members to be freed one by one,
  // which is why the comment field used to be fetched purely to release it.
  auto* ctx = GFModuleSdkContext();
  GFBufferRef name_buf = nullptr;
  GFBufferRef email_buf = nullptr;
  if (GFGpgKeyPrimaryUid(ctx, channel, key_id.toUtf8().constData(), &name_buf,
                         &email_buf, nullptr) != 0) {
    return {};
  }

  const auto name = gf::sdk::TakeString(ctx, name_buf);
  const auto email = gf::sdk::TakeString(ctx, email_buf);

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
                 "EMailModule",
                 "in %1, but not encrypted to it: they cannot read this "
                 "message")
                 .arg(row.header_field)});
        break;
      case RecipientMatch::kENCRYPTED_NOT_ADDRESSED:
        fields.append({row.info.uid.isEmpty() ? row.info.key_id : row.info.uid,
                       QApplication::translate(
                           "EMailModule",
                           "encrypted to, but not listed in the headers")});
        break;
      case RecipientMatch::kHIDDEN_RECIPIENT:
        fields.append(
            {QApplication::translate("EMailModule", "Hidden recipient"),
             QApplication::translate("EMailModule",
                                     "the sender withheld this key ID")});
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
                           const QByteArray& info_json = {},
                           const QString& description = {}) -> QString {
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

  // The caller's own account of the outcome wins over the engine's. It is
  // written from the ONE aggregated verdict -- the same one the badge quotes --
  // so the board names the actual reason rather than telling the user to go and
  // find it in the details.
  if (!description.isEmpty()) obj["description"] = description;

  // A payload with neither cards nor a description has nothing the board could
  // render, so the caller falls back to the plain-text path.
  if (cards.isEmpty() && !obj.contains("description")) return {};

  obj.insert("cards", cards);
  return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

}  // namespace

/// Identifier of the settings page this module owns.
namespace {
auto OpenRawAsEMailTab(const QByteArray& raw, const QString& title) -> bool;
auto TitleForImported(const QByteArray& raw) -> QString;
}  // namespace

auto OnActivate() -> GFResult {
  LOG_INFO("email module registering");

  // The MIME code carries no SDK symbol of its own so it can be unit-tested
  // without a module host; this is what gives it a logger at runtime.
  SetMimeLogSink([](const QString& m) { MLogDebug(m); });
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
  qRegisterMetaType<EMailFolderValidators>("EMailFolderValidators");
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

  // The module's own widgets, which ui/main.lua mounts: the message view of
  // an e-mail document (one per tab; the Host owns the page, its document
  // and the raw source beside it), the mail accounts settings page, and the
  // IMAP controller dialog. Presentation is untranslated: the Host
  // translates it when shown, after the module translators are installed.
  const bool editor = gf::ui::RegisterNativeWidgetFactory<EMailPageView>(
      "editor",
      {GC_TR("E-Mail"), "", "eml", GC_TR("E-Mail Message (*.eml);;All Files (*)"),
       ":/icons/email.png", 0, 0},
      [](const QCborMap& /*args*/) { return new EMailPageView(); });
  const bool settings = gf::ui::RegisterNativeWidget<EMailAccountSettingsPage>(
      "settings",
      {GC_TR("Mail Accounts"),
       GC_TR("mail,email,imap,smtp,account,send"), "", "", "", 0, 0},
      [](const QCborMap& /*args*/) { return new EMailAccountSettingsPage(); });
  const bool imap = gf::ui::RegisterNativeWidget<EMailImapController>(
      "imap", {GC_TR("IMAP Controller"), "", "", "", "", 0, 0},
      [](const QCborMap& /*args*/) {
        auto* controller = new EMailImapController();
        // The narrow boundary: raw bytes in, a document out. The controller
        // never touches a tab itself.
        QObject::connect(controller, &EMailImapController::SignalMessageChosen,
                         controller, [](const QByteArray& raw) {
                           OpenRawAsEMailTab(raw, TitleForImported(raw));
                         });
        return controller;
      });

  return editor && settings && imap
             ? GFResult::Ok()
             : GFResult::Fail("the mail widgets were not registered");
}

auto OnUnload() -> void { LOG_INFO("email module unregistering"); }

namespace {

/**
 * @brief Runs @p fn on the GUI thread and waits for it.
 *
 * Module event handlers do NOT run on the GUI thread -- they run on the
 * module task runner (see GlobalModuleContext) -- and a QWidget may only be
 * created, shown or read from the thread that owns it. Every widget touch in
 * this file goes through here.
 *
 * Already-on-the-GUI-thread is handled rather than assumed away, because the
 * same helpers are called from inside blocks that have already hopped over.
 * A blocking queued connection to oneself is a deadlock, not a no-op.
 */
void RunOnGui(const std::function<void()>& fn) {
  auto* app = QCoreApplication::instance();
  if (app == nullptr || QThread::currentThread() == app->thread()) {
    fn();
    return;
  }
  QMetaObject::invokeMethod(app, fn, Qt::BlockingQueuedConnection);
}

// The preflight dialog: what the user should know before this message leaves.
//
// Shown only when there is something to say, and it never refuses the save --
// the decision is the user's, and a check that blocks gets worked around
// rather than read. Returns false when the user chooses not to write the file.
//
// The decision itself is CheckBeforeExport(), which parses and inspects the
// message and knows nothing about widgets. Only the asking lives here, and
// only that part goes to the GUI thread: parsing a message the user just
// composed is not work the GUI thread should be doing.
auto ConfirmExport(QWidget* parent, const EMailExportCheck& check) -> bool {
  if (!check.needs_confirmation) return true;

  bool accepted = false;
  RunOnGui([&]() {
    QMessageBox box(parent);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(
        QApplication::translate("EMailModule", "Check before exporting"));
    box.setText(QApplication::translate(
        "EMailModule",
        "Something about this message is worth checking before you save it."));
    box.setInformativeText(check.risks.join("\n\n"));
    if (!check.notes.isEmpty()) {
      box.setDetailedText(check.notes.join("\n\n"));
    }
    box.setStandardButtons(QMessageBox::Save | QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Cancel);

    accepted = box.exec() == QMessageBox::Save;
  });
  return accepted;
}

}  // namespace

auto EMailConfirmExport(QWidget* parent, const EMailExportCheck& check)
    -> bool {
  return ConfirmExport(parent, check);
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

auto OnKeyDatabaseRefreshDone(const GFEvent& event) -> GFEventResult {
  // A verification that reported a missing key stops
  // being true once that key is imported; open messages
  // work their signatures out again rather than keeping
  // the stale verdict.
  EMailNotifyKeyringChanged();
  return GFEventResult::Ok();
}

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
  // Bytes, as a Blob: the Host records which line endings the message
  // arrived with, and a PGP/MIME signature covers the exact octets.
  return Commands()
      .Invoke<gf::cmd::host::DocumentOpen>(
          {QStringLiteral("email"), title, QString(),
           gf::cmd::MakeBlob(raw.constData(), static_cast<size_t>(raw.size())),
           false, false})
      .Ok();
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

namespace {

/// What the status board should make of a verification.
///
/// Derived from the ONE aggregated verdict, never from a second question put
/// to the engine: the board saying "success" while the badge beside it says
/// "bad signature" is the whole reason this path was unified.
auto ReportStatusFor(EMailBadgeState overall) -> int {
  switch (overall) {
    case EMailBadgeState::kSIGNED_GOOD:
      return 1;

    case EMailBadgeState::kSIGNED_BAD:
    case EMailBadgeState::kSIGNED_ERROR:
    case EMailBadgeState::kMALFORMED:
      return -1;

    case EMailBadgeState::kSIGNED_EXPIRED:
    case EMailBadgeState::kSIGNED_UNKNOWN_KEY:
    case EMailBadgeState::kSIGNED_MISMATCH:
    case EMailBadgeState::kSIGNED_UNVERIFIED:
    case EMailBadgeState::kNOT_PROTECTED:
    case EMailBadgeState::kENCRYPTED_ONLY:
      // Checked, with something worth saying about it. Not a failure: the
      // operation did what was asked, and the report says what it found.
      return 0;
  }
  return 0;
}

/// Why the verification came out the way it did, in one sentence.
///
/// The status board used to say only "completed with warnings -- please review
/// the details", which tells the user that something is wrong and makes them go
/// looking for what. Every outcome below has a specific, knowable cause, and
/// the board is where it belongs: it is the surface that announces the result.
///
/// Written from the aggregated verdict, the same one the badge and the
/// attachment list quote, so the card and the message surface cannot end up
/// describing the outcome differently.
auto ReportDescriptionFor(const EMailVerificationResult& result) -> QString {
  // Every string below is a literal at the translate() call, never routed
  // through a helper taking a const char*: lupdate reads the call site, and a
  // variable there makes it skip the string -- silently, and for the whole
  // file. See the notes on lupdate in the translation tooling.

  // Said wherever it applies, because it is the one cause that looks exactly
  // like a forgery and is not one -- and the one no amount of key importing
  // will fix.
  const auto rewritten =
      result.meta.signed_entity_non_canonical
          ? QApplication::translate(
                "EMailModule",
                " The bytes the signature covers were rewritten after it was "
                "made: their line endings are no longer CRLF. This is "
                "usually caused by a program that changed them while saving "
                "or copying the message. Checking the signature requires "
                "the original bytes.")
          : QString();

  switch (result.overall) {
    case EMailBadgeState::kSIGNED_GOOD:
      return QApplication::translate(
          "EMailModule",
          "The signature is valid, and the key that made it speaks for the "
          "address this message says it is from.");

    case EMailBadgeState::kSIGNED_MISMATCH:
      return QApplication::translate(
          "EMailModule",
          "The signature itself is valid, but the key that made it does not "
          "speak for the address this message says it is from. That is what a "
          "signature moved from another message looks like, so it is worth "
          "checking who the signer is before trusting the contents.");

    case EMailBadgeState::kSIGNED_UNKNOWN_KEY:
      return QApplication::translate(
          "EMailModule",
          "The key that made this signature is not in your keyring, so nothing "
          "here can say whether the signature is genuine. Import the sender's "
          "key and verify again.");

    case EMailBadgeState::kSIGNED_EXPIRED:
      return QApplication::translate(
          "EMailModule",
          "The signature was made with a key that has expired, or the "
          "signature itself has. It may still be genuine; what cannot be "
          "confirmed is that the key was valid at the time it was used.");

    case EMailBadgeState::kSIGNED_BAD:
      return QApplication::translate(
                 "EMailModule",
                 "The signature does not match the bytes it covers. Either the "
                 "message was changed after it was signed, or the signature "
                 "was not made for this message.") +
             rewritten;

    case EMailBadgeState::kSIGNED_ERROR:
      return QApplication::translate(
                 "EMailModule",
                 "The check could not be completed, so nothing is known about "
                 "this signature either way. This is not a statement that the "
                 "signature is bad.") +
             rewritten;

    case EMailBadgeState::kMALFORMED:
      return QApplication::translate(
          "EMailModule",
          "This message claims to carry an OpenPGP signature, but its "
          "structure does not hold up well enough to check one.");

    case EMailBadgeState::kENCRYPTED_ONLY:
      return QApplication::translate(
          "EMailModule",
          "The signatures in this message cover the encrypted data rather "
          "than the content you read. They confirm who encrypted the "
          "message, but say nothing about who wrote what is inside it.");

    case EMailBadgeState::kSIGNED_UNVERIFIED:
    case EMailBadgeState::kNOT_PROTECTED:
      break;
  }

  return QApplication::translate(
      "EMailModule", "There was no signature in this message to check.");
}

auto DoVerifyEMLData(int channel, const QByteArray& data, const GFEvent& event,
                     QString& error_string, EMailVerificationResult& result)
    -> int {
  auto ret = VerifyEMLMessage(channel, data, result, error_string);
  if (ret != kSUCCESS) {
    event.Answer().Ok({{"result_status", QString::number(-1)},
                       {"result", ErrorHelper(ret, error_string)}});
    return ret;
  }
  return kSUCCESS;
}
}  // namespace

auto OnEditTabTypeEmailOpVerify(const GFEvent& event) -> GFEventResult {
  if (event.Str("channel").isEmpty())
    return GFEventResult::Bad("channel is empty");
  if (event.Str("data").isEmpty()) return GFEventResult::Bad("data is empty");

  auto channel = event.Str("channel").toInt();
  auto data = QByteArray::fromBase64(QString(event.Str("data")).toLatin1());

  QString error_string;
  EMailVerificationResult result;
  if (DoVerifyEMLData(channel, data, event, error_string, result) != kSUCCESS) {
    return GFEventResult::Deferred();
  }

  const auto& meta_data = result.meta;
  const auto result_status = ReportStatusFor(result.overall);

  QString email_info;
  email_info.append("# E-Mail Information\n\n");
  email_info.append(QString("- %1: %2\n")
                        .arg(QApplication::translate("EMailModule", "From"))
                        .arg(meta_data.from));
  email_info.append(QString("- %1: %2\n")
                        .arg(QApplication::translate("EMailModule", "To"))
                        .arg(meta_data.to.join("; ")));
  email_info.append(QString("- %1: %2\n")
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
  email_info.append(QString("- %1: %2\n")
                        .arg(QApplication::translate(
                            "EMailModule", "Declared Signature Hash (micalg)"))
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

  email_info.append("#" + result.report_detail + "\n");

  const auto result_cards_param = BuildResultCardsParam(
      QApplication::translate("EMailModule", "Verify E-Mail"),
      BuildReadMetaCards(meta_data, true), result.report_cards,
      result.report_info_json, ReportDescriptionFor(result));

  // callback
  event.Answer().Ok({{"result_status", QString::number(result_status)},
                     {"result", email_info},
                     {"result_cards", result_cards_param},
                     {"verification", EncodeVerificationPayload(result)}});
  return GFEventResult::Deferred();
}

namespace {

auto DoDecryptEMLData(int channel, const QByteArray& data, const GFEvent& event,
                      int& result_status, QString& result_detail,
                      QString& result_cards, QByteArray& eml_data,
                      EMailMetaData& meta_data, QByteArray& decrypt_info_json)
    -> int {
  uint32_t err;
  QString capsule_id;
  auto ret =
      DecryptEMLData(channel, data, meta_data, eml_data, err, capsule_id);

  if (ret == kFAILED || ret == kEML_FAILED) {
    event.Answer().Ok(
        {{"data", data},
         {"result_status", QString::number(-1)},
         {"result", ErrorHelper(ret, QString::fromUtf8(eml_data))}});
    return ret;
  }

  // The Info variant, because the recipient cross-check needs the structured
  // recipients rather than the rendered report. The capsule is consumed by
  // whichever analyse call touches it, so everything has to come from this one.
  const auto analysis = gf::sdk::AnalyseResult(
      GFModuleSdkContext(), channel, GF_GPG_ANALYSE_DECRYPT, err, capsule_id);
  result_status = analysis.status;
  result_detail = analysis.report;
  result_cards = analysis.cards;
  decrypt_info_json = analysis.info_json.toUtf8();

  if (ret == kGPG_FAILED) {
    // decrypt failed. The analysis cards built above travel with it: a failure
    // is the outcome that most needs explaining, not the one to explain least.
    event.Answer().Ok(
        {{"data", data},
         {"result_status", QString::number(result_status)},
         {"result", result_detail},
         {"result_cards",
          BuildResultCardsParam(
              QApplication::translate("EMailModule", "Decrypt E-Mail"), {},
              result_cards, decrypt_info_json)}});
    return ret;
  }

  if (ret != kSUCCESS) {
    event.Answer().Ok(
        {{"data", data},
         {"result_status", QString::number(-1)},
         {"result", ErrorHelper(ret, QString::fromUtf8(eml_data))}});
    return ret;
  }

  return 0;
}

}  // namespace

auto OnEditTabTypeEmailOpDecrypt(const GFEvent& event) -> GFEventResult {
  if (event.Str("channel").isEmpty())
    return GFEventResult::Bad("channel is empty");
  if (event.Str("data").isEmpty()) return GFEventResult::Bad("data is empty");

  auto channel = event.Str("channel").toInt();
  auto data = QByteArray::fromBase64(QString(event.Str("data")).toLatin1());

  QByteArray eml_data;
  int result_status = 0;
  QString result_detail;
  QString result_cards;
  EMailMetaData meta_data;
  QByteArray decrypt_info_json;
  if (DoDecryptEMLData(channel, data, event, result_status, result_detail,
                       result_cards, eml_data, meta_data,
                       decrypt_info_json) != kSUCCESS) {
    return GFEventResult::Deferred();
  }

  QString email_info;
  email_info.append("# E-Mail Information\n\n");
  email_info.append(QString("- %1: %2\n")
                        .arg(QApplication::translate("EMailModule", "From"))
                        .arg(meta_data.from));
  email_info.append(QString("- %1: %2\n")
                        .arg(QApplication::translate("EMailModule", "To"))
                        .arg(meta_data.to.join("; ")));
  email_info.append(QString("- %1: %2\n")
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
  event.Answer().Ok({{"data", eml_data},
                     {"result_status", QString::number(result_status)},
                     {"result", email_info},
                     {"result_cards", result_cards_param}});
  return GFEventResult::Deferred();
}

namespace {

auto DoSignEMLData(int channel, const QString& sign_key,
                   vmime::shared_ptr<vmime::message>& message,
                   const QByteArray& body_data, const GFEvent& event,
                   int& result_status, QString& result_detail,
                   QString& result_cards, QByteArray& eml_data) -> int {
  EMailMetaData meta_data;
  auto ret = GetEMLMetaData(message, meta_data);

  if (ret != 0) {
    event.Answer().Fail("Get MetaData From EML Data Failed");
    return -1;
  }

  uint32_t err;
  QString capsule_id;
  ret = SignEMLData(channel, sign_key, message, eml_data, err, capsule_id);

  if (ret == kFAILED || ret == kEML_FAILED) {
    event.Answer().Ok(
        {{"data", body_data},
         {"result_status", QString::number(-1)},
         {"result", ErrorHelper(ret, QString::fromUtf8(eml_data))}});
    return ret;
  }

  QByteArray info_json;
  // The Info variant, not the plain one: the structured description
  // and details are what let a FAILURE explain itself, and without
  // them the board can only fall back to "<operation> failed."
  const auto analysis = gf::sdk::AnalyseResult(
      GFModuleSdkContext(), channel, GF_GPG_ANALYSE_SIGN, err, capsule_id);
  result_status = analysis.status;
  result_detail = analysis.report;
  result_cards = analysis.cards;
  info_json = analysis.info_json.toUtf8();

  if (ret == kGPG_FAILED) {
    // decrypt failed. The analysis cards built above travel with it: a failure
    // is the outcome that most needs explaining, not the one to explain least.
    event.Answer().Ok({{"data", body_data},
                       {"result_status", QString::number(result_status)},
                       {"result", result_detail},
                       {"result_cards",
                        BuildResultCardsParam(QApplication::translate(
                                                  "EMailModule", "Sign E-Mail"),
                                              {}, result_cards, info_json)}});
    return ret;
  }

  if (ret != kSUCCESS) {
    event.Answer().Ok(
        {{"data", body_data},
         {"result_status", QString::number(-1)},
         {"result", ErrorHelper(ret, QString::fromUtf8(eml_data))}});
    return ret;
  }

  return kSUCCESS;
}

auto DoSignPlainText(int channel, const QString& sign_key,
                     const EMailMetaData& meta_data,
                     const QByteArray& body_data, const GFEvent& event,
                     int& result_status, QString& result_detail,
                     QString& result_cards, QByteArray& eml_data) -> int {
  uint32_t err;
  QString capsule_id;

  auto ret = SignPlainText(channel, sign_key, meta_data, body_data, eml_data,
                           err, capsule_id);
  if (ret == kFAILED || ret == kEML_FAILED) {
    event.Answer().Ok(
        {{"data", body_data},
         {"result_status", QString::number(-1)},
         {"result", ErrorHelper(ret, QString::fromUtf8(eml_data))}});
    return ret;
  }

  QByteArray info_json;
  // The Info variant, not the plain one: the structured description
  // and details are what let a FAILURE explain itself, and without
  // them the board can only fall back to "<operation> failed."
  const auto analysis = gf::sdk::AnalyseResult(
      GFModuleSdkContext(), channel, GF_GPG_ANALYSE_SIGN, err, capsule_id);
  result_status = analysis.status;
  result_detail = analysis.report;
  result_cards = analysis.cards;
  info_json = analysis.info_json.toUtf8();

  if (ret == kGPG_FAILED) {
    // decrypt failed. The analysis cards built above travel with it: a failure
    // is the outcome that most needs explaining, not the one to explain least.
    event.Answer().Ok({{"data", body_data},
                       {"result_status", QString::number(result_status)},
                       {"result", result_detail},
                       {"result_cards",
                        BuildResultCardsParam(QApplication::translate(
                                                  "EMailModule", "Sign E-Mail"),
                                              {}, result_cards, info_json)}});
    return ret;
  }

  if (ret != kSUCCESS) {
    event.Answer().Ok(
        {{"data", body_data},
         {"result_status", QString::number(-1)},
         {"result", ErrorHelper(ret, QString::fromUtf8(eml_data))}});
    return ret;
  }

  return kSUCCESS;
}

}  // namespace

auto OnEditTabTypeEmailOpSign(const GFEvent& event) -> GFEventResult {
  if (event.Str("body_data").isEmpty())
    return GFEventResult::Bad("body_data is empty");
  if (event.Str("channel").isEmpty())
    return GFEventResult::Bad("channel is empty");
  if (event.Str("sign_key").isEmpty())
    return GFEventResult::Bad("sign_key is empty");

  auto channel = event.Str("channel").toInt();
  auto sign_key = event.Str("sign_key");

  FLOG_DEBUG("eml sign key: %1", sign_key);

  auto body_data =
      QByteArray::fromBase64(QString(event.Str("body_data")).toLatin1());

  vmime::shared_ptr<vmime::message> message;
  if (CheckIfEMLMessage(body_data, message)) {
    int result_status = 0;
    QString result_detail;
    QString result_cards;
    QByteArray eml_data;
    if (DoSignEMLData(channel, sign_key, message, body_data, event,
                      result_status, result_detail, result_cards,
                      eml_data) != kSUCCESS) {
      return GFEventResult::Deferred();
    }

    event.Answer().Ok({{"data", eml_data},
                       {"result_status", QString::number(result_status)},
                       {"result", result_detail},
                       {"result_cards",
                        BuildResultCardsParam(QApplication::translate(
                                                  "EMailModule", "Sign E-Mail"),
                                              {}, result_cards)}});
    return GFEventResult::Deferred();
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
    return GFEventResult::Deferred();
  }

  event.Answer().Ok({{"data", eml_data},
                     {"result_status", QString::number(result_status)},
                     {"result", result_detail},
                     {"result_cards",
                      BuildResultCardsParam(
                          QApplication::translate("EMailModule", "Sign E-Mail"),
                          {BuildEMailHeaderCard(meta_data)}, result_cards)}});
  return GFEventResult::Deferred();
}

namespace {

auto DoEncryptEMLData(int channel, const QStringList& encrypt_keys,
                      const vmime::shared_ptr<vmime::message>& message,
                      const QByteArray& body_data, const GFEvent& event,
                      int& result_status, QString& result_detail,
                      QString& result_cards, QByteArray& eml_data) -> int {
  uint32_t err;
  QString capsule_id;
  auto ret = EncryptEMLData(channel, encrypt_keys, message, body_data, eml_data,
                            err, capsule_id);

  if (ret == kFAILED || ret == kEML_FAILED) {
    event.Answer().Ok(
        {{"data", DocumentUnchangedOnFailure(body_data)},
         {"result_status", QString::number(-1)},
         {"result", ErrorHelper(ret, QString::fromUtf8(eml_data))}});
    return ret;
  }

  QByteArray info_json;
  // The Info variant, not the plain one: the structured description
  // and details are what let a FAILURE explain itself, and without
  // them the board can only fall back to "<operation> failed."
  const auto analysis = gf::sdk::AnalyseResult(
      GFModuleSdkContext(), channel, GF_GPG_ANALYSE_ENCRYPT, err, capsule_id);
  result_status = analysis.status;
  result_detail = analysis.report;
  result_cards = analysis.cards;
  info_json = analysis.info_json.toUtf8();

  if (ret == kGPG_FAILED) {
    // encrypt failed. The analysis cards built above travel with it: a failure
    // is the outcome that most needs explaining, not the one to explain least.
    event.Answer().Ok(
        {{"data", DocumentUnchangedOnFailure(body_data)},
         {"result_status", QString::number(result_status)},
         {"result", result_detail},
         {"result_cards",
          BuildResultCardsParam(
              QApplication::translate("EMailModule", "Encrypt E-Mail"), {},
              result_cards, info_json)}});
    return ret;
  }

  return kSUCCESS;
}

auto DoEncryptPlainText(int channel, const QStringList& encrypt_keys,
                        const EMailMetaData& meta_data,
                        const QByteArray& body_data, const GFEvent& event,
                        int& result_status, QString& result_detail,
                        QString& result_cards, QByteArray& eml_data) -> int {
  uint32_t err;
  QString capsule_id;
  QByteArray plain_text_eml_data;
  auto ret = BuildPlainTextEML(meta_data, body_data, plain_text_eml_data);

  if (ret != kSUCCESS) {
    event.Answer().Ok(
        {{"data", DocumentUnchangedOnFailure(body_data)},
         {"result_status", QString::number(-1)},
         {"result", ErrorHelper(ret, QString::fromUtf8(eml_data))}});
    return ret;
  }

  ret = EncryptPlainText(channel, encrypt_keys, meta_data, plain_text_eml_data,
                         eml_data, err, capsule_id);

  if (ret == kFAILED || ret == kEML_FAILED) {
    event.Answer().Ok(
        {{"data", DocumentUnchangedOnFailure(body_data)},
         {"result_status", QString::number(-1)},
         {"result", ErrorHelper(ret, QString::fromUtf8(eml_data))}});
    return ret;
  }

  QByteArray info_json;
  // The Info variant, not the plain one: the structured description
  // and details are what let a FAILURE explain itself, and without
  // them the board can only fall back to "<operation> failed."
  const auto analysis = gf::sdk::AnalyseResult(
      GFModuleSdkContext(), channel, GF_GPG_ANALYSE_ENCRYPT, err, capsule_id);
  result_status = analysis.status;
  result_detail = analysis.report;
  result_cards = analysis.cards;
  info_json = analysis.info_json.toUtf8();

  if (ret == kGPG_FAILED) {
    // encrypt failed. The analysis cards built above travel with it: a failure
    // is the outcome that most needs explaining, not the one to explain least.
    event.Answer().Ok(
        {{"data", DocumentUnchangedOnFailure(body_data)},
         {"result_status", QString::number(result_status)},
         {"result", result_detail},
         {"result_cards",
          BuildResultCardsParam(
              QApplication::translate("EMailModule", "Encrypt E-Mail"), {},
              result_cards, info_json)}});
    return ret;
  }

  return kSUCCESS;
}
};  // namespace

auto OnEditTabTypeEmailOpEncrypt(const GFEvent& event) -> GFEventResult {
  if (event.Str("body_data").isEmpty())
    return GFEventResult::Bad("body_data is empty");
  if (event.Str("channel").isEmpty())
    return GFEventResult::Bad("channel is empty");
  if (event.Str("encrypt_keys").isEmpty())
    return GFEventResult::Bad("encrypt_keys is empty");

  auto channel = event.Str("channel").toInt();
  auto encrypt_keys = event.Str("encrypt_keys").split(';');

  FLOG_DEBUG("eml encrypt keys: %1", encrypt_keys.join(';'));

  auto body_data =
      QByteArray::fromBase64(QString(event.Str("body_data")).toLatin1());

  vmime::shared_ptr<vmime::message> message;
  if (CheckIfEMLMessage(body_data, message)) {
    QByteArray eml_data;
    int result_status = 0;
    QString result_detail;
    QString result_cards;
    if (DoEncryptEMLData(channel, encrypt_keys, message, body_data, event,
                         result_status, result_detail, result_cards,
                         eml_data) != kSUCCESS) {
      return GFEventResult::Deferred();
    }

    event.Answer().Ok(
        {{"data", eml_data},
         {"result", result_detail},
         {"result_status", QString::number(result_status)},
         {"result_cards",
          BuildResultCardsParam(
              QApplication::translate("EMailModule", "Encrypt E-Mail"),
              BuildRecipientCards(channel, encrypt_keys), result_cards)}});
    return GFEventResult::Deferred();
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
    return GFEventResult::Deferred();
  }

  auto meta_cards = BuildRecipientCards(channel, encrypt_keys);
  meta_cards.prepend(BuildEMailHeaderCard(meta_data));
  event.Answer().Ok({{"data", eml_data},
                     {"result", result_detail},
                     {"result_status", QString::number(result_status)},
                     {"result_cards", BuildResultCardsParam(
                                          QApplication::translate(
                                              "EMailModule", "Encrypt E-Mail"),
                                          meta_cards, result_cards)}});
  return GFEventResult::Deferred();
}

namespace {

auto DoEncryptSignEMLData(int channel, const QStringList& encrypt_keys,
                          const QString& sign_key,
                          vmime::shared_ptr<vmime::message>& message,
                          QByteArray& body_data, const GFEvent& event,
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
    event.Answer().Fail("Parse Signed Message Failed");
    return -1;
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
                            QByteArray& body_data, const GFEvent& event,
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
    event.Answer().Fail("Parse Signed Message Failed");
    return -1;
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

auto OnEditTabTypeEmailOpEncryptSign(const GFEvent& event) -> GFEventResult {
  if (event.Str("body_data").isEmpty())
    return GFEventResult::Bad("body_data is empty");
  if (event.Str("channel").isEmpty())
    return GFEventResult::Bad("channel is empty");
  if (event.Str("encrypt_keys").isEmpty())
    return GFEventResult::Bad("encrypt_keys is empty");
  if (event.Str("sign_key").isEmpty())
    return GFEventResult::Bad("sign_key is empty");

  auto channel = event.Str("channel").toInt();
  auto sign_key = event.Str("sign_key");
  auto encrypt_keys = event.Str("encrypt_keys").split(';');

  FLOG_DEBUG("eml encrypt keys: %1", encrypt_keys.join(';'));

  auto body_data =
      QByteArray::fromBase64(QString(event.Str("body_data")).toLatin1());

  vmime::shared_ptr<vmime::message> message;
  if (CheckIfEMLMessage(body_data, message)) {
    QByteArray eml_data;
    int result_status = 0;
    QString result_detail;
    QString result_cards;
    if (DoEncryptSignEMLData(channel, encrypt_keys, sign_key, message,
                             body_data, event, result_status, result_detail,
                             result_cards, eml_data) != kSUCCESS) {
      return GFEventResult::Deferred();
    }
    event.Answer().Ok(
        {{"data", eml_data},
         {"result", result_detail},
         {"result_status", QString::number(result_status)},
         {"result_cards",
          BuildResultCardsParam(
              QApplication::translate("EMailModule", "Encrypt and Sign E-Mail"),
              BuildRecipientCards(channel, encrypt_keys), result_cards)}});
    return GFEventResult::Deferred();
  }

  // Not a message yet: address it from the signing key to the encryption
  // keys, rather than stopping to ask for what the key list already says.
  const auto meta_data = EnvelopeFromKeys(channel, sign_key, encrypt_keys);

  QByteArray eml_data;
  int result_status = 0;
  QString result_detail;
  QString result_cards;
  QByteArray body_data_copy = body_data;

  if (DoEncryptSignPlainText(
          channel, encrypt_keys, sign_key, meta_data, body_data_copy, event,
          result_status, result_detail, result_cards, eml_data) != kSUCCESS) {
    return GFEventResult::Deferred();
  }

  auto meta_cards = BuildRecipientCards(channel, encrypt_keys);
  meta_cards.prepend(BuildEMailHeaderCard(meta_data));
  event.Answer().Ok(
      {{"data", eml_data},
       {"result", result_detail},
       {"result_status", QString::number(result_status)},
       {"result_cards",
        BuildResultCardsParam(
            QApplication::translate("EMailModule", "Encrypt and Sign E-Mail"),
            meta_cards, result_cards)}});
  return GFEventResult::Deferred();
}

namespace {

auto DoDecryptVerifyEMLData(int channel, const QByteArray& data,
                            const GFEvent& event, int& result_status,
                            QString& result_detail, QString& result_cards,
                            QByteArray& eml_data, QString& error_string,
                            EMailMetaData& meta_data,
                            QByteArray& decrypt_info_json,
                            EMailPostDecryptPlan& plan,
                            EMailVerificationResult& verification) -> int {
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

  // Filled by the verify alone, not by the decrypt. Both walk the same
  // plaintext and ExtractParts() only ever appends, so sharing one metadata
  // object listed every attachment twice.
  //
  // UTF-8, not Latin-1: this is the decrypted plaintext and the signature
  // inside it is checked against these exact bytes. See DoEncryptSignEMLData.
  if (DoVerifyEMLData(channel, eml_data, event, error_string, verification) !=
      kSUCCESS) {
    return -1;
  }

  MergeVerifiedMetaData(meta_data, verification.meta);

  result_status =
      WorseStatus(ReportStatusFor(verification.overall), result_status);
  result_detail = verification.report_detail + "\n" + result_detail;
  result_cards = MergeCardArrays(verification.report_cards, decrypt_cards);

  return kSUCCESS;
}
}  // namespace

auto OnEditTabTypeEmailOpDecryptVerify(const GFEvent& event) -> GFEventResult {
  if (event.Str("channel").isEmpty())
    return GFEventResult::Bad("channel is empty");
  if (event.Str("data").isEmpty()) return GFEventResult::Bad("data is empty");

  auto channel = event.Str("channel").toInt();
  auto data = QByteArray::fromBase64(QString(event.Str("data")).toLatin1());

  auto body_data =
      QByteArray::fromBase64(QString(event.Str("body_data")).toLatin1());

  QByteArray eml_data;
  EMailMetaData meta_data;
  QString error_string;
  int result_status = 0;
  QString result_detail;
  QString result_cards;

  QByteArray decrypt_info_json;
  auto plan = EMailPostDecryptPlan::kVERIFY;
  EMailVerificationResult verification;
  if (DoDecryptVerifyEMLData(channel, data, event, result_status, result_detail,
                             result_cards, eml_data, error_string, meta_data,
                             decrypt_info_json, plan,
                             verification) != kSUCCESS) {
    return GFEventResult::Deferred();
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
  email_info.append(QString("- %1: %2\n")
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
            .arg(QApplication::translate("EMailModule",
                                         "Declared Signature Hash (micalg)"))
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

  // The same reason the Verify board gives, when a verification actually
  // happened. A message that carried no signature is not a verification
  // outcome at all, and the note above already says so.
  const auto result_cards_param = BuildResultCardsParam(
      QApplication::translate("EMailModule", "Decrypt and Verify E-Mail"),
      dv_meta_cards, result_cards, verification.report_info_json,
      verified ? ReportDescriptionFor(verification) : QString());

  // callback
  event.Answer().Ok({{"data", eml_data},
                     {"result_status", QString::number(result_status)},
                     {"result", email_info},
                     {"result_cards", result_cards_param}});
  return GFEventResult::Deferred();
}

namespace {

/// File > Workspace > Mail Editor: an empty message, in a tab of its own.
struct NewMessage {
  static constexpr gf::cmd::Meta kMeta{
      GF_MODULE_ID ".new_message", GC_TR("Mail Editor"),
      GC_TR("Open a new text editor for email."), "", 0,
      gf::cmd::kNeedsGuiThread};
  using Args = gf::cmd::Unit;
  using Result = gf::cmd::Unit;
};

auto DoNewMessage(const gf::cmd::CommandContext& /*ctx*/,
                  const gf::cmd::Unit& /*args*/)
    -> gf::cmd::Outcome<gf::cmd::Unit> {
  Commands().Invoke<gf::cmd::host::DocumentNew>(
      {QStringLiteral("email"), QStringLiteral("untitled.eml")});
  return gf::cmd::Outcome<gf::cmd::Unit>::Success({});
}

/// Advanced > Open IMAP Controller.
struct OpenImapController {
  static constexpr gf::cmd::Meta kMeta{
      GF_MODULE_ID ".open_imap_controller", GC_TR("Open IMAP Controller"),
      GC_TR("Open IMAP Controller Dialog"), "", 0, gf::cmd::kNeedsGuiThread};
  using Args = gf::cmd::Unit;
  using Result = gf::cmd::Unit;
};

auto DoOpenImapController(const gf::cmd::CommandContext& /*ctx*/,
                          const gf::cmd::Unit& /*args*/)
    -> gf::cmd::Outcome<gf::cmd::Unit> {
  // An entry that always fails teaches people to ignore it, so an account
  // that cannot be browsed gets told why instead of an empty controller.
  if (!EMailImapController::HasUsableAccount()) {
    Commands().Invoke<gf::cmd::host::AppMessage>(
        {gf::cmd::host::AppMessage::Severity::kInfo,
         QCoreApplication::translate("GTrC", "No mail account"),
         QCoreApplication::translate(
             "GTrC",
             "Configure a mail account with IMAP enabled in Settings first.")});
    return gf::cmd::Outcome<gf::cmd::Unit>::Success({});
  }
  Commands().Invoke<gf::cmd::host::ViewOpen>(
      {gf::cmd::ViewRef{QStringLiteral(GF_MODULE_ID ".imap")}});
  return gf::cmd::Outcome<gf::cmd::Unit>::Success({});
}

}  // namespace

// The module's whole framework surface.
// The crypto operations stay events: the Host hands the module an e-mail
// document's bytes and takes the result back, which is data, not UI.
constexpr std::array<GFEventBinding, 7> kEvents = {{
    {"EDIT_TAB_TYPE_EMAIL_OP_DECRYPT", &OnEditTabTypeEmailOpDecrypt},
    {"EDIT_TAB_TYPE_EMAIL_OP_DECRYPT_VERIFY",
     &OnEditTabTypeEmailOpDecryptVerify},
    {"EDIT_TAB_TYPE_EMAIL_OP_ENCRYPT", &OnEditTabTypeEmailOpEncrypt},
    {"EDIT_TAB_TYPE_EMAIL_OP_ENCRYPT_SIGN", &OnEditTabTypeEmailOpEncryptSign},
    {"EDIT_TAB_TYPE_EMAIL_OP_SIGN", &OnEditTabTypeEmailOpSign},
    {"EDIT_TAB_TYPE_EMAIL_OP_VERIFY", &OnEditTabTypeEmailOpVerify},
    {"KEY_DATABASE_REFRESH_DONE", &OnKeyDatabaseRefreshDone},
}};

const std::array<gf::cmd::Binding, 2> kCommands = {
    gf::cmd::Bind<NewMessage, &DoNewMessage>(),
    gf::cmd::Bind<OpenImapController, &DoOpenImapController>(),
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
