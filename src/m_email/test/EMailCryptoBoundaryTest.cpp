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
#include <cstdint>
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

  EMailVerificationResult verification;
  QString error;
  VerifyEMLMessage(0, eml, verification, error);

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

  EMailVerificationResult verification;
  QString error;
  VerifyEMLMessage(0, eml, verification, error);

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

  EMailVerificationResult verification;
  QString error;
  VerifyEMLMessage(0, eml, verification, error);

  ASSERT_EQ(Get().verify.size(), 1);
  EXPECT_EQ(Get().verify.first().data, expected);
  EXPECT_FALSE(Get().verify.first().data.contains("\xEF\xBF\xBD"))
      << "U+FFFD replacement character found: the bytes were re-encoded";
}

TEST_F(CryptoBoundaryTest, TheSignatureBlobItselfIsPassedVerbatim) {
  QByteArray entity = "Content-Type: text/plain\r\n\r\nhello";
  const auto eml = SignedMessage(entity);

  EMailVerificationResult verification;
  QString error;
  VerifyEMLMessage(0, eml, verification, error);

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

  EMailVerificationResult verification;
  QString error;
  VerifyEMLMessage(0, eml, verification, error);

  ASSERT_EQ(Get().verify.size(), 1);
  const auto verified_digest =
      QCryptographicHash::hash(Get().verify.first().data,
                               QCryptographicHash::Sha256)
          .toHex();

  // meta_data carries the digest the UI presents as "the bytes the signature
  // covers". If it is computed over a different range than GPG saw, the card
  // is telling the user something untrue.
  ASSERT_FALSE(verification.meta.signed_entity_digest.isEmpty());
  EXPECT_EQ(verification.meta.signed_entity_digest.toLower(),
            QString::fromLatin1(verified_digest).toLower());
}

// --- size ceiling -----------------------------------------------------------

TEST_F(CryptoBoundaryTest, ALargeEntityIsPassedWholeOrRefusedNeverTruncated) {
  QByteArray entity = "Content-Type: text/plain\r\n\r\n";
  entity += QByteArray(2 * 1024 * 1024, 'x');

  const auto eml = SignedMessage(entity);
  const auto expected = SignedEntityOf(eml);

  EMailVerificationResult verification;
  QString error;
  const auto ret = VerifyEMLMessage(0, eml, verification, error);

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
  uint32_t err = 0;
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
  uint32_t err = 0;
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
  uint32_t err = 0;
  QString capsule;
  EncryptPlainText(0, QStringList{"DEADBEEF"}, meta, body, eml, err, capsule);

  ASSERT_FALSE(Get().encrypt.isEmpty());
  EXPECT_TRUE(Get().encrypt.first().data.contains("secret two"))
      << "the plaintext was cut at the embedded NUL before encryption";
}

TEST_F(CryptoBoundaryTest, AnOversizedDecryptedPlaintextIsRefused) {
  // The ceiling on the way in bounds the CIPHERTEXT. An OpenPGP compressed
  // packet a few megabytes long expands to gigabytes, so the expansion has to
  // be refused on its own terms rather than trusted because its input was
  // small.
  QByteArray eml;
  eml += "From: Alice <alice@example.com>\r\n";
  eml += "To: Bob <bob@example.com>\r\n";
  eml += "MIME-Version: 1.0\r\n";
  eml +=
      "Content-Type: multipart/encrypted; "
      "protocol=\"application/pgp-encrypted\"; boundary=\"encb\"\r\n";
  eml += "\r\n--encb\r\n";
  eml += "Content-Type: application/pgp-encrypted\r\n\r\nVersion: 1\r\n";
  eml += "\r\n--encb\r\n";
  eml += "Content-Type: application/octet-stream\r\n\r\n";
  eml +=
      "-----BEGIN PGP MESSAGE-----\r\nc3R1Yg==\r\n"
      "-----END PGP MESSAGE-----\r\n";
  eml += "\r\n--encb--\r\n";

  // A small ciphertext that "decrypts" to more than the ceiling allows.
  Get().decrypt_output = QByteArray(kMaxParseInputBytes + 1, 'x');

  EMailMetaData meta;
  QByteArray out;
  uint32_t err = 0;
  QString capsule;
  const auto ret = DecryptEMLData(0, eml, meta, out, err, capsule);

  EXPECT_EQ(ret, kEML_FAILED);
  EXPECT_LT(out.size(), kMaxParseInputBytes)
      << "the oversized plaintext was carried forward anyway";
}

// --- bounded verification work ----------------------------------------------

namespace {

/// A message carrying @p count sibling multipart/signed regions.
auto ManySignedRegions(int count) -> QByteArray {
  QByteArray eml;
  eml += "From: Alice <alice@example.com>\r\n";
  eml += "To: Bob <bob@example.com>\r\n";
  eml += "Subject: many\r\n";
  eml += "MIME-Version: 1.0\r\n";
  eml += "Content-Type: multipart/mixed; boundary=\"outer\"\r\n\r\n";

  for (int i = 0; i < count; ++i) {
    const auto b = QByteArray("sig") + QByteArray::number(i);
    eml += "--outer\r\n";
    eml +=
        "Content-Type: multipart/signed; micalg=pgp-sha256; "
        "protocol=\"application/pgp-signature\"; boundary=\"" +
        b + "\"\r\n\r\n";
    eml += "--" + b + "\r\n";
    eml += "Content-Type: text/plain\r\n\r\npart\r\n";
    eml += "--" + b + "\r\n";
    eml += "Content-Type: application/pgp-signature\r\n\r\n";
    eml +=
        "-----BEGIN PGP SIGNATURE-----\r\naGVsbG8=\r\n"
        "-----END PGP SIGNATURE-----\r\n";
    eml += "--" + b + "--\r\n";
  }
  eml += "--outer--\r\n";
  return eml;
}

}  // namespace

TEST_F(CryptoBoundaryTest, VerificationStopsAtTheRegionBudget) {
  // Every region costs a blocking GPG call on the GUI thread, with no timeout
  // and no cancel. Without a cap, a message decides how long the application
  // stops responding for.
  const auto raw = ManySignedRegions(kMaxVerifiedRegions + 12);

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(raw, message));
  ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0);
  ASSERT_GT(regions.size(), kMaxVerifiedRegions)
      << "the fixture did not produce enough regions to test the cap";

  EMailVerificationResult verification;
  QString error;
  ASSERT_EQ(VerifyEMLMessage(0, raw, verification, error), kSUCCESS);

  EXPECT_LE(verification.verdicts.size(), kMaxVerifiedRegions);
  EXPECT_LE(Get().verify.size(), kMaxVerifiedRegions)
      << "more GPG calls were made than the budget allows";
}

TEST_F(CryptoBoundaryTest, AnOrdinaryMessageIsNotAffectedByTheBudget) {
  // The cap must be well clear of anything real: the worst legitimate case in
  // the corpus is a nested signature, i.e. two regions.
  const auto raw = ManySignedRegions(2);

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(raw, message));
  ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0);

  EMailVerificationResult verification;
  QString error;
  ASSERT_EQ(VerifyEMLMessage(0, raw, verification, error), kSUCCESS);

  EXPECT_EQ(verification.verdicts.size(), regions.size())
      << "a normal message was cut short";
}

TEST_F(CryptoBoundaryTest, EachRegionIsVerifiedOnItsOwnExactBytes) {
  // The cap must not disturb which bytes go with which region.
  const auto raw = ManySignedRegions(3);

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(raw, message));
  ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0);

  EMailVerificationResult verification;
  QString error;
  ASSERT_EQ(VerifyEMLMessage(0, raw, verification, error), kSUCCESS);

  ASSERT_EQ(Get().verify.size(), regions.size());
  for (int i = 0; i < regions.size(); ++i) {
    const auto expected = raw.mid(static_cast<int>(regions[i].raw_offset),
                                  static_cast<int>(regions[i].raw_length));
    EXPECT_EQ(Get().verify[i].data, expected)
        << "region " << i << " was verified on the wrong bytes";
  }
}

}  // namespace

// --- decrypt then verify -----------------------------------------------------
//
// An encrypted message that is not signed is the ordinary case. The operation
// used to route its plaintext into VerifyEMLData() unconditionally, which
// refuses anything that is not multipart/signed -- and that refusal was
// reported as a failure of the whole operation, with no data on the callback,
// so the host discarded the plaintext the user had just decrypted.

namespace {

/// A PGP/MIME encrypted message. The ciphertext is a stub; what comes back out
/// of the fake engine is whatever the test put in `decrypt_output`.
auto EncryptedMessage() -> QByteArray {
  QByteArray eml;
  eml += "From: Alice <alice@example.com>\r\n";
  eml += "To: Bob <bob@example.com>\r\n";
  eml += "Subject: ...\r\n";
  eml += "MIME-Version: 1.0\r\n";
  eml +=
      "Content-Type: multipart/encrypted; "
      "protocol=\"application/pgp-encrypted\"; boundary=\"encb\"\r\n";
  eml += "\r\n--encb\r\n";
  eml += "Content-Type: application/pgp-encrypted\r\n\r\n";
  eml += "Version: 1\r\n";
  eml += "\r\n--encb\r\n";
  eml += "Content-Type: application/octet-stream\r\n\r\n";
  eml +=
      "-----BEGIN PGP MESSAGE-----\r\nc3R1Yg==\r\n"
      "-----END PGP MESSAGE-----\r\n";
  eml += "\r\n--encb--\r\n";
  return eml;
}

/// An ordinary, unsigned message carrying one attachment.
auto UnsignedInnerMessage() -> QByteArray {
  QByteArray inner;
  inner += "From: Alice <alice@example.com>\r\n";
  inner += "To: Bob <bob@example.com>\r\n";
  inner += "Subject: the real subject\r\n";
  inner += "MIME-Version: 1.0\r\n";
  inner += "Content-Type: multipart/mixed; boundary=\"inb\"\r\n";
  inner += "\r\n--inb\r\n";
  inner += "Content-Type: text/plain; charset=utf-8\r\n\r\n";
  inner += "the secret\r\n";
  inner += "--inb\r\n";
  inner +=
      "Content-Type: application/pdf; name=\"report.pdf\"\r\n"
      "Content-Disposition: attachment; filename=\"report.pdf\"\r\n\r\n";
  inner += "PDFBYTES\r\n";
  inner += "--inb--\r\n";
  return inner;
}

}  // namespace

TEST_F(CryptoBoundaryTest, VerifyRefusesAnUnsignedMessage) {
  // The reason the old decrypt-and-verify lost the plaintext. This is correct
  // behaviour for VerifyEMLData on its own -- the mistake was treating it as a
  // failure of the whole operation.
  EMailVerificationResult verification;
  QString error;

  EXPECT_EQ(VerifyEMLMessage(0, UnsignedInnerMessage(), verification, error),
            kEML_FAILED);
}

TEST_F(CryptoBoundaryTest, DecryptOfAnUnsignedMessageKeepsThePlaintext) {
  Get().decrypt_output = UnsignedInnerMessage();

  EMailMetaData meta;
  QByteArray out;
  uint32_t err = 0;
  QString capsule;

  ASSERT_EQ(DecryptEMLData(0, EncryptedMessage(), meta, out, err, capsule),
            kSUCCESS);

  // The plaintext survives, and the operation must not go on to call the
  // top-level verify with it.
  EXPECT_TRUE(out.contains("the secret"));
  EXPECT_EQ(PlanVerifyAfterDecrypt(out), EMailPostDecryptPlan::kNOT_SIGNED);

  // One walk, one listing. Handing this same object to a verify afterwards was
  // what listed the attachment twice.
  ASSERT_EQ(meta.attachments.size(), 1);
  EXPECT_EQ(meta.attachments.first().filename, QString("report.pdf"));
}

TEST_F(CryptoBoundaryTest, AVerifyAfterADecryptListsEachAttachmentOnce) {
  // The signed case: decrypt fills one object, verify fills its own, and the
  // merge joins them. Both walked the same bytes, so a concatenation here is
  // exactly the duplication this replaced.
  QByteArray inner;
  inner += "From: Alice <alice@example.com>\r\n";
  inner += "MIME-Version: 1.0\r\n";
  inner +=
      "Content-Type: multipart/signed; micalg=pgp-sha256; "
      "protocol=\"application/pgp-signature\"; boundary=\"sb\"\r\n";
  inner += "\r\n--sb\r\n";
  inner += "Content-Type: multipart/mixed; boundary=\"mb\"\r\n";
  inner += "\r\n--mb\r\n";
  inner += "Content-Type: text/plain\r\n\r\ninner body\r\n";
  inner += "--mb\r\n";
  inner +=
      "Content-Type: application/pdf; name=\"a.pdf\"\r\n"
      "Content-Disposition: attachment; filename=\"a.pdf\"\r\n\r\n";
  inner += "BYTES\r\n";
  inner += "--mb--\r\n";
  inner += "\r\n--sb\r\n";
  inner += "Content-Type: application/pgp-signature\r\n\r\n";
  inner += "-----BEGIN PGP SIGNATURE-----\r\nx\r\n";
  inner += "-----END PGP SIGNATURE-----\r\n";
  inner += "\r\n--sb--\r\n";

  Get().decrypt_output = inner;

  EMailMetaData decrypted;
  QByteArray out;
  uint32_t err = 0;
  QString capsule;
  ASSERT_EQ(DecryptEMLData(0, EncryptedMessage(), decrypted, out, err, capsule),
            kSUCCESS);
  ASSERT_EQ(PlanVerifyAfterDecrypt(out), EMailPostDecryptPlan::kVERIFY);

  EMailVerificationResult verification;
  QString error;
  ASSERT_EQ(VerifyEMLMessage(0, out, verification, error), kSUCCESS);

  const auto before = decrypted.attachments.size();
  MergeVerifiedMetaData(decrypted, verification.meta);

  EXPECT_EQ(decrypted.attachments.size(), before);
  ASSERT_EQ(decrypted.attachments.size(), 1);
  EXPECT_EQ(decrypted.attachments.first().filename, QString("a.pdf"));

  // And the signature side arrived.
  EXPECT_FALSE(decrypted.signed_entity_digest.isEmpty());
  EXPECT_EQ(decrypted.micalg, QString("pgp-sha256"));
}

// --- transfer encoding ------------------------------------------------------
//
// A MIME part carries a Content-Transfer-Encoding, and the OpenPGP payload is
// what is left AFTER undoing it. Reading the part with body::generate() yields
// the wire form instead -- base64 text where the signature should be -- so a
// perfectly good signature fails as bad data and the user is shown a
// verification failure indistinguishable from a forgery.

namespace {

constexpr auto kArmoredSignature =
    "-----BEGIN PGP SIGNATURE-----\r\n"
    "\r\n"
    "iQEzBAABCgAdFiEE\r\n"
    "=ArMr\r\n"
    "-----END PGP SIGNATURE-----\r\n";

constexpr auto kArmoredMessage =
    "-----BEGIN PGP MESSAGE-----\r\n"
    "\r\n"
    "hQEMAwAAAAAAAAAA\r\n"
    "=CiPh\r\n"
    "-----END PGP MESSAGE-----\r\n";

/// A signed message whose SIGNATURE part is transfer-encoded.
auto SignedMessageWithEncodedSignature(const QByteArray& cte,
                                       const QByteArray& encoded_body)
    -> QByteArray {
  QByteArray eml;
  eml += "From: Alice <alice@example.com>\r\n";
  eml += "To: Bob <bob@example.com>\r\n";
  eml += "MIME-Version: 1.0\r\n";
  eml += QByteArray("Content-Type: multipart/signed; micalg=pgp-sha256; ") +
         "protocol=\"application/pgp-signature\"; boundary=\"" + kBoundary +
         "\"\r\n";
  eml += "\r\n";
  eml += QByteArray("--") + kBoundary + "\r\n";
  eml += "Content-Type: text/plain; charset=utf-8\r\n";
  eml += "\r\n";
  eml += "signed body\r\n";
  eml += QByteArray("\r\n--") + kBoundary + "\r\n";
  eml += "Content-Type: application/pgp-signature; name=\"signature.asc\"\r\n";
  eml += "Content-Transfer-Encoding: " + cte + "\r\n";
  eml += "\r\n";
  eml += encoded_body;
  eml += QByteArray("\r\n--") + kBoundary + "--\r\n";
  return eml;
}

/// An encrypted message whose CIPHERTEXT part is transfer-encoded.
auto EncryptedMessageWithEncodedCiphertext(const QByteArray& cte,
                                           const QByteArray& encoded_body)
    -> QByteArray {
  QByteArray eml;
  eml += "From: Alice <alice@example.com>\r\n";
  eml += "To: Bob <bob@example.com>\r\n";
  eml += "MIME-Version: 1.0\r\n";
  eml +=
      "Content-Type: multipart/encrypted; "
      "protocol=\"application/pgp-encrypted\"; boundary=\"encb\"\r\n";
  eml += "\r\n--encb\r\n";
  eml += "Content-Type: application/pgp-encrypted\r\n\r\n";
  eml += "Version: 1\r\n";
  eml += "\r\n--encb\r\n";
  eml += "Content-Type: application/octet-stream\r\n";
  eml += "Content-Transfer-Encoding: " + cte + "\r\n";
  eml += "\r\n";
  eml += encoded_body;
  eml += "\r\n--encb--\r\n";
  return eml;
}

}  // namespace

TEST_F(CryptoBoundaryTest, ABase64SignaturePartIsDecodedBeforeVerifying) {
  const QByteArray armor(kArmoredSignature);
  const auto encoded = armor.toBase64();

  EMailVerificationResult verification;
  QString error;
  VerifyEMLMessage(0, SignedMessageWithEncodedSignature("base64", encoded),
                   verification, error);

  ASSERT_EQ(Get().verify.size(), 1);
  const auto handed = Get().verify.first().signature;

  EXPECT_EQ(handed, armor);
  // The failure this exists to catch: the base64 text reaching the engine
  // instead of the signature it encodes.
  EXPECT_FALSE(handed.contains(encoded))
      << "the engine was handed the wire form, not the signature";
}

TEST_F(CryptoBoundaryTest, AQuotedPrintableSignaturePartIsDecoded) {
  // The armor checksum line begins with '=', which is exactly the character
  // quoted-printable escapes, so this is not a contrived case.
  QByteArray armor(kArmoredSignature);
  QByteArray encoded = armor;
  encoded.replace("=", "=3D");

  EMailVerificationResult verification;
  QString error;
  VerifyEMLMessage(
      0, SignedMessageWithEncodedSignature("quoted-printable", encoded),
      verification, error);

  ASSERT_EQ(Get().verify.size(), 1);
  const auto handed = Get().verify.first().signature;

  EXPECT_TRUE(handed.contains("=ArMr"));
  EXPECT_FALSE(handed.contains("=3DArMr"))
      << "the quoted-printable escaping was never undone";
}

TEST_F(CryptoBoundaryTest, ABase64CiphertextPartIsDecodedBeforeDecrypting) {
  const QByteArray armor(kArmoredMessage);
  const auto encoded = armor.toBase64();

  Get().decrypt_output = "Content-Type: text/plain\r\n\r\nplain\r\n";

  EMailMetaData meta;
  QByteArray out;
  uint32_t err = 0;
  QString capsule;
  DecryptEMLData(0, EncryptedMessageWithEncodedCiphertext("base64", encoded),
                 meta, out, err, capsule);

  ASSERT_EQ(Get().decrypt.size(), 1);
  const auto handed = Get().decrypt.first().data;

  EXPECT_EQ(handed, armor);
  EXPECT_FALSE(handed.contains(encoded))
      << "the engine was handed the wire form, not the ciphertext";
}

TEST_F(CryptoBoundaryTest, AnUnencodedSignaturePartIsStillPassedVerbatim) {
  // The ordinary case must be unchanged by the decode: a 7bit part's decoded
  // form is its literal bytes.
  const QByteArray armor(kArmoredSignature);

  EMailVerificationResult verification;
  QString error;
  VerifyEMLMessage(0, SignedMessageWithEncodedSignature("7bit", armor),
                   verification, error);

  ASSERT_EQ(Get().verify.size(), 1);
  EXPECT_EQ(Get().verify.first().signature, armor);
}

// --- Content-Type parameters ------------------------------------------------
//
// Wrapping a message changes what its top-level Content-Type describes, and
// every parameter of the OLD type has to go with it: a charset belonging to a
// text/plain body says nothing about a multipart, and a micalg naming the hash
// of a signature that has just been sealed inside ciphertext is a claim this
// layer cannot make.
//
// That happens to be free today. setValue(const string&) does not set a value;
// it re-PARSES the field, and parameterizedHeaderField::parseImpl() resets the
// parameter list before reading the new one. So the old parameters are gone
// before appendParameter() adds a single one of its own, and the duplicate
// protocol parameter this looks like it should produce never appears.
//
// It is free only because of that overload. The typed setValue(), taking a
// mediaType, sets the value object and leaves the parameters untouched --
// switching to it, which reads like a pure cleanup, would silently start
// emitting two protocol parameters naming different protocols. RFC 2045
// forbids the duplicate; vmime reads the last and would round-trip its own
// output, so nothing here would notice, while a reader taking the first is
// told a signed message arrived when an encrypted one was sent.
//
// These tests pin the output shape so that swap cannot be made quietly.

namespace {

/// The outermost Content-Type field, exactly as it was written.
auto OuterContentTypeLine(const QByteArray& eml) -> QByteArray {
  const auto end = eml.indexOf("\r\n\r\n");
  const auto block = end >= 0 ? eml.left(end + 2) : eml;
  for (const auto& field : SplitRawHeaderFields(block)) {
    if (field.name.compare("Content-Type", Qt::CaseInsensitive) == 0) {
      return field.raw_line;
    }
  }
  return {};
}

auto CountOf(const QByteArray& haystack, const QByteArray& needle) -> int {
  int n = 0;
  for (auto at = haystack.indexOf(needle); at >= 0;
       at = haystack.indexOf(needle, at + needle.size())) {
    ++n;
  }
  return n;
}

auto MetaForSigning() -> EMailMetaData {
  EMailMetaData meta;
  meta.from = "alice@example.com";
  meta.to = QStringList{"bob@example.com"};
  meta.subject = "s";
  return meta;
}

}  // namespace

TEST_F(CryptoBoundaryTest, SigningDoesNotLeaveACharsetOnTheMultipart) {
  // The charset belonged to the text/plain body, which is now one part down.
  // On a multipart/signed it describes nothing at all.
  QByteArray eml;
  uint32_t err = 0;
  QString capsule;
  ASSERT_EQ(SignPlainText(0, "DEADBEEF", MetaForSigning(), "hello", eml, err,
                          capsule),
            kSUCCESS);

  const auto line = OuterContentTypeLine(eml);
  ASSERT_FALSE(line.isEmpty());
  EXPECT_TRUE(line.contains("multipart/signed")) << line.constData();
  EXPECT_FALSE(line.contains("charset")) << line.constData();
  EXPECT_EQ(CountOf(line, "protocol="), 1) << line.constData();
}

TEST_F(CryptoBoundaryTest, EncryptingASignedMessageEmitsOneProtocol) {
  // The exact shape encrypt-and-sign produces, asserted on a message that
  // really does arrive carrying a protocol and a micalg of its own.
  QByteArray signed_eml;
  uint32_t err = 0;
  QString capsule;
  ASSERT_EQ(SignPlainText(0, "DEADBEEF", MetaForSigning(), "hello", signed_eml,
                          err, capsule),
            kSUCCESS);
  ASSERT_TRUE(OuterContentTypeLine(signed_eml).contains("micalg"));

  vmime::shared_ptr<vmime::message> parsed;
  ASSERT_TRUE(CheckIfEMLMessage(signed_eml, parsed));

  QByteArray encrypted_eml;
  ASSERT_EQ(EncryptEMLData(0, QStringList{"DEADBEEF"}, parsed, signed_eml,
                           encrypted_eml, err, capsule),
            kSUCCESS);

  const auto line = OuterContentTypeLine(encrypted_eml);
  ASSERT_FALSE(line.isEmpty());

  EXPECT_TRUE(line.contains("multipart/encrypted")) << line.constData();
  EXPECT_EQ(CountOf(line, "protocol="), 1) << line.constData();
  EXPECT_TRUE(line.contains("application/pgp-encrypted")) << line.constData();
  EXPECT_FALSE(line.contains("application/pgp-signature")) << line.constData();

  // The micalg described the signature that is now sealed inside the
  // ciphertext. Leaving it on the outside announces a hash for a signature
  // this layer does not have.
  EXPECT_FALSE(line.contains("micalg")) << line.constData();
}

TEST_F(CryptoBoundaryTest, EncryptingStillCarriesItsOwnBoundary) {
  // The other direction: whatever clears the old parameters must not take the
  // new ones with it. A multipart with no boundary is unreadable.
  QByteArray eml;
  uint32_t err = 0;
  QString capsule;
  ASSERT_EQ(EncryptPlainText(0, QStringList{"DEADBEEF"}, MetaForSigning(),
                             "hello", eml, err, capsule),
            kSUCCESS);

  const auto line = OuterContentTypeLine(eml);
  EXPECT_TRUE(line.contains("multipart/encrypted")) << line.constData();
  EXPECT_TRUE(line.contains("boundary=")) << line.constData();
  EXPECT_EQ(CountOf(line, "protocol="), 1) << line.constData();
  EXPECT_FALSE(line.contains("charset")) << line.constData();
}

// --- results handed back with a failure -------------------------------------
//
// The SDK allocates its result struct before it can know whether the operation
// will succeed, so a non-zero return still comes back with a live struct --
// and usually with the engine's own account of what went wrong inside it.
// Checking only the return code leaked the struct and left the user with
// "Operation Failed." where a reason existed.

TEST_F(CryptoBoundaryTest, AFailedSignReclaimsItsResultAndSaysWhy) {
  Get().fail_next = true;
  Get().fail_error_string = "No secret key for DEADBEEF";

  const auto before = crypto_recorder::OutstandingAllocations();

  QByteArray eml;
  uint32_t err = 0;
  QString capsule;
  EXPECT_EQ(SignPlainText(0, "DEADBEEF", MetaForSigning(), "hello", eml, err,
                          capsule),
            kFAILED);

  EXPECT_TRUE(QString::fromUtf8(eml).contains("No secret key for DEADBEEF"))
      << "the engine's reason never reached the user: " << eml.constData();
  EXPECT_EQ(crypto_recorder::OutstandingAllocations(), before)
      << "the result the SDK allocated alongside the failure was never freed";
}

TEST_F(CryptoBoundaryTest, AFailedEncryptReclaimsItsResultAndSaysWhy) {
  Get().fail_next = true;
  Get().fail_error_string = "Unusable public key";

  const auto before = crypto_recorder::OutstandingAllocations();

  QByteArray eml;
  uint32_t err = 0;
  QString capsule;
  EXPECT_EQ(EncryptPlainText(0, QStringList{"DEADBEEF"}, MetaForSigning(),
                             "hello", eml, err, capsule),
            kFAILED);

  EXPECT_TRUE(QString::fromUtf8(eml).contains("Unusable public key"))
      << eml.constData();
  EXPECT_EQ(crypto_recorder::OutstandingAllocations(), before);
}

TEST_F(CryptoBoundaryTest, AFailedVerifyReclaimsItsResultAndSaysSoPerRegion) {
  QByteArray entity;
  entity += "Content-Type: text/plain\r\n\r\nhi";
  const auto eml = SignedMessage(entity);

  Get().fail_next = true;
  Get().fail_error_string = "Engine unavailable";

  const auto before = crypto_recorder::OutstandingAllocations();

  EMailVerificationResult verification;
  QString error;

  // The PASS ran. An engine that could not answer is a property of the region
  // it could not answer for, not a failure of the operation: the walk goes on
  // to the other regions, and every surface is told which regions could not
  // be checked rather than being handed one blanket refusal.
  EXPECT_EQ(VerifyEMLMessage(0, eml, verification, error), kSUCCESS);

  ASSERT_EQ(verification.verdicts.size(), 1);

  // Execution and verdict are answers to different questions, and this is the
  // case that separates them: nothing here says anything about the signature.
  EXPECT_EQ(verification.verdicts.first().exec, EMailVerifyExec::kENGINE_ERROR);
  EXPECT_EQ(verification.verdicts.first().verdict,
            EMailBadgeState::kSIGNED_ERROR);
  EXPECT_EQ(verification.overall, EMailBadgeState::kSIGNED_ERROR);

  // Not dropped. A region the engine failed on used to vanish from the
  // results, which reads to every consumer as a message with one signature
  // fewer -- a failure presented as an absence.
  EXPECT_TRUE(verification.signatures.isEmpty());

  EXPECT_EQ(crypto_recorder::OutstandingAllocations(), before);
}

TEST_F(CryptoBoundaryTest, AFailureWithNoMessageStillReadsAsAFailure) {
  // Two of the SDK's failure paths set no message at all. Saying nothing is
  // better than inventing a cause, but it must still read as a failure.
  Get().fail_next = true;
  Get().fail_error_string = {};

  const auto before = crypto_recorder::OutstandingAllocations();

  QByteArray eml;
  uint32_t err = 0;
  QString capsule;
  EXPECT_EQ(SignPlainText(0, "DEADBEEF", MetaForSigning(), "hello", eml, err,
                          capsule),
            kFAILED);

  EXPECT_TRUE(QString::fromUtf8(eml).contains("Failed")) << eml.constData();
  EXPECT_EQ(crypto_recorder::OutstandingAllocations(), before);
}
