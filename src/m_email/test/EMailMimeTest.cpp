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

#include <QCoreApplication>
#include <QRegularExpression>
#include <QStringList>

#include "EMailHelper.h"
#include "EMailModel.h"

namespace {

// Round-trips `meta`/`body` through the builder and the parser, so a test can
// assert on what actually survives the wire format rather than on the struct
// it started from.
auto RoundTrip(const EMailMetaData& meta, const QByteArray& body,
               EMailMetaData& out) -> bool {
  QString eml;
  if (BuildPlainTextEML(meta, body, eml) != 0) return false;

  vmime::shared_ptr<vmime::message> message;
  if (!CheckIfEMLMessage(eml.toUtf8(), message)) return false;

  return GetEMLMetaData(message, out) == 0;
}

auto BasicMeta() -> EMailMetaData {
  EMailMetaData m;
  m.from = "Alice <alice@example.com>";
  m.to = QStringList{"Bob <bob@example.com>"};
  m.subject = "Hello";
  return m;
}

}  // namespace

TEST(EMailMimeTest, MicalgFormatAcceptsOnlyPgpPrefixedValues) {
  EXPECT_TRUE(IsValidMicalgFormat("pgp-sha256"));
  EXPECT_TRUE(IsValidMicalgFormat("pgp-sha512"));
  EXPECT_FALSE(IsValidMicalgFormat("sha256"));
  EXPECT_FALSE(IsValidMicalgFormat(""));
}

TEST(EMailMimeTest, ParseEmailStringSplitsNameAndAddress) {
  QString name;
  QString email;

  ASSERT_TRUE(ParseEmailString("Alice <alice@example.com>", name, email));
  EXPECT_EQ(name.trimmed(), QString("Alice"));
  EXPECT_EQ(email, QString("alice@example.com"));
}

TEST(EMailMimeTest, CheckIfEMLMessageRejectsPlainText) {
  vmime::shared_ptr<vmime::message> message;
  EXPECT_FALSE(CheckIfEMLMessage("just some body text", message));
}

TEST(EMailMimeTest, BuildsAndReparsesASimpleMessage) {
  EMailMetaData out;
  ASSERT_TRUE(RoundTrip(BasicMeta(), "hello there", out));

  EXPECT_TRUE(out.from.contains("alice@example.com"));
  EXPECT_EQ(out.subject, QString("Hello"));
  ASSERT_EQ(out.to.size(), 1);
  EXPECT_TRUE(out.to.front().contains("bob@example.com"));
}

// --- regressions -----------------------------------------------------------
//
// Each test below pins a defect that was live in this module. They are written
// against the observable behaviour of the MIME layer, not against the shape of
// the fix, so they stay meaningful if the implementation is reworked.

namespace {

// Parses a literal message. Takes the text with LF endings for readability and
// converts to the CRLF the wire format actually uses.
auto ParseLiteral(const QString& text,
                  vmime::shared_ptr<vmime::message>& message) -> bool {
  auto crlf = text;
  crlf.replace("\n", "\r\n");
  return CheckIfEMLMessage(crlf.toUtf8(), message);
}

}  // namespace

// A display name may legitimately contain a comma ("Doe, John"). Splitting a
// joined address list on "," tears such a name in half and invents a recipient.
TEST(EMailMimeTest, DisplayNameContainingACommaSurvivesParsing) {
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseLiteral(
      "From: a@example.com\n"
      "To: \"Doe, John\" <john@example.com>, Jane <jane@example.com>\n"
      "Subject: s\n"
      "MIME-Version: 1.0\n"
      "Content-Type: text/plain\n"
      "\n"
      "body\n",
      message));

  EMailMetaData meta;
  ASSERT_EQ(GetEMLMetaData(message, meta), 0);

  EXPECT_EQ(meta.to.size(), 2);
  EXPECT_TRUE(meta.to.front().contains("john@example.com"));
  EXPECT_TRUE(meta.to.back().contains("jane@example.com"));
}

// The Date header was parsed into a local and then never assigned, so every
// report showed an invalid date.
TEST(EMailMimeTest, DateHeaderReachesTheMetaData) {
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(
      ParseLiteral("From: a@example.com\n"
                   "To: b@example.com\n"
                   "Subject: s\n"
                   "Date: Sun, 28 Jun 2026 18:58:21 +0200\n"
                   "MIME-Version: 1.0\n"
                   "Content-Type: text/plain\n"
                   "\n"
                   "body\n",
                   message));

  EMailMetaData meta;
  ASSERT_EQ(GetEMLMetaData(message, meta), 0);

  EXPECT_TRUE(meta.datetime.isValid());
  EXPECT_EQ(meta.datetime.date().year(), 2026);
}

// vmime's header::getField<T>() inserts the field when it is absent, so merely
// *reading* an optional header used to add an empty one to the message. A
// message with no Cc must not grow one by being inspected.
TEST(EMailMimeTest, ReadingAbsentHeadersDoesNotInjectThem) {
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(
      ParseLiteral("From: a@example.com\n"
                   "To: b@example.com\n"
                   "MIME-Version: 1.0\n"
                   "Content-Type: text/plain\n"
                   "\n"
                   "body\n",
                   message));

  EMailMetaData meta;
  ASSERT_EQ(GetEMLMetaData(message, meta), 0);

  const auto regenerated = QString::fromStdString(
      message->generate(vmime::lineLengthLimits::convenient));

  EXPECT_FALSE(regenerated.contains("Cc:"));
  EXPECT_FALSE(regenerated.contains("Subject:"));
}

// An empty optional header must produce an empty list, not a list holding one
// empty string -- otherwise the UI renders a phantom recipient.
TEST(EMailMimeTest, AbsentRecipientListsAreEmptyNotBlank) {
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(
      ParseLiteral("From: a@example.com\n"
                   "To: b@example.com\n"
                   "Subject: s\n"
                   "MIME-Version: 1.0\n"
                   "Content-Type: text/plain\n"
                   "\n"
                   "body\n",
                   message));

  EMailMetaData meta;
  ASSERT_EQ(GetEMLMetaData(message, meta), 0);

  EXPECT_TRUE(meta.cc.isEmpty());
  EXPECT_TRUE(meta.bcc_header.isEmpty());
}

// A non-ASCII subject must survive RFC 2047 encoding and come back intact.
TEST(EMailMimeTest, NonAsciiSubjectSurvivesARoundTrip) {
  auto meta = BasicMeta();
  meta.subject = QString::fromUtf8("Grüße 你好 🔐");

  EMailMetaData out;
  ASSERT_TRUE(RoundTrip(meta, "body", out));

  EXPECT_EQ(out.subject, meta.subject);
}

// --- attachments -----------------------------------------------------------

namespace {

auto MakeAttachment(const QString& name, const QString& type,
                    const QByteArray& data) -> EMailAttachment {
  EMailAttachment a;
  a.filename = name;
  a.mime_type = type;
  a.data = data;
  return a;
}

}  // namespace

// With nothing attached the wire format must not change at all: existing
// messages, and the signatures over them, depend on it.
TEST(EMailMimeTest, NoAttachmentsProducesTheSameBytesAsBefore) {
  QString via_plain;
  QString via_mime;

  ASSERT_EQ(BuildPlainTextEML(BasicMeta(), "body", via_plain), 0);
  ASSERT_EQ(BuildMimeEML(BasicMeta(), "body", {}, via_mime), 0);

  // The Date header is generated from the clock, so compare everything else.
  auto strip_date = [](QString s) {
    return s.remove(QRegularExpression("Date: [^\r\n]*\r\n"));
  };
  EXPECT_EQ(strip_date(via_plain), strip_date(via_mime));
  EXPECT_FALSE(via_mime.contains("multipart/mixed"));
}

TEST(EMailMimeTest, AttachmentsRoundTripThroughMultipartMixed) {
  const QByteArray pdf("%PDF-1.4 fake pdf bytes");
  const QList<EMailAttachment> attachments{
      MakeAttachment("report.pdf", "application/pdf", pdf)};

  QString eml;
  ASSERT_EQ(BuildMimeEML(BasicMeta(), "see attached", attachments, eml), 0);
  EXPECT_TRUE(eml.contains("multipart/mixed"));

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml.toUtf8(), message));

  EMailMetaData parsed;
  ASSERT_EQ(ExtractParts(message, parsed), 0);

  EXPECT_EQ(parsed.body.trimmed(), QByteArray("see attached"));
  ASSERT_EQ(parsed.attachments.size(), 1);
  EXPECT_EQ(parsed.attachments[0].filename, QString("report.pdf"));
  EXPECT_EQ(parsed.attachments[0].mime_type, QString("application/pdf"));
  EXPECT_EQ(parsed.attachments[0].data, pdf);
}

// Binary content must survive base64 exactly; a transfer encoding that mangles
// 0x00 or 0xFF silently corrupts every non-text attachment.
TEST(EMailMimeTest, BinaryAttachmentSurvivesByteForByte) {
  QByteArray binary;
  for (int i = 0; i < 256; ++i) binary.append(static_cast<char>(i));

  QString eml;
  ASSERT_EQ(BuildMimeEML(BasicMeta(), "b",
                         {MakeAttachment("blob.bin", "application/octet-stream",
                                         binary)},
                         eml),
            0);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml.toUtf8(), message));

  EMailMetaData parsed;
  ASSERT_EQ(ExtractParts(message, parsed), 0);
  ASSERT_EQ(parsed.attachments.size(), 1);
  EXPECT_EQ(parsed.attachments[0].data, binary);
}

TEST(EMailMimeTest, SeveralAttachmentsKeepTheirOrderAndNames) {
  const QList<EMailAttachment> attachments{
      MakeAttachment("a.txt", "text/plain", "aaa"),
      MakeAttachment("b.png", "image/png", "bbb"),
      MakeAttachment("c.pdf", "application/pdf", "ccc")};

  QString eml;
  ASSERT_EQ(BuildMimeEML(BasicMeta(), "body", attachments, eml), 0);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml.toUtf8(), message));

  EMailMetaData parsed;
  ASSERT_EQ(ExtractParts(message, parsed), 0);
  ASSERT_EQ(parsed.attachments.size(), 3);
  EXPECT_EQ(parsed.attachments[0].filename, QString("a.txt"));
  EXPECT_EQ(parsed.attachments[1].filename, QString("b.png"));
  EXPECT_EQ(parsed.attachments[2].filename, QString("c.pdf"));
}

TEST(EMailMimeTest, NonAsciiAttachmentNameSurvives) {
  const auto name = QString::fromUtf8("Jahresbericht Grüße 报告.pdf");

  QString eml;
  ASSERT_EQ(BuildMimeEML(BasicMeta(), "b",
                         {MakeAttachment(name, "application/pdf", "x")}, eml),
            0);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml.toUtf8(), message));

  EMailMetaData parsed;
  ASSERT_EQ(ExtractParts(message, parsed), 0);
  ASSERT_EQ(parsed.attachments.size(), 1);
  EXPECT_EQ(parsed.attachments[0].filename, name);
}

// --- attachment filenames are attacker-controlled ---------------------------
//
// Saving an attachment writes a file at a path derived from a string the
// sender chose. Everything below is about that string never escaping the
// directory the user picked, and never naming something the OS treats
// specially.

TEST(EMailMimeTest, SanitizerContainsPathTraversal) {
  for (const auto& hostile :
       {"../../etc/passwd", "/etc/passwd",
        "..\\..\\windows\\system32\\evil.dll", "C:\\Windows\\evil.exe",
        "foo/bar.pdf", "/../..//x.txt"}) {
    const auto safe = SanitizeAttachmentFileName(hostile);

    EXPECT_FALSE(safe.contains('/')) << hostile;
    EXPECT_FALSE(safe.contains('\\')) << hostile;
    EXPECT_FALSE(safe.startsWith('.')) << hostile;
    EXPECT_NE(safe, QString("..")) << hostile;
    EXPECT_FALSE(safe.isEmpty()) << hostile;
  }
}

TEST(EMailMimeTest, SanitizerKeepsTheLastComponent) {
  EXPECT_EQ(SanitizeAttachmentFileName("../../etc/passwd"), QString("passwd"));
  EXPECT_EQ(SanitizeAttachmentFileName("foo/bar.pdf"), QString("bar.pdf"));
}

TEST(EMailMimeTest, SanitizerRenamesReservedDeviceNames) {
  EXPECT_NE(SanitizeAttachmentFileName("CON").toUpper(), QString("CON"));
  EXPECT_NE(SanitizeAttachmentFileName("NUL.txt").toUpper(),
            QString("NUL.TXT"));
  EXPECT_NE(SanitizeAttachmentFileName("LPT1.pdf").toUpper(),
            QString("LPT1.PDF"));

  // The extension is still preserved, so the file stays openable.
  EXPECT_TRUE(SanitizeAttachmentFileName("NUL.txt").endsWith(".txt"));
}

TEST(EMailMimeTest, SanitizerStripsControlCharacters) {
  QString hostile("re");
  hostile.append(QChar(0x00));
  hostile.append("port");
  hostile.append(QChar(0x1B));
  hostile.append(".pdf");

  const auto safe = SanitizeAttachmentFileName(hostile);
  for (const auto ch : safe) EXPECT_GE(ch.unicode(), 0x20);
  EXPECT_TRUE(safe.endsWith(".pdf"));
}

TEST(EMailMimeTest, SanitizerFallsBackWhenNothingUsableIsLeft) {
  EXPECT_FALSE(SanitizeAttachmentFileName("").isEmpty());
  EXPECT_FALSE(SanitizeAttachmentFileName("   ").isEmpty());
  EXPECT_FALSE(SanitizeAttachmentFileName("...").isEmpty());
  EXPECT_FALSE(SanitizeAttachmentFileName("/").isEmpty());

  // The MIME type is what gives the fallback a usable extension.
  EXPECT_TRUE(
      SanitizeAttachmentFileName("", "application/pdf").endsWith(".pdf"));
}

TEST(EMailMimeTest, SanitizerTruncatesButKeepsTheExtension) {
  const auto safe =
      SanitizeAttachmentFileName(QString(4000, 'a') + QString(".pdf"));

  EXPECT_LT(safe.size(), 255);
  EXPECT_TRUE(safe.endsWith(".pdf"));
}

TEST(EMailMimeTest, DuplicateNamesAreNumberedDeterministically) {
  const QList<EMailAttachment> attachments{
      MakeAttachment("foo.pdf", "application/pdf", "1"),
      MakeAttachment("foo.pdf", "application/pdf", "2"),
      MakeAttachment("foo.pdf", "application/pdf", "3")};

  const auto names = UniqueAttachmentFileNames(attachments);
  ASSERT_EQ(names.size(), 3);
  EXPECT_EQ(names[0], QString("foo.pdf"));
  EXPECT_EQ(names[1], QString("foo (2).pdf"));
  EXPECT_EQ(names[2], QString("foo (3).pdf"));

  // Same message, same names, every time.
  EXPECT_EQ(UniqueAttachmentFileNames(attachments), names);
}

TEST(EMailMimeTest, ExistingFilesOnDiskAreNotOverwritten) {
  const QList<EMailAttachment> attachments{
      MakeAttachment("foo.pdf", "application/pdf", "1")};

  const auto names = UniqueAttachmentFileNames(attachments, {"foo.pdf"});
  ASSERT_EQ(names.size(), 1);
  EXPECT_EQ(names[0], QString("foo (2).pdf"));
}

TEST(EMailMimeTest, CollisionCheckIgnoresCase) {
  const QList<EMailAttachment> attachments{
      MakeAttachment("Foo.PDF", "application/pdf", "1")};

  // A case-insensitive filesystem would treat these as the same file.
  const auto names = UniqueAttachmentFileNames(attachments, {"foo.pdf"});
  EXPECT_NE(names[0].toLower(), QString("foo.pdf"));
}

// --- parse guards ----------------------------------------------------------
//
// Part walking runs synchronously on input from outside, so a message can be
// shaped to cost far more than its size suggests. These bounds are what makes
// it safe to accept messages above the old blanket 1 MB refusal.

namespace {

// A multipart nested `depth` levels deep, which is cheap to write and
// expensive to walk.
auto NestedMultipart(int depth) -> QString {
  QString head =
      "From: a@example.com\n"
      "To: b@example.com\n"
      "Subject: s\n"
      "MIME-Version: 1.0\n";

  QString body;
  for (int i = 0; i < depth; ++i) {
    const auto boundary = QString("b%1").arg(i);
    if (i == 0) {
      head += QString("Content-Type: multipart/mixed; boundary=\"%1\"\n\n")
                  .arg(boundary);
    } else {
      body += QString("Content-Type: multipart/mixed; boundary=\"%1\"\n\n")
                  .arg(boundary);
    }
    body += QString("--%1\n").arg(boundary);
  }
  body += "Content-Type: text/plain\n\ndeep\n";
  return head + body;
}

}  // namespace

TEST(EMailMimeTest, DeeplyNestedMessageIsRefusedNotWalked) {
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseLiteral(NestedMultipart(40), message));

  EMailMetaData parsed;
  EMailParseLimits limits;
  limits.max_depth = 4;

  EXPECT_EQ(ExtractParts(message, parsed, limits), -1);
}

TEST(EMailMimeTest, TooManyPartsIsRefused) {
  QList<EMailAttachment> attachments;
  for (int i = 0; i < 20; ++i) {
    attachments.append(MakeAttachment(QString("f%1.txt").arg(i), "text/plain",
                                      QByteArray("x")));
  }

  QString eml;
  ASSERT_EQ(BuildMimeEML(BasicMeta(), "body", attachments, eml), 0);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml.toUtf8(), message));

  EMailMetaData parsed;
  EMailParseLimits limits;
  limits.max_parts = 5;

  EXPECT_EQ(ExtractParts(message, parsed, limits), -1);
}

TEST(EMailMimeTest, OversizedDecodedContentIsRefused) {
  const QByteArray big(200 * 1024, 'x');

  QString eml;
  ASSERT_EQ(
      BuildMimeEML(BasicMeta(), "body",
                   {MakeAttachment("big.bin", "application/octet-stream", big)},
                   eml),
      0);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml.toUtf8(), message));

  EMailMetaData parsed;
  EMailParseLimits limits;
  limits.max_total_bytes = 64 * 1024;

  EXPECT_EQ(ExtractParts(message, parsed, limits), -1);
}

TEST(EMailMimeTest, AMessageWithinTheLimitsIsAccepted) {
  QString eml;
  ASSERT_EQ(BuildMimeEML(BasicMeta(), "body",
                         {MakeAttachment("a.txt", "text/plain", "hello")}, eml),
            0);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml.toUtf8(), message));

  EMailMetaData parsed;
  EXPECT_EQ(ExtractParts(message, parsed), 0);
  EXPECT_EQ(parsed.attachments.size(), 1);
}

// --- what the signature actually covers ------------------------------------

// In PGP/MIME only the multipart/signed subtree is authenticated. A part
// outside it arrived unsigned, however trustworthy the rest of the message
// looks, and the UI has to be able to say so.
TEST(EMailMimeTest, PartsOutsideTheSignedSubtreeAreMarkedUnsigned) {
  // A well-formed RFC 3156 message: multipart/signed has exactly two parts,
  // the signed entity and the signature. The signed entity is itself a
  // multipart, which is how a signed message carries an attachment.
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseLiteral(
      "From: a@example.com\n"
      "To: b@example.com\n"
      "Subject: s\n"
      "MIME-Version: 1.0\n"
      "Content-Type: multipart/mixed; boundary=\"outer\"\n"
      "\n"
      "--outer\n"
      "Content-Type: multipart/signed; "
      "protocol=\"application/pgp-signature\";\n"
      " micalg=pgp-sha256; boundary=\"inner\"\n"
      "\n"
      "--inner\n"
      "Content-Type: multipart/mixed; boundary=\"signed\"\n"
      "\n"
      "--signed\n"
      "Content-Type: text/plain\n"
      "\n"
      "signed body\n"
      "--signed\n"
      "Content-Type: application/octet-stream\n"
      "Content-Disposition: attachment; filename=\"trusted.bin\"\n"
      "\n"
      "aaa\n"
      "--signed--\n"
      "--inner\n"
      "Content-Type: application/pgp-signature\n"
      "\n"
      "-----BEGIN PGP SIGNATURE-----\n"
      "-----END PGP SIGNATURE-----\n"
      "--inner--\n"
      "--outer\n"
      "Content-Type: application/octet-stream\n"
      "Content-Disposition: attachment; filename=\"smuggled.bin\"\n"
      "\n"
      "bbb\n"
      "--outer--\n",
      message));

  EMailMetaData parsed;
  ASSERT_EQ(ExtractParts(message, parsed), 0);

  QMap<QString, bool> signed_by_name;
  for (const auto& a : parsed.attachments) {
    signed_by_name.insert(a.filename, a.inside_signed_part);
  }

  ASSERT_TRUE(signed_by_name.contains("trusted.bin"));
  ASSERT_TRUE(signed_by_name.contains("smuggled.bin"));
  EXPECT_TRUE(signed_by_name.value("trusted.bin"));
  EXPECT_FALSE(signed_by_name.value("smuggled.bin"));
}

TEST(EMailMimeTest, SiblingOfTheSignedEntityIsNotCovered) {
  // RFC 3156 signs the FIRST part of a multipart/signed and nothing else. A
  // part smuggled in beside it is not covered, however much it looks like it
  // is sitting inside the signed container -- treating it as covered is how
  // an attacker gets content to inherit someone else's signature.
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseLiteral(
      "From: a@example.com\n"
      "To: b@example.com\n"
      "Subject: s\n"
      "MIME-Version: 1.0\n"
      "Content-Type: multipart/signed; "
      "protocol=\"application/pgp-signature\";\n"
      " micalg=pgp-sha256; boundary=\"inner\"\n"
      "\n"
      "--inner\n"
      "Content-Type: text/plain\n"
      "\n"
      "signed body\n"
      "--inner\n"
      "Content-Type: application/octet-stream\n"
      "Content-Disposition: attachment; filename=\"appended.bin\"\n"
      "\n"
      "ccc\n"
      "--inner--\n",
      message));

  EMailMetaData parsed;
  ASSERT_EQ(ExtractParts(message, parsed), 0);

  ASSERT_EQ(parsed.attachments.size(), 1);
  EXPECT_EQ(parsed.attachments[0].filename, QString("appended.bin"));
  EXPECT_FALSE(parsed.attachments[0].inside_signed_part);
}

TEST(EMailMimeTest, OpenPgpKeyPartsAreClassified) {
  QString eml;
  ASSERT_EQ(
      BuildMimeEML(BasicMeta(), "body",
                   {MakeAttachment("key.asc", "application/pgp-keys",
                                   "-----BEGIN PGP PUBLIC KEY BLOCK-----")},
                   eml),
      0);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml.toUtf8(), message));

  EMailMetaData parsed;
  ASSERT_EQ(ExtractParts(message, parsed), 0);
  ASSERT_EQ(parsed.attachments.size(), 1);
  EXPECT_TRUE(parsed.attachments[0].is_openpgp_key);
}

// --- the inner part of an encrypted message --------------------------------
//
// Encryption carries the original body over as a raw, still-encoded slice. If
// the header rebuilt around it loses Content-Transfer-Encoding, the recipient
// decrypts successfully and is then handed base64 as if it were text. This is
// silent corruption: nothing fails, the message is just wrong.

TEST(EMailMimeTest, InnerPartHeaderKeepsContentTransferEncoding) {
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(
      ParseLiteral("From: a@example.com\n"
                   "To: b@example.com\n"
                   "Cc: c@example.com\n"
                   "Reply-To: r@example.com\n"
                   "Subject: s\n"
                   "Date: Sun, 28 Jun 2026 18:58:21 +0200\n"
                   "Message-ID: <abc@example.com>\n"
                   "MIME-Version: 1.0\n"
                   "Content-Type: text/plain; charset=UTF-8\n"
                   "Content-Transfer-Encoding: base64\n"
                   "\n"
                   "aGVsbG8gd29ybGQ=\n",
                   message));

  const auto header = BuildInnerPartHeader(message->getHeader());

  EXPECT_TRUE(header.contains("Content-Transfer-Encoding: base64"));
  EXPECT_TRUE(header.contains("Content-Type: text/plain"));
  EXPECT_TRUE(header.contains("MIME-Version: 1.0"));
  EXPECT_TRUE(header.contains("Cc: "));
  EXPECT_TRUE(header.contains("Reply-To: "));
  EXPECT_TRUE(header.contains("Date: "));
  EXPECT_TRUE(header.contains("Message-ID: "));
}

TEST(EMailMimeTest, InnerPartHeaderKeepsMultipartBoundary) {
  QString eml;
  ASSERT_EQ(
      BuildMimeEML(BasicMeta(), "body",
                   {MakeAttachment("a.pdf", "application/pdf", "x")}, eml),
      0);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml.toUtf8(), message));

  const auto header = BuildInnerPartHeader(message->getHeader());

  // Without the boundary parameter the carried-over body is unparseable, so
  // every attachment in an encrypted message would be lost.
  EXPECT_TRUE(header.contains("multipart/mixed"));
  EXPECT_TRUE(header.contains("boundary="));
}

TEST(EMailMimeTest, InnerPartHeaderDropsRoutingAndTraceHeaders) {
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(
      ParseLiteral("From: a@example.com\n"
                   "To: b@example.com\n"
                   "Subject: s\n"
                   "Received: from evil.example by mx.example\n"
                   "X-Mailer: something\n"
                   "MIME-Version: 1.0\n"
                   "Content-Type: text/plain\n"
                   "\n"
                   "body\n",
                   message));

  const auto header = BuildInnerPartHeader(message->getHeader());

  EXPECT_FALSE(header.contains("Received:"));
  EXPECT_FALSE(header.contains("X-Mailer:"));
}

// --- content that is not a message yet -------------------------------------
//
// The sign/encrypt handlers take a different path when the tab's content does
// not parse as a message: they wrap it in an envelope derived from the keys
// the user selected. These two pin which content lands on which path, since
// that is what decides whether an operation keeps the user's headers or
// synthesizes new ones.

TEST(EMailMimeTest, ViewOutputAlwaysParsesAsAMessage) {
  // Even with nothing filled in, what the view hands back must be a message:
  // otherwise pressing Encrypt discards the headers the user typed and
  // re-derives them from their keys instead.
  EMailMetaData blank;

  QString eml;
  ASSERT_EQ(BuildMimeEML(blank, QByteArray(), {}, eml), 0);

  vmime::shared_ptr<vmime::message> message;
  EXPECT_TRUE(CheckIfEMLMessage(eml.toUtf8(), message));
}

TEST(EMailMimeTest, BareBodyTextDoesNotParseAsAMessage) {
  // The case that gets the derived envelope: text typed straight into the raw
  // source view, never round-tripped through the message view.
  vmime::shared_ptr<vmime::message> message;
  EXPECT_FALSE(CheckIfEMLMessage("just a line the user typed", message));
}

// A draft the user has not addressed yet must still round-trip. It used to
// fail to serialize, and the view's fallback then replaced the document with
// the body alone -- quietly discarding every header already typed, and every
// attachment already added.
TEST(EMailMimeTest, UnaddressedDraftKeepsItsHeadersAndAttachments) {
  EMailMetaData draft;
  draft.from = "Alice <alice@example.com>";
  draft.subject = "Still writing this";

  QString eml;
  ASSERT_EQ(
      BuildMimeEML(draft, "half a thought",
                   {MakeAttachment("notes.txt", "text/plain", "abc")}, eml),
      0);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml.toUtf8(), message));

  EMailMetaData parsed;
  ASSERT_EQ(GetEMLMetaData(message, parsed), 0);
  ASSERT_EQ(ExtractParts(message, parsed), 0);

  EXPECT_EQ(parsed.subject, QString("Still writing this"));
  EXPECT_TRUE(parsed.from.contains("alice@example.com"));
  EXPECT_TRUE(parsed.to.isEmpty());
  ASSERT_EQ(parsed.attachments.size(), 1);
  EXPECT_EQ(parsed.attachments[0].filename, QString("notes.txt"));
}

// --- composite operation status --------------------------------------------
//
// The host reads a status as one of three buckets: greater than zero is OK,
// zero is a warning, and negative is a failure. An encrypt-and-sign is two
// operations reported as one, so the rule for combining them decides what the
// user is told about a message that half worked.

TEST(EMailMimeTest, WorseStatusPrefersCriticalOverWarningOverOk) {
  EXPECT_EQ(WorseStatus(1, 1), 1);
  EXPECT_EQ(WorseStatus(1, 0), 0);
  EXPECT_EQ(WorseStatus(0, 1), 0);
  EXPECT_EQ(WorseStatus(0, -1), -1);
  EXPECT_EQ(WorseStatus(1, -1), -1);
  EXPECT_EQ(WorseStatus(-1, -3), -3);
}

TEST(EMailMimeTest, WorseStatusIsCommutativeAndIdempotent) {
  for (int a : {-3, -1, 0, 1}) {
    for (int b : {-3, -1, 0, 1}) {
      EXPECT_EQ(WorseStatus(a, b), WorseStatus(b, a));
    }
    EXPECT_EQ(WorseStatus(a, a), a);
  }
}

TEST(EMailMimeTest, UnwrapRejectsUnusableOffsets) {
  // A tree whose offsets do not index the bytes it was given. The helper must
  // refuse rather than read past the end of the array.
  EMailPart root;
  root.content_type = "multipart/signed";

  EMailPart entity;
  entity.content_type = "text/plain";
  entity.raw_offset = -1;
  entity.raw_length = 0;

  EMailPart signature;
  signature.content_type = "application/pgp-signature";
  signature.raw_offset = 0;
  signature.raw_length = 1;

  root.children = {entity, signature};

  QByteArray out;
  EXPECT_EQ(UnwrapProtectedLayer(root, QByteArray("short"), out),
            EMailUnwrapResult::kMALFORMED);
  EXPECT_TRUE(out.isEmpty());

  // An offset that starts inside the buffer but runs off the end of it.
  root.children[0].raw_offset = 2;
  root.children[0].raw_length = 9999;
  EXPECT_EQ(UnwrapProtectedLayer(root, QByteArray("short"), out),
            EMailUnwrapResult::kMALFORMED);
}

TEST(EMailMimeTest, UnwrapNeedsASignatureAsTheSecondPart) {
  EMailPart root;
  root.content_type = "multipart/signed";
  root.raw_offset = 0;
  root.body_offset = 4;

  EMailPart a;
  a.content_type = "text/plain";
  a.raw_offset = 0;
  a.raw_length = 2;

  // Two parts, but the second is not a detached signature: not an RFC 3156
  // shape, so there is no "the signature" to remove.
  root.children = {a, a};

  QByteArray out;
  EXPECT_EQ(UnwrapProtectedLayer(root, QByteArray("hello"), out),
            EMailUnwrapResult::kMALFORMED);

  // One part is not it either.
  root.children = {a};
  EXPECT_EQ(UnwrapProtectedLayer(root, QByteArray("hello"), out),
            EMailUnwrapResult::kMALFORMED);
}
