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

#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QStringList>

#include "EMailModel.h"

/**
 * @brief A message that is ready to be submitted, and cannot change again.
 *
 * "Frozen" is the operative word. Serializing the workspace twice does not
 * produce the same bytes -- the Date header is regenerated from the clock and
 * MIME boundaries are re-randomised -- so a design that serialized once for
 * the preview and again for submission would send something the user never
 * saw, under a Message-ID nobody could later look up.
 *
 * So the bytes are produced exactly once, and everything downstream refers to
 * this object rather than going back to the view.
 */
struct EMailOutgoingMessage {
  /// The exact octets to submit. For a signed message these are the bytes as
  /// they arrived; nothing has re-encoded them.
  QByteArray eml;

  /// The Message-ID inside @ref eml, without angle brackets. Empty only when
  /// the message carried none and we refused to add one.
  QString message_id;

  /// Envelope sender, address only.
  QString envelope_from;

  /// Envelope recipients: To, Cc and Bcc, deduplicated, address only. Bcc is
  /// here and nowhere else -- it must never appear in @ref eml.
  QStringList envelope_rcpt;

  /// Recipients the user cannot see in the message itself, for the send
  /// confirmation. Informational only; the envelope is what gets used.
  QStringList blind_rcpt;

  /// Subject as it stands in @ref eml. For the confirm summary and nothing
  /// else -- no decision downstream may be made from it.
  QString subject;

  /// How many attachments the message carries. Summary only, as above, and 0
  /// on the byte-preserving path where counting would mean re-reading bytes
  /// that exist precisely so they are not touched.
  int attachment_count{0};

  [[nodiscard]] auto IsValid() const -> bool {
    return !eml.isEmpty() && !envelope_from.isEmpty() &&
           !envelope_rcpt.isEmpty();
  }
};
Q_DECLARE_METATYPE(EMailOutgoingMessage)

/// Why a message could not be prepared for sending.
enum class EMailFreezeResult : uint8_t {
  kOK = 0,
  kNO_SENDER,
  kNO_RECIPIENT,
  kSERIALIZE_FAILED,
};

/**
 * @brief Extract the address from "Name <addr@example.org>".
 *
 * An SMTP envelope carries an addr-spec and nothing else: passing a display
 * name through would produce a malformed RCPT TO.
 */
auto MailAddressOnly(const QString& mailbox) -> QString;

/**
 * @brief Normalize and deduplicate a recipient list.
 *
 * Deduplication happens after To, Cc and Bcc have been merged, so someone who
 * is both visibly addressed and blind-copied receives one message rather than
 * two -- and is not then described to the user as a blind recipient.
 */
auto MailDedupeAddresses(const QStringList& addresses) -> QStringList;

/**
 * @brief Mint a Message-ID for an outgoing message.
 *
 * Built from a random UUID and the sender's domain. Deliberately not vmime's
 * own generator, which folds in the local host name and would leak it into
 * every message.
 */
auto MailGenerateMessageId(const QString& sender) -> QString;

/**
 * @brief Read the Message-ID out of a serialized message.
 *
 * @return the value without angle brackets, or empty when there is none
 */
auto MailExtractMessageId(const QByteArray& eml) -> QString;

/**
 * @brief The Subject of @p eml, decoded, or empty when it carries none.
 *
 * Header block only, for the same reason MailExtractMessageId stops there: a
 * quoted reply further down contains lines that look exactly like headers.
 * The result is for showing to a person and never for matching.
 */
/**
 * @brief Whether @p mailbox could plausibly be an address.
 *
 * "Plausible", not "valid". RFC 5322 permits far more than anyone types, and a
 * checker strict enough to be called a validator would reject real addresses
 * -- which, on a Send button, means refusing to send mail that would have
 * arrived. So this only catches what is unambiguously broken: no local part,
 * no domain, no dot in the domain, or whitespace inside the address. A display
 * name around it is fine; only the addr-spec is examined.
 */
auto MailIsPlausibleAddress(const QString& mailbox) -> bool;

auto MailExtractSubject(const QByteArray& eml) -> QString;

/**
 * @brief Whether a workspace's stored bytes may be submitted verbatim.
 *
 * The rule, in one place, because getting it wrong is silent both ways: reuse
 * bytes that should have been rebuilt and the message goes out without the
 * Message-ID a Sent-folder check searches on; rebuild bytes that should have
 * been reused and a signature computed over exactly those octets is voided.
 *
 * @param source_is_original the bytes came from OUTSIDE -- loaded, or lifted
 *        out of a loaded message -- rather than from our own serializer. Bytes
 *        we wrote carry nothing worth preserving, however clean they are.
 * @param dirty the user has edited the document since those bytes were stored
 * @param forensic the document is locked for inspection and is never rebuilt
 * @param source_empty there are no stored bytes at all
 */
auto MailShouldReuseSource(bool source_is_original, bool dirty, bool forensic,
                           bool source_empty) -> bool;

/**
 * @brief Produce the final, unchanging form of a message about to be sent.
 *
 * Pure: no widgets, no network, no SDK. This is where every rule about
 * identity, byte preservation and envelopes lives, so that all of it can be
 * tested without constructing a single QWidget.
 *
 * Two paths, and the difference matters:
 *
 *  - @p existing_source is used verbatim when the view has no unsaved edits.
 *    Re-serializing a signed message would invalidate its signature, so a
 *    message that is merely being forwarded on must not be rebuilt. If those
 *    bytes carry no Message-ID we do not add one: that would change the signed
 *    octets. Sent-confirmation is then simply unavailable, which is reported
 *    rather than worked around.
 *  - Otherwise the message is built from @p meta, with a Message-ID pinned
 *    into it first so the value in the bytes is the one we keep.
 *
 * @param meta message headers as the user has them
 * @param compose envelope-only state; its Bcc never reaches the message
 * @param body the body text
 * @param attachments the attachments
 * @param existing_source the untouched document bytes, or empty to rebuild
 * @param out receives the frozen message
 * @return kOK, or why not
 */
auto FreezeOutgoing(const EMailMetaData& meta, const EMailComposeState& compose,
                    const QByteArray& body,
                    const QList<EMailAttachment>& attachments,
                    const QByteArray& existing_source,
                    EMailOutgoingMessage& out) -> EMailFreezeResult;

/**
 * @brief How well a folder matches "this is where sent mail goes".
 *
 * Split out from the IMAP code so the ranking can be reasoned about, and
 * tested, with no server in the picture.
 *
 * @param name the folder's last path component
 * @param special_use true when the server advertises it as \Sent
 * @param is_override true when the user named this folder explicitly
 * @return a score; higher wins, 0 means "not a candidate"
 */
auto RankSentCandidate(const QString& name, bool special_use, bool is_override)
    -> int;
