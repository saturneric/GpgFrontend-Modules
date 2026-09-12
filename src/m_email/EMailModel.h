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
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

// vmime
// The test target defines this on the command line; the module build does
// not, so it is set here and guarded rather than assumed either way.
#ifndef VMIME_STATIC
#define VMIME_STATIC
#endif
#include <vmime/vmime.hpp>
// vmime extra
#include <vmime/contentDispositionField.hpp>
#include <vmime/contentTypeField.hpp>

/**
 * @brief One non-protocol part of a message.
 *
 * "Attachment" here means every part that is not the message body and not an
 * OpenPGP control part -- so it covers inline images as well as things the
 * user would call an attachment.
 */
struct EMailAttachment {
  QString filename;   ///< as it arrived; NOT safe to use as a path
  QString mime_type;  ///< e.g. "application/pdf"
  QString description;
  QString disposition;  ///< "attachment" or "inline"
  QByteArray data;      ///< decoded content

  /// application/pgp-keys, which the UI can offer to import rather than save.
  bool is_openpgp_key{false};

  /// Whether this part sits inside the multipart/signed subtree. Only those
  /// parts are covered by the signature; anything else arrived unauthenticated
  /// and must be shown as such, however trustworthy the rest of the message
  /// looks.
  bool inside_signed_part{false};
};

/**
 * @brief How far verification of this message's signed regions has got.
 *
 * Three states rather than a flag because a signed message with no results is
 * ambiguous between "nothing has looked at this yet" and "something looked and
 * found nothing to say", and those call for different words and different
 * offers. With a flag, the Security tab said "nothing has verified it yet" for
 * both, and a verification that came back empty left the user with no way to
 * know it had run and no way to ask again.
 */
enum class EMailVerifyState : uint8_t {
  kNOT_ATTEMPTED = 0,  ///< no verification has run against this document
  kATTEMPTED_EMPTY,    ///< one ran and produced no results
  kVERIFIED,           ///< one ran and produced results
};

/**
 * @brief What an OpenPGP message claims to be, judged from structure alone.
 *
 * Derived by walking content types; no cryptography and no network. It exists
 * to tell the user which action applies before they click anything, not to
 * decide whether a message is trustworthy -- that is what verify and decrypt
 * are for. Deliberately more forgiving than the strict RFC 3156 validator in
 * EMailBasicGpgOpera.cpp, which runs during verify and is allowed to refuse.
 */
enum class EMailSecurityState : uint8_t {
  kPLAIN = 0,         ///< no OpenPGP structure at all
  kSIGNED,            ///< multipart/signed
  kENCRYPTED,         ///< multipart/encrypted
  kSIGNED_ENCRYPTED,  ///< both present, in either nesting order
  kMALFORMED_PGP,     ///< claims OpenPGP but the structure does not hold up
};

/**
 * @brief One multipart/signed subtree: the exact bytes that are being signed.
 *
 * Structure only. Whether anything actually verifies over these bytes is not
 * knowable at parse time and is never stored here -- see EMailSignatureResult.
 */
struct EMailSignatureRegion {
  int region_id{};        ///< stable pre-order id, carried through verification
  int nesting_depth{};    ///< 0 == outermost region
  qint64 raw_offset{-1};  ///< the signed entity, as an offset into the ORIGINAL
  qint64 raw_length{0};   ///< bytes -- never into anything reserialized

  /// micalg as DECLARED in the Content-Type header. An assertion by whoever
  /// wrote the message, not cryptographic truth: compare it against the hash
  /// algorithm the signature itself reports and warn when they disagree.
  QString declared_micalg;

  /// EMailPart::index of the entity being signed and of the signature that
  /// signs it. Recorded so a nested region can be verified on its own rather
  /// than only the outermost one.
  /// The signed entity is itself a multipart/encrypted. The signature is
  /// then over CIPHERTEXT: it says who wrapped the encrypted blob, and says
  /// nothing at all about the plaintext that comes out of it. Structural
  /// containment inside a signed subtree is not authentication of the content
  /// the user ends up reading, and the two must never be shown as the same
  /// thing -- anyone can take someone else's ciphertext and sign it.
  bool covers_ciphertext_only{false};

  int signed_part_index{-1};
  int signature_part_index{-1};
};

/**
 * @brief One concrete verification result over one region.
 *
 * A region may carry more than one signature, so the relationship is
 * region -> 0..N results. Never assume 1:1, and never pair a result to a
 * region by list position: @ref region_id is the only valid join.
 */
struct EMailSignatureResult {
  int region_id{-1};
  QString fingerprint;
  QString pubkey_algo;
  QString hash_algo;  ///< what the signature reports, unlike declared_micalg
  QString uid;
  QDateTime sign_time;
  /// Mirrors GpgSigValidity, but defaults to -1 rather than 0: 0 is
  /// kFULLY_VALID, so an absent or unparseable field would otherwise read as a
  /// fully valid, fully trusted signature. -1 falls through to "unknown" and
  /// is not treated as good.
  int validity{-1};
  QStringList warnings;

  /// Set when the region's declared_micalg disagrees with hash_algo.
  bool micalg_mismatch{false};
};

/**
 * @brief One recipient an encrypted message was actually encrypted to.
 *
 * The encryption side of the picture. What the headers say is a separate
 * thing, and the two are compared rather than conflated.
 */
struct EMailRecipientInfo {
  QString uid;  ///< empty when the key is not in the local keyring
  QString fingerprint;
  QString key_id;
  QString pubkey_algo;
  bool key_found{false};
  /// True when key_id/pubkey_algo describe the primary key rather than the
  /// encryption subkey actually used. GnuPG does not report the subkey.
  bool algo_is_primary_key{false};
  /// The sender asked the engine to withhold the recipient key id, so this
  /// recipient is deliberately unidentifiable rather than missing.
  bool hidden{false};
};

/**
 * @brief How one address or encryption recipient lines up with the other side.
 *
 * Deliberately asymmetric. An addressed recipient who cannot decrypt is a real
 * problem: they were told the message is for them and it is not. A recipient
 * that does not appear in the headers is ordinary -- BCC, an archive or
 * self-encryption key, or a deliberately hidden recipient -- and reporting it
 * as suspicious would cry wolf on everyday, correct messages.
 */
enum class RecipientMatch : uint8_t {
  kMATCHED = 0,              ///< addressed and encrypted to
  kADDRESSED_NOT_ENCRYPTED,  ///< WARNING: they cannot read it
  kENCRYPTED_NOT_ADDRESSED,  ///< informational: BCC, archive or sender key
  kHIDDEN_RECIPIENT,         ///< the key id was withheld on purpose
};

/**
 * @brief One row of the recipient cross-check.
 */
struct EMailRecipientRow {
  QString address;  ///< from the headers; empty for an unaddressed recipient
  QString header_field;  ///< "To", "Cc" or "Bcc"; empty when not addressed
  RecipientMatch match{RecipientMatch::kMATCHED};
  EMailRecipientInfo info;  ///< encryption side; default when not encrypted to
};

/**
 * @brief One node of the parsed MIME tree.
 *
 * Offsets index the original message bytes, which is what makes byte-exact
 * signature coverage and a truthful raw-header view possible. Nothing here is
 * derived from a reserialization.
 */
struct EMailPart {
  int index{};  ///< stable pre-order id
  int depth{};

  QString content_type;  ///< lowercased, e.g. "text/plain"
  QString charset;
  QString disposition;
  QString filename;
  QString content_id;  ///< Content-ID with the angle brackets stripped
  QString transfer_encoding;

  /// Decoded, in the order they appeared. The raw form lives in the byte
  /// slice; this is the semantic view.
  QList<QPair<QString, QString>> header_fields;

  qint64 raw_offset{-1};  ///< whole part (headers + body)
  qint64 raw_length{0};
  qint64 body_offset{-1};  ///< body only, so raw_offset..body_offset is the
  qint64 body_length{0};   ///< exact header block as it was written
  qint64 decoded_size{0};

  /// Decoded content of a leaf part. Empty for multiparts and for the OpenPGP
  /// protocol parts. Holds plaintext once a message has been decrypted, so it
  /// must be zeroed by whatever wipes the view.
  QByteArray data;

  bool is_protocol_part{false};  ///< application/pgp-encrypted | pgp-signature
  bool is_openpgp_key{false};
  bool is_multipart{false};
  int alternative_group{-1};  ///< siblings under one multipart/alternative
  bool is_selected_body{false};

  /// Every signature REGION covering this part, outermost first. Empty means
  /// no region covers it. A part under nested signatures carries several ids.
  QList<int> covered_by_regions;

  QList<EMailPart> children;
};

/**
 * @brief How seriously to take one finding.
 *
 * Three levels, and the gap between them matters. Most of what this tool
 * notices is worth stating and not worth alarming over; reserving the top
 * level for things that are actually wrong is what keeps it meaningful.
 */
enum class EMailFindingLevel : uint8_t {
  kNOTE = 0,  ///< worth knowing, not a problem
  kWARN,      ///< worth checking before trusting or sending
  kRISK,      ///< actively wrong or deceptive
};

/**
 * @brief One thing noticed about a message.
 *
 * Every check behind these is local and offline: they read the message that is
 * already open and nothing else.
 */
struct EMailFinding {
  EMailFindingLevel level{EMailFindingLevel::kNOTE};
  QString title;
  QString detail;
};

/**
 * @brief Compose/envelope state.
 *
 * Deliberately NOT part of EMailMetaData: none of it is a property of the
 * serialized message. BCC is the load-bearing case -- it selects encryption
 * recipients and must never reach a header -- but the rule is general.
 */
struct EMailComposeState {
  QStringList bcc;  ///< encryption recipients; never emitted as a header
  QStringList extra_encrypt_keys;
};

/**
 * @brief Per-tab view/session state.
 *
 * Not envelope state and not message state: it says how this tab is currently
 * displaying things, nothing more. Never persisted, discarded with the tab.
 */
struct EMailViewState {
  bool remote_content_allowed{false};
};

struct EMailMetaData {
  // Basic MetaData
  QString from;
  QStringList to;
  QStringList cc;
  /// Blind recipients as they LITERALLY appeared in the source. Almost always
  /// empty: a correctly produced message does not carry this header at all.
  /// When it is present that is itself worth seeing, so it is parsed and
  /// shown -- but it is never written back. Blind recipients being composed
  /// live in EMailComposeState::bcc, which is not part of the message.
  QStringList bcc_header;
  QString subject;
  QDateTime datetime;
  QString micalg;
  QString reply_to;
  QString organization;

  /// Threading headers. Carried so a reply can point back at what it answers;
  /// this is per-message data, not a thread model -- nothing here tracks
  /// conversations or keeps state between messages.
  QString message_id;
  QString in_reply_to;
  QStringList references;

  // OpenPGP MetaData
  QString public_keys;
  QByteArray mime;

  /// Digest WE compute over the exact bytes a signature covers, so the user
  /// can confirm what was signed. This is not the signature's own hash
  /// algorithm -- see EMailSignatureRegion::declared_micalg for what the
  /// message claims, and EMailSignatureResult::hash_algo for what the
  /// signature actually reports.
  QString signed_entity_digest;
  QString signed_entity_digest_algo;

  /// Set when the bytes a signature covers are not in MIME canonical form,
  /// which means they are no longer the bytes that were signed. The usual
  /// cause is a program that rewrote the line endings while saving the
  /// message. It makes a verification fail in a way that looks exactly like a
  /// forgery, so it is worth reporting as the separate thing it is.
  bool signed_entity_non_canonical{false};

  QByteArray signature;
  QByteArray encrypted_data;

  /// Every multipart/signed subtree found while parsing, outermost first.
  /// Present whether or not anything has been verified yet.
  QList<EMailSignatureRegion> signature_regions;

  // Content
  QByteArray body;            ///< decoded text body, UTF-8
  QString body_content_type;  ///< e.g. "text/plain"
  QList<EMailAttachment> attachments;
};