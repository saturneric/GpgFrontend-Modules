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

#include <QDateTime>
#include <QString>

#include "EMailModel.h"

auto inline Q_SC(const std::string& s) -> QString {
  return QString::fromStdString(s);
}

/**
 * @brief A QString as vmime text, tagged UTF-8.
 *
 * vmime::text(std::string) assumes the *local* charset, so handing it UTF-8
 * bytes quietly replaces every non-ASCII character with '?'. Display names and
 * subjects must go through here so they survive RFC 2047 encoding.
 */
auto inline Q_TEXT(const QString& s) -> vmime::text {
  return {s.toStdString(), vmime::charsets::UTF_8};
}

/**
 * @brief Sink this translation unit reports diagnostics through.
 *
 * The MIME code is deliberately free of any GF SDK symbol so it can be linked
 * into a plain unit-test binary that has no module host behind it. Logging is
 * the one thing it did need from the SDK, so it is injected instead: the
 * module points this at its own logger during registration, and the tests
 * leave it at the default no-op.
 *
 * Never pass message content here -- sizes and digests only. See the note in
 * EMailBasicGpgOpera.cpp about what used to be logged.
 */
using MimeLogFn = void (*)(const QString&);

/**
 * @brief Points the diagnostic sink at @p fn. Passing nullptr restores the
 * default no-op sink.
 */
void SetMimeLogSink(MimeLogFn fn);

/**
 * @brief Reports one diagnostic through the current sink.
 */
void MimeLog(const QString& message);

/**
 * @brief
 *
 * @param prm_micalg_value
 * @return true
 * @return false
 */
auto IsValidMicalgFormat(const QString& prm_micalg_value) -> bool;

/**
 * @brief
 *
 * @param header
 * @param field_name
 * @return QString
 */
auto ExtractFieldValue(const vmime::shared_ptr<vmime::header>& header,
                       const QString& field_name) -> QString;

/**
 * @brief
 *
 * @param header
 * @param field_name
 * @return QString
 */
auto ExtractFieldValueText(const vmime::shared_ptr<vmime::header>& header,
                           const QString& field_name) -> QString;

/**
 * @brief
 *
 * @param header
 * @param field_name
 * @return QString
 */
auto ExtractFieldValueMailBox(const vmime::shared_ptr<vmime::header>& header,
                              const QString& field_name) -> QString;

/**
 * @brief
 *
 * @param header
 * @param field_name
 * @return QString
 */
auto ExtractFieldValueAddressList(
    const vmime::shared_ptr<vmime::header>& header, const QString& field_name)
    -> QString;

/**
 * @brief The addresses of a header field, one entry per mailbox.
 *
 * Prefer this to ExtractFieldValueAddressList() wherever the result is going
 * to be treated as a list: a display name may legitimately contain a comma, so
 * splitting the joined form back apart invents recipients.
 *
 * @param header
 * @param field_name
 * @return QStringList, empty when the field is absent
 */
auto ExtractFieldValueAddressListItems(
    const vmime::shared_ptr<vmime::header>& header, const QString& field_name)
    -> QStringList;

/**
 * @brief The named field, or nullptr when the header does not have it.
 *
 * vmime's own header::getField() creates and inserts a missing field, so it
 * cannot be used to ask whether a header is present without changing the
 * message. Every read of an optional field must come through here.
 *
 * @param header
 * @param field_name
 * @return vmime::shared_ptr<vmime::headerField>
 */
auto TryGetField(const vmime::shared_ptr<vmime::header>& header,
                 const QString& field_name)
    -> vmime::shared_ptr<vmime::headerField>;

/**
 * @brief
 *
 * @param header
 * @param field_name
 * @return QDateTime
 */
auto ExtractFieldValueDateTime(const vmime::shared_ptr<vmime::header>& header,
                               const QString& field_name) -> QDateTime;

/**
 * @brief
 *
 * @param input
 * @param name
 * @param email
 * @return true
 * @return false
 */
auto ParseEmailString(const QString& input, QString& name, QString& email)
    -> bool;

/**
 * @brief
 *
 * @param data
 * @param lineLength
 * @return QString
 */
auto EncodeBase64WithLineBreaks(const QByteArray& data, int lineLength = 76)
    -> QString;

/**
 * @brief
 *
 * @param data
 * @return true
 * @return false
 */
auto CheckIfEMLMessage(const QByteArray& data,
                       vmime::shared_ptr<vmime::message>& message) -> bool;

/**
 * @brief
 *
 * @param meta_data
 * @param eml_data
 * @return int
 */
auto BuildPlainTextEML(const EMailMetaData& meta_data,
                       const QByteArray& body_data, QString& eml_data) -> int;

/**
 * @brief Builds an EML with an optional set of attachments.
 *
 * With no attachments the output is a single text/plain part, byte for byte
 * what BuildPlainTextEML() has always produced. With attachments the message
 * becomes multipart/mixed with that text part first.
 *
 * @param meta_data headers
 * @param body_data body, UTF-8
 * @param attachments parts to attach, in order
 * @param eml_data receives the generated message, or the error text
 * @return 0 on success, -1 on a vmime error
 */
auto BuildMimeEML(const EMailMetaData& meta_data, const QByteArray& body_data,
                  const QList<EMailAttachment>& attachments, QString& eml_data)
    -> int;

/**
 * @brief
 *
 * @param body_data
 * @param meta_data
 * @return int
 */
auto GetEMLMetaData(vmime::shared_ptr<vmime::message>& message,
                    EMailMetaData& meta_data) -> int;

/**
 * @brief Limits applied when walking an untrusted message tree.
 *
 * Parsing runs synchronously on input that arrived from outside, so a message
 * can be shaped to cost far more than its size suggests -- deeply nested
 * multiparts, thousands of tiny parts, or parts whose decoded size dwarfs the
 * encoded one. These bounds are what makes it safe to accept messages larger
 * than the old blanket 1 MB refusal.
 */
struct EMailParseLimits {
  int max_depth{8};
  int max_parts{64};
  qint64 max_total_bytes{64LL * 1024 * 1024};
};

/**
 * @brief Collects the body and every attachment of a parsed message.
 *
 * Parts inside the multipart/signed subtree are marked as such, because only
 * those are covered by the signature.
 *
 * @param message parsed message
 * @param meta_data receives body, body_content_type and attachments
 * @param limits guards against a hostile message shape
 * @return 0 on success, -1 when a limit was exceeded
 */
auto ExtractParts(const vmime::shared_ptr<vmime::message>& message,
                  EMailMetaData& meta_data, const EMailParseLimits& limits = {})
    -> int;

/**
 * @brief Turns a filename from a message into one safe to write to disk.
 *
 * Attachment filenames are chosen by whoever sent the message, so they are
 * treated as hostile: path separators, "." and "..", absolute paths and drive
 * letters, control characters and Windows reserved device names are all
 * removed or replaced. The result is always a single path component, never
 * empty, and short enough for a filesystem.
 *
 * @param raw the filename as it arrived
 * @param mime_type used to pick an extension when nothing usable is left
 * @return a safe single-component filename
 */
auto SanitizeAttachmentFileName(const QString& raw,
                                const QString& mime_type = {}) -> QString;

/**
 * @brief Sanitized, collision-free names for a set of attachments.
 *
 * Two parts may legitimately carry the same filename. Numbering is assigned in
 * part order -- "foo.pdf", "foo (2).pdf" -- so the same message always yields
 * the same names. @p taken seeds the set with names already present in the
 * destination directory, so an existing file is never silently overwritten.
 *
 * @param attachments in part order
 * @param taken names already spoken for; matched case-insensitively
 * @return one name per attachment, in the same order
 */
auto UniqueAttachmentFileNames(const QList<EMailAttachment>& attachments,
                               const QStringList& taken = {}) -> QStringList;

/**
 * @brief The header for the inner part of a PGP/MIME encrypted message.
 *
 * Encryption wraps the original message as a part inside multipart/encrypted,
 * carrying its body over as a raw, still-encoded slice. Everything that says
 * how to decode that body -- every Content-* field -- must therefore travel
 * with it, alongside the headers that describe the message to a human.
 *
 * @param source header of the message being encrypted
 * @return the generated header block
 */
auto BuildInnerPartHeader(const vmime::shared_ptr<vmime::header>& source)
    -> QString;