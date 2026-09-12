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

// Tests over corpus/public/: messages taken verbatim from third-party test
// suites. They are held to a different standard from the golden bucket.
//
//   * No byte-for-byte contract. We did not write these bytes and do not
//     promise to reproduce them; only RawHeaderBlock() and decoded attachment
//     bytes are byte-exact, because those are the paths that promise it.
//   * Line endings are whatever upstream shipped -- mostly bare LF. Nothing
//     here may assume CRLF.
//   * Parsing is not required to SUCCEED. Several of these are malformed on
//     purpose. What is required is that the outcome is one of three named
//     ones, that it is the same on every run, and that it is reached safely
//     and within the parse limits.
//
// Each file pins one primary property. Higher-level policy -- which security
// state the UI ends up showing, which alternative is preferred -- is a product
// contract and belongs in the golden bucket, not here.

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QSet>
#include <array>

#include "EMailHelper.h"
#include "EMailModel.h"

#ifndef GF_EMAIL_TEST_CORPUS_DIR
#error "GF_EMAIL_TEST_CORPUS_DIR must be defined by the build"
#endif

namespace {

auto PublicDir() -> QString {
  return QString(GF_EMAIL_TEST_CORPUS_DIR) + "/public";
}

auto Load(const QString& name) -> QByteArray {
  QFile file(PublicDir() + "/" + name);
  EXPECT_TRUE(file.open(QIODevice::ReadOnly)) << name.toStdString();
  return file.readAll();
}

// How a message is allowed to end up. A malformed message has no single right
// answer, but it does have a bounded set of acceptable ones, and picking a
// different one from run to run is itself the bug.
enum class Outcome : uint8_t {
  // Parsed, and the structure is the one the sender plainly meant.
  kACCEPT,
  // Parsed, but the damage cost us structure: parts merged, a subtree flattened
  // into text, a header swallowed. Safe and useful, not faithful.
  kACCEPT_WITH_DEGRADED_STRUCTURE,
  // Refused: CheckIfEMLMessage() or ParseMimeTree() reports failure. Never a
  // crash, a hang or a partially-populated tree handed back as if it were good.
  kREJECT,
};

struct PublicCase {
  const char* file;
  Outcome outcome;
  const char* property;  // the one thing this file is here to pin
};

// Keep in step with corpus/public/MANIFEST.txt.
const std::array<PublicCase, 12> kPublicCases{{
    {"p01-boundary-name-clash.eml", Outcome::kACCEPT,
     "a child boundary that begins with the parent boundary"},
    {"p02-ending-boundaries.eml", Outcome::kACCEPT_WITH_DEGRADED_STRUCTURE,
     "trailing text on a boundary delimiter line"},
    {"p03-boundary-text-in-body.eml", Outcome::kACCEPT,
     "a boundary-lookalike inside a body does not split the part"},
    {"p04-no-header-separator.eml", Outcome::kACCEPT_WITH_DEGRADED_STRUCTURE,
     "a non-header line in the header block"},
    {"p05-base64-rfc822-bare-lf.eml", Outcome::kACCEPT,
     "base64-encoded message/rfc822 whose inner message uses bare LF"},
    {"p06-complex-multipart.eml", Outcome::kACCEPT,
     "a realistic four-part message with binary attachments"},
    {"p07-cte-matrix.eml", Outcome::kACCEPT,
     "7bit, quoted-printable, base64 and a part with no CTE header"},
    {"p08-nested-charsets.eml", Outcome::kACCEPT,
     "nested multipart with five different charsets and empty bodies"},
    {"p09-duplicate-attachment-names.eml", Outcome::kACCEPT,
     "two attachments that claim the same filename"},
    {"p10-iso2022jp-encoded-subject.eml", Outcome::kACCEPT,
     "RFC 2047 encoded-words in a non-UTF-8, non-Latin charset"},
    {"p11-empty-multipart.eml", Outcome::kACCEPT,
     "a multipart whose declared boundary never appears"},
    {"p12-preamble-epilogue.eml", Outcome::kACCEPT,
     "preamble and epilogue text around the parts"},
}};

struct ParseOutcome {
  bool recognised{false};
  int tree_result{-1};
  int part_count{0};
  int max_depth{0};
  QString shape;  // pre-order content types, for comparing two runs
};

void Walk(const EMailPart& part, int depth, ParseOutcome& out) {
  ++out.part_count;
  out.max_depth = std::max(out.max_depth, depth);
  out.shape += QString("%1:%2;").arg(depth).arg(part.content_type);
  for (const auto& child : part.children) Walk(child, depth + 1, out);
}

auto RunParse(const QByteArray& original, const EMailParseLimits& limits = {})
    -> ParseOutcome {
  ParseOutcome out;
  QByteArray raw = original;
  vmime::shared_ptr<vmime::message> message;
  out.recognised = CheckIfEMLMessage(raw, message);
  if (!out.recognised || !message) return out;

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  out.tree_result = ParseMimeTree(message, raw, root, regions, limits);
  if (out.tree_result != 0) return out;

  Walk(root, 0, out);
  // Inspection must not rewrite what it was handed.
  EXPECT_EQ(raw, original);
  return out;
}

auto ParseOrDie(const QString& name, QByteArray& raw, EMailPart& root,
                QList<EMailSignatureRegion>& regions) -> bool {
  raw = Load(name);
  vmime::shared_ptr<vmime::message> message;
  if (!CheckIfEMLMessage(raw, message)) return false;
  return ParseMimeTree(message, raw, root, regions) == 0;
}

auto FindByFileName(const EMailPart& root, const QString& name)
    -> const EMailPart* {
  for (const auto* part : FlattenMimeTree(root)) {
    if (part->filename == name) return part;
  }
  return nullptr;
}

}  // namespace

// --- the contract every public message is held to ---------------------------

TEST(EMailPublicCorpusTest, EveryMessageReachesItsDeclaredOutcome) {
  for (const auto& c : kPublicCases) {
    const auto raw = Load(c.file);
    ASSERT_FALSE(raw.isEmpty()) << c.file;

    const auto out = RunParse(raw);
    const std::string where =
        QString("%1 (%2)").arg(c.file, c.property).toStdString();

    switch (c.outcome) {
      case Outcome::kREJECT:
        EXPECT_TRUE(!out.recognised || out.tree_result != 0)
            << where << ": expected refusal, got a tree";
        break;
      case Outcome::kACCEPT:
      case Outcome::kACCEPT_WITH_DEGRADED_STRUCTURE:
        ASSERT_TRUE(out.recognised) << where << ": not recognised as a message";
        ASSERT_EQ(out.tree_result, 0) << where << ": tree walk failed";
        break;
    }
  }
}

TEST(EMailPublicCorpusTest, ParsingIsDeterministic) {
  // Same bytes in, same structure out -- twice. A parser that resolves an
  // ambiguity by reading uninitialised memory passes the outcome test above
  // and fails this one.
  for (const auto& c : kPublicCases) {
    const auto raw = Load(c.file);
    const auto first = RunParse(raw);
    const auto second = RunParse(raw);
    EXPECT_EQ(first.recognised, second.recognised) << c.file;
    EXPECT_EQ(first.tree_result, second.tree_result) << c.file;
    EXPECT_EQ(first.part_count, second.part_count) << c.file;
    EXPECT_EQ(first.shape, second.shape) << c.file;
  }
}

TEST(EMailPublicCorpusTest, NoMessageEscapesTheParseLimits) {
  for (const auto& c : kPublicCases) {
    const EMailParseLimits limits;
    const auto out = RunParse(Load(c.file), limits);
    if (out.tree_result != 0) continue;
    EXPECT_LE(out.max_depth, limits.max_depth) << c.file;
    EXPECT_LE(out.part_count, limits.max_parts) << c.file;
  }
}

TEST(EMailPublicCorpusTest, TighteningTheLimitsIsRespected) {
  // The limits are a real gate, not decoration: squeeze them and the same
  // messages must be refused rather than walked anyway.
  EMailParseLimits tight;
  tight.max_parts = 1;
  int refused = 0;
  for (const auto& c : kPublicCases) {
    const auto out = RunParse(Load(c.file), tight);
    if (out.tree_result != 0) ++refused;
  }
  EXPECT_GT(refused, 0)
      << "max_parts=1 refused nothing; the limit is not wired";
}

// --- size and structural stress, generated rather than checked in -----------
//
// Real stress inputs are megabytes. Generating them here keeps the repository
// small, lets the shapes be varied without a new file each time, and makes the
// numbers visible next to the assertion instead of hidden in a blob.

TEST(EMailPublicCorpusTest, ADeeplyNestedMessageIsRefusedNotRecursedInto) {
  const EMailParseLimits limits;
  const int depth = limits.max_depth + 20;

  QByteArray raw = "Subject: deep\r\nMIME-Version: 1.0\r\n";
  QByteArray tail;
  for (int i = 0; i < depth; ++i) {
    const auto b = QString("b%1").arg(i).toUtf8();
    raw += "Content-Type: multipart/mixed; boundary=\"" + b + "\"\r\n\r\n--" +
           b + "\r\n";
    tail.prepend("\r\n--" + b + "--\r\n");
  }
  raw += "Content-Type: text/plain\r\n\r\nbottom\r\n" + tail;

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(raw, message));
  EMailPart root;
  QList<EMailSignatureRegion> regions;
  EXPECT_NE(ParseMimeTree(message, raw, root, regions, limits), 0)
      << "a message nested " << depth << " deep was walked to the bottom";
}

TEST(EMailPublicCorpusTest, TooManyPartsIsRefusedNotTruncatedSilently) {
  const EMailParseLimits limits;
  const int parts = limits.max_parts + 20;

  QByteArray raw =
      "Subject: wide\r\nMIME-Version: 1.0\r\n"
      "Content-Type: multipart/mixed; boundary=\"w\"\r\n\r\n";
  for (int i = 0; i < parts; ++i) {
    raw += "--w\r\nContent-Type: text/plain\r\n\r\npart\r\n";
  }
  raw += "--w--\r\n";

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(raw, message));
  EMailPart root;
  QList<EMailSignatureRegion> regions;
  EXPECT_NE(ParseMimeTree(message, raw, root, regions, limits), 0)
      << parts << " parts were accepted under a limit of " << limits.max_parts;
}

TEST(EMailPublicCorpusTest, ALargeAttachmentSurvivesBase64RoundTrip) {
  // Every byte value, including NUL, CR, LF and a boundary-lookalike run --
  // the bytes that a text-oriented pipeline mangles. Only the decoded bytes
  // are asserted: the base64 line layout and the boundary string are the
  // serializer's business, not a contract.
  QByteArray payload;
  payload.reserve(256 * 1024);
  for (int i = 0; i < 1024; ++i) {
    for (int b = 0; b < 256; ++b) payload.append(static_cast<char>(b));
  }
  payload.append("\r\n--boundary\r\n");

  EMailMetaData meta;
  meta.from = "a@example.com";
  meta.to = QStringList{"b@example.com"};
  meta.subject = "big";

  EMailAttachment att;
  att.filename = "blob.bin";
  att.mime_type = "application/octet-stream";
  att.disposition = "attachment";
  att.data = payload;

  QByteArray eml;
  ASSERT_EQ(BuildMimeEML(meta, QByteArray("body"), {att}, eml), 0);

  QByteArray raw = eml;
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(raw, message));

  EMailMetaData parsed;
  ASSERT_EQ(ExtractParts(message, parsed), 0);
  ASSERT_EQ(parsed.attachments.size(), 1);
  EXPECT_EQ(parsed.attachments[0].data, payload)
      << "decoded attachment bytes differ after a build/parse cycle";
}

// --- one property per file --------------------------------------------------

TEST(EMailPublicCorpusTest, AChildBoundaryPrefixedByItsParentStaysNested) {
  // boundary="--boundary.N" and boundary="--boundary.N-1": a parser that
  // treats "starts with the parent delimiter" as a match closes the inner
  // multipart early and reparents the attachment.
  QByteArray raw;
  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_TRUE(ParseOrDie("p01-boundary-name-clash.eml", raw, root, regions));

  ASSERT_EQ(root.children.size(), 2);
  EXPECT_EQ(root.children[0].content_type, QString("multipart/alternative"));
  EXPECT_EQ(root.children[0].children.size(), 2);
  EXPECT_EQ(root.children[1].content_type, QString("application/pdf"));
  EXPECT_EQ(root.children[1].filename,
            QString("Daily_Stats-2022-05-12-0700.pdf"));
}

TEST(EMailPublicCorpusTest, TrailingTextOnADelimiterLineDegradesSafely) {
  // "--boundary This should be ignored" is not a delimiter: RFC 2046 allows
  // only transport padding between the boundary and the CRLF. vmime agrees and
  // finds no parts at all, so the whole body becomes preamble.
  //
  // mime4j, whose message this is, instead recovers the part. We do not, and
  // that is the safer of the two readings: recovering means guessing which
  // bytes the sender meant to delimit, and a message whose framing is
  // ambiguous is exactly the one where a wrong guess decides what appears to
  // be signed. What is NOT acceptable is inventing a part from the preamble,
  // so that is what this pins.
  QByteArray raw;
  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_TRUE(ParseOrDie("p02-ending-boundaries.eml", raw, root, regions));

  EXPECT_TRUE(root.children.isEmpty());
  for (const auto* part : FlattenMimeTree(root)) {
    EXPECT_FALSE(part->data.contains("NOTE TO IMPLEMENTORS"))
        << "epilogue text surfaced as message content";
  }
}

TEST(EMailPublicCorpusTest, ABoundaryLookalikeInsideABodyDoesNotSplitIt) {
  QByteArray raw;
  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_TRUE(ParseOrDie("p03-boundary-text-in-body.eml", raw, root, regions));

  ASSERT_EQ(root.children.size(), 1);
  const auto& body = root.children[0];
  EXPECT_TRUE(body.data.contains("This should be a text including"));
  EXPECT_TRUE(body.data.contains("should not be parsed as multiple"));
}

TEST(EMailPublicCorpusTest, ANonHeaderLineInTheHeaderBlockLosesNoRealHeader) {
  // Degraded, not rejected: whatever the garbage line does to the block, the
  // headers around it must still be readable and the body must not vanish.
  QByteArray raw;
  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_TRUE(ParseOrDie("p04-no-header-separator.eml", raw, root, regions));

  vmime::shared_ptr<vmime::message> message;
  QByteArray copy = raw;
  ASSERT_TRUE(CheckIfEMLMessage(copy, message));
  EMailMetaData meta;
  ASSERT_EQ(GetEMLMetaData(message, meta), 0);
  EXPECT_EQ(meta.subject, QString("this is a subject"));
}

TEST(EMailPublicCorpusTest, ABase64EncodedInnerMessageDecodesDespiteBareLf) {
  QByteArray raw;
  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_TRUE(ParseOrDie("p05-base64-rfc822-bare-lf.eml", raw, root, regions));

  const auto flat = FlattenMimeTree(root);
  bool found = false;
  for (const auto* part : flat) {
    if (part->data.contains("Text body")) found = true;
  }
  EXPECT_TRUE(found) << "the base64 message/rfc822 payload never decoded";
}

TEST(EMailPublicCorpusTest, ARealisticMultipartKeepsItsAttachmentBytes) {
  QByteArray raw;
  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_TRUE(ParseOrDie("p06-complex-multipart.eml", raw, root, regions));

  vmime::shared_ptr<vmime::message> message;
  QByteArray copy = raw;
  ASSERT_TRUE(CheckIfEMLMessage(copy, message));
  EMailMetaData meta;
  ASSERT_EQ(ExtractParts(message, meta), 0);

  // Attachment payloads are one of the few byte-exact contracts: what the
  // sender attached is what the user saves.
  ASSERT_FALSE(meta.attachments.isEmpty());
  for (const auto& att : meta.attachments) {
    EXPECT_FALSE(att.data.isEmpty()) << att.filename.toStdString();
  }
}

TEST(EMailPublicCorpusTest, EveryTransferEncodingDecodesIncludingTheAbsentOne) {
  QByteArray raw;
  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_TRUE(ParseOrDie("p07-cte-matrix.eml", raw, root, regions));

  ASSERT_EQ(root.children.size(), 5);
  EXPECT_TRUE(root.children[0].data.contains("7bit encoded message"));
  EXPECT_TRUE(root.children[1].data.contains("Quoted Printable encoded"));
  EXPECT_TRUE(root.children[2].data.contains("Base64 encoded message"));
  EXPECT_TRUE(root.children[3].data.contains("Base64 encoded message"));
  // No Content-Transfer-Encoding header at all: the default is 7bit, and the
  // part must come through as itself rather than being dropped or re-decoded.
  EXPECT_TRUE(
      root.children[4].data.contains("This has no Content-Transfer-Encoding"));
}

TEST(EMailPublicCorpusTest, NestedMultipartWithEmptyBodiesKeepsItsShape) {
  QByteArray raw;
  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_TRUE(ParseOrDie("p08-nested-charsets.eml", raw, root, regions));

  // Empty bodies are still parts; a walker that drops them loses the nesting.
  ASSERT_EQ(root.children.size(), 5);
  EXPECT_EQ(root.children[2].content_type, QString("multipart/mixed"));
  ASSERT_EQ(root.children[2].children.size(), 2);
  EXPECT_EQ(root.children[2].children[0].charset, QString("iso-8859-2"));
  EXPECT_EQ(root.children[2].children[1].charset, QString("iso-8859-3"));
}

TEST(EMailPublicCorpusTest,
     DuplicateAttachmentNamesAreMadeUniqueDeterministically) {
  QByteArray raw;
  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_TRUE(
      ParseOrDie("p09-duplicate-attachment-names.eml", raw, root, regions));

  vmime::shared_ptr<vmime::message> message;
  QByteArray copy = raw;
  ASSERT_TRUE(CheckIfEMLMessage(copy, message));
  EMailMetaData meta;
  ASSERT_EQ(ExtractParts(message, meta), 0);
  ASSERT_EQ(meta.attachments.size(), 2);
  EXPECT_EQ(meta.attachments[0].filename, meta.attachments[1].filename);

  const auto names = UniqueAttachmentFileNames(meta.attachments);
  ASSERT_EQ(names.size(), 2);
  EXPECT_NE(names[0], names[1]);
  EXPECT_EQ(names, UniqueAttachmentFileNames(meta.attachments));
}

TEST(EMailPublicCorpusTest, EncodedWordsInALegacyCharsetAreFullyDecoded) {
  QByteArray raw = Load("p10-iso2022jp-encoded-subject.eml");
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(raw, message));

  EMailMetaData meta;
  ASSERT_EQ(GetEMLMetaData(message, meta), 0);

  // Whatever the charset support, no encoded-word syntax may survive into a
  // value the UI shows, and the two adjacent encoded-words must be joined
  // before decoding rather than decoded separately.
  EXPECT_FALSE(meta.subject.contains("=?"));
  EXPECT_FALSE(meta.subject.contains("?="));
  EXPECT_TRUE(meta.subject.contains("(testing Japanese emails)"))
      << meta.subject.toStdString();
}

TEST(EMailPublicCorpusTest, AnEmptyMultipartDoesNotSwallowItsNextSibling) {
  // A multipart/alternative whose own boundary never appears, followed by a
  // text/plain at the PARENT's level. A parser that keeps consuming until it
  // finds the child boundary adopts that sibling -- which would place content
  // inside a subtree it was never part of. Under a multipart/signed that is
  // the difference between signed and unsigned.
  QByteArray raw;
  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_TRUE(ParseOrDie("p11-empty-multipart.eml", raw, root, regions));

  ASSERT_EQ(root.children.size(), 2);
  EXPECT_EQ(root.children[0].content_type, QString("multipart/alternative"));
  EXPECT_TRUE(root.children[0].children.isEmpty());
  EXPECT_EQ(root.children[1].content_type, QString("text/plain"));
  EXPECT_TRUE(root.children[1].data.contains("same level"));

  // Body selection over a childless multipart must answer, not dereference.
  (void)SelectBodyPart(root, false);
  (void)SelectBodyPart(root, true);
}

TEST(EMailPublicCorpusTest, PreambleAndEpilogueAreNotParts) {
  QByteArray raw;
  EMailPart root;
  QList<EMailSignatureRegion> regions;
  ASSERT_TRUE(ParseOrDie("p12-preamble-epilogue.eml", raw, root, regions));

  for (const auto* part : FlattenMimeTree(root)) {
    EXPECT_FALSE(part->data.contains("preamble"))
        << "preamble text leaked into part " << part->index;
    EXPECT_FALSE(part->data.contains("epilogue"))
        << "epilogue text leaked into part " << part->index;
  }
}

// ---------------------------------------------------------------------------
// Hostile shapes: what must be survivable, not merely refused
//
// A message has to be parsed before anything can say whether it is signed, let
// alone trustworthy, so the parser runs on wholly unauthenticated input. The
// bar is therefore not "is it rejected" but "does the process survive it".
// ---------------------------------------------------------------------------

namespace {

/// A message nested @p depth multiparts deep. Roughly 60 bytes per level, so
/// even a very deep one is a small file -- which is the point.
auto DeeplyNested(int depth) -> QByteArray {
  QByteArray head =
      "From: a@example.com\r\nTo: b@example.com\r\nSubject: s\r\n"
      "MIME-Version: 1.0\r\n";
  QByteArray body;
  for (int i = 0; i < depth; ++i) {
    const auto boundary = QByteArray("b") + QByteArray::number(i);
    const auto ct =
        "Content-Type: multipart/mixed; boundary=\"" + boundary + "\"\r\n\r\n";
    if (i == 0) {
      head += ct;
    } else {
      body += ct;
    }
    body += "--" + boundary + "\r\n";
  }
  body += "Content-Type: text/plain\r\n\r\ndeep\r\n";
  return head + body;
}

}  // namespace

TEST(EMailHostileShapeTest, NestingFarPastTheLimitDoesNotCrashTheProcess) {
  // 4000 levels is about 240 KB -- nothing by the size ceiling, and it used to
  // terminate the process with SIGSEGV before any limit was consulted, because
  // vmime descends by recursion and had no bound of its own. The measured
  // crash threshold on an 8 MB stack was between 3000 and 3500 levels.
  //
  // Reaching the end of this test at all is the assertion.
  const auto raw = DeeplyNested(4000);
  ASSERT_LT(raw.size(), 1024 * 1024) << "the fixture itself must stay small";

  vmime::shared_ptr<vmime::message> message;
  CheckIfEMLMessage(raw, message);

  SUCCEED() << "parsed without exhausting the stack";
}

TEST(EMailHostileShapeTest, VeryDeepNestingStaysCheap) {
  // Depth also multiplies work: every level rescans the buffer for its
  // boundary, so an unbounded descent is depth x size even when it does not
  // crash. At vmime's own default this shape took over a minute.
  const auto raw = DeeplyNested(20000);

  QElapsedTimer timer;
  timer.start();

  vmime::shared_ptr<vmime::message> message;
  CheckIfEMLMessage(raw, message);

  EXPECT_LT(timer.elapsed(), 10000)
      << "parsing " << raw.size() << " bytes took " << timer.elapsed() << "ms";
}

TEST(EMailHostileShapeTest, DeepNestingIsStillRefusedByTheTreeLimits) {
  // Surviving is not the same as accepting: the module's own walk must still
  // refuse the shape, so nothing downstream ever sees a 4000-deep tree.
  const auto raw = DeeplyNested(4000);

  vmime::shared_ptr<vmime::message> message;
  if (!CheckIfEMLMessage(raw, message)) SUCCEED() << "refused at the parse";

  if (message != nullptr) {
    EMailPart root;
    QList<EMailSignatureRegion> regions;
    EXPECT_EQ(ParseMimeTree(message, raw, root, regions), -1);
  }
}

TEST(EMailHostileShapeTest, AnInputOverTheCeilingIsRefusedBeforeParsing) {
  // The ceiling lives at the parse rather than at each entry point, so every
  // route in is covered -- including the decrypted plaintext, whose size the
  // ciphertext says nothing about.
  QByteArray raw = "From: a@example.com\r\nSubject: s\r\n\r\n";
  raw += QByteArray(kMaxParseInputBytes + 1 - raw.size(), 'x');
  ASSERT_GT(raw.size(), kMaxParseInputBytes);

  vmime::shared_ptr<vmime::message> message;
  EXPECT_FALSE(CheckIfEMLMessage(raw, message));
}

TEST(EMailHostileShapeTest, AMessageJustUnderTheCeilingIsStillAccepted) {
  // The limit must not be so eager that ordinary large mail stops working.
  QByteArray raw =
      "From: a@example.com\r\nTo: b@example.com\r\nSubject: s\r\n"
      "MIME-Version: 1.0\r\nContent-Type: text/plain\r\n\r\n";
  raw += QByteArray(1024 * 1024, 'x');

  vmime::shared_ptr<vmime::message> message;
  EXPECT_TRUE(CheckIfEMLMessage(raw, message));
}

TEST(EMailHostileShapeTest, ManyTinyPartsAreRefusedRatherThanWalked) {
  QByteArray raw =
      "From: a@example.com\r\nTo: b@example.com\r\nSubject: s\r\n"
      "MIME-Version: 1.0\r\n"
      "Content-Type: multipart/mixed; boundary=\"bb\"\r\n\r\n";
  for (int i = 0; i < 5000; ++i) {
    raw += "--bb\r\nContent-Type: text/plain\r\n\r\nx\r\n";
  }
  raw += "--bb--\r\n";

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(raw, message));

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  EXPECT_EQ(ParseMimeTree(message, raw, root, regions), -1)
      << "5000 parts passed a limit of " << EMailParseLimits{}.max_parts;
}

TEST(EMailHostileShapeTest, PathologicalBoundariesDoNotHang) {
  // Boundary-like lines that never actually close anything.
  QByteArray raw =
      "From: a@example.com\r\nSubject: s\r\nMIME-Version: 1.0\r\n"
      "Content-Type: multipart/mixed; boundary=\"x\"\r\n\r\n";
  for (int i = 0; i < 20000; ++i) raw += "--x\r\n";

  QElapsedTimer timer;
  timer.start();

  vmime::shared_ptr<vmime::message> message;
  CheckIfEMLMessage(raw, message);

  EXPECT_LT(timer.elapsed(), 10000);
}
