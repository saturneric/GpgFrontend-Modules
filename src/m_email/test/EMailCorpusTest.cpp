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

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSet>

#include "EMailHelper.h"
#include "EMailModel.h"

#ifndef GF_EMAIL_TEST_CORPUS_DIR
#error "GF_EMAIL_TEST_CORPUS_DIR must be defined by the build"
#endif

namespace {

// --- corpus access ---------------------------------------------------------

auto CorpusDir() -> QString { return QString(GF_EMAIL_TEST_CORPUS_DIR); }

// Names are relative to corpus/, so they always carry their bucket:
// "golden/04-pgpmime-signed.eml", "public/p07-cte-matrix.eml". The two buckets
// are held to different standards -- see corpus/public/MANIFEST.txt -- so no
// sweep may quietly mix them.
auto BucketNames(const QString& bucket) -> QStringList {
  QDir dir(CorpusDir() + "/" + bucket);
  QStringList names;
  for (const auto& name : dir.entryList({"*.eml"}, QDir::Files, QDir::Name)) {
    names.append(bucket + "/" + name);
  }
  EXPECT_FALSE(names.isEmpty())
      << "corpus bucket is empty: " << bucket.toStdString();
  return names;
}

// The GpgFrontend-owned fixtures: well-formed, CRLF, byte-level contracts.
auto CorpusNames() -> QStringList { return BucketNames("golden"); }

auto LoadCorpus(const QString& name) -> QByteArray {
  QFile file(CorpusDir() + "/" + name);
  EXPECT_TRUE(file.open(QIODevice::ReadOnly))
      << "failed to open corpus message: " << name.toStdString();
  return file.readAll();
}

auto ParseCorpus(const QString& name, QByteArray& raw,
                 vmime::shared_ptr<vmime::message>& message) -> bool {
  raw = LoadCorpus(name);
  return CheckIfEMLMessage(raw, message);
}

// --- the pre-tree walker, kept as a differential reference -----------------
//
// This is ExtractParts() as it stood before the MIME tree was introduced,
// copied here verbatim (helpers included) rather than referenced, so it cannot
// quietly drift with the implementation it is supposed to be checking. It is
// deliberately self-contained: nothing below calls into the production walk.

auto LegacyPartContentType(const vmime::shared_ptr<const vmime::bodyPart>& part)
    -> QString {
  auto field = part->getHeader()->findField(vmime::fields::CONTENT_TYPE);
  if (!field) return "text/plain";
  auto value = field->getValue<vmime::mediaType>();
  if (!value) return "text/plain";
  return QString::fromStdString(value->generate()).trimmed().toLower();
}

auto LegacyPartDisposition(const vmime::shared_ptr<const vmime::bodyPart>& part)
    -> QString {
  auto field = part->getHeader()->findField(vmime::fields::CONTENT_DISPOSITION);
  if (!field) return {};
  auto value = field->getValue<vmime::contentDisposition>();
  if (!value) return {};
  return QString::fromStdString(value->generate()).trimmed().toLower();
}

auto LegacyPartFileName(const vmime::shared_ptr<const vmime::bodyPart>& part)
    -> QString {
  if (auto field =
          part->getHeader()->findField(vmime::fields::CONTENT_DISPOSITION)) {
    auto disp = vmime::dynamicCast<const vmime::contentDispositionField>(field);
    if (disp && disp->hasParameter("filename")) {
      return QString::fromStdString(disp->getFilename().getBuffer());
    }
  }
  if (auto field = part->getHeader()->findField(vmime::fields::CONTENT_TYPE)) {
    auto ct = vmime::dynamicCast<const vmime::contentTypeField>(field);
    if (ct) {
      if (auto name = ct->findParameter("name")) {
        return QString::fromStdString(name->getValue().getBuffer());
      }
    }
  }
  return {};
}

auto LegacyDecodePart(const vmime::shared_ptr<const vmime::bodyPart>& part)
    -> QByteArray {
  auto contents = part->getBody()->getContents();
  if (!contents) return {};
  std::ostringstream oss;
  vmime::utility::outputStreamAdapter osa(oss);
  contents->extract(osa);
  osa.flush();
  return QByteArray::fromStdString(oss.str());
}

auto LegacyIsProtocolPart(const QString& content_type) -> bool {
  return content_type == "application/pgp-encrypted" ||
         content_type == "application/pgp-signature";
}

struct LegacyWalkState {
  int parts_seen{0};
  qint64 bytes_seen{0};
  bool limit_hit{false};
};

void LegacyWalkPart(const vmime::shared_ptr<const vmime::bodyPart>& part,
                    EMailMetaData& meta, const EMailParseLimits& limits,
                    LegacyWalkState& state, int depth, bool inside_signed) {
  if (state.limit_hit) return;

  if (depth > limits.max_depth || ++state.parts_seen > limits.max_parts) {
    state.limit_hit = true;
    return;
  }

  const auto content_type = LegacyPartContentType(part);
  const auto sub_parts = part->getBody()->getPartCount();

  if (sub_parts > 0) {
    const bool signed_subtree =
        inside_signed || content_type == "multipart/signed";
    for (size_t i = 0; i < sub_parts; ++i) {
      LegacyWalkPart(part->getBody()->getPartAt(i), meta, limits, state,
                     depth + 1, signed_subtree);
      if (state.limit_hit) return;
    }
    return;
  }

  if (LegacyIsProtocolPart(content_type)) return;

  const auto data = LegacyDecodePart(part);
  state.bytes_seen += data.size();
  if (state.bytes_seen > limits.max_total_bytes) {
    state.limit_hit = true;
    return;
  }

  const auto filename = LegacyPartFileName(part);
  const auto disposition = LegacyPartDisposition(part);

  const bool is_body = meta.body.isEmpty() && filename.isEmpty() &&
                       content_type == "text/plain" &&
                       !disposition.startsWith("attachment");
  if (is_body) {
    meta.body = data;
    meta.body_content_type = content_type;
    return;
  }

  EMailAttachment att;
  att.filename = filename;
  att.mime_type = content_type;
  att.disposition = disposition;
  att.data = data;
  att.is_openpgp_key = content_type == "application/pgp-keys";
  att.inside_signed_part = inside_signed;
  meta.attachments.append(att);
}

auto ExtractPartsLegacy(const vmime::shared_ptr<vmime::message>& message,
                        EMailMetaData& meta_data,
                        const EMailParseLimits& limits = {}) -> int {
  if (!message) return -1;
  LegacyWalkState state;
  try {
    LegacyWalkPart(message, meta_data, limits, state, 0, false);
  } catch (const vmime::exception&) {
    return -1;
  }
  return state.limit_hit ? -1 : 0;
}

// --- tree helpers ----------------------------------------------------------

auto FindByContentType(const EMailPart& root, const QString& type)
    -> const EMailPart* {
  for (const auto* part : FlattenMimeTree(root)) {
    if (part->content_type == type) return part;
  }
  return nullptr;
}

auto FindByFileName(const EMailPart& root, const QString& name)
    -> const EMailPart* {
  for (const auto* part : FlattenMimeTree(root)) {
    if (part->filename == name) return part;
  }
  return nullptr;
}

}  // namespace

// --- the corpus is intact --------------------------------------------------
//
// Line endings in the golden bucket are load-bearing: raw header slices,
// signature region offsets and HasBareLineFeeds() all read them directly. A
// checkout with core.autocrlf on, or an editor that "helpfully" normalises,
// rewrites them and every one of those assertions goes on passing while
// testing something else. .gitattributes marks the corpus binary; this is the
// test that notices when that protection is missing or has been undone.

TEST(EMailCorpusTest, GoldenFixturesKeepTheirCrlfLineEndings) {
  for (const auto& name : CorpusNames()) {
    const auto raw = LoadCorpus(name);
    ASSERT_FALSE(raw.isEmpty()) << name.toStdString();

    EXPECT_TRUE(raw.contains("\r\n"))
        << name.toStdString()
        << " has no CRLF at all -- the checkout converted the corpus";

    // 16 is the one fixture whose whole point is bare LF inside the signed
    // entity. Everywhere else a lone LF means conversion damage.
    if (name.endsWith("16-signed-lf-mangled.eml")) {
      EXPECT_TRUE(HasBareLineFeeds(raw))
          << name.toStdString() << " lost the damage it exists to carry";
      continue;
    }
    EXPECT_FALSE(HasBareLineFeeds(raw))
        << name.toStdString() << " has bare LFs -- line endings were rewritten";
  }
}

// --- differential compatibility --------------------------------------------
//
// The 35 hand-written cases pin specific behaviours; they do not prove the
// tree-backed ExtractParts() agrees with the walker it replaced across real
// messages. This does, over the whole corpus. Any intentional divergence is
// named in corpus/MANIFEST.txt and asserted separately.

TEST(EMailCorpusTest, TreeExtractionMatchesTheLegacyWalker) {
  for (const auto& name : CorpusNames()) {
    QByteArray raw;
    vmime::shared_ptr<vmime::message> message;
    ASSERT_TRUE(ParseCorpus(name, raw, message)) << name.toStdString();

    EMailMetaData current;
    EMailMetaData legacy;
    ASSERT_EQ(ExtractParts(message, current), 0) << name.toStdString();
    ASSERT_EQ(ExtractPartsLegacy(message, legacy), 0) << name.toStdString();

    EXPECT_EQ(current.body, legacy.body)
        << "body differs in " << name.toStdString();
    EXPECT_EQ(current.body_content_type, legacy.body_content_type)
        << "body content type differs in " << name.toStdString();
    ASSERT_EQ(current.attachments.size(), legacy.attachments.size())
        << "attachment count differs in " << name.toStdString();

    for (int i = 0; i < current.attachments.size(); ++i) {
      const auto& a = current.attachments[i];
      const auto& b = legacy.attachments[i];
      const auto where =
          QString("%1 attachment %2").arg(name).arg(i).toStdString();
      EXPECT_EQ(a.filename, b.filename) << where;
      EXPECT_EQ(a.mime_type, b.mime_type) << where;
      EXPECT_EQ(a.disposition, b.disposition) << where;
      EXPECT_EQ(a.is_openpgp_key, b.is_openpgp_key) << where;
      EXPECT_EQ(a.data, b.data) << where;
      // inside_signed_part is the one field allowed to diverge; every corpus
      // message is well-formed, so it must agree here too.
      EXPECT_EQ(a.inside_signed_part, b.inside_signed_part) << where;
    }
  }
}

TEST(EMailCorpusTest, EveryCorpusMessageParsesAndWalksWithinLimits) {
  for (const auto& name : CorpusNames()) {
    QByteArray raw;
    vmime::shared_ptr<vmime::message> message;
    ASSERT_TRUE(ParseCorpus(name, raw, message)) << name.toStdString();

    EMailPart root;
    QList<EMailSignatureRegion> regions;
    EXPECT_EQ(ParseMimeTree(message, raw, root, regions), 0)
        << name.toStdString();
  }
}

// --- byte preservation -----------------------------------------------------
//
// Inspection is byte-preserving; composition may reserialize. Parsing a
// message and walking its tree is inspection, so the bytes it was parsed from
// must come out untouched.

TEST(EMailCorpusTest, InspectionDoesNotTouchTheSourceBytes) {
  for (const auto& name : CorpusNames()) {
    const auto original = LoadCorpus(name);

    QByteArray raw = original;
    vmime::shared_ptr<vmime::message> message;
    ASSERT_TRUE(CheckIfEMLMessage(raw, message)) << name.toStdString();

    EMailPart root;
    QList<EMailSignatureRegion> regions;
    ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0)
        << name.toStdString();

    // Drive every read-only path a view would.
    (void)ClassifyOpenPGPStructure(root, regions);
    (void)SelectBodyPart(root, false);
    (void)SelectBodyPart(root, true);
    for (const auto* part : FlattenMimeTree(root)) {
      (void)RawHeaderBlock(*part, raw);
    }
    EMailMetaData meta;
    (void)GetEMLMetaData(message, meta);
    (void)ExtractParts(message, meta);

    EXPECT_EQ(raw, original)
        << "source bytes changed for " << name.toStdString();
  }
}

TEST(EMailCorpusTest, RawHeaderBlockComesFromTheOriginalBytes) {
  QByteArray raw;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseCorpus("golden/12-odd-headers.eml", raw, message));

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0);

  const auto block = RawHeaderBlock(root, raw);
  ASSERT_FALSE(block.isEmpty());

  // The slice must be exactly what the sender wrote: original order, the tab
  // continuation lines, and both copies of the duplicated field. A
  // reserialized header block would lose all three.
  EXPECT_TRUE(raw.startsWith(block));
  EXPECT_TRUE(block.contains("\r\n\tby mx.example.com with ESMTP id ABC123"));
  EXPECT_EQ(block.count("X-Custom:"), 2);
  EXPECT_LT(block.indexOf("Received:"), block.indexOf("From:"));
  EXPECT_LT(block.indexOf("X-Custom: first"),
            block.indexOf("X-Custom: second"));

  // Decoded fields keep both copies too, so nothing is silently collapsed.
  int custom = 0;
  for (const auto& field : root.header_fields) {
    if (field.first.compare("X-Custom", Qt::CaseInsensitive) == 0) ++custom;
  }
  EXPECT_EQ(custom, 2);
}

// --- structure classification ----------------------------------------------

namespace {

auto ClassifyCorpus(const QString& name) -> EMailSecurityState {
  QByteArray raw;
  vmime::shared_ptr<vmime::message> message;
  EXPECT_TRUE(ParseCorpus(name, raw, message)) << name.toStdString();

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  EXPECT_EQ(ParseMimeTree(message, raw, root, regions), 0)
      << name.toStdString();
  return ClassifyOpenPGPStructure(root, regions);
}

}  // namespace

TEST(EMailCorpusTest, StructureClassificationMatchesTheManifest) {
  EXPECT_EQ(ClassifyCorpus("golden/01-plain-text.eml"),
            EMailSecurityState::kPLAIN);
  EXPECT_EQ(ClassifyCorpus("golden/02-alternative.eml"),
            EMailSecurityState::kPLAIN);
  EXPECT_EQ(ClassifyCorpus("golden/03-mixed-attachments.eml"),
            EMailSecurityState::kPLAIN);
  EXPECT_EQ(ClassifyCorpus("golden/04-pgpmime-signed.eml"),
            EMailSecurityState::kSIGNED);
  EXPECT_EQ(ClassifyCorpus("golden/05-pgpmime-encrypted.eml"),
            EMailSecurityState::kENCRYPTED);
  EXPECT_EQ(ClassifyCorpus("golden/06-signed-inside-encrypted.eml"),
            EMailSecurityState::kSIGNED_ENCRYPTED);
  EXPECT_EQ(ClassifyCorpus("golden/10-pgp-keys-attachment.eml"),
            EMailSecurityState::kPLAIN);
  EXPECT_EQ(ClassifyCorpus("golden/11-nested-signatures.eml"),
            EMailSecurityState::kSIGNED);
  EXPECT_EQ(ClassifyCorpus("golden/14-signed-with-attachment.eml"),
            EMailSecurityState::kSIGNED);
  EXPECT_EQ(ClassifyCorpus("golden/17-encrypted-inside-signed.eml"),
            EMailSecurityState::kSIGNED_ENCRYPTED);
}

// --- structural containment is not authentication --------------------------
//
// The whole point of this group. A part can be structurally inside a signed
// subtree and still be authenticated by nothing at all. Everything below runs
// without a GPG engine and therefore asserts SHAPE only: no test here may be
// read as saying a signature verified. That claim needs the crypto path and
// belongs in the future gpgme-enabled target.

TEST(EMailCorpusTest, ASignatureOverCiphertextIsMarkedAsCoveringCiphertext) {
  // Encrypt-then-sign. The signature is over the encrypted block, so it says
  // who forwarded the ciphertext and nothing about who wrote the plaintext --
  // signing a copy of someone else's encrypted blob requires nothing of
  // theirs. Presenting this as "signed by X" would be a lie about provenance.
  QByteArray raw;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(
      ParseCorpus("golden/17-encrypted-inside-signed.eml", raw, message));

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0);

  ASSERT_EQ(regions.size(), 1);
  EXPECT_TRUE(regions[0].covers_ciphertext_only);

  // Structurally the ciphertext IS inside the region. That is exactly the
  // containment that must not be reported as authentication of the plaintext.
  const auto* blob = FindByFileName(root, "encrypted.asc");
  ASSERT_NE(blob, nullptr);
  EXPECT_EQ(blob->covered_by_regions, QList<int>({regions[0].region_id}));

  const auto findings = InspectMessage({}, root, regions, raw);
  bool warned = false;
  for (const auto& f : findings) {
    if (f.title.contains("encrypted data only")) warned = true;
  }
  EXPECT_TRUE(warned)
      << "a signature over ciphertext was reported like any other signature";
}

TEST(EMailCorpusTest, OrdinarySignaturesAreNotMarkedAsCoveringCiphertext) {
  // The flag must be specific, or the warning becomes noise and stops being
  // read. Every other signed fixture is a signature over content.
  for (const auto& name : {QString("golden/04-pgpmime-signed.eml"),
                           QString("golden/06-signed-inside-encrypted.eml"),
                           QString("golden/11-nested-signatures.eml"),
                           QString("golden/14-signed-with-attachment.eml")}) {
    QByteArray raw;
    vmime::shared_ptr<vmime::message> message;
    ASSERT_TRUE(ParseCorpus(name, raw, message)) << name.toStdString();

    EMailPart root;
    QList<EMailSignatureRegion> regions;
    ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0)
        << name.toStdString();
    ASSERT_FALSE(regions.isEmpty()) << name.toStdString();
    for (const auto& region : regions) {
      EXPECT_FALSE(region.covers_ciphertext_only) << name.toStdString();
    }
  }
}

TEST(EMailCorpusTest, ClassificationAloneCannotTellTheTwoOrderingsApart) {
  // 06 places an encrypted and a signed section side by side; 17 signs the
  // encrypted section itself. Both are kSIGNED_ENCRYPTED, which is why the
  // state enum must never be the thing a security claim is built on -- the
  // regions carry what actually differs.
  EXPECT_EQ(ClassifyCorpus("golden/06-signed-inside-encrypted.eml"),
            ClassifyCorpus("golden/17-encrypted-inside-signed.eml"));
}

TEST(EMailCorpusTest, AMalformedSignedStructureIsCalledOut) {
  // multipart/signed with a single part: it claims OpenPGP but there is
  // nothing to check the claim against.
  const QByteArray raw = QByteArray(
      "From: a@example.com\r\n"
      "To: b@example.com\r\n"
      "Subject: s\r\n"
      "MIME-Version: 1.0\r\n"
      "Content-Type: multipart/signed; "
      "protocol=\"application/pgp-signature\";\r\n"
      " micalg=pgp-sha256; boundary=\"b\"\r\n"
      "\r\n"
      "--b\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n"
      "lonely\r\n"
      "--b--\r\n");

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(raw, message));

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0);
  EXPECT_EQ(ClassifyOpenPGPStructure(root, regions),
            EMailSecurityState::kMALFORMED_PGP);
}

// --- signature regions and coverage ----------------------------------------

TEST(EMailCorpusTest, ASignedMessageHasOneRegionOverItsFirstPart) {
  QByteArray raw;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseCorpus("golden/04-pgpmime-signed.eml", raw, message));

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0);

  ASSERT_EQ(regions.size(), 1);
  EXPECT_EQ(regions[0].region_id, 0);
  EXPECT_EQ(regions[0].nesting_depth, 0);
  EXPECT_EQ(regions[0].declared_micalg, QString("pgp-sha256"));

  // The region must name the signed entity's own bytes, headers included.
  const auto slice = raw.mid(static_cast<int>(regions[0].raw_offset),
                             static_cast<int>(regions[0].raw_length));
  EXPECT_TRUE(slice.startsWith("Content-Type: text/plain"));
  EXPECT_TRUE(slice.contains("covered by the signature"));
  EXPECT_FALSE(slice.contains("BEGIN PGP SIGNATURE"));

  const auto* body = FindByContentType(root, "text/plain");
  ASSERT_NE(body, nullptr);
  EXPECT_EQ(body->covered_by_regions, QList<int>({0}));

  const auto* sig = FindByContentType(root, "application/pgp-signature");
  ASSERT_NE(sig, nullptr);
  EXPECT_TRUE(sig->is_protocol_part);
  EXPECT_TRUE(sig->covered_by_regions.isEmpty());
}

TEST(EMailCorpusTest, NestedSignaturesProduceTwoRegionsAndDoubleCoverage) {
  QByteArray raw;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseCorpus("golden/11-nested-signatures.eml", raw, message));

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0);

  ASSERT_EQ(regions.size(), 2);
  EXPECT_EQ(regions[0].nesting_depth, 0);
  EXPECT_EQ(regions[0].declared_micalg, QString("pgp-sha256"));
  EXPECT_EQ(regions[1].nesting_depth, 1);
  EXPECT_EQ(regions[1].declared_micalg, QString("pgp-sha512"));

  // Ids are distinct and stable, which is what results are joined on.
  EXPECT_NE(regions[0].region_id, regions[1].region_id);

  // The inner region is contained in the outer one.
  EXPECT_GE(regions[1].raw_offset, regions[0].raw_offset);
  EXPECT_LE(regions[1].raw_offset + regions[1].raw_length,
            regions[0].raw_offset + regions[0].raw_length);

  const auto* body = FindByContentType(root, "text/plain");
  ASSERT_NE(body, nullptr);
  // Outermost first -- a single boolean cannot say this.
  EXPECT_EQ(body->covered_by_regions,
            QList<int>({regions[0].region_id, regions[1].region_id}));
}

TEST(EMailCorpusTest, CoverageDistinguishesInsideFromOutsideTheSignature) {
  QByteArray raw;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(
      ParseCorpus("golden/14-signed-with-attachment.eml", raw, message));

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0);
  ASSERT_EQ(regions.size(), 1);

  const auto* covered = FindByFileName(root, "covered.bin");
  const auto* uncovered = FindByFileName(root, "uncovered.bin");
  ASSERT_NE(covered, nullptr);
  ASSERT_NE(uncovered, nullptr);

  EXPECT_EQ(covered->covered_by_regions, QList<int>({regions[0].region_id}));
  EXPECT_TRUE(uncovered->covered_by_regions.isEmpty());
}

TEST(EMailCorpusTest, RegionsRecordTheDeclaredMicalgOnly) {
  // The region carries what the message CLAIMS. Whether the signature really
  // used that algorithm is not knowable at parse time, and must not be
  // inferred from the header.
  QByteArray raw;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(
      ParseCorpus("golden/13-signed-micalg-mismatch.eml", raw, message));

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0);

  ASSERT_EQ(regions.size(), 1);
  EXPECT_EQ(regions[0].declared_micalg, QString("pgp-sha512"));
}

// --- multipart/alternative -------------------------------------------------

TEST(EMailCorpusTest, AlternativeMembersShareAGroupAndOnlyOneIsTheBody) {
  QByteArray raw;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseCorpus("golden/02-alternative.eml", raw, message));

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0);

  const auto* plain = FindByContentType(root, "text/plain");
  const auto* html = FindByContentType(root, "text/html");
  ASSERT_NE(plain, nullptr);
  ASSERT_NE(html, nullptr);

  EXPECT_NE(plain->alternative_group, -1);
  EXPECT_EQ(plain->alternative_group, html->alternative_group);

  // Selection picks one member rather than whichever is walked first.
  EXPECT_EQ(SelectBodyPart(root, false), plain);
  EXPECT_EQ(SelectBodyPart(root, true), html);
}

TEST(EMailCorpusTest, BodySelectionFallsBackWhenThePreferredFormIsAbsent) {
  QByteArray raw;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseCorpus("golden/01-plain-text.eml", raw, message));

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0);

  const auto* selected = SelectBodyPart(root, true);
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(selected->content_type, QString("text/plain"));
}

TEST(EMailCorpusTest, AttachmentsAreNeverChosenAsTheBody) {
  QByteArray raw;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseCorpus("golden/03-mixed-attachments.eml", raw, message));

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0);

  const auto* selected = SelectBodyPart(root, false);
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(selected->content_type, QString("text/plain"));
  EXPECT_TRUE(selected->filename.isEmpty());
}

// --- part metadata ---------------------------------------------------------

TEST(EMailCorpusTest, ContentIdIsExposedWithoutAngleBrackets) {
  QByteArray raw;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseCorpus("golden/07-inline-cid-image.eml", raw, message));

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0);

  const auto* image = FindByContentType(root, "image/png");
  ASSERT_NE(image, nullptr);
  // A "cid:" URL in the HTML references the bare id, so that is the form the
  // tree must expose or every inline image fails to resolve.
  EXPECT_EQ(image->content_id, QString("logo-1@example.com"));
  EXPECT_EQ(image->disposition, QString("inline"));
  EXPECT_EQ(image->transfer_encoding, QString("base64"));

  const auto* html = FindByContentType(root, "text/html");
  ASSERT_NE(html, nullptr);
  EXPECT_TRUE(
      QString::fromUtf8(html->data).contains("cid:" + image->content_id));
}

TEST(EMailCorpusTest, EncodedWordsAreDecodedInHeadersAndFileNames) {
  QByteArray raw;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseCorpus("golden/08-encoded-words.eml", raw, message));

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0);

  QString subject;
  for (const auto& field : root.header_fields) {
    if (field.first.compare("Subject", Qt::CaseInsensitive) == 0) {
      subject = field.second;
    }
  }
  EXPECT_TRUE(subject.contains(QString::fromUtf8("Überraschung")))
      << subject.toStdString();

  EMailMetaData meta;
  ASSERT_EQ(ExtractParts(message, meta), 0);
  ASSERT_EQ(meta.attachments.size(), 1);
  EXPECT_EQ(meta.attachments[0].filename,
            QString::fromUtf8("Béricht-über.pdf"));
}

TEST(EMailCorpusTest, OpenPgpKeyAttachmentsAreFlaggedInTheTree) {
  QByteArray raw;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseCorpus("golden/10-pgp-keys-attachment.eml", raw, message));

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0);

  const auto* keys = FindByContentType(root, "application/pgp-keys");
  ASSERT_NE(keys, nullptr);
  EXPECT_TRUE(keys->is_openpgp_key);
  EXPECT_FALSE(keys->is_protocol_part);
}

TEST(EMailCorpusTest, HostileFileNamesSurviveParsingAndAreSanitizedOnSave) {
  QByteArray raw;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseCorpus("golden/09-hostile-filenames.eml", raw, message));

  EMailMetaData meta;
  ASSERT_EQ(ExtractParts(message, meta), 0);
  ASSERT_EQ(meta.attachments.size(), 4);

  // Parsing reports the name as it arrived, untouched -- the sanitizing
  // belongs to the save path, not to the parse.
  EXPECT_EQ(meta.attachments[0].filename, QString("../../../../etc/passwd"));

  const auto names = UniqueAttachmentFileNames(meta.attachments);
  ASSERT_EQ(names.size(), 4);
  for (const auto& name : names) {
    EXPECT_FALSE(name.contains('/'));
    EXPECT_FALSE(name.contains('\\'));
    EXPECT_NE(name, QString("."));
    EXPECT_NE(name, QString(".."));
  }
  EXPECT_NE(names[1].toUpper(), QString("CON.TXT"));
  EXPECT_NE(names[2], names[3]);

  EXPECT_EQ(QSet<QString>(names.begin(), names.end()).size(), names.size());
}

TEST(EMailCorpusTest, TreeIndicesArePreOrderAndUnique) {
  for (const auto& name : CorpusNames()) {
    QByteArray raw;
    vmime::shared_ptr<vmime::message> message;
    ASSERT_TRUE(ParseCorpus(name, raw, message)) << name.toStdString();

    EMailPart root;
    QList<EMailSignatureRegion> regions;
    ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0)
        << name.toStdString();

    const auto flat = FlattenMimeTree(root);
    QSet<int> seen;
    for (int i = 0; i < flat.size(); ++i) {
      EXPECT_EQ(flat[i]->index, i) << name.toStdString();
      EXPECT_FALSE(seen.contains(flat[i]->index)) << name.toStdString();
      seen.insert(flat[i]->index);
    }
  }
}

TEST(EMailCorpusTest, EmptyLeadingTextPartDoesNotBecomeTheBody) {
  QByteArray raw;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseCorpus("golden/15-empty-body-parts.eml", raw, message));

  EMailMetaData meta;
  ASSERT_EQ(ExtractParts(message, meta), 0);
  EXPECT_TRUE(QString::fromUtf8(meta.body).contains("becomes the body"));
}

// --- structured crypto results ---------------------------------------------
//
// The module decodes the analyse call's JSON rather than re-reading its prose.
// These pin the decode, the region association and the declared-vs-actual
// hash check, none of which need a GPG engine to exercise.

namespace {

auto VerifyInfoJson(const QString& hash_algo) -> QByteArray {
  return QString(R"({
    "status": 1, "operation": "Verify", "engine": "GPG v2.4.1",
    "signatures": [
      {"fingerprint": "AAAA1111", "keyId": "1111", "uid": "Alice <a@example.com>",
       "pubkeyAlgo": "Ed25519", "hashAlgo": "%1",
       "signTime": "2025-03-04T10:18:00Z", "validity": 0, "warnings": []}
    ]})")
      .arg(hash_algo)
      .toUtf8();
}

auto RecipientInfoJson() -> QByteArray {
  return QByteArray(R"({
    "status": 1, "operation": "Decrypt",
    "recipients": [
      {"uid": "Bob <bob@example.com>", "fingerprint": "BBBB", "keyId": "2222",
       "pubkeyAlgo": "RSA", "keyFound": true, "algoIsPrimaryKey": false},
      {"uid": "", "fingerprint": "", "keyId": "0000000000000000",
       "pubkeyAlgo": "RSA", "keyFound": false, "algoIsPrimaryKey": false},
      {"uid": "Archive <archive@example.com>", "fingerprint": "CCCC",
       "keyId": "3333", "pubkeyAlgo": "RSA", "keyFound": true,
       "algoIsPrimaryKey": false}
    ]})");
}

}  // namespace

TEST(EMailCorpusTest, SignatureResultsCarryTheRegionTheyWereVerifiedFor) {
  const auto results = ParseSignatureResults(VerifyInfoJson("SHA256"), 7);

  ASSERT_EQ(results.size(), 1);
  // Stamped by the caller, which knows which region's bytes it just handed to
  // the engine. Nothing is inferred from position.
  EXPECT_EQ(results[0].region_id, 7);
  EXPECT_EQ(results[0].fingerprint, QString("AAAA1111"));
  EXPECT_EQ(results[0].pubkey_algo, QString("Ed25519"));
  EXPECT_EQ(results[0].hash_algo, QString("SHA256"));
  EXPECT_TRUE(results[0].sign_time.isValid());
}

TEST(EMailCorpusTest, AVerificationNotTiedToARegionSaysSo) {
  const auto results = ParseSignatureResults(VerifyInfoJson("SHA256"), -1);
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].region_id, -1);
}

TEST(EMailCorpusTest, MalformedResultJsonYieldsNothingRatherThanGuesses) {
  EXPECT_TRUE(ParseSignatureResults("not json at all", 0).isEmpty());
  EXPECT_TRUE(ParseSignatureResults("{}", 0).isEmpty());
  EXPECT_TRUE(ParseRecipientInfos("[]").isEmpty());
}

TEST(EMailCorpusTest, DeclaredMicalgIsCheckedAgainstTheActualHash) {
  EMailSignatureRegion region;
  region.region_id = 0;
  region.declared_micalg = "pgp-sha512";

  auto agreeing = ParseSignatureResults(VerifyInfoJson("SHA512"), 0);
  CheckMicalgAgreement(agreeing, region);
  ASSERT_EQ(agreeing.size(), 1);
  EXPECT_FALSE(agreeing[0].micalg_mismatch);

  auto disagreeing = ParseSignatureResults(VerifyInfoJson("SHA256"), 0);
  CheckMicalgAgreement(disagreeing, region);
  ASSERT_EQ(disagreeing.size(), 1);
  // The header said one thing and the signature did another. Neither value is
  // treated as authoritative; the disagreement is what gets reported.
  EXPECT_TRUE(disagreeing[0].micalg_mismatch);
}

TEST(EMailCorpusTest, MicalgCheckOnlyTouchesItsOwnRegion) {
  EMailSignatureRegion region;
  region.region_id = 1;
  region.declared_micalg = "pgp-sha512";

  auto results = ParseSignatureResults(VerifyInfoJson("SHA256"), 0);
  CheckMicalgAgreement(results, region);
  ASSERT_EQ(results.size(), 1);
  EXPECT_FALSE(results[0].micalg_mismatch);
}

TEST(EMailCorpusTest, MicalgCheckStaysQuietWhenNothingWasDeclared) {
  EMailSignatureRegion region;
  region.region_id = 0;

  auto results = ParseSignatureResults(VerifyInfoJson("SHA256"), 0);
  CheckMicalgAgreement(results, region);
  EXPECT_FALSE(results[0].micalg_mismatch);
}

// --- recipient cross-check -------------------------------------------------

TEST(EMailCorpusTest, RecipientMismatchIsAsymmetric) {
  const auto encrypted = ParseRecipientInfos(RecipientInfoJson());
  ASSERT_EQ(encrypted.size(), 3);

  const auto rows = MatchRecipients(
      {"Bob <bob@example.com>"}, {"Carol <carol@example.com>"}, {}, encrypted);

  QMap<QString, RecipientMatch> by_address;
  QList<RecipientMatch> unaddressed;
  for (const auto& row : rows) {
    if (row.address.isEmpty()) {
      unaddressed.append(row.match);
    } else {
      by_address.insert(AddressOfUid(row.address), row.match);
    }
  }

  // Addressed and encrypted to: fine.
  EXPECT_EQ(by_address.value("bob@example.com"), RecipientMatch::kMATCHED);

  // Addressed but NOT encrypted to: the one genuine warning. Carol was told
  // this message is for her and cannot open it.
  EXPECT_EQ(by_address.value("carol@example.com"),
            RecipientMatch::kADDRESSED_NOT_ENCRYPTED);

  // Encrypted to but not addressed: ordinary, and never a warning. One is an
  // archive key, the other was deliberately hidden by the sender.
  ASSERT_EQ(unaddressed.size(), 2);
  EXPECT_TRUE(unaddressed.contains(RecipientMatch::kENCRYPTED_NOT_ADDRESSED));
  EXPECT_TRUE(unaddressed.contains(RecipientMatch::kHIDDEN_RECIPIENT));
}

TEST(EMailCorpusTest, HiddenRecipientsAreNamedRatherThanGuessedAt) {
  const auto encrypted = ParseRecipientInfos(RecipientInfoJson());
  const auto* hidden = static_cast<const EMailRecipientInfo*>(nullptr);
  for (const auto& info : encrypted) {
    if (info.hidden) hidden = &info;
  }
  ASSERT_NE(hidden, nullptr);
  // An all-zero key id is the sender asking the engine to withhold the
  // recipient, not a key that failed to resolve.
  EXPECT_FALSE(hidden->key_found);
  EXPECT_EQ(hidden->key_id, QString("0000000000000000"));
}

TEST(EMailCorpusTest, ABccRecipientCountsAsAddressed) {
  const auto encrypted = ParseRecipientInfos(RecipientInfoJson());

  // BCC recipients are real encryption recipients; they simply never reach a
  // header. Matching must see them or every BCC would read as an extra.
  const auto rows =
      MatchRecipients({}, {}, {"Bob <bob@example.com>"}, encrypted);

  bool matched_bcc = false;
  for (const auto& row : rows) {
    if (row.header_field == "Bcc" && row.match == RecipientMatch::kMATCHED) {
      matched_bcc = true;
    }
  }
  EXPECT_TRUE(matched_bcc);
}

TEST(EMailCorpusTest, AnUnencryptedMessageMakesEveryAddressAWarning) {
  const auto rows =
      MatchRecipients({"a@example.com"}, {"b@example.com"}, {}, {});
  ASSERT_EQ(rows.size(), 2);
  for (const auto& row : rows) {
    EXPECT_EQ(row.match, RecipientMatch::kADDRESSED_NOT_ENCRYPTED);
  }
}

TEST(EMailCorpusTest, OneKeyIsNotCreditedToTwoAddresses) {
  const auto encrypted = ParseRecipientInfos(RecipientInfoJson());
  const auto rows = MatchRecipients(
      {"Bob <bob@example.com>", "Bob <bob@example.com>"}, {}, {}, encrypted);

  int matched = 0;
  for (const auto& row : rows) {
    if (row.match == RecipientMatch::kMATCHED) ++matched;
  }
  EXPECT_EQ(matched, 1);
}

TEST(EMailCorpusTest, AddressExtractionHandlesTheUidForms) {
  EXPECT_EQ(AddressOfUid("Alice <a@example.com>"), QString("a@example.com"));
  EXPECT_EQ(AddressOfUid("Alice (work) <a@example.com>"),
            QString("a@example.com"));
  EXPECT_EQ(AddressOfUid("a@example.com"), QString("a@example.com"));
  EXPECT_EQ(AddressOfUid("A@Example.COM"), QString("a@example.com"));
  EXPECT_TRUE(AddressOfUid("no address here").isEmpty());
  EXPECT_TRUE(AddressOfUid("").isEmpty());
}

// --- BCC never becomes a header --------------------------------------------
//
// Compose state is not message state. A Bcc header defeats the entire point
// of a blind copy, and these bytes are what the user saves and sends.

TEST(EMailCorpusTest, BlindRecipientsNeverReachTheMessage) {
  EMailMetaData meta;
  meta.from = "alice@example.com";
  meta.to = {"bob@example.com"};
  meta.subject = "subject";
  // Even when a Bcc somehow arrives on the metadata -- a message parsed from a
  // source that carried one -- serializing must not write it back out.
  meta.bcc_header = {"secret@example.com"};

  QString eml;
  ASSERT_EQ(BuildPlainTextEML(meta, "body", eml), 0);

  EXPECT_FALSE(eml.contains("secret@example.com"));
  EXPECT_FALSE(eml.contains(QRegularExpression(
      "^Bcc:", QRegularExpression::MultilineOption |
                   QRegularExpression::CaseInsensitiveOption)));

  // The rest of the envelope is untouched.
  EXPECT_TRUE(eml.contains("bob@example.com"));
  EXPECT_TRUE(eml.contains("subject"));
}

TEST(EMailCorpusTest, AMessageAddressedOnlyBlindlyStillRoundTrips) {
  EMailMetaData meta;
  meta.from = "alice@example.com";
  meta.subject = "blind only";
  meta.bcc_header = {"secret@example.com"};

  QString eml;
  ASSERT_EQ(BuildPlainTextEML(meta, "body", eml), 0);
  EXPECT_FALSE(eml.contains("secret@example.com"));

  // With no visible addressee it takes the draft path rather than failing, so
  // the subject and body the user wrote are not lost.
  vmime::shared_ptr<vmime::message> parsed;
  ASSERT_TRUE(CheckIfEMLMessage(eml.toUtf8(), parsed));

  EMailMetaData reparsed;
  ASSERT_EQ(GetEMLMetaData(parsed, reparsed), 0);
  EXPECT_EQ(reparsed.subject, QString("blind only"));
  EXPECT_TRUE(reparsed.bcc_header.isEmpty());
}

TEST(EMailCorpusTest, AnIncomingBccHeaderIsStillReadable) {
  // The other half of the rule: a message that really does carry a Bcc header
  // is unusual and worth seeing, so parsing reports it. Only writing is
  // forbidden.
  const QByteArray raw =
      "From: a@example.com\r\n"
      "To: b@example.com\r\n"
      "Bcc: hidden@example.com\r\n"
      "Subject: s\r\n"
      "MIME-Version: 1.0\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n"
      "body\r\n";

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(raw, message));

  EMailMetaData meta;
  ASSERT_EQ(GetEMLMetaData(message, meta), 0);
  EXPECT_EQ(meta.bcc_header, QStringList({"hidden@example.com"}));
}

// --- deriving a new message ------------------------------------------------
//
// Reply and Forward generate a NEW document. Nothing tracks a thread, nothing
// touches a mailbox, and the message being answered is never modified.

namespace {

auto SourceForReply() -> EMailMetaData {
  EMailMetaData source;
  source.from = "Alice <alice@example.com>";
  source.to = {"Me <me@example.com>", "Carol <carol@example.com>"};
  source.cc = {"Dave <dave@example.com>"};
  source.bcc_header = {"Blind <blind@example.com>"};
  source.subject = "Quarterly numbers";
  source.message_id = "<orig-1@example.com>";
  source.references = {"<older-1@example.com>"};
  source.body = "The numbers are attached.";
  source.datetime = QDateTime::fromString("2025-03-04T10:15:00Z", Qt::ISODate);
  return source;
}

}  // namespace

TEST(EMailCorpusTest, ReplyGoesToTheSenderAlone) {
  EMailMetaData out;
  BuildDerivedMetaData(SourceForReply(), EMailReplyMode::kREPLY,
                       "me@example.com", out);

  EXPECT_EQ(out.to, QStringList({"Alice <alice@example.com>"}));
  EXPECT_TRUE(out.cc.isEmpty());
  EXPECT_EQ(out.subject, QString("Re: Quarterly numbers"));
}

TEST(EMailCorpusTest, ReplyHonoursReplyToOverFrom) {
  auto source = SourceForReply();
  source.reply_to = "Alice Support <support@example.com>";

  EMailMetaData out;
  BuildDerivedMetaData(source, EMailReplyMode::kREPLY, "me@example.com", out);

  // Reply-To exists so the sender can redirect answers. Ignoring it sends the
  // reply somewhere they explicitly asked it not to go.
  EXPECT_EQ(out.to, QStringList({"Alice Support <support@example.com>"}));
}

TEST(EMailCorpusTest, ReplyAllKeepsEveryoneVisibleAndDropsTheUser) {
  EMailMetaData out;
  BuildDerivedMetaData(SourceForReply(), EMailReplyMode::kREPLY_ALL,
                       "me@example.com", out);

  EXPECT_EQ(out.to, QStringList({"Alice <alice@example.com>"}));

  QStringList cc_addresses;
  for (const auto& entry : out.cc) cc_addresses.append(AddressOfUid(entry));

  EXPECT_TRUE(cc_addresses.contains("carol@example.com"));
  EXPECT_TRUE(cc_addresses.contains("dave@example.com"));
  // Replying to yourself is noise.
  EXPECT_FALSE(cc_addresses.contains("me@example.com"));
  // And never the blind recipients: they were blind, and a reply-all that
  // names them undoes the sender's choice after the fact.
  EXPECT_FALSE(cc_addresses.contains("blind@example.com"));
}

TEST(EMailCorpusTest, ReplyAllAddressesEachPersonOnce) {
  auto source = SourceForReply();
  source.cc.append("carol@example.com");  // same person, bare form

  EMailMetaData out;
  BuildDerivedMetaData(source, EMailReplyMode::kREPLY_ALL, "me@example.com",
                       out);

  int carol = 0;
  for (const auto& entry : out.cc) {
    if (AddressOfUid(entry) == "carol@example.com") ++carol;
  }
  EXPECT_EQ(carol, 1);
}

TEST(EMailCorpusTest, SubjectPrefixesDoNotStack) {
  auto source = SourceForReply();
  source.subject = "Re: Quarterly numbers";

  EMailMetaData out;
  BuildDerivedMetaData(source, EMailReplyMode::kREPLY, "me@example.com", out);
  EXPECT_EQ(out.subject, QString("Re: Quarterly numbers"));

  BuildDerivedMetaData(source, EMailReplyMode::kFORWARD, "me@example.com", out);
  EXPECT_EQ(out.subject, QString("Fwd: Re: Quarterly numbers"));
}

TEST(EMailCorpusTest, RepliesPointBackAtWhatTheyAnswer) {
  EMailMetaData out;
  BuildDerivedMetaData(SourceForReply(), EMailReplyMode::kREPLY,
                       "me@example.com", out);

  EXPECT_EQ(out.in_reply_to, QString("<orig-1@example.com>"));
  // The chain accumulates rather than being replaced.
  EXPECT_EQ(out.references,
            QStringList({"<older-1@example.com>", "<orig-1@example.com>"}));
}

TEST(EMailCorpusTest, ForwardCarriesAttachmentsButReplyDoesNot) {
  auto source = SourceForReply();
  EMailAttachment att;
  att.filename = "numbers.pdf";
  att.mime_type = "application/pdf";
  att.data = "pdf";
  source.attachments.append(att);

  EMailMetaData forwarded;
  BuildDerivedMetaData(source, EMailReplyMode::kFORWARD, "me@example.com",
                       forwarded);
  ASSERT_EQ(forwarded.attachments.size(), 1);
  EXPECT_EQ(forwarded.attachments[0].filename, QString("numbers.pdf"));
  // A forward is addressed by the person forwarding it; guessing recipients is
  // how mail reaches the wrong people.
  EXPECT_TRUE(forwarded.to.isEmpty());

  EMailMetaData replied;
  BuildDerivedMetaData(source, EMailReplyMode::kREPLY, "me@example.com",
                       replied);
  // Silently re-sending someone's files back to them is a surprise at best.
  EXPECT_TRUE(replied.attachments.isEmpty());
}

TEST(EMailCorpusTest, QuotingMarksEveryLineIncludingBlankOnes) {
  auto source = SourceForReply();
  source.body = "first\n\nsecond";

  const auto quoted =
      QString::fromUtf8(BuildQuotedBody(source, EMailReplyMode::kREPLY));

  EXPECT_TRUE(quoted.contains("> first"));
  EXPECT_TRUE(quoted.contains("> second"));
  // An unquoted blank line reads as the end of the quotation.
  EXPECT_TRUE(quoted.contains("\n>\n"));
  EXPECT_TRUE(quoted.contains("alice@example.com"));
}

TEST(EMailCorpusTest, ForwardReproducesTheOriginalHeaders) {
  const auto forwarded = QString::fromUtf8(
      BuildQuotedBody(SourceForReply(), EMailReplyMode::kFORWARD));

  EXPECT_TRUE(forwarded.contains("Forwarded message"));
  EXPECT_TRUE(forwarded.contains("From: Alice <alice@example.com>"));
  EXPECT_TRUE(forwarded.contains("Subject: Quarterly numbers"));
  // A forward is read on its own, so the original text must not be quoted
  // away behind markers.
  EXPECT_TRUE(forwarded.contains("The numbers are attached."));
  EXPECT_FALSE(forwarded.contains("> The numbers"));
}

TEST(EMailCorpusTest, DerivingDoesNotModifyTheSourceMessage) {
  const auto before = SourceForReply();
  auto source = SourceForReply();

  EMailMetaData out;
  BuildDerivedMetaData(source, EMailReplyMode::kREPLY_ALL, "me@example.com",
                       out);
  (void)BuildQuotedBody(source, EMailReplyMode::kREPLY_ALL);

  EXPECT_EQ(source.to, before.to);
  EXPECT_EQ(source.cc, before.cc);
  EXPECT_EQ(source.subject, before.subject);
  EXPECT_EQ(source.body, before.body);
  EXPECT_EQ(source.references, before.references);
}

TEST(EMailCorpusTest, ThreadingHeadersReachTheGeneratedMessage) {
  EMailMetaData out;
  BuildDerivedMetaData(SourceForReply(), EMailReplyMode::kREPLY,
                       "me@example.com", out);

  QString eml;
  ASSERT_EQ(BuildPlainTextEML(out, "reply body", eml), 0);

  EXPECT_TRUE(eml.contains("In-Reply-To:"));
  EXPECT_TRUE(eml.contains("<orig-1@example.com>"));
  EXPECT_TRUE(eml.contains("References:"));
}

TEST(EMailCorpusTest, AnOrdinaryMessageGainsNoThreadingHeaders) {
  EMailMetaData meta;
  meta.from = "a@example.com";
  meta.to = {"b@example.com"};
  meta.subject = "fresh";

  QString eml;
  ASSERT_EQ(BuildPlainTextEML(meta, "body", eml), 0);

  EXPECT_FALSE(eml.contains("In-Reply-To:"));
  EXPECT_FALSE(eml.contains("References:"));
}

// --- deceptive structure and preflight -------------------------------------
//
// Every check here is a local read of the message in hand. The levels matter
// as much as the findings: a tool that shouts at ordinary mail gets ignored
// exactly when it is right.

namespace {

auto InspectCorpus(const QString& name) -> QList<EMailFinding> {
  QByteArray raw;
  vmime::shared_ptr<vmime::message> message;
  EXPECT_TRUE(ParseCorpus(name, raw, message)) << name.toStdString();

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  EXPECT_EQ(ParseMimeTree(message, raw, root, regions), 0);

  EMailMetaData meta;
  EXPECT_EQ(GetEMLMetaData(message, meta), 0);
  EXPECT_EQ(ExtractParts(message, meta), 0);

  return InspectMessage(meta, root, regions, raw);
}

auto HasFinding(const QList<EMailFinding>& findings, const QString& fragment)
    -> bool {
  for (const auto& finding : findings) {
    if (finding.title.contains(fragment, Qt::CaseInsensitive)) return true;
  }
  return false;
}

auto WorstLevel(const QList<EMailFinding>& findings) -> EMailFindingLevel {
  auto worst = EMailFindingLevel::kNOTE;
  for (const auto& finding : findings) {
    if (finding.level > worst) worst = finding.level;
  }
  return worst;
}

}  // namespace

TEST(EMailCorpusTest, AnOrdinaryMessageProducesNoAlarms) {
  // The most important case. If a plain, correct message raises anything, the
  // findings stop being read at all.
  const auto findings = InspectCorpus("golden/01-plain-text.eml");
  EXPECT_TRUE(findings.isEmpty()) << findings.size();

  EXPECT_TRUE(InspectCorpus("golden/02-alternative.eml").isEmpty());
  EXPECT_TRUE(InspectCorpus("golden/03-mixed-attachments.eml").isEmpty());
}

TEST(EMailCorpusTest, DuplicateSingletonHeadersAreFlagged) {
  const QByteArray raw =
      "From: a@example.com\r\n"
      "From: attacker@example.net\r\n"
      "To: b@example.com\r\n"
      "Subject: s\r\n"
      "MIME-Version: 1.0\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n"
      "body\r\n";

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(raw, message));

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0);

  EMailMetaData meta;
  ASSERT_EQ(GetEMLMetaData(message, meta), 0);

  const auto findings = InspectMessage(meta, root, regions);
  EXPECT_TRUE(HasFinding(findings, "Duplicate"));
  // Readers disagree about which copy wins, so this is genuinely wrong rather
  // than merely untidy.
  EXPECT_EQ(WorstLevel(findings), EMailFindingLevel::kRISK);
}

TEST(EMailCorpusTest, AReplyToOnAnotherDomainIsNotedNotAccused) {
  const auto findings = InspectCorpus("golden/12-odd-headers.eml");

  EXPECT_TRUE(HasFinding(findings, "Replies go somewhere else"));

  for (const auto& finding : findings) {
    if (!finding.title.contains("Replies")) continue;
    // Mailing lists and ticket systems do this constantly. It is worth seeing
    // and not worth an accusation.
    EXPECT_EQ(finding.level, EMailFindingLevel::kWARN);
  }
}

TEST(EMailCorpusTest, HomographDomainsAreRecognised) {
  // Cyrillic "а" in place of Latin "a": identical on screen, different domain.
  EXPECT_TRUE(LooksLikeSpoofedAddress(
      QString::fromUtf8("Support <support@exаmple.com>")));
  EXPECT_TRUE(LooksLikeSpoofedAddress(QString::fromUtf8("a@рaypal.com")));

  // And ordinary addresses are left alone.
  EXPECT_FALSE(LooksLikeSpoofedAddress("Alice <alice@example.com>"));
  EXPECT_FALSE(LooksLikeSpoofedAddress("alice@sub.example.co.uk"));
  EXPECT_FALSE(LooksLikeSpoofedAddress("not an address"));
  EXPECT_FALSE(LooksLikeSpoofedAddress(""));
}

TEST(EMailCorpusTest, UnsignedPartsBesideASignatureAreReported) {
  const auto findings = InspectCorpus("golden/14-signed-with-attachment.eml");
  EXPECT_TRUE(HasFinding(findings, "Not everything is signed"));
}

TEST(EMailCorpusTest, AFullySignedMessageIsNotReportedAsPartlyUnsigned) {
  const auto findings = InspectCorpus("golden/04-pgpmime-signed.eml");
  EXPECT_FALSE(HasFinding(findings, "Not everything is signed"));
}

TEST(EMailCorpusTest, AVisibleBccHeaderOnAReceivedMessageIsReported) {
  const QByteArray raw =
      "From: a@example.com\r\n"
      "To: b@example.com\r\n"
      "Bcc: blind@example.com\r\n"
      "Subject: s\r\n"
      "MIME-Version: 1.0\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n"
      "body\r\n";

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(raw, message));

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0);

  EMailMetaData meta;
  ASSERT_EQ(GetEMLMetaData(message, meta), 0);

  EXPECT_TRUE(HasFinding(InspectMessage(meta, root, regions), "Blind"));
}

// --- preflight -------------------------------------------------------------

TEST(EMailCorpusTest, PreflightRefusesToLetABccHeaderOut) {
  EMailMetaData meta;
  meta.from = "a@example.com";
  meta.to = {"b@example.com"};
  meta.bcc_header = {"blind@example.com"};

  const auto findings = PreflightMessage(meta, EMailPart{}, {}, {});
  EXPECT_TRUE(HasFinding(findings, "Bcc header present"));
  EXPECT_EQ(WorstLevel(findings), EMailFindingLevel::kRISK);
}

TEST(EMailCorpusTest, PreflightExplainsWhereBlindRecipientsGo) {
  EMailMetaData meta;
  meta.from = "a@example.com";
  meta.to = {"b@example.com"};

  const auto findings =
      PreflightMessage(meta, EMailPart{}, {}, {"blind@example.com"});

  EXPECT_TRUE(HasFinding(findings, "Blind recipients"));
  // Correct behaviour, so it is stated rather than warned about.
  for (const auto& finding : findings) {
    if (finding.title.contains("Blind")) {
      EXPECT_EQ(finding.level, EMailFindingLevel::kNOTE);
    }
  }
}

TEST(EMailCorpusTest, PreflightNoticesAnUnaddressedMessage) {
  EMailMetaData meta;
  meta.from = "a@example.com";

  EXPECT_TRUE(
      HasFinding(PreflightMessage(meta, EMailPart{}, {}, {}), "No recipients"));
}

TEST(EMailCorpusTest, PreflightMentionsThatNothingIsSigned) {
  EMailMetaData meta;
  meta.from = "a@example.com";
  meta.to = {"b@example.com"};

  const auto findings = PreflightMessage(meta, EMailPart{}, {}, {});
  EXPECT_TRUE(HasFinding(findings, "Nothing is signed"));
  // An unsigned message is the ordinary case, not a fault.
  for (const auto& finding : findings) {
    if (finding.title.contains("Nothing is signed")) {
      EXPECT_EQ(finding.level, EMailFindingLevel::kNOTE);
    }
  }
}

TEST(EMailCorpusTest, PreflightCatchesADisguisedRecipient) {
  EMailMetaData meta;
  meta.from = "a@example.com";
  meta.to = {QString::fromUtf8("Bank <security@bаnk.com>")};

  const auto findings = PreflightMessage(meta, EMailPart{}, {}, {});
  EXPECT_TRUE(HasFinding(findings, "disguised"));
  EXPECT_EQ(WorstLevel(findings), EMailFindingLevel::kRISK);
}

TEST(EMailCorpusTest, ReplyToIsActuallyParsed) {
  // Regression: Reply-To is an address list in RFC 5322, and vmime registers
  // it as one. Reading it as a single mailbox made getValue<mailbox>() return
  // null every time, so this field was silently always empty -- replies
  // ignored it, and a Reply-To pointing at an unrelated domain could not be
  // noticed at all.
  QByteArray raw;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseCorpus("golden/12-odd-headers.eml", raw, message));

  EMailMetaData meta;
  ASSERT_EQ(GetEMLMetaData(message, meta), 0);

  EXPECT_FALSE(meta.reply_to.isEmpty());
  EXPECT_TRUE(meta.reply_to.contains("no-reply@elsewhere.invalid"));
}

TEST(EMailCorpusTest, ARepliedMessageUsesTheParsedReplyTo) {
  QByteArray raw;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseCorpus("golden/12-odd-headers.eml", raw, message));

  EMailMetaData meta;
  ASSERT_EQ(GetEMLMetaData(message, meta), 0);

  EMailMetaData reply;
  BuildDerivedMetaData(meta, EMailReplyMode::kREPLY, "bob@example.com", reply);

  ASSERT_EQ(reply.to.size(), 1);
  EXPECT_EQ(AddressOfUid(reply.to[0]), QString("no-reply@elsewhere.invalid"));
}

TEST(EMailCorpusTest, AnAttachedPublicKeyIsNotedWithoutImplyingTrust) {
  const auto findings = InspectCorpus("golden/10-pgp-keys-attachment.eml");
  EXPECT_TRUE(HasFinding(findings, "Public key attached"));

  for (const auto& finding : findings) {
    if (!finding.title.contains("Public key")) continue;
    // Attaching a key is entirely normal, so this is a note. What it must not
    // do is suggest the key belongs to whoever the From line names.
    EXPECT_EQ(finding.level, EMailFindingLevel::kNOTE);
    EXPECT_TRUE(finding.detail.contains("proves nothing"));
  }
}

TEST(EMailCorpusTest, ASignaturePartOfTheStructureIsNotCalledDetached) {
  // The pgp-signature inside a multipart/signed is the message's own
  // signature, not a file that happens to be a signature.
  const auto findings = InspectCorpus("golden/04-pgpmime-signed.eml");
  EXPECT_FALSE(HasFinding(findings, "Detached signature"));
}

// A signature is computed over exact octets in MIME canonical form. When a
// message reaches the user with those octets rewritten, verification fails in
// a way that is indistinguishable from a forgery -- and importing the sender's
// key, the obvious thing to try, cannot possibly help. These tests pin the
// detection that lets the difference be explained.

TEST(EMailCorpusTest, BareLineFeedsAreDetectedInSignedBytes) {
  EXPECT_FALSE(HasBareLineFeeds(QByteArray("a\r\nb\r\n")));
  EXPECT_TRUE(HasBareLineFeeds(QByteArray("a\r\nb\n")));
  EXPECT_TRUE(HasBareLineFeeds(QByteArray("\na\r\n")));
  EXPECT_FALSE(HasBareLineFeeds(QByteArray("no line endings at all")));
  EXPECT_FALSE(HasBareLineFeeds(QByteArray()));
  // A lone CR is not this problem, and claiming it would be is worse than
  // saying nothing.
  EXPECT_FALSE(HasBareLineFeeds(QByteArray("a\rb")));
}

TEST(EMailCorpusTest, ASignedPartRewrittenToLfIsReported) {
  const auto findings = InspectCorpus("golden/16-signed-lf-mangled.eml");

  EXPECT_TRUE(HasFinding(findings, "canonical form"));

  // A warning, not a risk: the overwhelmingly likely cause is a program that
  // normalised line endings, not an attacker, and the finding says so.
  for (const auto& finding : findings) {
    if (!finding.title.contains("canonical")) continue;
    EXPECT_EQ(finding.level, EMailFindingLevel::kWARN);
    // The point of the finding is the explanation, so it has to name the one
    // thing the user would otherwise waste their time on.
    EXPECT_TRUE(finding.detail.contains("importing", Qt::CaseInsensitive));
  }
}

TEST(EMailCorpusTest, IntactSignedMessagesAreNotAccusedOfBeingRewritten) {
  // The check has to stay silent on every well-formed message, or it becomes
  // noise on exactly the messages that are fine.
  for (const auto& name :
       {"golden/04-pgpmime-signed.eml", "golden/06-signed-inside-encrypted.eml",
        "golden/11-nested-signatures.eml",
        "golden/13-signed-micalg-mismatch.eml",
        "golden/14-signed-with-attachment.eml"}) {
    EXPECT_FALSE(HasFinding(InspectCorpus(name), "canonical form")) << name;
  }
}

TEST(EMailCorpusTest, TheCheckNeedsTheOriginalBytesToSayAnything) {
  // Called without the document, it reports nothing rather than guessing --
  // the signed bytes are the only thing that can answer the question.
  QByteArray raw;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseCorpus("golden/16-signed-lf-mangled.eml", raw, message));

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, raw, root, regions), 0);
  EMailMetaData meta;
  ASSERT_EQ(GetEMLMetaData(message, meta), 0);

  EXPECT_FALSE(HasFinding(InspectMessage(meta, root, regions), "canonical"));
  EXPECT_TRUE(
      HasFinding(InspectMessage(meta, root, regions, raw), "canonical"));
}

// --- lifting a protected layer off a message -------------------------------
//
// "Remove the signature" has to leave a message that is still the message. The
// tests below are mostly about what is NOT allowed to change: the octets of
// the signed entity, and of anything signed inside it.

namespace {

struct UnwrapCase {
  QByteArray raw;
  EMailPart root;
  QList<EMailSignatureRegion> regions;
};

auto LoadForUnwrap(const QString& name) -> UnwrapCase {
  UnwrapCase c;
  vmime::shared_ptr<vmime::message> message;
  EXPECT_TRUE(ParseCorpus(name, c.raw, message)) << name.toStdString();
  EXPECT_EQ(ParseMimeTree(message, c.raw, c.root, c.regions), 0);
  return c;
}

auto Unwrap(const QString& name, QByteArray& out) -> EMailUnwrapResult {
  const auto c = LoadForUnwrap(name);
  return UnwrapProtectedLayer(c.root, c.raw, out);
}

// The header block of a message, as bytes -- everything before the first empty
// line. Used to assert on what the unwrap kept and dropped.
auto HeaderBlockOf(const QByteArray& eml) -> QByteArray {
  const int crlf = eml.indexOf("\r\n\r\n");
  if (crlf >= 0) return eml.left(crlf + 2);
  const int lf = eml.indexOf("\n\n");
  return lf >= 0 ? eml.left(lf + 1) : eml;
}

}  // namespace

TEST(EMailCorpusTest, UnwrappingASignedMessageYieldsAnOrdinaryOne) {
  QByteArray out;
  ASSERT_EQ(Unwrap("golden/04-pgpmime-signed.eml", out),
            EMailUnwrapResult::kOK);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(out, message));

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, out, root, regions), 0);

  EXPECT_EQ(ClassifyOpenPGPStructure(root, regions),
            EMailSecurityState::kPLAIN);
  EXPECT_TRUE(regions.isEmpty());

  // The message is still the same message: the envelope survives the unwrap.
  const auto before = LoadForUnwrap("golden/04-pgpmime-signed.eml");
  vmime::shared_ptr<vmime::message> original;
  ASSERT_TRUE(CheckIfEMLMessage(before.raw, original));
  EMailMetaData meta_before;
  EMailMetaData meta_after;
  ASSERT_EQ(GetEMLMetaData(original, meta_before), 0);
  ASSERT_EQ(GetEMLMetaData(message, meta_after), 0);
  EXPECT_EQ(meta_after.subject, meta_before.subject);
  EXPECT_EQ(meta_after.from, meta_before.from);
  EXPECT_EQ(meta_after.to, meta_before.to);
}

TEST(EMailCorpusTest, UnwrappingTakesTheSignedEntityByteForByte) {
  const auto c = LoadForUnwrap("golden/04-pgpmime-signed.eml");
  QByteArray out;
  ASSERT_EQ(UnwrapProtectedLayer(c.root, c.raw, out), EMailUnwrapResult::kOK);

  ASSERT_EQ(c.root.children.size(), 2);
  const auto& entity = c.root.children.at(0);
  const auto expected = c.raw.mid(static_cast<int>(entity.raw_offset),
                                  static_cast<int>(entity.raw_length));

  // Not "contains" and not "reparses the same": the entity's octets are what a
  // nested signature would cover, so they must be carried over untouched.
  ASSERT_FALSE(expected.isEmpty());
  EXPECT_TRUE(out.endsWith(expected));
}

TEST(EMailCorpusTest, UnwrappingKeepsANestedSignatureByteExact) {
  // The load-bearing case. Removing the outer signature must leave the inner
  // one verifiable, which is only true if its bytes never went through a
  // reserialization.
  const auto c = LoadForUnwrap("golden/11-nested-signatures.eml");
  ASSERT_GE(c.regions.size(), 2);

  // The innermost region is the one that has to survive.
  const EMailSignatureRegion* inner = &c.regions.at(0);
  for (const auto& region : c.regions) {
    if (region.nesting_depth > inner->nesting_depth) inner = &region;
  }
  const auto inner_bytes = c.raw.mid(static_cast<int>(inner->raw_offset),
                                     static_cast<int>(inner->raw_length));
  ASSERT_FALSE(inner_bytes.isEmpty());

  QByteArray out;
  ASSERT_EQ(UnwrapProtectedLayer(c.root, c.raw, out), EMailUnwrapResult::kOK);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(out, message));
  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, out, root, regions), 0);

  EXPECT_EQ(regions.size(), c.regions.size() - 1);
  ASSERT_FALSE(regions.isEmpty());

  bool found = false;
  for (const auto& region : regions) {
    const auto bytes = out.mid(static_cast<int>(region.raw_offset),
                               static_cast<int>(region.raw_length));
    if (bytes == inner_bytes) found = true;
  }
  EXPECT_TRUE(found) << "the nested signed bytes did not survive the unwrap";
}

TEST(EMailCorpusTest, UnwrappingASignatureOverCiphertextLeavesAnEncrypted) {
  // Signing wrapped around an encrypted part: removing the signature is a
  // legitimate answer, and what is left is an encrypted, unsigned message.
  QByteArray out;
  ASSERT_EQ(Unwrap("golden/17-encrypted-inside-signed.eml", out),
            EMailUnwrapResult::kOK);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(out, message));
  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, out, root, regions), 0);

  EXPECT_EQ(ClassifyOpenPGPStructure(root, regions),
            EMailSecurityState::kENCRYPTED);
}

TEST(EMailCorpusTest, UnwrappingRefusesWhenTheSignatureIsInsideTheCiphertext) {
  // There may well be a signature in there, but it is not visible from here
  // and guessing would be worse than saying so.
  QByteArray out;
  EXPECT_EQ(Unwrap("golden/05-pgpmime-encrypted.eml", out),
            EMailUnwrapResult::kNOT_SUPPORTED);
  EXPECT_TRUE(out.isEmpty());
}

TEST(EMailCorpusTest, UnwrappingFindsASignedSubtreeBesideAnEncryptedOne) {
  // multipart/mixed { multipart/encrypted, multipart/signed }. The signature
  // that IS visible can be removed; the encrypted part is left alone.
  QByteArray out;
  ASSERT_EQ(Unwrap("golden/06-signed-inside-encrypted.eml", out),
            EMailUnwrapResult::kOK);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(out, message));
  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, out, root, regions), 0);

  EXPECT_TRUE(regions.isEmpty());
  EXPECT_EQ(ClassifyOpenPGPStructure(root, regions),
            EMailSecurityState::kENCRYPTED);
}

TEST(EMailCorpusTest, UnwrappingASignedSubtreeKeepsEverythingAroundIt) {
  // The realistic shape: multipart/mixed { multipart/signed { ... }, extra }.
  // Only the wrapper goes; the sibling attachment and the message headers are
  // untouched bytes on either side of the splice.
  const auto c = LoadForUnwrap("golden/14-signed-with-attachment.eml");
  QByteArray out;
  ASSERT_EQ(UnwrapProtectedLayer(c.root, c.raw, out), EMailUnwrapResult::kOK);

  // The message headers come through byte-for-byte -- nothing was filtered,
  // because the wrapper was never the message itself.
  const auto original_headers = HeaderBlockOf(c.raw);
  EXPECT_TRUE(out.startsWith(original_headers));

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(out, message));
  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_EQ(ParseMimeTree(message, out, root, regions), 0);

  EXPECT_TRUE(regions.isEmpty());
  EXPECT_EQ(ClassifyOpenPGPStructure(root, regions),
            EMailSecurityState::kPLAIN);

  // The part that was never covered by the signature is still there.
  EMailMetaData meta;
  ASSERT_EQ(GetEMLMetaData(message, meta), 0);
  ASSERT_EQ(ExtractParts(message, meta), 0);
  bool kept_uncovered = false;
  for (const auto& att : meta.attachments) {
    if (att.filename.contains("uncovered")) kept_uncovered = true;
  }
  EXPECT_TRUE(kept_uncovered);
}

TEST(EMailCorpusTest, UnwrappingAPlainMessageIsANoOp) {
  for (const auto* name :
       {"golden/01-plain-text.eml", "golden/03-mixed-attachments.eml"}) {
    QByteArray out;
    EXPECT_EQ(Unwrap(name, out), EMailUnwrapResult::kNOT_PROTECTED) << name;
    EXPECT_TRUE(out.isEmpty()) << name;
  }
}

TEST(EMailCorpusTest, UnwrappingDropsOnlyTheOuterContentHeaders) {
  // A message that IS the multipart/signed: its headers are the message's, so
  // they are filtered rather than spliced around.
  const auto c = LoadForUnwrap("golden/04-pgpmime-signed.eml");
  QByteArray out;
  ASSERT_EQ(UnwrapProtectedLayer(c.root, c.raw, out), EMailUnwrapResult::kOK);

  const auto original_headers = HeaderBlockOf(c.raw);
  const auto headers = HeaderBlockOf(out);

  // Everything that identifies the message is kept exactly as it was written.
  for (const auto& field : SplitRawHeaderFields(original_headers)) {
    if (field.name.startsWith("Content-", Qt::CaseInsensitive)) continue;
    EXPECT_TRUE(headers.contains(field.raw_line))
        << "lost header: " << field.name.toStdString();
  }

  // ...and the wrapper's own description of itself is gone, continuation
  // lines included. An orphaned boundary= line is the failure this guards.
  EXPECT_FALSE(headers.toLower().contains("multipart/signed"));
  EXPECT_FALSE(headers.toLower().contains("boundary="));

  // Exactly one Content-Type survives, and it is the entity's: describing the
  // content is the entity's job now that the wrapper describing it is gone.
  // Two would mean the wrapper's was kept alongside it.
  QList<QByteArray> content_types;
  for (const auto& field : SplitRawHeaderFields(headers)) {
    if (field.name.compare("Content-Type", Qt::CaseInsensitive) == 0) {
      content_types.append(field.value);
    }
  }
  ASSERT_EQ(content_types.size(), 1);
  EXPECT_TRUE(content_types.at(0).toLower().startsWith("text/plain"))
      << content_types.at(0).toStdString();
}

TEST(EMailCorpusTest, UnwrappingNeverIntroducesBareLineFeeds) {
  // A bare LF where CRLF was makes a signature fail in a way that looks
  // exactly like a forgery -- see 16-signed-lf-mangled.eml for why that
  // distinction matters. The unwrap must never be the cause of one.
  for (const auto& name : CorpusNames()) {
    const auto c = LoadForUnwrap(name);
    if (ClassifyOpenPGPStructure(c.root, c.regions) !=
        EMailSecurityState::kSIGNED) {
      continue;
    }

    QByteArray out;
    if (UnwrapProtectedLayer(c.root, c.raw, out) != EMailUnwrapResult::kOK) {
      continue;
    }

    // The fixture that is already mangled cannot get any cleaner.
    if (HasBareLineFeeds(c.raw)) continue;
    EXPECT_FALSE(HasBareLineFeeds(out)) << name.toStdString();
  }
}

// --- raw header fields -----------------------------------------------------

TEST(EMailCorpusTest, SplittingRawHeadersKeepsOrderAndDuplicates) {
  const auto c = LoadForUnwrap("golden/12-odd-headers.eml");
  const auto block = RawHeaderBlock(c.root, c.raw);
  ASSERT_FALSE(block.isEmpty());

  const auto fields = SplitRawHeaderFields(block);
  ASSERT_FALSE(fields.isEmpty());

  // Order is the order on the wire.
  QStringList names;
  for (const auto& field : fields) names.append(field.name);
  QStringList in_message;
  for (const auto& field : c.root.header_fields) in_message.append(field.first);
  EXPECT_EQ(names.size(), in_message.size());

  // A repeated header is listed once per occurrence, never merged.
  QSet<QString> seen;
  bool has_duplicate = false;
  for (const auto& name : names) {
    if (seen.contains(name.toLower())) has_duplicate = true;
    seen.insert(name.toLower());
  }
  EXPECT_TRUE(has_duplicate)
      << "12-odd-headers.eml is expected to carry a repeated field";
}

TEST(EMailCorpusTest, SplittingRawHeadersJoinsFoldedContinuations) {
  const QByteArray block =
      "Subject: a subject that\r\n"
      " continues on the next line\r\n"
      "To: someone@example.com\r\n"
      "\r\n";

  const auto fields = SplitRawHeaderFields(block);
  ASSERT_EQ(fields.size(), 2);
  EXPECT_EQ(fields.at(0).name, "Subject");
  EXPECT_EQ(fields.at(0).value,
            QByteArray("a subject that continues on the next line"));
  // The whole field, folds included, is what a raw view has to be able to show.
  EXPECT_TRUE(fields.at(0).raw_line.contains("\r\n "));
  EXPECT_EQ(fields.at(1).name, "To");
}

TEST(EMailCorpusTest, SplittingRawHeadersKeepsMalformedLines) {
  // Neither a continuation nor `name: value`. Dropping it would hide exactly
  // what someone reading the raw headers is looking for.
  const QByteArray block =
      "From: a@example.com\r\n"
      "this-line-has-no-colon\r\n"
      "To: b@example.com\r\n"
      "\r\n";

  const auto fields = SplitRawHeaderFields(block);
  ASSERT_EQ(fields.size(), 3);
  EXPECT_EQ(fields.at(1).name, QString());
  EXPECT_EQ(fields.at(1).value, QByteArray("this-line-has-no-colon"));
}

TEST(EMailCorpusTest, SplittingRawHeadersStopsAtTheHeaderTerminator) {
  const QByteArray eml =
      "From: a@example.com\r\n"
      "\r\n"
      "To: this is body text, not a header\r\n";

  const auto fields = SplitRawHeaderFields(eml);
  ASSERT_EQ(fields.size(), 1);
  EXPECT_EQ(fields.at(0).name, "From");
}

TEST(EMailCorpusTest, EveryGoldenMessageSplitsIntoItsParsedFields) {
  for (const auto& name : CorpusNames()) {
    const auto c = LoadForUnwrap(name);
    const auto block = RawHeaderBlock(c.root, c.raw);
    if (block.isEmpty()) continue;

    const auto fields = SplitRawHeaderFields(block);
    EXPECT_EQ(fields.size(), c.root.header_fields.size())
        << name.toStdString()
        << ": the raw split and the parsed field list must agree, or the "
           "Headers view cannot pair them";
  }
}

TEST(EMailCorpusTest, EncryptedMessagesAreClassifiedBeforeAnyDecryption) {
  // The premise of the locked panel: the view knows a message is encrypted
  // from its structure alone, with no cryptography and no key, so it can say
  // so instead of showing an empty editable body.
  const struct {
    const char* name;
    EMailSecurityState state;
  } cases[] = {
      {"golden/05-pgpmime-encrypted.eml", EMailSecurityState::kENCRYPTED},
      {"golden/06-signed-inside-encrypted.eml",
       EMailSecurityState::kSIGNED_ENCRYPTED},
  };

  for (const auto& c : cases) {
    const auto loaded = LoadForUnwrap(c.name);
    EXPECT_EQ(ClassifyOpenPGPStructure(loaded.root, loaded.regions), c.state)
        << c.name;

    // A message that is nothing but ciphertext has no body to show at all,
    // which is the case the locked panel exists for. 06 deliberately carries a
    // readable part beside the encrypted one, so it is not that case.
    if (c.state == EMailSecurityState::kENCRYPTED) {
      const auto* body = SelectBodyPart(loaded.root, false);
      if (body != nullptr) {
        EXPECT_TRUE(body->is_protocol_part || body->data.isEmpty()) << c.name;
      }
    }
  }
}
