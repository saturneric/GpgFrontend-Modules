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
  QByteArray eml;
  if (BuildPlainTextEML(meta, body, eml) != 0) return false;

  vmime::shared_ptr<vmime::message> message;
  if (!CheckIfEMLMessage(eml, message)) return false;

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
  QByteArray via_plain;
  QByteArray via_mime;

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

  QByteArray eml;
  ASSERT_EQ(BuildMimeEML(BasicMeta(), "see attached", attachments, eml), 0);
  EXPECT_TRUE(eml.contains("multipart/mixed"));

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml, message));

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

  QByteArray eml;
  ASSERT_EQ(BuildMimeEML(BasicMeta(), "b",
                         {MakeAttachment("blob.bin", "application/octet-stream",
                                         binary)},
                         eml),
            0);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml, message));

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

  QByteArray eml;
  ASSERT_EQ(BuildMimeEML(BasicMeta(), "body", attachments, eml), 0);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml, message));

  EMailMetaData parsed;
  ASSERT_EQ(ExtractParts(message, parsed), 0);
  ASSERT_EQ(parsed.attachments.size(), 3);
  EXPECT_EQ(parsed.attachments[0].filename, QString("a.txt"));
  EXPECT_EQ(parsed.attachments[1].filename, QString("b.png"));
  EXPECT_EQ(parsed.attachments[2].filename, QString("c.pdf"));
}

TEST(EMailMimeTest, NonAsciiAttachmentNameSurvives) {
  const auto name = QString::fromUtf8("Jahresbericht Grüße 报告.pdf");

  QByteArray eml;
  ASSERT_EQ(BuildMimeEML(BasicMeta(), "b",
                         {MakeAttachment(name, "application/pdf", "x")}, eml),
            0);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml, message));

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

  QByteArray eml;
  ASSERT_EQ(BuildMimeEML(BasicMeta(), "body", attachments, eml), 0);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml, message));

  EMailMetaData parsed;
  EMailParseLimits limits;
  limits.max_parts = 5;

  EXPECT_EQ(ExtractParts(message, parsed, limits), -1);
}

TEST(EMailMimeTest, OversizedDecodedContentIsRefused) {
  const QByteArray big(200 * 1024, 'x');

  QByteArray eml;
  ASSERT_EQ(
      BuildMimeEML(BasicMeta(), "body",
                   {MakeAttachment("big.bin", "application/octet-stream", big)},
                   eml),
      0);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml, message));

  EMailMetaData parsed;
  EMailParseLimits limits;
  limits.max_total_bytes = 64 * 1024;

  EXPECT_EQ(ExtractParts(message, parsed, limits), -1);
}

TEST(EMailMimeTest, AMessageWithinTheLimitsIsAccepted) {
  QByteArray eml;
  ASSERT_EQ(BuildMimeEML(BasicMeta(), "body",
                         {MakeAttachment("a.txt", "text/plain", "hello")}, eml),
            0);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml, message));

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
  QByteArray eml;
  ASSERT_EQ(
      BuildMimeEML(BasicMeta(), "body",
                   {MakeAttachment("key.asc", "application/pgp-keys",
                                   "-----BEGIN PGP PUBLIC KEY BLOCK-----")},
                   eml),
      0);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml, message));

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
  QByteArray eml;
  ASSERT_EQ(
      BuildMimeEML(BasicMeta(), "body",
                   {MakeAttachment("a.pdf", "application/pdf", "x")}, eml),
      0);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml, message));

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

  QByteArray eml;
  ASSERT_EQ(BuildMimeEML(blank, QByteArray(), {}, eml), 0);

  vmime::shared_ptr<vmime::message> message;
  EXPECT_TRUE(CheckIfEMLMessage(eml, message));
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

  QByteArray eml;
  ASSERT_EQ(
      BuildMimeEML(draft, "half a thought",
                   {MakeAttachment("notes.txt", "text/plain", "abc")}, eml),
      0);

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(CheckIfEMLMessage(eml, message));

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

namespace {

// One layer of exactly what SignEMLData() writes: a multipart/signed wrapping a
// multipart/mixed that holds the original body beside the signer's own key.
auto SignedLayer(const QString& inner, const QString& key_id,
                 const QString& outer_boundary, const QString& inner_boundary)
    -> QString {
  return QString(
             "Content-Type: multipart/signed; "
             "protocol=\"application/pgp-signature\"; micalg=pgp-sha256; "
             "boundary=\"%1\"\n"
             "\n"
             "--%1\n"
             "Content-Type: multipart/mixed; boundary=\"%2\"\n"
             "\n"
             "--%2\n"
             "%3"
             "--%2\n"
             "Content-Type: application/pgp-keys; name=\"OpenPGP_0x%4.asc\"\n"
             "Content-Description: OpenPGP public key\n"
             "Content-Disposition: attachment; "
             "filename=\"OpenPGP_0x%4.asc\"\n"
             "\n"
             "KEY-%4\n"
             "--%2--\n"
             "\n"
             "--%1\n"
             "Content-Type: application/pgp-signature; "
             "name=\"OpenPGP_signature.asc\"\n"
             "\n"
             "SIGNATURE-%4\n"
             "--%1--\n")
      .arg(outer_boundary, inner_boundary, inner, key_id);
}

auto Headers() -> QString {
  return "From: a@example.com\n"
         "To: b@example.com\n"
         "Subject: s\n"
         "MIME-Version: 1.0\n";
}

// What the message looks like after being written back out and read again --
// the only view that says what a recipient would actually get.
auto Reparse(const vmime::shared_ptr<vmime::message>& message, EMailPart& root)
    -> bool {
  const auto raw =
      Q_SC(message->generate(vmime::lineLengthLimits::convenient)).toUtf8();

  vmime::shared_ptr<vmime::message> reparsed;
  if (!CheckIfEMLMessage(raw, reparsed)) return false;

  QList<EMailSignatureRegion> regions;
  return ParseMimeTree(reparsed, raw, root, regions) == 0;
}

}  // namespace

// Signing takes the message as it stands, so signing an already-signed message
// would cover the old signature rather than replace it.
TEST(EMailMimeTest, StrippingASignatureLeavesExactlyTheMessageThatWasSigned) {
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseLiteral(
      Headers() + SignedLayer("Content-Type: text/plain; charset=UTF-8\n"
                              "\n"
                              "hello\n",
                              "AAA", "OUTER", "INNER"),
      message));

  ASSERT_TRUE(StripPreviousSignature(message));

  EMailPart root;
  ASSERT_TRUE(Reparse(message, root));
  EXPECT_EQ(root.content_type, "text/plain");
  EXPECT_TRUE(root.children.isEmpty());
  EXPECT_TRUE(root.data.contains("hello"));
}

// The envelope is not part of what was signed and must survive untouched --
// stripping a signature must not turn the message into a different message.
TEST(EMailMimeTest, StrippingASignatureKeepsTheEnvelopeHeaders) {
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseLiteral(
      Headers() + SignedLayer("Content-Type: text/plain; charset=UTF-8\n"
                              "\n"
                              "hello\n",
                              "AAA", "OUTER", "INNER"),
      message));

  ASSERT_TRUE(StripPreviousSignature(message));

  EMailMetaData meta;
  ASSERT_EQ(GetEMLMetaData(message, meta), 0);
  EXPECT_EQ(meta.subject, "s");
  EXPECT_EQ(meta.to, QStringList{"b@example.com"});
}

// The signer's key is attached unconditionally, so every re-sign left another
// one behind: the user saw every key they had ever signed with, oldest first.
TEST(EMailMimeTest, StrippingRemovesTheKeySignAttachedItself) {
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseLiteral(
      Headers() + SignedLayer("Content-Type: text/plain; charset=UTF-8\n"
                              "\n"
                              "hello\n",
                              "AAA", "OUTER", "INNER"),
      message));

  ASSERT_TRUE(StripPreviousSignature(message));

  const auto out = Q_SC(message->generate(vmime::lineLengthLimits::convenient));
  EXPECT_FALSE(out.contains("KEY-AAA"));
  EXPECT_FALSE(out.contains("pgp-keys"));
  EXPECT_FALSE(out.contains("SIGNATURE-AAA"));
}

// Signatures stacked by repeated re-signing all have to come off, and they
// alternate with the key parts as the layers are peeled.
TEST(EMailMimeTest, StrippingUnwindsEverySignatureEverStacked) {
  const auto once = SignedLayer(
      "Content-Type: text/plain; charset=UTF-8\n\nhello\n", "AAA", "O1", "I1");

  // The second sign wrapped whatever the first one produced, headers and all.
  const auto twice = SignedLayer(once, "BBB", "O2", "I2");
  const auto thrice = SignedLayer(twice, "CCC", "O3", "I3");

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseLiteral(Headers() + thrice, message));

  ASSERT_TRUE(StripPreviousSignature(message));

  EMailPart root;
  ASSERT_TRUE(Reparse(message, root));
  EXPECT_EQ(root.content_type, "text/plain");
  EXPECT_TRUE(root.data.contains("hello"));

  const auto out = Q_SC(message->generate(vmime::lineLengthLimits::convenient));
  for (const auto& key : {"AAA", "BBB", "CCC"}) {
    EXPECT_FALSE(out.contains(QString("KEY-%1").arg(key))) << key;
    EXPECT_FALSE(out.contains(QString("SIGNATURE-%1").arg(key))) << key;
  }
}

// A key the user deliberately attached is theirs. Only the part Sign writes on
// its own initiative is taken back off, and the two are told apart by the
// Content-Description Sign puts on its own.
TEST(EMailMimeTest, StrippingKeepsAPublicKeyTheUserAttached) {
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseLiteral(
      Headers() +
          SignedLayer("Content-Type: multipart/mixed; boundary=\"USER\"\n"
                      "\n"
                      "--USER\n"
                      "Content-Type: text/plain; charset=UTF-8\n"
                      "\n"
                      "hello\n"
                      "--USER\n"
                      "Content-Type: application/pgp-keys; name=\"1234.asc\"\n"
                      "Content-Disposition: attachment; "
                      "filename=\"1234.asc\"\n"
                      "\n"
                      "USER-CHOSE-THIS\n"
                      "--USER--\n",
                      "AAA", "OUTER", "INNER"),
      message));

  ASSERT_TRUE(StripPreviousSignature(message));

  const auto out = Q_SC(message->generate(vmime::lineLengthLimits::convenient));
  EXPECT_TRUE(out.contains("USER-CHOSE-THIS"));
  EXPECT_FALSE(out.contains("KEY-AAA"));
}

// Nothing to take off is the ordinary case -- the first signature of a draft --
// and it must not disturb the message on the way through.
TEST(EMailMimeTest, StrippingAnUnsignedMessageChangesNothing) {
  const auto literal = Headers() +
                       "Content-Type: text/plain; charset=UTF-8\n"
                       "\n"
                       "hello\n";

  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseLiteral(literal, message));

  const auto before =
      Q_SC(message->generate(vmime::lineLengthLimits::convenient));
  EXPECT_FALSE(StripPreviousSignature(message));
  EXPECT_EQ(Q_SC(message->generate(vmime::lineLengthLimits::convenient)),
            before);
}

// multipart/signed is not OpenPGP's alone. A signature under some other
// protocol is not ours to remove, and removing it would destroy a message we
// cannot rebuild.
TEST(EMailMimeTest, StrippingLeavesASignatureOfAnotherProtocolAlone) {
  vmime::shared_ptr<vmime::message> message;
  ASSERT_TRUE(ParseLiteral(
      Headers() + "Content-Type: multipart/signed; "
                  "protocol=\"application/pkcs7-signature\"; micalg=sha-256; "
                  "boundary=\"B\"\n"
                  "\n"
                  "--B\n"
                  "Content-Type: text/plain; charset=UTF-8\n"
                  "\n"
                  "hello\n"
                  "--B\n"
                  "Content-Type: application/pkcs7-signature\n"
                  "\n"
                  "SMIME\n"
                  "--B--\n",
      message));

  EXPECT_FALSE(StripPreviousSignature(message));

  EMailPart root;
  ASSERT_TRUE(Reparse(message, root));
  EXPECT_EQ(root.content_type, "multipart/signed");
}

// ---------------------------------------------------------------------------
// IsSafeToOpenAttachment
//
// Decides whether a part that arrived from a stranger may be handed to the
// desktop to open. Getting this wrong in the permissive direction runs the
// stranger's code, so the tests below are mostly about what it must REFUSE.
// ---------------------------------------------------------------------------

namespace {

auto AttachmentNamed(const QString& filename, const QString& mime_type = {})
    -> EMailAttachment {
  EMailAttachment att;
  att.filename = filename;
  att.mime_type = mime_type;
  return att;
}

}  // namespace

TEST(EMailMimeTest, OpenableAllowsOrdinaryDocuments) {
  for (const auto* name :
       {"report.pdf", "notes.txt", "sheet.csv", "photo.jpg", "scan.PNG",
        "slides.pptx", "song.mp3", "clip.mp4", "card.vcf", "invite.ics"}) {
    EXPECT_TRUE(IsSafeToOpenAttachment(AttachmentNamed(name))) << name;
  }
}

TEST(EMailMimeTest, OpenableRefusesExecutables) {
  // The whole point of the allow-list. Every one of these is a file whose
  // registered handler RUNS it.
  for (const auto* name :
       {"invoice.exe", "setup.msi", "run.bat", "run.cmd", "script.ps1",
        "install.sh", "payload.scr", "shortcut.lnk", "macro.vbs", "app.jar",
        "tool.com", "lib.dll", "thing.app", "pkg.deb", "pkg.rpm"}) {
    EXPECT_FALSE(IsSafeToOpenAttachment(AttachmentNamed(name))) << name;
  }
}

TEST(EMailMimeTest, OpenableRefusesArchivesAndScriptableImages) {
  // An archive hides what is inside it until it is already out; SVG carries
  // script, so a viewer that honours it runs a stranger's code.
  for (const auto* name :
       {"bundle.zip", "bundle.tar", "bundle.gz", "bundle.7z", "bundle.rar",
        "drawing.svg", "page.html", "page.htm"}) {
    EXPECT_FALSE(IsSafeToOpenAttachment(AttachmentNamed(name))) << name;
  }
}

TEST(EMailMimeTest, OpenableRefusesWhatItCannotRecognise) {
  // No extension, or one nobody has heard of: refused, because the decision
  // is an allow-list and an unknown type is not on it.
  EXPECT_FALSE(IsSafeToOpenAttachment(AttachmentNamed("README")));
  EXPECT_FALSE(IsSafeToOpenAttachment(AttachmentNamed("data.")));
  EXPECT_FALSE(IsSafeToOpenAttachment(AttachmentNamed("")));
  EXPECT_FALSE(IsSafeToOpenAttachment(AttachmentNamed("thing.qqzz")));
}

TEST(EMailMimeTest, OpenableJudgesTheSanitizedName) {
  // The decision has to be made about the name the desktop will actually
  // dispatch on, which is the sanitized one. A part that smuggles a path in
  // its filename must be judged on the component that survives.
  EXPECT_TRUE(IsSafeToOpenAttachment(AttachmentNamed("../../etc/report.pdf")));
  EXPECT_FALSE(IsSafeToOpenAttachment(AttachmentNamed("../../etc/evil.exe")));

  // A double extension is judged on the LAST one, which is what the system
  // dispatches on -- "report.pdf.exe" is an executable.
  EXPECT_FALSE(IsSafeToOpenAttachment(AttachmentNamed("report.pdf.exe")));
  EXPECT_TRUE(IsSafeToOpenAttachment(AttachmentNamed("report.exe.pdf")));
}

TEST(EMailMimeTest, OpenableIgnoresTheSendersContentType) {
  // The Content-Type is chosen by the sender and need not agree with the
  // extension. The extension is what the desktop acts on, so it is what
  // decides -- a declared "text/plain" must not launder an .exe.
  EXPECT_FALSE(
      IsSafeToOpenAttachment(AttachmentNamed("evil.exe", "text/plain")));
  EXPECT_TRUE(IsSafeToOpenAttachment(
      AttachmentNamed("report.pdf", "application/octet-stream")));
}

// ---------------------------------------------------------------------------
// SuggestedEMailFileName
//
// The name a save dialog offers for a message. Built from the subject, which
// is written by whoever sent the message, so it is treated as hostile.
// ---------------------------------------------------------------------------

TEST(EMailMimeTest, SuggestedNameComesFromTheSubject) {
  EXPECT_EQ(SuggestedEMailFileName("Quarterly report"),
            QString("Quarterly report.eml"));
}

TEST(EMailMimeTest, SuggestedNameFallsBackWhenThereIsNoSubject) {
  for (const auto* subject : {"", "   ", "\t\n"}) {
    EXPECT_EQ(SuggestedEMailFileName(subject), QString("untitled.eml"))
        << subject;
  }
}

TEST(EMailMimeTest, SuggestedNameAlwaysEndsInEml) {
  for (const auto* subject : {"Report", "Report.eml", "Report.pdf",
                              "../../etc/passwd", "///", "..."}) {
    EXPECT_TRUE(SuggestedEMailFileName(subject).endsWith(".eml")) << subject;
  }
}

TEST(EMailMimeTest, SuggestedNameCannotEscapeTheChosenFolder) {
  // A subject is attacker-chosen. Whatever it contains, the result has to stay
  // one path component.
  for (const auto* subject :
       {"../../etc/passwd", "/etc/shadow", "C:\\Windows\\evil", "a/b/c",
        "..\\..\\x", "re: \"quoted\" <thing>"}) {
    const auto name = SuggestedEMailFileName(subject);

    EXPECT_FALSE(name.contains('/')) << subject;
    EXPECT_FALSE(name.contains('\\')) << subject;
    EXPECT_FALSE(name.startsWith('.')) << subject;
    EXPECT_NE(name, QString("..")) << subject;
  }
}

TEST(EMailMimeTest, SuggestedNameIsShortEnoughToRead) {
  const QString long_subject(500, QChar('x'));
  const auto name = SuggestedEMailFileName(long_subject);

  EXPECT_LE(name.size(), 70);
  EXPECT_TRUE(name.endsWith(".eml"));
}

TEST(EMailMimeTest, SuggestedNameAvoidsReservedDeviceNames) {
  // "NUL.eml" is still NUL on Windows. The sanitizer handles this; this test
  // pins that the suggestion path actually goes through it.
  EXPECT_NE(SuggestedEMailFileName("NUL").toUpper(), QString("NUL.EML"));
  EXPECT_NE(SuggestedEMailFileName("CON").toUpper(), QString("CON.EML"));
}

// --- decrypt-then-verify -----------------------------------------------------
//
// What happens to a plaintext AFTER it has been decrypted is a policy decision,
// and it used to live inside the module event handler where nothing could test
// it. The rule it got wrong: an encrypted message that is not signed is the
// ordinary case, and "there is no signature to check" must never be reported as
// a failure of the operation -- the handler discarded the plaintext when it
// was, which cost the user the message they had just decrypted.

namespace {

auto EncryptedThenPlaintext(const QByteArray& inner) -> QByteArray {
  return inner;
}

auto SignedPlaintext() -> QByteArray {
  QByteArray eml;
  eml += "From: alice@example.com\r\n";
  eml += "To: bob@example.com\r\n";
  eml += "MIME-Version: 1.0\r\n";
  eml +=
      "Content-Type: multipart/signed; micalg=pgp-sha256; "
      "protocol=\"application/pgp-signature\"; boundary=\"b\"\r\n";
  eml += "\r\n";
  eml += "--b\r\n";
  eml += "Content-Type: text/plain\r\n\r\n";
  eml += "hello\r\n";
  eml += "--b\r\n";
  eml += "Content-Type: application/pgp-signature\r\n\r\n";
  eml += "-----BEGIN PGP SIGNATURE-----\r\nx\r\n";
  eml += "-----END PGP SIGNATURE-----\r\n";
  eml += "--b--\r\n";
  return eml;
}

auto UnsignedPlaintext() -> QByteArray {
  QByteArray eml;
  eml += "From: alice@example.com\r\n";
  eml += "To: bob@example.com\r\n";
  eml += "Subject: inner\r\n";
  eml += "MIME-Version: 1.0\r\n";
  eml += "Content-Type: text/plain; charset=utf-8\r\n";
  eml += "\r\n";
  eml += "the secret\r\n";
  return eml;
}

}  // namespace

TEST(EMailMimeTest, AnUnsignedPlaintextIsNotSomethingToVerify) {
  // The case the whole change exists for. Nothing here is an error: the
  // decrypt worked and there is simply no signature.
  EXPECT_EQ(PlanVerifyAfterDecrypt(EncryptedThenPlaintext(UnsignedPlaintext())),
            EMailPostDecryptPlan::kNOT_SIGNED);
}

TEST(EMailMimeTest, ASignedPlaintextIsHandedToTheVerify) {
  EXPECT_EQ(PlanVerifyAfterDecrypt(SignedPlaintext()),
            EMailPostDecryptPlan::kVERIFY);
}

TEST(EMailMimeTest, AMalformedSignedPlaintextStillReachesTheVerify) {
  // It CLAIMS multipart/signed and is missing the protocol parameter. The
  // verify refuses it and names what is wrong, and that diagnosis is worth
  // more than quietly calling the message unsigned.
  QByteArray eml;
  eml += "From: alice@example.com\r\n";
  eml += "MIME-Version: 1.0\r\n";
  eml += "Content-Type: multipart/signed; boundary=\"b\"\r\n";
  eml += "\r\n--b\r\nContent-Type: text/plain\r\n\r\nhi\r\n--b--\r\n";

  EXPECT_EQ(PlanVerifyAfterDecrypt(eml), EMailPostDecryptPlan::kVERIFY);
}

TEST(EMailMimeTest, PlaintextThatIsNotAMessageIsUnreadableNotUnsigned) {
  EXPECT_EQ(PlanVerifyAfterDecrypt(QByteArray()),
            EMailPostDecryptPlan::kUNREADABLE);
  EXPECT_EQ(PlanVerifyAfterDecrypt("   \r\n  "),
            EMailPostDecryptPlan::kUNREADABLE);
  EXPECT_EQ(PlanVerifyAfterDecrypt("just some words, no headers at all"),
            EMailPostDecryptPlan::kUNREADABLE);
}

TEST(EMailMimeTest, NestedSignatureDoesNotForceATopLevelVerify) {
  // A signature deeper inside is the per-region walk's business. Sending this
  // to VerifyEMLData would only produce a refusal.
  QByteArray eml;
  eml += "From: alice@example.com\r\n";
  eml += "MIME-Version: 1.0\r\n";
  eml += "Content-Type: multipart/mixed; boundary=\"out\"\r\n";
  eml += "\r\n--out\r\n";
  eml +=
      "Content-Type: multipart/signed; micalg=pgp-sha256; "
      "protocol=\"application/pgp-signature\"; boundary=\"in\"\r\n";
  eml += "\r\n--in\r\nContent-Type: text/plain\r\n\r\nhi\r\n--in\r\n";
  eml += "Content-Type: application/pgp-signature\r\n\r\nSIG\r\n--in--\r\n";
  eml += "\r\n--out--\r\n";

  EXPECT_EQ(PlanVerifyAfterDecrypt(eml), EMailPostDecryptPlan::kNOT_SIGNED);
}

TEST(EMailMimeTest, MergingVerifiedMetaDataDoesNotDuplicateAttachments) {
  // Decrypt and verify walk the SAME plaintext, and ExtractParts() only ever
  // appends. Sharing one metadata object between them listed every attachment
  // twice; this is the join that replaced it.
  EMailMetaData decrypted;
  decrypted.body = "body";
  decrypted.body_content_type = "text/plain";
  for (const auto* name : {"a.pdf", "b.png"}) {
    EMailAttachment att;
    att.filename = name;
    att.data = "x";
    decrypted.attachments.append(att);
  }

  EMailMetaData verified = decrypted;  // the same walk, done again
  verified.signed_entity_digest = "abcd";
  verified.signed_entity_digest_algo = "SHA-256";
  verified.micalg = "pgp-sha256";

  MergeVerifiedMetaData(decrypted, verified);

  ASSERT_EQ(decrypted.attachments.size(), 2);
  EXPECT_EQ(decrypted.attachments[0].filename, QString("a.pdf"));
  EXPECT_EQ(decrypted.attachments[1].filename, QString("b.png"));
  EXPECT_EQ(decrypted.body, QByteArray("body"));

  // The signature side is what the verify was for.
  EXPECT_EQ(decrypted.signed_entity_digest, QString("abcd"));
  EXPECT_EQ(decrypted.micalg, QString("pgp-sha256"));
}

TEST(EMailMimeTest, MergingPrefersTheHeadersFromInsideTheCiphertext) {
  // The inner headers are the ones an outer envelope cannot have rewritten.
  EMailMetaData decrypted;
  decrypted.from = "outer@example.com";
  decrypted.subject = "...";
  decrypted.to = QStringList{"outer-to@example.com"};

  EMailMetaData verified;
  verified.from = "inner@example.com";
  verified.subject = "the real subject";
  verified.to = QStringList{"inner-to@example.com"};

  MergeVerifiedMetaData(decrypted, verified);

  EXPECT_EQ(decrypted.from, QString("inner@example.com"));
  EXPECT_EQ(decrypted.subject, QString("the real subject"));
  EXPECT_EQ(decrypted.to, QStringList{"inner-to@example.com"});
}

TEST(EMailMimeTest, MergingKeepsWhatTheVerifyNeverSaw) {
  // A verify that reports nothing must not blank out what the decrypt found.
  EMailMetaData decrypted;
  decrypted.from = "alice@example.com";
  decrypted.subject = "kept";
  decrypted.body = "kept body";
  decrypted.encrypted_data = "CIPHER";

  MergeVerifiedMetaData(decrypted, EMailMetaData{});

  EXPECT_EQ(decrypted.from, QString("alice@example.com"));
  EXPECT_EQ(decrypted.subject, QString("kept"));
  EXPECT_EQ(decrypted.body, QByteArray("kept body"));
  EXPECT_EQ(decrypted.encrypted_data, QByteArray("CIPHER"));
}

// --- what a failed operation leaves behind ----------------------------------

TEST(EMailMimeTest, AFailedOperationLeavesTheDocumentByteIdentical) {
  // The host writes the result's data straight into the editor, so a failing
  // operation's payload REPLACES what the user was composing. Encrypt used to
  // answer with a base64 rendering of it, which is how a cancelled passphrase
  // prompt turned a message into a screen of base64.
  QByteArray original;
  original += "From: a@example.com\r\n";
  original += "Subject: \xC3\xA9t\xC3\xA9\r\n";  // UTF-8, not ASCII
  original += "\r\n";
  original += "body with an embedded ";
  original += '\0';
  original += " NUL and a trailing space \r\n";

  const auto kept = DocumentUnchangedOnFailure(original);

  EXPECT_EQ(kept, original);
  EXPECT_NE(kept, original.toBase64());
  EXPECT_TRUE(kept.contains('\0'));
}

TEST(EMailMimeTest, AFailedOperationDoesNotInventAnEmptyDocument) {
  // Handing back nothing would be its own kind of loss: the host writes what
  // it is given, so an empty payload empties the tab.
  const QByteArray original("composed text");
  EXPECT_FALSE(DocumentUnchangedOnFailure(original).isEmpty());
}

// --- what the security surface may claim -------------------------------------
//
// The badge was derived from ClassifyOpenPGPStructure() alone, which does no
// cryptography. A multipart/signed whose signature part held forty bytes of
// garbage therefore read as "Signed", in the accent colour, and nothing on the
// message surface ever said otherwise -- the honest wording lived in a Security
// tab the user had to go and open.

namespace {

auto Sig(int validity, const QString& uid = "Alice <alice@example.com>")
    -> EMailSignatureResult {
  EMailSignatureResult r;
  r.validity = validity;
  r.uid = uid;
  return r;
}

constexpr auto kFrom = "Alice <alice@example.com>";

}  // namespace

TEST(EMailMimeTest, AStructurallySignedMessageIsNotYetAVerifiedOne) {
  // The whole point. Nothing has looked at it, so nothing may say it is good.
  EXPECT_EQ(DeriveSecurityBadge(EMailSecurityState::kSIGNED,
                                EMailVerifyState::kNOT_ATTEMPTED, {}, kFrom),
            EMailBadgeState::kSIGNED_UNVERIFIED);

  // A verification that ran and found nothing is equally not a good verdict.
  EXPECT_EQ(DeriveSecurityBadge(EMailSecurityState::kSIGNED,
                                EMailVerifyState::kATTEMPTED_EMPTY, {}, kFrom),
            EMailBadgeState::kSIGNED_UNVERIFIED);
}

TEST(EMailMimeTest, AVerifiedSignatureIsTheOnlyThingCalledGood) {
  for (const auto validity : {0, 2}) {
    EXPECT_EQ(DeriveSecurityBadge(EMailSecurityState::kSIGNED,
                                  EMailVerifyState::kVERIFIED, {Sig(validity)},
                                  kFrom),
              EMailBadgeState::kSIGNED_GOOD)
        << "validity " << validity;
    EXPECT_EQ(ToneForBadge(EMailBadgeState::kSIGNED_GOOD),
              EMailBadgeTone::kGOOD);
  }
}

TEST(EMailMimeTest, ARedSignatureIsBadNotAMinorIssue) {
  // validity 1 is GPGME_SIGSUM_RED. It was labelled "valid, with issues" and
  // painted the same amber as a missing key.
  const auto badge =
      DeriveSecurityBadge(EMailSecurityState::kSIGNED,
                          EMailVerifyState::kVERIFIED, {Sig(1)}, kFrom);

  EXPECT_EQ(badge, EMailBadgeState::kSIGNED_BAD);
  EXPECT_EQ(ToneForBadge(badge), EMailBadgeTone::kDANGER);
}

TEST(EMailMimeTest, AnInvalidOrRevokedSignatureIsAlsoBad) {
  for (const auto validity : {3, 5}) {
    EXPECT_EQ(DeriveSecurityBadge(EMailSecurityState::kSIGNED,
                                  EMailVerifyState::kVERIFIED, {Sig(validity)},
                                  kFrom),
              EMailBadgeState::kSIGNED_BAD)
        << "validity " << validity;
  }
}

TEST(EMailMimeTest, AMissingKeyIsItsOwnAnswerAndNotAForgery) {
  const auto badge =
      DeriveSecurityBadge(EMailSecurityState::kSIGNED,
                          EMailVerifyState::kVERIFIED, {Sig(4)}, kFrom);

  EXPECT_EQ(badge, EMailBadgeState::kSIGNED_UNKNOWN_KEY);
  // Worth checking, not an alarm: importing a key resolves it.
  EXPECT_EQ(ToneForBadge(badge), EMailBadgeTone::kWARN);
}

TEST(EMailMimeTest, AnExpiredSignatureIsDistinguishedFromABadOne) {
  for (const auto validity : {6, 7}) {
    EXPECT_EQ(DeriveSecurityBadge(EMailSecurityState::kSIGNED,
                                  EMailVerifyState::kVERIFIED, {Sig(validity)},
                                  kFrom),
              EMailBadgeState::kSIGNED_EXPIRED)
        << "validity " << validity;
  }
}

TEST(EMailMimeTest, AnUnknownValidityIsNeverTreatedAsGood) {
  // -1 is the default in EMailSignatureResult precisely so that an absent or
  // unparseable field cannot read as fully valid.
  for (const auto validity : {-1, 99}) {
    EXPECT_NE(DeriveSecurityBadge(EMailSecurityState::kSIGNED,
                                  EMailVerifyState::kVERIFIED, {Sig(validity)},
                                  kFrom),
              EMailBadgeState::kSIGNED_GOOD)
        << "validity " << validity;
  }
}

TEST(EMailMimeTest, TheWorstSignatureDecides) {
  // One good signature beside one bad one is not a good message. Reporting the
  // best of them would let an attacker simply add a second signature.
  EXPECT_EQ(
      DeriveSecurityBadge(EMailSecurityState::kSIGNED,
                          EMailVerifyState::kVERIFIED, {Sig(0), Sig(1)}, kFrom),
      EMailBadgeState::kSIGNED_BAD);

  EXPECT_EQ(
      DeriveSecurityBadge(EMailSecurityState::kSIGNED,
                          EMailVerifyState::kVERIFIED, {Sig(0), Sig(4)}, kFrom),
      EMailBadgeState::kSIGNED_UNKNOWN_KEY);
}

TEST(EMailMimeTest, AValidSignatureByAnotherAddressIsFlagged) {
  // The transplanted-signature shape: the cryptography is fine and the key
  // belongs to somebody else entirely.
  const auto badge = DeriveSecurityBadge(
      EMailSecurityState::kSIGNED, EMailVerifyState::kVERIFIED,
      {Sig(0, "Mallory <mallory@evil.example>")}, kFrom);

  EXPECT_EQ(badge, EMailBadgeState::kSIGNED_MISMATCH);
  EXPECT_EQ(ToneForBadge(badge), EMailBadgeTone::kWARN);
}

TEST(EMailMimeTest, IdentityIsCheckedOnlyAfterTheCryptographyPasses) {
  // A bad signature by another address is reported as bad. Leading with the
  // address would bury the fact that the signature does not verify at all.
  EXPECT_EQ(DeriveSecurityBadge(
                EMailSecurityState::kSIGNED, EMailVerifyState::kVERIFIED,
                {Sig(1, "Mallory <mallory@evil.example>")}, kFrom),
            EMailBadgeState::kSIGNED_BAD);
}

TEST(EMailMimeTest, MatchingAddressesAreComparedNotStringified) {
  // Display names and case differ constantly and mean nothing; the address is
  // what is being compared.
  EXPECT_TRUE(SignerMatchesAddress(Sig(0, "A. Person <ALICE@Example.COM>"),
                                   "alice@example.com"));
  EXPECT_TRUE(SignerMatchesAddress(Sig(0, "alice@example.com"), kFrom));
  EXPECT_FALSE(SignerMatchesAddress(Sig(0, "alice@example.com.evil"),
                                    "alice@example.com"));
}

TEST(EMailMimeTest, AnAddressThatCannotBeComparedIsNotCalledAMismatch) {
  // A result with no UID is the unknown-key case, which the badge already
  // reports on its own. Calling it a mismatch as well would say the same thing
  // twice, and in stronger words than the evidence supports.
  EXPECT_TRUE(SignerMatchesAddress(Sig(0, ""), kFrom));
  EXPECT_TRUE(SignerMatchesAddress(Sig(0), ""));
}

TEST(EMailMimeTest, AnUnprotectedMessageIsStatedQuietly) {
  const auto badge = DeriveSecurityBadge(
      EMailSecurityState::kPLAIN, EMailVerifyState::kNOT_ATTEMPTED, {}, kFrom);

  EXPECT_EQ(badge, EMailBadgeState::kNOT_PROTECTED);
  // Not a fault. Painting the ordinary case as a warning is how people learn
  // to ignore the warnings that matter.
  EXPECT_EQ(ToneForBadge(badge), EMailBadgeTone::kMUTED);
}

TEST(EMailMimeTest, EncryptionAloneSaysNothingAboutWhoSent) {
  const auto badge =
      DeriveSecurityBadge(EMailSecurityState::kENCRYPTED,
                          EMailVerifyState::kNOT_ATTEMPTED, {}, kFrom);

  EXPECT_EQ(badge, EMailBadgeState::kENCRYPTED_ONLY);
  EXPECT_EQ(ToneForBadge(badge), EMailBadgeTone::kMUTED);
}

TEST(EMailMimeTest, ASignedAndEncryptedMessageIsStillJudgedOnItsSignature) {
  EXPECT_EQ(DeriveSecurityBadge(EMailSecurityState::kSIGNED_ENCRYPTED,
                                EMailVerifyState::kNOT_ATTEMPTED, {}, kFrom),
            EMailBadgeState::kSIGNED_UNVERIFIED);

  EXPECT_EQ(DeriveSecurityBadge(EMailSecurityState::kSIGNED_ENCRYPTED,
                                EMailVerifyState::kVERIFIED, {Sig(0)}, kFrom),
            EMailBadgeState::kSIGNED_GOOD);
}

TEST(EMailMimeTest, AMalformedStructureIsReportedAsWrong) {
  const auto badge =
      DeriveSecurityBadge(EMailSecurityState::kMALFORMED_PGP,
                          EMailVerifyState::kNOT_ATTEMPTED, {}, kFrom);

  EXPECT_EQ(badge, EMailBadgeState::kMALFORMED);
  EXPECT_EQ(ToneForBadge(badge), EMailBadgeTone::kDANGER);
}

TEST(EMailMimeTest, ResultsWithoutAVerifiedStateAreNotTrusted) {
  // Results present but the state says the walk never completed: the results
  // cannot be the whole story, so they must not produce a good verdict.
  EXPECT_EQ(
      DeriveSecurityBadge(EMailSecurityState::kSIGNED,
                          EMailVerifyState::kNOT_ATTEMPTED, {Sig(0)}, kFrom),
      EMailBadgeState::kSIGNED_UNVERIFIED);
}
