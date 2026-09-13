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

// One verification, one result, every surface quoting it.
//
// The bug these exist for: a signed message reported as SUCCESS by the status
// board while the badge beside it said "not checked" and the security details
// said "BAD signature" -- three surfaces, two verifiers, two byte sequences,
// and no two of them describing the same thing. What is pinned here is not the
// verdict for any one message but the property that there is only ONE verdict
// to quote, that it is about the bytes the document actually holds, and that
// it cannot be improved by anything other than the cryptography.

#include <gtest/gtest.h>

#include <QByteArray>
#include <QCryptographicHash>

#include "EMailBasicGpgOpera.h"
#include "EMailCryptoRecorder.h"
#include "EMailHelper.h"
#include "EMailVerificationPayload.h"

namespace {

using crypto_recorder::Get;
using crypto_recorder::Reset;

/// The analysis JSON for one signature of a given validity.
///
/// Mirrors GpgSigValidity, which is what BadgeForValidity() reads: 0 fully
/// valid, 1 bad, 4 key missing, 6 expired.
auto SignatureJson(int validity, const char* uid = "Alice <alice@example.com>")
    -> QByteArray {
  return QByteArray(R"({"signatures":[{"fingerprint":"DEADBEEF",)"
                    R"("hashAlgo":"SHA256","uid":")") +
         uid + R"(","validity":)" + QByteArray::number(validity) + "}]}";
}

/// Two signatures over one region, of differing validity: a countersigned
/// part, which the rules combine worst-first.
auto TwoSignaturesJson(int first, int second) -> QByteArray {
  return QByteArray(R"({"signatures":[)"
                    R"({"fingerprint":"AAAA","hashAlgo":"SHA256",)"
                    R"("uid":"Alice <alice@example.com>","validity":)") +
         QByteArray::number(first) +
         R"(},{"fingerprint":"BBBB","hashAlgo":"SHA256",)"
         R"("uid":"Alice <alice@example.com>","validity":)" +
         QByteArray::number(second) + "}]}";
}

constexpr auto kSignatureBlock =
    "-----BEGIN PGP SIGNATURE-----\r\naGVsbG8=\r\n"
    "-----END PGP SIGNATURE-----\r\n";

/// A PGP/MIME signed message whose signed entity is exactly @p entity.
auto SignedMessage(const QByteArray& entity, const char* boundary = "sig")
    -> QByteArray {
  const QByteArray b(boundary);
  QByteArray eml;
  eml += "From: Alice <alice@example.com>\r\n";
  eml += "To: Bob <bob@example.com>\r\n";
  eml += "Subject: signed\r\n";
  eml += "MIME-Version: 1.0\r\n";
  eml += "Content-Type: multipart/signed; micalg=pgp-sha256; "
         "protocol=\"application/pgp-signature\"; boundary=\"" +
         b + "\"\r\n\r\n";
  eml += "--" + b + "\r\n";
  eml += entity;
  eml += "\r\n--" + b + "\r\n";
  eml += "Content-Type: application/pgp-signature\r\n\r\n";
  eml += kSignatureBlock;
  eml += "--" + b + "--\r\n";
  return eml;
}

/// A signed message carrying another signed message inside it: the outer
/// signature covers the inner one, and the two can disagree.
auto NestedSignedMessage() -> QByteArray {
  QByteArray inner;
  inner += "Content-Type: multipart/signed; micalg=pgp-sha256; "
           "protocol=\"application/pgp-signature\"; boundary=\"in\"\r\n\r\n";
  inner += "--in\r\n";
  inner += "Content-Type: text/plain\r\n\r\nforwarded\r\n";
  inner += "--in\r\n";
  inner += "Content-Type: application/pgp-signature\r\n\r\n";
  inner += kSignatureBlock;
  inner += "--in--\r\n";
  return SignedMessage(inner, "out");
}

/// Two signed parts side by side under a multipart/mixed: neither contains
/// the other, and both cover content the reader is shown.
auto SiblingSignedRegions() -> QByteArray {
  QByteArray eml;
  eml += "From: Alice <alice@example.com>\r\n";
  eml += "To: Bob <bob@example.com>\r\n";
  eml += "Subject: two parts\r\n";
  eml += "MIME-Version: 1.0\r\n";
  eml += "Content-Type: multipart/mixed; boundary=\"outer\"\r\n\r\n";

  for (const char* b : {"one", "two"}) {
    const QByteArray bound(b);
    eml += "--outer\r\n";
    eml += "Content-Type: multipart/signed; micalg=pgp-sha256; "
           "protocol=\"application/pgp-signature\"; boundary=\"" +
           bound + "\"\r\n\r\n";
    eml += "--" + bound + "\r\n";
    eml += "Content-Type: text/plain\r\n\r\npart\r\n";
    eml += "--" + bound + "\r\n";
    eml += "Content-Type: application/pgp-signature\r\n\r\n";
    eml += kSignatureBlock;
    eml += "--" + bound + "--\r\n";
  }
  eml += "--outer--\r\n";
  return eml;
}

/// A verdict, described the way the aggregation rules read it.
auto Verdict(int region_id, int depth, EMailBadgeState state,
             bool ciphertext_only = false,
             EMailVerifyExec exec = EMailVerifyExec::kOK)
    -> EMailRegionVerdict {
  EMailRegionVerdict verdict;
  verdict.region_id = region_id;
  verdict.nesting_depth = depth;
  verdict.covers_ciphertext_only = ciphertext_only;
  verdict.exec = exec;
  verdict.verdict = state;
  return verdict;
}

/// Regions matching @p verdicts, since the rules take both.
auto RegionsFor(const QList<EMailRegionVerdict>& verdicts)
    -> QList<EMailSignatureRegion> {
  QList<EMailSignatureRegion> regions;
  for (const auto& verdict : verdicts) {
    EMailSignatureRegion region;
    region.region_id = verdict.region_id;
    region.nesting_depth = verdict.nesting_depth;
    region.covers_ciphertext_only = verdict.covers_ciphertext_only;
    regions.append(region);
  }
  return regions;
}

auto Aggregate(const QList<EMailRegionVerdict>& verdicts,
               EMailSecurityState structure = EMailSecurityState::kSIGNED)
    -> EMailBadgeState {
  return AggregateVerification(structure, RegionsFor(verdicts), verdicts);
}

class VerifyUnificationTest : public ::testing::Test {
 protected:
  void SetUp() override { Reset(); }
};

// --- the aggregation rules, with no engine anywhere near them ---------------

TEST(VerifyAggregationTest, ASingleGoodSignatureIsTheAnswer) {
  EXPECT_EQ(Aggregate({Verdict(0, 0, EMailBadgeState::kSIGNED_GOOD)}),
            EMailBadgeState::kSIGNED_GOOD);
}

TEST(VerifyAggregationTest, ASingleBadSignatureIsTheAnswer) {
  EXPECT_EQ(Aggregate({Verdict(0, 0, EMailBadgeState::kSIGNED_BAD)}),
            EMailBadgeState::kSIGNED_BAD);
}

TEST(VerifyAggregationTest, SiblingRegionsAreCombinedWorstFirst) {
  // Both cover content the reader is shown, and a message is only as good as
  // the weakest signature over what is in front of them. Reporting the better
  // of the two is how a second signature becomes a second attempt.
  EXPECT_EQ(Aggregate({Verdict(0, 0, EMailBadgeState::kSIGNED_GOOD),
                       Verdict(1, 0, EMailBadgeState::kSIGNED_BAD)}),
            EMailBadgeState::kSIGNED_BAD);
}

TEST(VerifyAggregationTest, ANestedBadSignatureDoesNotCondemnTheMessage) {
  // A signed message quoted inside a signed message. The inner one going bad
  // says something about the quoted content, not about whether THIS message's
  // signature holds -- and the badge speaks for this message.
  EXPECT_EQ(Aggregate({Verdict(0, 0, EMailBadgeState::kSIGNED_GOOD),
                       Verdict(1, 1, EMailBadgeState::kSIGNED_BAD)}),
            EMailBadgeState::kSIGNED_GOOD);
}

TEST(VerifyAggregationTest, ANestedGoodSignatureDoesNotRescueTheMessage) {
  // And the other direction, which matters more: a good signature on quoted
  // content must never make a message whose own signature is bad read as
  // signed. The candidate depth decides, in both directions.
  EXPECT_EQ(Aggregate({Verdict(0, 0, EMailBadgeState::kSIGNED_BAD),
                       Verdict(1, 1, EMailBadgeState::kSIGNED_GOOD)}),
            EMailBadgeState::kSIGNED_BAD);
}

TEST(VerifyAggregationTest, ASignatureOverCiphertextDoesNotDecideAnything) {
  // Anyone can take someone else's ciphertext and sign it. Such a signature
  // says who wrapped a blob, never who wrote the plaintext that comes out.
  EXPECT_EQ(Aggregate({Verdict(0, 0, EMailBadgeState::kSIGNED_GOOD, true),
                       Verdict(1, 0, EMailBadgeState::kSIGNED_BAD)}),
            EMailBadgeState::kSIGNED_BAD);
}

TEST(VerifyAggregationTest, NothingButCiphertextSignaturesReadsAsEncrypted) {
  EXPECT_EQ(Aggregate({Verdict(0, 0, EMailBadgeState::kSIGNED_GOOD, true)}),
            EMailBadgeState::kENCRYPTED_ONLY);
}

TEST(VerifyAggregationTest, AnEngineFailureIsCarriedRatherThanDropped) {
  // A region the engine could not answer for used to vanish from the results,
  // which reads to every surface as a message with one signature fewer.
  EXPECT_EQ(Aggregate({Verdict(0, 0, EMailBadgeState::kSIGNED_ERROR, false,
                               EMailVerifyExec::kENGINE_ERROR)}),
            EMailBadgeState::kSIGNED_ERROR);
}

TEST(VerifyAggregationTest, ADefiniteForgeryOutranksAnInconclusiveCheck) {
  EXPECT_EQ(Aggregate({Verdict(0, 0, EMailBadgeState::kSIGNED_ERROR, false,
                               EMailVerifyExec::kENGINE_ERROR),
                       Verdict(1, 0, EMailBadgeState::kSIGNED_BAD)}),
            EMailBadgeState::kSIGNED_BAD);
}

TEST(VerifyAggregationTest, AnInconclusiveCheckOutranksEveryMerelyWeakOne) {
  // Expired, unknown-key and mismatch are all statements ABOUT a signature.
  // "Could not check" is the absence of one, and must not be presented as the
  // milder outcome.
  EXPECT_EQ(Aggregate({Verdict(0, 0, EMailBadgeState::kSIGNED_ERROR, false,
                               EMailVerifyExec::kENGINE_ERROR),
                       Verdict(1, 0, EMailBadgeState::kSIGNED_MISMATCH),
                       Verdict(2, 0, EMailBadgeState::kSIGNED_EXPIRED),
                       Verdict(3, 0, EMailBadgeState::kSIGNED_UNKNOWN_KEY)}),
            EMailBadgeState::kSIGNED_ERROR);
}

TEST(VerifyAggregationTest, SignedStructureWithNoRegionIsMalformed) {
  // It said it was signed and not one region resolved. That is the structure
  // failing to hold up, not a message that simply carries no signature.
  EXPECT_EQ(Aggregate({}, EMailSecurityState::kSIGNED),
            EMailBadgeState::kMALFORMED);
}

TEST(VerifyAggregationTest, APlainMessageIsNotProtectedAndNotAFault) {
  EXPECT_EQ(Aggregate({}, EMailSecurityState::kPLAIN),
            EMailBadgeState::kNOT_PROTECTED);
}

TEST(VerifyAggregationTest, RegionsNobodyLookedAtReadAsUnchecked) {
  // The walk stopped on its budget before reaching any candidate region. No
  // verdict exists, and inventing the best one would be the worst of them.
  QList<EMailSignatureRegion> regions;
  EMailSignatureRegion region;
  region.region_id = 0;
  regions.append(region);

  EXPECT_EQ(AggregateVerification(EMailSecurityState::kSIGNED, regions, {}),
            EMailBadgeState::kSIGNED_UNVERIFIED);
}

// --- the staleness anchor ---------------------------------------------------

TEST(VerifyStalenessTest, AResultDescribesOnlyTheDocumentItWasTakenOver) {
  const QByteArray source_a = "the message as it was verified\r\n";
  const QByteArray source_b = "the message as it is now\r\n";

  EMailVerificationResult result;
  result.source_length = source_a.size();
  result.source_sha256 =
      QCryptographicHash::hash(source_a, QCryptographicHash::Sha256);

  EXPECT_TRUE(VerificationMatchesSource(result, source_a));

  // The document was edited while the answer was in flight. Painting the old
  // verdict onto the new bytes tells the user something about a message that
  // is not in front of them.
  EXPECT_FALSE(VerificationMatchesSource(result, source_b));
}

TEST(VerifyStalenessTest, ADifferentDocumentOfTheSameLengthIsStillRejected) {
  // The near miss the length check alone would wave through.
  QByteArray source_a = "aaaaaaaaaaaaaaaa";
  QByteArray source_b = "aaaaaaaaaaaaaaab";
  ASSERT_EQ(source_a.size(), source_b.size());

  EMailVerificationResult result;
  result.source_length = source_a.size();
  result.source_sha256 =
      QCryptographicHash::hash(source_a, QCryptographicHash::Sha256);

  EXPECT_FALSE(VerificationMatchesSource(result, source_b));
}

TEST(VerifyStalenessTest, OneMoreNewlineIsADifferentDocument) {
  // The everyday edit, and the one that breaks a signature: a document that
  // gained a trailing newline is not the document that was verified.
  const QByteArray source = "Content-Type: text/plain\r\n\r\nhello\r\n";

  EMailVerificationResult result;
  result.source_length = source.size();
  result.source_sha256 =
      QCryptographicHash::hash(source, QCryptographicHash::Sha256);

  EXPECT_FALSE(VerificationMatchesSource(result, source + "\r\n"));
}

TEST(VerifyStalenessTest, AnUnanchoredResultIsNeverAccepted) {
  // No digest means nothing to check it against, and an answer that cannot be
  // tied to a document is one that can be shown beside the wrong message.
  EXPECT_FALSE(VerificationMatchesSource({}, "anything at all"));
}

// --- the wire form ----------------------------------------------------------

TEST(VerifyPayloadTest, AResultSurvivesTheTripToThePage) {
  const auto raw = SignedMessage("Content-Type: text/plain\r\n\r\nhello");

  Reset();
  Get().verify_info_json.append(SignatureJson(0));

  EMailVerificationResult verified;
  QString error;
  ASSERT_EQ(VerifyEMLMessage(0, raw, verified, error), kSUCCESS);

  EMailVerificationResult decoded;
  ASSERT_TRUE(
      DecodeVerificationPayload(EncodeVerificationPayload(verified), decoded));

  // Semantic, not bitwise: list order within a region and the precision a
  // timestamp survives in are not part of what either result CLAIMS.
  EXPECT_TRUE(VerificationResultsEquivalent(verified, decoded));

  // And the parts the surfaces actually render came through.
  EXPECT_EQ(decoded.overall, verified.overall);
  EXPECT_EQ(decoded.verdicts.size(), verified.verdicts.size());
  EXPECT_EQ(decoded.signatures.size(), verified.signatures.size());
  EXPECT_TRUE(VerificationMatchesSource(decoded, raw));
}

TEST(VerifyPayloadTest, APayloadFromAnotherSchemaIsRefused) {
  // Refused rather than half-read. The states are written as names precisely
  // because their numbering changes when one is inserted in severity order,
  // and a verdict misread across builds is worse than no verdict.
  EMailVerificationResult decoded;
  EXPECT_FALSE(DecodeVerificationPayload(
      R"({"schema_version":99,"state":"verified","overall":"signed_good"})",
      decoded));
}

TEST(VerifyPayloadTest, APayloadNamingAnUnknownVerdictIsRefused) {
  EMailVerificationResult decoded;
  EXPECT_FALSE(DecodeVerificationPayload(
      QByteArray(R"({"schema_version":)") +
          QByteArray::number(kEMailVerificationPayloadVersion) +
          R"(,"state":"verified","overall":"signed_superb",)"
          R"("source_length":1,"source_sha256":"ab"})",
      decoded));
}

TEST(VerifyPayloadTest, AnUnanchoredPayloadIsRefused) {
  EMailVerificationResult decoded;
  EXPECT_FALSE(DecodeVerificationPayload(
      QByteArray(R"({"schema_version":)") +
          QByteArray::number(kEMailVerificationPayloadVersion) +
          R"(,"state":"verified","overall":"signed_good"})",
      decoded));
}

TEST(VerifyPayloadTest, RubbishIsRefusedRatherThanPartlyBelieved) {
  EMailVerificationResult decoded;
  EXPECT_FALSE(DecodeVerificationPayload("not json at all", decoded));
  EXPECT_FALSE(DecodeVerificationPayload("", decoded));
}

// --- one pass, whole message ------------------------------------------------

TEST_F(VerifyUnificationTest, ASignedUntouchedMessageReportsGood) {
  // The message from the bug report, asserted once at the layer every surface
  // now quotes -- rather than by trusting two verifiers to agree.
  const auto raw = SignedMessage("Content-Type: text/plain\r\n\r\nHello");
  Get().verify_info_json.append(SignatureJson(0));

  EMailVerificationResult result;
  QString error;
  ASSERT_EQ(VerifyEMLMessage(0, raw, result, error), kSUCCESS);

  EXPECT_EQ(result.state, EMailVerifyState::kVERIFIED);
  EXPECT_EQ(result.overall, EMailBadgeState::kSIGNED_GOOD);
  ASSERT_EQ(result.verdicts.size(), 1);
  EXPECT_EQ(result.verdicts.first().exec, EMailVerifyExec::kOK);
  EXPECT_FALSE(result.verdicts.first().signed_bytes_non_canonical);
  EXPECT_TRUE(VerificationMatchesSource(result, raw));
}

TEST_F(VerifyUnificationTest, TheSameMessageTwiceMeansTheSameThingTwice) {
  const auto raw = SignedMessage("Content-Type: text/plain\r\n\r\nHello");

  Get().verify_info_json.append(SignatureJson(0));
  EMailVerificationResult first;
  QString error;
  ASSERT_EQ(VerifyEMLMessage(0, raw, first, error), kSUCCESS);

  Get().verify_info_json.append(SignatureJson(0));
  EMailVerificationResult second;
  ASSERT_EQ(VerifyEMLMessage(0, raw, second, error), kSUCCESS);

  EXPECT_TRUE(VerificationResultsEquivalent(first, second));
}

TEST_F(VerifyUnificationTest, ACountersignedRegionIsOnlyAsGoodAsItsWorst) {
  const auto raw = SignedMessage("Content-Type: text/plain\r\n\r\nHello");
  Get().verify_info_json.append(TwoSignaturesJson(0, 1));

  EMailVerificationResult result;
  QString error;
  ASSERT_EQ(VerifyEMLMessage(0, raw, result, error), kSUCCESS);

  ASSERT_EQ(result.signatures.size(), 2) << "both signatures must be reported";
  EXPECT_EQ(result.overall, EMailBadgeState::kSIGNED_BAD);
}

TEST_F(VerifyUnificationTest, EveryRegionOfANestedMessageIsWalked) {
  // The toolbar verify used to check the OUTERMOST signature and stop, so a
  // message like this reached the status board as a single verdict. Unifying
  // on the per-region walk is what makes the board and the badge able to
  // describe the same thing -- and it has to actually walk.
  const auto raw = NestedSignedMessage();

  Get().verify_info_json.append(SignatureJson(0));  // outer: good
  Get().verify_info_json.append(SignatureJson(1));  // inner: bad

  EMailVerificationResult result;
  QString error;
  ASSERT_EQ(VerifyEMLMessage(0, raw, result, error), kSUCCESS);

  ASSERT_EQ(result.verdicts.size(), 2) << "the nested region was not verified";
  EXPECT_EQ(Get().verify.size(), 2);

  // Reported in full, and the message still reads by its own signature.
  EXPECT_EQ(result.overall, EMailBadgeState::kSIGNED_GOOD);
  EXPECT_EQ(result.verdicts[1].verdict, EMailBadgeState::kSIGNED_BAD);
  EXPECT_GT(result.verdicts[1].nesting_depth, result.verdicts[0].nesting_depth);
}

TEST_F(VerifyUnificationTest, SignedPartsInsideAnOrdinaryMessageAreVerified) {
  // Not a PGP/MIME message: its outermost layer is multipart/mixed. The old
  // outermost-only verify refused these outright, while the security tab
  // verified them -- exactly the disagreement being removed. Unifying must
  // keep the answer, not the refusal.
  const auto raw = SiblingSignedRegions();

  Get().verify_info_json.append(SignatureJson(0));
  Get().verify_info_json.append(SignatureJson(1));

  EMailVerificationResult result;
  QString error;
  ASSERT_EQ(VerifyEMLMessage(0, raw, result, error), kSUCCESS)
      << error.toStdString();

  ASSERT_EQ(result.verdicts.size(), 2);
  // Siblings, both covering content the reader is shown: the worse one wins.
  EXPECT_EQ(result.overall, EMailBadgeState::kSIGNED_BAD);
}

TEST_F(VerifyUnificationTest, AnUnsignedMessageIsRefusedAndSaysWhy) {
  QByteArray raw;
  raw += "From: Alice <alice@example.com>\r\n";
  raw += "Subject: nothing to check\r\n";
  raw += "Content-Type: text/plain\r\n\r\nhello\r\n";

  EMailVerificationResult result;
  QString error;
  EXPECT_NE(VerifyEMLMessage(0, raw, result, error), kSUCCESS);
  EXPECT_FALSE(error.isEmpty()) << "a refusal has to name what is wrong";
}

// --- a diagnosis never becomes a verdict ------------------------------------

TEST_F(VerifyUnificationTest, RewrittenLineEndingsAreReportedNotRepaired) {
  // The false "BAD signature" from the bug report came from bytes whose line
  // endings had been rewritten after signing. The rewrite is worth saying --
  // importing a key will never help -- but the bytes are still judged as they
  // stand, and what the engine says about them is what is reported.
  auto raw = SignedMessage("Content-Type: text/plain\r\n\r\nHello");
  raw.replace("\r\n", "\n");

  Get().verify_info_json.append(SignatureJson(1));  // the engine: bad

  EMailVerificationResult result;
  QString error;
  ASSERT_EQ(VerifyEMLMessage(0, raw, result, error), kSUCCESS);

  ASSERT_EQ(result.verdicts.size(), 1);
  EXPECT_TRUE(result.verdicts.first().signed_bytes_non_canonical)
      << "the rewrite was not noticed";
  EXPECT_TRUE(result.meta.signed_entity_non_canonical);

  // Diagnosed, not excused.
  EXPECT_EQ(result.overall, EMailBadgeState::kSIGNED_BAD);
}

TEST_F(VerifyUnificationTest, NonCanonicalBytesAreVerifiedExactlyAsTheyStand) {
  // The other half of it: the verifier must hand the engine the bytes the
  // document holds. Canonicalizing them first would answer for a document the
  // user does not have -- and would report a good signature over it.
  auto raw = SignedMessage("Content-Type: text/plain\r\n\r\nHello");
  raw.replace("\r\n", "\n");

  EMailVerificationResult result;
  QString error;
  ASSERT_EQ(VerifyEMLMessage(0, raw, result, error), kSUCCESS);

  ASSERT_EQ(Get().verify.size(), 1);
  const auto handed = Get().verify.first().data;
  EXPECT_FALSE(handed.contains("\r\n"))
      << "the bytes were canonicalized before being verified";
  EXPECT_EQ(handed, raw.mid(static_cast<int>(result.regions.first().raw_offset),
                            static_cast<int>(result.regions.first().raw_length)));
}

TEST_F(VerifyUnificationTest, NoDiagnosisCanMakeABadSignatureGood) {
  // The invariant behind both tests above, stated on its own: nothing this
  // module knows about a message -- least of all a rewrite it can explain --
  // may improve what the cryptography said.
  for (const int validity : {1, 3, 4, 5, 6, 7, -1}) {
    auto raw = SignedMessage("Content-Type: text/plain\r\n\r\nHello");
    raw.replace("\r\n", "\n");

    Reset();
    Get().verify_info_json.append(SignatureJson(validity));

    EMailVerificationResult result;
    QString error;
    ASSERT_EQ(VerifyEMLMessage(0, raw, result, error), kSUCCESS);
    EXPECT_NE(result.overall, EMailBadgeState::kSIGNED_GOOD)
        << "validity " << validity << " was reported as a good signature";
  }
}

// --- the findings quote the verification ------------------------------------

TEST_F(VerifyUnificationTest, TheCanonicalFindingComesFromTheVerification) {
  // InspectMessage used to scan the document itself. Two places deciding why
  // a signature failed is two places that can disagree, and the security tab
  // saying "rewritten" beside a report that says "verified" is that bug.
  auto raw = SignedMessage("Content-Type: text/plain\r\n\r\nHello");
  raw.replace("\r\n", "\n");

  Get().verify_info_json.append(SignatureJson(1));

  EMailVerificationResult result;
  QString error;
  ASSERT_EQ(VerifyEMLMessage(0, raw, result, error), kSUCCESS);

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(raw, message));
  ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0);

  const auto with_verification =
      InspectMessage(result.meta, root, regions, result.verdicts);
  bool reported = false;
  for (const auto& finding : with_verification) {
    if (finding.title.contains("canonical")) reported = true;
  }
  EXPECT_TRUE(reported);

  // And with no verification behind it, it says nothing rather than going and
  // working it out for itself.
  const auto without = InspectMessage(result.meta, root, regions, {});
  for (const auto& finding : without) {
    EXPECT_FALSE(finding.title.contains("canonical"));
  }
}

}  // namespace
