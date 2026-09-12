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

// What octets actually reach GPG.
//
// A signature covers exact bytes. Everything the user is told -- "this is
// signed", the digest shown as "the bytes the signature covers", the body on
// screen -- is a claim about a specific byte range, and it is only true if
// that same range is what GPG was handed. These tests hold the module to that
// by recording the crypto boundary and comparing it with the slice the module
// says it verified.
//
// The failure they exist to prevent is not a crash. It is a GOOD verdict
// reported over a prefix of what the user can see.

#include <gtest/gtest.h>

#include <QByteArray>
#include <QCryptographicHash>

#include "EMailBasicGpgOpera.h"
#include "EMailCryptoRecorder.h"
#include "EMailHelper.h"

namespace {

using crypto_recorder::Get;
using crypto_recorder::Reset;

constexpr auto kBoundary = "sigboundary";

/// Builds a PGP/MIME multipart/signed message whose signed entity is exactly
/// @p entity_body, verbatim -- the caller controls every octet of it.
auto SignedMessage(const QByteArray& entity_body) -> QByteArray {
  QByteArray eml;
  eml += "From: Alice <alice@example.com>\r\n";
  eml += "To: Bob <bob@example.com>\r\n";
  eml += "Subject: signed\r\n";
  eml += "MIME-Version: 1.0\r\n";
  eml += QByteArray("Content-Type: multipart/signed; micalg=pgp-sha256; ") +
         "protocol=\"application/pgp-signature\"; boundary=\"" + kBoundary +
         "\"\r\n";
  eml += "\r\n";
  eml += QByteArray("--") + kBoundary + "\r\n";
  eml += entity_body;
  eml += QByteArray("\r\n--") + kBoundary + "\r\n";
  eml += "Content-Type: application/pgp-signature; name=\"signature.asc\"\r\n";
  eml += "\r\n";
  eml += "-----BEGIN PGP SIGNATURE-----\r\n";
  eml += "aGVsbG8=\r\n";
  eml += "-----END PGP SIGNATURE-----\r\n";
  eml += QByteArray("\r\n--") + kBoundary + "--\r\n";
  return eml;
}

/// The signed entity as the module's own parser locates it, so the expectation
/// is derived the same way the production slice is rather than recomputed by
/// hand.
auto SignedEntityOf(const QByteArray& eml) -> QByteArray {
  EMailPart root;
  QList<EMailSignatureRegion> regions;
  vmime::shared_ptr<vmime::message> message;
  EXPECT_TRUE(CheckIfEMLMessage(eml, message));
  EXPECT_EQ(ParseMimeTree(message, eml, root, regions), 0);
  EXPECT_EQ(regions.size(), 1);
  if (regions.isEmpty()) return {};
  return eml.mid(static_cast<int>(regions.first().raw_offset),
                 static_cast<int>(regions.first().raw_length));
}

class CryptoBoundaryTest : public ::testing::Test {
 protected:
  void SetUp() override { Reset(); }
};

// --- embedded NUL -----------------------------------------------------------

TEST_F(CryptoBoundaryTest, AnEmbeddedNulDoesNotTruncateWhatIsVerified) {
  QByteArray entity;
  entity += "Content-Type: text/plain; charset=utf-8\r\n";
  entity += "Content-Transfer-Encoding: 8bit\r\n";
  entity += "\r\n";
  entity += "visible before";
  entity += '\0';
  entity += "HIDDEN AFTER THE NUL";

  const auto eml = SignedMessage(entity);
  const auto expected = SignedEntityOf(eml);
  ASSERT_TRUE(expected.contains('\0'));

  EMailMetaData meta;
  QString error;
  gpgme_error_t err = 0;
  QString capsule;
  VerifyEMLData(0, eml, meta, error, err, capsule);

  ASSERT_EQ(Get().verify.size(), 1);
  // The whole point: every octet the signature is claimed to cover must have
  // been handed to GPG. A prefix means a GOOD verdict over bytes that are not
  // the bytes on screen.
  EXPECT_EQ(Get().verify.first().data, expected);
  EXPECT_TRUE(Get().verify.first().data.contains("HIDDEN AFTER THE NUL"));
}

TEST_F(CryptoBoundaryTest, AnEmbeddedNulSurvivesPerRegionVerification) {
  QByteArray entity;
  entity += "Content-Type: text/plain\r\n\r\n";
  entity += "before";
  entity += '\0';
  entity += "after";

  const auto eml = SignedMessage(entity);
  const auto expected = SignedEntityOf(eml);

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml, message));
  ASSERT_EQ(ParseMimeTree(message, eml, root, regions), 0);

  QList<EMailSignatureResult> results;
  VerifyEMLRegions(0, eml, root, regions, results);

  ASSERT_EQ(Get().verify.size(), 1);
  EXPECT_EQ(Get().verify.first().data, expected);
}

// --- non-UTF-8 --------------------------------------------------------------

TEST_F(CryptoBoundaryTest, IllFormedUtf8IsNotRewrittenBeforeVerification) {
  QByteArray entity;
  entity += "Content-Type: text/plain; charset=iso-8859-1\r\n";
  entity += "Content-Transfer-Encoding: 8bit\r\n";
  entity += "\r\n";
  // Lone 0xA9 / 0xFE / 0xFF: valid Latin-1, invalid UTF-8. A UTF-8 round trip
  // turns each into U+FFFD and changes the byte count, so the signature would
  // be checked over bytes that were never transmitted.
  entity += QByteArray("caf\xE9 \xA9 \xFE\xFF end", 16);

  const auto eml = SignedMessage(entity);
  const auto expected = SignedEntityOf(eml);

  EMailMetaData meta;
  QString error;
  gpgme_error_t err = 0;
  QString capsule;
  VerifyEMLData(0, eml, meta, error, err, capsule);

  ASSERT_EQ(Get().verify.size(), 1);
  EXPECT_EQ(Get().verify.first().data, expected);
  EXPECT_FALSE(Get().verify.first().data.contains("\xEF\xBF\xBD"))
      << "U+FFFD replacement character found: the bytes were re-encoded";
}

TEST_F(CryptoBoundaryTest, TheSignatureBlobItselfIsPassedVerbatim) {
  QByteArray entity = "Content-Type: text/plain\r\n\r\nhello";
  const auto eml = SignedMessage(entity);

  EMailMetaData meta;
  QString error;
  gpgme_error_t err = 0;
  QString capsule;
  VerifyEMLData(0, eml, meta, error, err, capsule);

  ASSERT_EQ(Get().verify.size(), 1);
  EXPECT_TRUE(
      Get().verify.first().signature.contains("-----BEGIN PGP SIGNATURE-----"));
  EXPECT_TRUE(
      Get().verify.first().signature.contains("-----END PGP SIGNATURE-----"));
}

// --- the digest the user is shown -------------------------------------------

TEST_F(CryptoBoundaryTest, TheDisplayedDigestCoversExactlyTheVerifiedBytes) {
  QByteArray entity;
  entity += "Content-Type: text/plain\r\n\r\n";
  entity += "shown";
  entity += '\0';
  entity += "also shown but not signed?";

  const auto eml = SignedMessage(entity);

  EMailMetaData meta;
  QString error;
  gpgme_error_t err = 0;
  QString capsule;
  VerifyEMLData(0, eml, meta, error, err, capsule);

  ASSERT_EQ(Get().verify.size(), 1);
  const auto verified_digest =
      QCryptographicHash::hash(Get().verify.first().data,
                               QCryptographicHash::Sha256)
          .toHex();

  // meta_data carries the digest the UI presents as "the bytes the signature
  // covers". If it is computed over a different range than GPG saw, the card
  // is telling the user something untrue.
  ASSERT_FALSE(meta.signed_entity_digest.isEmpty());
  EXPECT_EQ(meta.signed_entity_digest.toLower(),
            QString::fromLatin1(verified_digest).toLower());
}

// --- size ceiling -----------------------------------------------------------

TEST_F(CryptoBoundaryTest, ALargeEntityIsPassedWholeOrRefusedNeverTruncated) {
  QByteArray entity = "Content-Type: text/plain\r\n\r\n";
  entity += QByteArray(2 * 1024 * 1024, 'x');

  const auto eml = SignedMessage(entity);
  const auto expected = SignedEntityOf(eml);

  EMailMetaData meta;
  QString error;
  gpgme_error_t err = 0;
  QString capsule;
  const auto ret = VerifyEMLData(0, eml, meta, error, err, capsule);

  if (ret == kSUCCESS || !Get().verify.isEmpty()) {
    ASSERT_EQ(Get().verify.size(), 1);
    EXPECT_EQ(Get().verify.first().data.size(), expected.size());
    EXPECT_EQ(Get().verify.first().data, expected);
  }
}

// --- decrypt ----------------------------------------------------------------

TEST_F(CryptoBoundaryTest, DecryptedOutputWithAnEmbeddedNulIsNotTruncated) {
  QByteArray inner;
  inner += "Content-Type: text/plain\r\n";
  inner += "Content-Transfer-Encoding: 8bit\r\n";
  inner += "\r\n";
  inner += "plain";
  inner += '\0';
  inner += "tail";

  QByteArray eml;
  eml += "From: Alice <alice@example.com>\r\n";
  eml += "To: Bob <bob@example.com>\r\n";
  eml += "MIME-Version: 1.0\r\n";
  eml +=
      "Content-Type: multipart/encrypted; "
      "protocol=\"application/pgp-encrypted\"; boundary=\"encb\"\r\n";
  eml += "\r\n";
  eml += "--encb\r\n";
  eml += "Content-Type: application/pgp-encrypted\r\n\r\n";
  eml += "Version: 1\r\n";
  eml += "\r\n--encb\r\n";
  eml += "Content-Type: application/octet-stream\r\n\r\n";
  eml +=
      "-----BEGIN PGP MESSAGE-----\r\nc3R1Yg==\r\n"
      "-----END PGP MESSAGE-----\r\n";
  eml += "\r\n--encb--\r\n";

  Get().decrypt_output = inner;

  EMailMetaData meta;
  QByteArray out;
  gpgme_error_t err = 0;
  QString capsule;
  DecryptEMLData(0, eml, meta, out, err, capsule);

  ASSERT_EQ(Get().decrypt.size(), 1);
  // The ciphertext handed down must be the whole armored blob.
  EXPECT_TRUE(Get().decrypt.first().data.contains("BEGIN PGP MESSAGE"));
  // And the plaintext handed back must not have been cut at the NUL.
  EXPECT_TRUE(out.contains("tail"))
      << "decrypted plaintext was truncated at the embedded NUL";
}

// --- sign / encrypt ---------------------------------------------------------

TEST_F(CryptoBoundaryTest, SigningPassesTheBodyOctetsWhole) {
  EMailMetaData meta;
  meta.from = "alice@example.com";
  meta.to = QStringList{"bob@example.com"};
  meta.subject = "s";

  QByteArray body = "line one";
  body += '\0';
  body += "line two";

  QByteArray eml;
  gpgme_error_t err = 0;
  QString capsule;
  SignPlainText(0, "DEADBEEF", meta, body, eml, err, capsule);

  ASSERT_FALSE(Get().sign.isEmpty());
  // SignPlainText base64-encodes the body into the signed part, so the octets
  // are checked through that encoding rather than as literal text.
  EXPECT_TRUE(Get().sign.first().data.contains(body.toBase64()))
      << "the signed entity does not carry the body octets intact";
  EXPECT_EQ(Get().sign.first().sign_mode, 1) << "must be a detached signature";
}

TEST_F(CryptoBoundaryTest, EncryptingPassesTheBodyOctetsWhole) {
  EMailMetaData meta;
  meta.from = "alice@example.com";
  meta.to = QStringList{"bob@example.com"};
  meta.subject = "s";

  QByteArray body = "secret one";
  body += '\0';
  body += "secret two";

  QByteArray eml;
  gpgme_error_t err = 0;
  QString capsule;
  EncryptPlainText(0, QStringList{"DEADBEEF"}, meta, body, eml, err, capsule);

  ASSERT_FALSE(Get().encrypt.isEmpty());
  EXPECT_TRUE(Get().encrypt.first().data.contains("secret two"))
      << "the plaintext was cut at the embedded NUL before encryption";
}

}  // namespace
