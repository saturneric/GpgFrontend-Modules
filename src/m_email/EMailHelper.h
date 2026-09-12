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
 * @brief The worse of two operation statuses.
 *
 * The host reads a status as one of three buckets: greater than zero is OK,
 * zero is a warning, and negative is a failure. A composite operation is only
 * as good as its worst leg, so the aggregate is the minimum -- the same rule
 * GpgResultAnalyse::setStatus applies within a single operation.
 */
auto WorseStatus(int a, int b) -> int;

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
 * @brief Parses a message into a tree of parts plus its signature regions.
 *
 * This is the backbone the flat views are built on. Every node records the
 * slice of @p raw it came from, so signature coverage and a raw header view
 * can be byte-exact rather than reconstructed -- nothing here is derived from
 * a reserialization.
 *
 * Leaf content is decoded once, into EMailPart::data, and the same parse
 * limits that guard ExtractParts() apply here: this walks input that arrived
 * from outside.
 *
 * @param message parsed message
 * @param raw the ORIGINAL bytes @p message was parsed from; offsets index it
 * @param root receives the tree
 * @param regions receives one entry per multipart/signed subtree, outermost
 *                first, each with a stable region_id
 * @param limits guards against a hostile message shape
 * @return 0 on success, -1 when a limit was exceeded
 */
auto ParseMimeTree(const vmime::shared_ptr<vmime::message>& message,
                   const QByteArray& raw, EMailPart& root,
                   QList<EMailSignatureRegion>& regions,
                   const EMailParseLimits& limits = {}) -> int;

/**
 * @brief The part of @p root that should be shown as the message body.
 *
 * Unlike the body rule inside ExtractParts(), which is kept byte-compatible
 * with what it has always done, this understands multipart/alternative: it
 * picks one member of an alternative set rather than whichever happened to be
 * walked first, and honours @p prefer_html within that set.
 *
 * @return a node owned by @p root, or nullptr when there is no body
 */
auto SelectBodyPart(const EMailPart& root, bool prefer_html = false)
    -> const EMailPart*;

/**
 * @brief What the message claims to be, from structure alone.
 *
 * No cryptography and no network -- this only has to be right enough to tell
 * the user which action applies. It is deliberately more forgiving than the
 * RFC 3156 validation performed during an actual verify.
 */
auto ClassifyOpenPGPStructure(const EMailPart& root,
                              const QList<EMailSignatureRegion>& regions)
    -> EMailSecurityState;

/**
 * @brief Every node of @p root, pre-order, as flat pointers.
 *
 * Convenience for views and tests that need to walk the tree without
 * recursing themselves.
 */
auto FlattenMimeTree(const EMailPart& root) -> QList<const EMailPart*>;

/**
 * @brief The exact header block of @p part, as it was written.
 *
 * Returns the slice of @p raw between the part's start and the start of its
 * body, so what the user reads as "raw" is what the sender actually sent --
 * header order, folding and encoding included. Never route this through vmime:
 * a reserialized header block is a different thing wearing the same name.
 *
 * @return the slice, or an empty array when the offsets are not usable
 */
auto RawHeaderBlock(const EMailPart& part, const QByteArray& raw) -> QByteArray;

/**
 * @brief One header field exactly as it appeared.
 *
 * @ref name is the field name as written; @ref value keeps the bytes of the
 * value, with folded continuation lines joined but otherwise untouched.
 */
struct EMailRawHeaderField {
  QString name;
  QByteArray value;
  /// The whole field as it stood in the message, continuation lines included.
  QByteArray raw_line;
};

/**
 * @brief Splits a raw header block into its fields, in order.
 *
 * Duplicates are preserved rather than merged: a repeated header is itself
 * worth seeing. Folding-aware -- a line starting with a space or a tab
 * continues the field above it.
 *
 * A line that is neither a continuation nor `name: value` is returned with an
 * empty @ref EMailRawHeaderField::name rather than dropped. A malformed header
 * is exactly the kind of thing someone reading this view is looking for.
 */
auto SplitRawHeaderFields(const QByteArray& block)
    -> QList<EMailRawHeaderField>;

/**
 * @brief Why a protected layer could not be lifted off a message.
 */
enum class EMailUnwrapResult : uint8_t {
  kOK = 0,
  kNOT_PROTECTED,  ///< nothing to lift out; @p out_eml is left untouched
  kNOT_SUPPORTED,  ///< the outermost layer is encrypted -- decrypt it first
  kMALFORMED,      ///< not an RFC 3156 shape, or the offsets are unusable
};

/**
 * @brief Lifts the signed entity out of the outermost multipart/signed of
 * @p root, discarding the signature part, and hands back what remains.
 *
 * Byte-copying, never reserializing. The signed entity's octets -- headers and
 * body -- are taken verbatim out of @p raw, so a signature NESTED inside that
 * entity still verifies over the result. Reserializing through vmime would
 * rewrite boundaries, header order and encodings, and break exactly that.
 *
 * Only the outermost layer is removed, and only when it is a signature. An
 * encrypted outermost layer yields kNOT_SUPPORTED rather than a guess: the
 * signed entity inside it is ciphertext this function cannot see.
 *
 * The outer message's own headers are kept exactly as written, minus every
 * Content-* field, which the inner entity supplies instead -- the mirror of
 * BuildInnerPartHeader().
 *
 * @param root the parsed tree of @p raw
 * @param raw the ORIGINAL bytes whose offsets @p root indexes
 * @param out_eml receives the unwrapped message; only written on kOK
 */
auto UnwrapProtectedLayer(const EMailPart& root, const QByteArray& raw,
                          QByteArray& out_eml) -> EMailUnwrapResult;

/**
 * @brief Examines a message for deceptive or ambiguous structure.
 *
 * Everything here is a local, offline reading of the message already in hand:
 * header fields that contradict each other, addresses built to be misread, and
 * MIME shapes that different readers would resolve differently.
 *
 * These are observations, not verdicts. A finding says what was noticed and
 * leaves the judgement to the person reading it, because most of these
 * patterns have innocent explanations and a tool that cries wolf gets ignored
 * exactly when it is right.
 *
 * @param meta parsed headers
 * @param root the parsed tree
 * @param regions signature regions found in @p root
 * @param raw the original document, needed to read the signed bytes
 *            themselves; omit it to skip the checks that require them
 */
auto InspectMessage(const EMailMetaData& meta, const EMailPart& root,
                    const QList<EMailSignatureRegion>& regions,
                    const QByteArray& raw = {}) -> QList<EMailFinding>;

/**
 * @brief Whether @p bytes contain a line feed that is not part of a CRLF pair.
 *
 * RFC 3156 signs the entity in MIME canonical form, in which every line ends
 * CRLF. A bare LF inside bytes that are supposed to be signed therefore means
 * they are no longer the bytes that were signed: something rewrote them
 * afterwards, and saving or copying a message through a tool that normalises
 * line endings is the everyday way that happens.
 *
 * Worth detecting on its own, because the resulting verification failure is
 * indistinguishable from a forgery to anyone reading the verdict, and no
 * amount of importing the sender's key will ever change it.
 */
auto HasBareLineFeeds(const QByteArray& bytes) -> bool;

/**
 * @brief Whether @p address is built to be read as a different one.
 *
 * Catches the two cheap tricks: a domain whose Unicode form differs from its
 * punycode form (so it can be drawn to look like a familiar name), and a
 * domain mixing scripts, which almost never happens by accident.
 */
auto LooksLikeSpoofedAddress(const QString& address) -> bool;

/**
 * @brief What to check before a message leaves.
 *
 * The counterpart of InspectMessage for the sending direction: unsigned parts,
 * blind recipients that would leak, and anything about the message that the
 * sender is unlikely to have intended.
 *
 * @param meta the message about to be written
 * @param root its parsed tree
 * @param regions signature regions found in @p root
 * @param compose_bcc blind recipients being composed to
 */
auto PreflightMessage(const EMailMetaData& meta, const EMailPart& root,
                      const QList<EMailSignatureRegion>& regions,
                      const QStringList& compose_bcc) -> QList<EMailFinding>;

/**
 * @brief Which kind of new message to derive from an existing one.
 */
enum class EMailReplyMode : uint8_t {
  kREPLY = 0,  ///< back to the sender alone
  kREPLY_ALL,  ///< sender plus everyone else who was addressed
  kFORWARD,    ///< to nobody yet, carrying the original along
};

/**
 * @brief Derives a new message from @p source.
 *
 * This generates a fresh document and nothing else: no thread is tracked, no
 * mailbox state is touched, and @p source is not modified. The threading
 * headers are filled in because a reply that does not point at what it answers
 * is broken for the recipient, not because anything here models a conversation.
 *
 * @p self_address, when given, is removed from the derived recipients so a
 * reply-all does not address the sender back to themselves.
 *
 * Attachments are carried over for a forward only: a reply that silently
 * re-sends the original's files would be a surprise, and often a leak.
 *
 * @param source the message being answered or passed on
 * @param mode which derivation to perform
 * @param self_address the user's own address, or empty
 * @param out receives the derived headers
 */
void BuildDerivedMetaData(const EMailMetaData& source, EMailReplyMode mode,
                          const QString& self_address, EMailMetaData& out);

/**
 * @brief The quoted form of @p source's body for a reply or forward.
 *
 * A reply quotes with "> " and an attribution line; a forward reproduces the
 * original's own headers above the text, which is what makes a forwarded
 * message readable on its own.
 */
auto BuildQuotedBody(const EMailMetaData& source, EMailReplyMode mode)
    -> QByteArray;

/**
 * @brief The bare e-mail address inside a UID or address string.
 *
 * Accepts "Name (Comment) <a@b>" as well as a bare "a@b". Returns an empty
 * string when there is nothing address-shaped to take.
 */
auto AddressOfUid(const QString& uid) -> QString;

/**
 * @brief Decodes the signatures in a GFAnalyse*ResultInfoByCapsule payload.
 *
 * @p region_id is stamped onto every result the call produced. The caller
 * knows which region it just verified, so the association is made where it is
 * unambiguous rather than inferred afterwards from list positions -- signature
 * count, region count and emission order are all independent of each other.
 *
 * @param info_json the `info_json` out-param of the analyse call
 * @param region_id the region whose bytes were verified, or -1 when the
 *                  verification was not tied to a MIME region
 * @return one entry per reported signature, in the order reported
 */
auto ParseSignatureResults(const QByteArray& info_json, int region_id)
    -> QList<EMailSignatureResult>;

/**
 * @brief Decodes the recipients in a GFAnalyse*ResultInfoByCapsule payload.
 */
auto ParseRecipientInfos(const QByteArray& info_json)
    -> QList<EMailRecipientInfo>;

/**
 * @brief Flags results whose hash algorithm disagrees with what @p region
 * declared.
 *
 * micalg is written by whoever composed the message; the hash algorithm in the
 * signature is what was actually used. A disagreement is worth showing rather
 * than resolving silently in favour of either one.
 */
void CheckMicalgAgreement(QList<EMailSignatureResult>& results,
                          const EMailSignatureRegion& region);

/**
 * @brief Cross-checks the addressed recipients against the encryption ones.
 *
 * Asymmetric on purpose -- see RecipientMatch. Addressed recipients are
 * reported first, in header order, followed by any encryption recipients that
 * matched no address.
 *
 * @param to, cc, bcc addresses taken from the message
 * @param encrypted recipients the message was really encrypted to
 */
auto MatchRecipients(const QStringList& to, const QStringList& cc,
                     const QStringList& bcc,
                     const QList<EMailRecipientInfo>& encrypted)
    -> QList<EMailRecipientRow>;

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

/**
 * @brief Removes from @p message, in place, everything a previous Sign added.
 *
 * Signing takes the message exactly as it stands, so signing a message that is
 * already signed produces a second signature OVER the first one and its
 * wrapper, and attaches a second copy of the signer's public key. Do it a few
 * times and the message is a stack of wrappers with every key ever used still
 * in it, the oldest sitting ahead of the newest. "Sign it with this key
 * instead" is what a user means by re-signing, so the previous layer comes off
 * before the new one goes on.
 *
 * The signed entity is taken from the parsed message rather than the original
 * octets, which would normally break byte-exactness. It is safe here and only
 * here: the sign path reserializes the whole message anyway, and the signature
 * being discarded is the one that covered those bytes.
 *
 * A public key part the USER attached is never removed -- only the one Sign
 * writes itself, which it marks with its own Content-Description.
 *
 * @return whether anything was removed
 */
auto StripPreviousSignature(const vmime::shared_ptr<vmime::message>& message)
    -> bool;