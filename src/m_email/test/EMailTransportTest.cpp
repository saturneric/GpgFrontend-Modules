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
#include <QJsonDocument>
#include <QSet>
#include <vmime/exception.hpp>
#include <vmime/net/smtp/SMTPExceptions.hpp>
#include <vmime/security/cert/certificateExpiredException.hpp>
#include <vmime/security/cert/certificateNotTrustedException.hpp>
#include <vmime/security/cert/serverIdentityException.hpp>

#include "EMailAccountModel.h"
#include "EMailHelper.h"
#include "EMailNetError.h"
#include "EMailOutgoing.h"

// ---------------------------------------------------------------------------
// Account model: ports, cleartext policy, page sizes
// ---------------------------------------------------------------------------

TEST(EMailAccountModelTest, SecurityChoiceImpliesTheWellKnownPort) {
  EXPECT_EQ(MailDefaultPort(true, MailTlsMode::kIMPLICIT), 993);
  EXPECT_EQ(MailDefaultPort(true, MailTlsMode::kSTARTTLS), 143);
  EXPECT_EQ(MailDefaultPort(false, MailTlsMode::kIMPLICIT), 465);
  EXPECT_EQ(MailDefaultPort(false, MailTlsMode::kSTARTTLS), 587);
}

TEST(EMailAccountModelTest, ThePortFollowsTheSecurityChoiceAndNothingElse) {
  // There is no port field any more: a port is policy. What matters is that
  // changing the connection kind changes the port, so a port number can never
  // be the thing that decides whether a session is encrypted.
  MailTransportConfig config;
  config.tls = MailTlsMode::kSTARTTLS;
  EXPECT_EQ(config.EffectivePort(true), 143);
  EXPECT_EQ(config.EffectivePort(false), 587);

  config.tls = MailTlsMode::kIMPLICIT;
  EXPECT_EQ(config.EffectivePort(true), 993);
  EXPECT_EQ(config.EffectivePort(false), 465);
}

TEST(EMailAccountModelTest, AStoredPortOverrideIsIgnoredAndNeverWrittenBack) {
  // Accounts written by an earlier build may still carry one. Reading it back
  // must retire it rather than keep a hidden override alive that no part of
  // the interface can show or change.
  const QJsonObject json{{"enabled", true},
                         {"host", "mail.example.org"},
                         {"tls", "starttls"},
                         {"port", 1143},
                         {"username", "someone"}};

  const auto config = MailTransportConfig::FromJson(json);
  EXPECT_EQ(config.EffectivePort(true), 143);
  EXPECT_FALSE(config.ToJson().contains("port"));
}

TEST(EMailAccountModelTest, OnlyLoopbackMayBeSpokenToInTheClear) {
  EXPECT_TRUE(MailHostAllowsCleartext("localhost"));
  EXPECT_TRUE(MailHostAllowsCleartext("127.0.0.1"));
  EXPECT_TRUE(MailHostAllowsCleartext("::1"));

  EXPECT_FALSE(MailHostAllowsCleartext("mail.example.org"));
  EXPECT_FALSE(MailHostAllowsCleartext("192.168.1.10"));
  EXPECT_FALSE(MailHostAllowsCleartext(""));
}

TEST(EMailAccountModelTest, AHostThatMerelyLooksLocalIsStillRemote) {
  // Matching on text rather than parsing the address would hand this one a
  // cleartext session, which is exactly the trick this guards against.
  EXPECT_FALSE(MailHostAllowsCleartext("127.0.0.1.example.org"));
  EXPECT_FALSE(MailHostAllowsCleartext("localhost.example.org"));
  EXPECT_FALSE(MailHostAllowsCleartext("notlocalhost"));
}

TEST(EMailAccountModelTest, AStoredCleartextModeIsRefusedForARemoteHost) {
  // Simulates a settings file edited by hand, or written before the policy
  // existed. Reading it back must not produce a cleartext remote transport.
  QJsonObject json{{"enabled", true},
                   {"host", "mail.example.org"},
                   {"tls", "none"},
                   {"username", "someone"}};

  const auto config = MailTransportConfig::FromJson(json);
  EXPECT_NE(config.tls, MailTlsMode::kNONE);
}

TEST(EMailAccountModelTest, AStoredCleartextModeSurvivesForLoopback) {
  QJsonObject json{{"enabled", true},
                   {"host", "127.0.0.1"},
                   {"tls", "none"},
                   {"username", "someone"}};

  EXPECT_EQ(MailTransportConfig::FromJson(json).tls, MailTlsMode::kNONE);
}

TEST(EMailAccountModelTest, AnUnrecognisedSecurityValueReadsAsTheSafestOne) {
  EXPECT_EQ(MailTlsModeFromString("startls"), MailTlsMode::kIMPLICIT);
  EXPECT_EQ(MailTlsModeFromString(""), MailTlsMode::kIMPLICIT);
  EXPECT_EQ(MailTlsModeFromString("plaintext"), MailTlsMode::kIMPLICIT);
}

TEST(EMailAccountModelTest, PageSizesAreClampedToTheOfferedSet) {
  // No longer configurable either, but the worker still clamps whatever it is
  // handed, so a caller cannot ask a server for an unbounded page.
  EXPECT_EQ(MailClampPageSize(25), 25);
  EXPECT_EQ(MailClampPageSize(200), 200);

  EXPECT_EQ(MailClampPageSize(75), 50);
  EXPECT_EQ(MailClampPageSize(1), 25);
  EXPECT_EQ(MailClampPageSize(100000), 200);
  EXPECT_EQ(MailClampPageSize(-5), 25);
}

TEST(EMailAccountModelTest, AStoredPageSizeIsIgnored) {
  const QJsonObject json{
      {"address", "someone@example.org"},
      {"page_size", 175},
      {"imap", QJsonObject{{"enabled", true}, {"host", "mail.example.org"}}}};

  EXPECT_FALSE(
      MailAccountConfig::FromJson(json).ToJson().contains("page_size"));
}

TEST(EMailAccountModelTest, SerializedAccountsNeverCarryASecret) {
  MailAccountConfig account;
  account.id = "an-id";
  account.address = "someone@example.org";
  account.imap.enabled = true;
  account.imap.host = "imap.example.org";
  account.imap.username = "someone";

  const auto json = account.ToJson();

  // Checked by key rather than by substring: "remember_password" is a boolean
  // preference and belongs here, while a key that actually holds a secret does
  // not. The account record goes into ordinary settings, gets logged and may
  // be exported, so the distinction has to be exact.
  const QStringList forbidden{"password", "secret", "token", "credential",
                              "passphrase"};
  for (const auto& key : json.keys()) {
    EXPECT_FALSE(forbidden.contains(key.toLower()))
        << "account JSON carries a secret-bearing key: " << key.toStdString();
  }

  for (const auto& transport :
       {json.value("imap").toObject(), json.value("smtp").toObject()}) {
    for (const auto& key : transport.keys()) {
      EXPECT_FALSE(forbidden.contains(key.toLower()))
          << "transport JSON carries a secret-bearing key: "
          << key.toStdString();
    }
  }

  // The value side: the account holds a reference, never the secret itself.
  EXPECT_TRUE(json.contains("id"));
}

TEST(EMailAccountModelTest, AnAccountNeedsAnAddressAndOneTransport) {
  MailAccountConfig account;
  EXPECT_FALSE(account.IsUsable());

  account.address = "someone@example.org";
  EXPECT_FALSE(account.IsUsable());

  account.imap.enabled = true;
  EXPECT_FALSE(account.IsUsable());  // no host yet

  account.imap.host = "imap.example.org";
  EXPECT_TRUE(account.IsUsable());
}

TEST(EMailAccountModelTest, EitherTransportAloneIsEnough) {
  MailAccountConfig smtp_only;
  smtp_only.address = "someone@example.org";
  smtp_only.smtp.enabled = true;
  smtp_only.smtp.host = "smtp.example.org";

  // Neither protocol may be required merely because the other is configured.
  EXPECT_TRUE(smtp_only.IsUsable());
  EXPECT_FALSE(smtp_only.imap.enabled);
}

// ---------------------------------------------------------------------------
// Envelope construction and BCC
// ---------------------------------------------------------------------------

TEST(EMailOutgoingTest, AddressesAreReducedToAddrSpec) {
  EXPECT_EQ(MailAddressOnly("Someone <a@example.org>"), "a@example.org");
  EXPECT_EQ(MailAddressOnly("  a@example.org "), "a@example.org");
  EXPECT_EQ(MailAddressOnly(""), "");
}

TEST(EMailOutgoingTest, RecipientsAreDeduplicatedCaseInsensitively) {
  const auto result =
      MailDedupeAddresses({"A@example.org", "a@example.org",
                           "Someone <A@EXAMPLE.ORG>", "b@example.org"});

  ASSERT_EQ(result.size(), 2);
  EXPECT_EQ(result.at(0), "A@example.org");
  EXPECT_EQ(result.at(1), "b@example.org");
}

TEST(EMailOutgoingTest, BccReachesTheEnvelopeButNeverTheMessage) {
  EMailMetaData meta;
  meta.from = "Sender <sender@example.org>";
  meta.to = {"to@example.org"};
  meta.cc = {"cc@example.org"};
  meta.subject = "Hello";

  EMailComposeState compose;
  compose.bcc = {"blind@example.org"};

  EMailOutgoingMessage out;
  ASSERT_EQ(FreezeOutgoing(meta, compose, "body", {}, {}, out),
            EMailFreezeResult::kOK);

  EXPECT_TRUE(out.envelope_rcpt.contains("blind@example.org"));
  EXPECT_TRUE(out.blind_rcpt.contains("blind@example.org"));

  // The whole point of BCC: it must not be derivable from the bytes anyone
  // receives.
  const auto text = QString::fromUtf8(out.eml).toLower();
  EXPECT_FALSE(text.contains("bcc:"));
  EXPECT_FALSE(text.contains("blind@example.org"));
}

TEST(EMailOutgoingTest, ABlindRecipientWhoIsAlsoVisibleIsNotReportedAsBlind) {
  EMailMetaData meta;
  meta.from = "sender@example.org";
  meta.to = {"both@example.org"};

  EMailComposeState compose;
  compose.bcc = {"both@example.org"};

  EMailOutgoingMessage out;
  ASSERT_EQ(FreezeOutgoing(meta, compose, "body", {}, {}, out),
            EMailFreezeResult::kOK);

  // One copy, and describing them as blind would be wrong -- everyone can see
  // them in To.
  EXPECT_EQ(out.envelope_rcpt.size(), 1);
  EXPECT_TRUE(out.blind_rcpt.isEmpty());
}

TEST(EMailOutgoingTest, ABlindOnlyMessageStillHasAnEnvelope) {
  EMailMetaData meta;
  meta.from = "sender@example.org";

  EMailComposeState compose;
  compose.bcc = {"blind@example.org"};

  EMailOutgoingMessage out;
  ASSERT_EQ(FreezeOutgoing(meta, compose, "body", {}, {}, out),
            EMailFreezeResult::kOK);
  EXPECT_EQ(out.envelope_rcpt.size(), 1);
}

TEST(EMailOutgoingTest, ASenderAndARecipientAreBothRequired) {
  EMailMetaData meta;
  EMailComposeState compose;
  EMailOutgoingMessage out;

  EXPECT_EQ(FreezeOutgoing(meta, compose, "body", {}, {}, out),
            EMailFreezeResult::kNO_SENDER);

  meta.from = "sender@example.org";
  EXPECT_EQ(FreezeOutgoing(meta, compose, "body", {}, {}, out),
            EMailFreezeResult::kNO_RECIPIENT);
}

// ---------------------------------------------------------------------------
// Message-ID
// ---------------------------------------------------------------------------

TEST(EMailOutgoingTest, AFrozenMessageCarriesAStableMessageId) {
  EMailMetaData meta;
  meta.from = "sender@example.org";
  meta.to = {"to@example.org"};

  EMailOutgoingMessage out;
  ASSERT_EQ(FreezeOutgoing(meta, {}, "body", {}, {}, out),
            EMailFreezeResult::kOK);

  ASSERT_FALSE(out.message_id.isEmpty());

  // The identifier we keep has to be the one actually written into the bytes,
  // or a later Sent-folder search looks for something that was never sent.
  EXPECT_EQ(MailExtractMessageId(out.eml), out.message_id);
}

TEST(EMailOutgoingTest, TheMessageIdUsesTheSenderDomainAndNotTheHostname) {
  const auto id = MailGenerateMessageId("Someone <user@example.org>");
  EXPECT_TRUE(id.endsWith("@example.org"));
}

TEST(EMailOutgoingTest, AnAlreadySerializedMessageIsReusedByteForByte) {
  const QByteArray original =
      "From: sender@example.org\r\n"
      "To: to@example.org\r\n"
      "Message-ID: <kept@example.org>\r\n"
      "Subject: Signed\r\n"
      "\r\n"
      "body\r\n";

  EMailMetaData meta;
  meta.from = "sender@example.org";
  meta.to = {"to@example.org"};

  EMailOutgoingMessage out;
  ASSERT_EQ(FreezeOutgoing(meta, {}, "ignored", {}, original, out),
            EMailFreezeResult::kOK);

  // Byte preservation: a PGP/MIME signature covers exactly these octets, so
  // re-serializing would invalidate it.
  EXPECT_EQ(out.eml, original);
  EXPECT_EQ(out.message_id, "kept@example.org");
}

TEST(EMailOutgoingTest, NoMessageIdIsInjectedIntoAnExistingMessage) {
  const QByteArray original =
      "From: sender@example.org\r\n"
      "To: to@example.org\r\n"
      "\r\n"
      "body\r\n";

  EMailMetaData meta;
  meta.from = "sender@example.org";
  meta.to = {"to@example.org"};

  EMailOutgoingMessage out;
  ASSERT_EQ(FreezeOutgoing(meta, {}, "ignored", {}, original, out),
            EMailFreezeResult::kOK);

  // Adding one would change signed bytes. Confirmation is simply unavailable,
  // which is reported rather than worked around.
  EXPECT_EQ(out.eml, original);
  EXPECT_TRUE(out.message_id.isEmpty());
}

TEST(EMailOutgoingTest, MessageIdIsReadFromTheHeaderBlockOnly) {
  const QByteArray message =
      "From: a@example.org\r\n"
      "Message-ID: <real@example.org>\r\n"
      "\r\n"
      "Message-ID: <quoted@example.org>\r\n";

  EXPECT_EQ(MailExtractMessageId(message), "real@example.org");
}

TEST(EMailOutgoingTest, BuildMimeEmlMintsNoIdentityOfItsOwn) {
  EMailMetaData meta;
  meta.from = "sender@example.org";
  meta.to = {"to@example.org"};

  // Called for every draft save and reserialization, so inventing an
  // identifier here would give one message many identities.
  QString eml;
  ASSERT_EQ(BuildMimeEML(meta, "body", {}, eml), 0);
  EXPECT_TRUE(MailExtractMessageId(eml.toUtf8()).isEmpty());
}

// ---------------------------------------------------------------------------
// Sent-folder resolution
// ---------------------------------------------------------------------------

TEST(EMailOutgoingTest, AnExplicitSentFolderOutranksDiscovery) {
  EXPECT_GT(RankSentCandidate("Anything", false, true),
            RankSentCandidate("Sent", true, false));
}

TEST(EMailOutgoingTest, SpecialUseOutranksAName) {
  EXPECT_GT(RankSentCandidate("Archive", true, false),
            RankSentCandidate("Sent", false, false));
}

TEST(EMailOutgoingTest, AnUnrelatedFolderIsNeverACandidate) {
  EXPECT_EQ(RankSentCandidate("Inbox", false, false), 0);
  EXPECT_EQ(RankSentCandidate("Projects/2026", false, false), 0);
}

// ---------------------------------------------------------------------------
// Error classification
// ---------------------------------------------------------------------------

TEST(EMailNetErrorTest, ACancelledOperationIsNotAServerFault) {
  vmime::exceptions::operation_timed_out e;
  const auto error = ClassifyVmimeException(e, MailStage::kFETCH, true);

  EXPECT_EQ(error.category, MailErrorCategory::kCANCELLED);
}

TEST(EMailNetErrorTest, ATimeoutIsReportedAsATimeout) {
  vmime::exceptions::operation_timed_out e;
  const auto error = ClassifyVmimeException(e, MailStage::kFETCH, false);

  EXPECT_EQ(error.category, MailErrorCategory::kTIMEOUT);
}

TEST(EMailNetErrorTest, DnsAndConnectFailuresAreToldApart) {
  vmime::exceptions::connection_error dns("Cannot resolve address.");
  vmime::exceptions::connection_error refused("Error while connecting socket.");

  EXPECT_EQ(ClassifyVmimeException(dns, MailStage::kCONNECT, false).category,
            MailErrorCategory::kDNS);
  EXPECT_EQ(
      ClassifyVmimeException(refused, MailStage::kCONNECT, false).category,
      MailErrorCategory::kCONNECT);
}

TEST(EMailNetErrorTest, CertificateFailuresKeepTheirDistinctCauses) {
  vmime::security::cert::certificateNotTrustedException untrusted;
  vmime::security::cert::certificateExpiredException expired;
  vmime::security::cert::serverIdentityException identity;

  EXPECT_EQ(ClassifyVmimeException(untrusted, MailStage::kTLS, false).category,
            MailErrorCategory::kTLS_UNTRUSTED);
  EXPECT_EQ(ClassifyVmimeException(expired, MailStage::kTLS, false).category,
            MailErrorCategory::kTLS_EXPIRED);
  EXPECT_EQ(ClassifyVmimeException(identity, MailStage::kTLS, false).category,
            MailErrorCategory::kTLS_HOSTNAME);
}

TEST(EMailNetErrorTest, OnlyAnUntrustedChainMayBePinned) {
  vmime::security::cert::certificateNotTrustedException untrusted;
  vmime::security::cert::certificateExpiredException expired;
  vmime::security::cert::serverIdentityException identity;

  // A pin says "trust this one certificate", not "ignore TLS". Offering it for
  // an expired certificate or the wrong hostname would turn it into the
  // latter.
  EXPECT_TRUE(
      ClassifyVmimeException(untrusted, MailStage::kTLS, false).IsPinnable());
  EXPECT_FALSE(
      ClassifyVmimeException(expired, MailStage::kTLS, false).IsPinnable());
  EXPECT_FALSE(
      ClassifyVmimeException(identity, MailStage::kTLS, false).IsPinnable());
}

TEST(EMailNetErrorTest, AuthenticationFailureIsItsOwnCategory) {
  vmime::exceptions::authentication_error e("nope");
  EXPECT_EQ(ClassifyVmimeException(e, MailStage::kAUTH, false).category,
            MailErrorCategory::kAUTH);
}

TEST(EMailNetErrorTest, SmtpRejectionsAreAttributedToTheRightStage) {
  using vmime::net::smtp::SMTPCommandError;
  vmime::net::smtp::SMTPResponse::enhancedStatusCode code;

  SMTPCommandError sender("MAIL FROM:<a@example.org>", "550 no", 550, code);
  SMTPCommandError recipient("RCPT TO:<b@example.org>", "550 no", 550, code);
  SMTPCommandError data("DATA", "552 too big", 552, code);

  EXPECT_EQ(ClassifyVmimeException(sender, MailStage::kSUBMIT_ENVELOPE, false)
                .category,
            MailErrorCategory::kSMTP_SENDER_REJECTED);
  EXPECT_EQ(
      ClassifyVmimeException(recipient, MailStage::kSUBMIT_ENVELOPE, false)
          .category,
      MailErrorCategory::kSMTP_RECIPIENT_REJECTED);
  EXPECT_EQ(
      ClassifyVmimeException(data, MailStage::kSUBMIT_BODY, false).category,
      MailErrorCategory::kSMTP_DATA_REJECTED);
}

TEST(EMailNetErrorTest, ARejectedRecipientIsNamedAndSaysNobodyGotIt) {
  using vmime::net::smtp::SMTPCommandError;
  vmime::net::smtp::SMTPResponse::enhancedStatusCode code;
  SMTPCommandError recipient("RCPT TO:<bad@example.org>", "550 no", 550, code);

  const auto error =
      ClassifyVmimeException(recipient, MailStage::kSUBMIT_ENVELOPE, false);

  EXPECT_TRUE(error.title.contains("bad@example.org"));
  // vmime abandons the whole submission at the first bad recipient, so
  // implying the others were delivered would be wrong.
  EXPECT_TRUE(error.detail.contains("not sent to anyone"));
}

TEST(EMailNetErrorTest, AFourHundredSeriesRejectionIsTransient) {
  using vmime::net::smtp::SMTPCommandError;
  vmime::net::smtp::SMTPResponse::enhancedStatusCode code;

  SMTPCommandError busy("MAIL FROM:<a@example.org>", "451 later", 451, code);
  SMTPCommandError refused("MAIL FROM:<a@example.org>", "550 never", 550, code);

  EXPECT_TRUE(ClassifyVmimeException(busy, MailStage::kSUBMIT_ENVELOPE, false)
                  .transient);
  EXPECT_FALSE(
      ClassifyVmimeException(refused, MailStage::kSUBMIT_ENVELOPE, false)
          .transient);
}

TEST(EMailNetErrorTest, ALostConnectionAfterTheBodyIsAmbiguousNotAFailure) {
  vmime::exceptions::socket_exception e;

  // Before the body is on the wire, nothing was delivered and saying so is
  // safe.
  EXPECT_EQ(ClassifyVmimeException(e, MailStage::kSUBMIT_BODY, false).category,
            MailErrorCategory::kCONNECT);

  // After it, the server may or may not have taken the message. Claiming
  // either outcome would be a guess, and claiming failure invites a resend
  // that delivers twice.
  EXPECT_EQ(ClassifyVmimeException(e, MailStage::kSUBMIT_FINAL, false).category,
            MailErrorCategory::kSMTP_AMBIGUOUS);
}

TEST(EMailNetErrorTest, AnAmbiguousOutcomeIsNotMarkedRetryable) {
  vmime::exceptions::socket_exception e;
  const auto error = ClassifyVmimeException(e, MailStage::kSUBMIT_FINAL, false);

  EXPECT_FALSE(error.transient);
}

TEST(EMailNetErrorTest, ARefusedStartTlsIsAPolicyFailureNotAGenericOne) {
  const auto error = MailTlsRequiredError("mail.example.org");

  EXPECT_EQ(error.category, MailErrorCategory::kTLS_REQUIRED);
  EXPECT_TRUE(error.title.contains("mail.example.org"));
  // The reassurance that matters most after this failure.
  EXPECT_TRUE(error.detail.contains("never sent"));
}

TEST(EMailNetErrorTest, NothingCollapsesIntoAGenericNetworkError) {
  // Every category a user can hit must be distinguishable from every other:
  // that is the whole reason the taxonomy exists.
  vmime::exceptions::connection_error dns("Cannot resolve address.");
  vmime::exceptions::authentication_error auth("no");
  vmime::exceptions::folder_not_found folder;
  vmime::security::cert::certificateNotTrustedException cert;
  vmime::exceptions::socket_exception socket;

  QList<MailErrorCategory> seen{
      ClassifyVmimeException(dns, MailStage::kCONNECT, false).category,
      ClassifyVmimeException(auth, MailStage::kAUTH, false).category,
      ClassifyVmimeException(folder, MailStage::kFOLDER, false).category,
      ClassifyVmimeException(cert, MailStage::kTLS, false).category,
      ClassifyVmimeException(socket, MailStage::kSUBMIT_FINAL, false).category,
  };

  for (const auto category : seen) {
    EXPECT_NE(category, MailErrorCategory::kNONE);
  }
  EXPECT_EQ(QSet<MailErrorCategory>(seen.begin(), seen.end()).size(),
            seen.size());
}

TEST(EMailNetErrorTest, AuthCredentialsNeverReachTheProtocolDetail) {
  using vmime::net::smtp::SMTPCommandError;
  vmime::net::smtp::SMTPResponse::enhancedStatusCode code;

  // vmime puts the literal command line into the exception, and for AUTH that
  // line contains the base64 of the password.
  SMTPCommandError e("AUTH PLAIN AHVzZXIAc2VjcmV0", "535 bad", 535, code);
  const auto error = ClassifyVmimeException(e, MailStage::kAUTH, false);

  EXPECT_FALSE(error.protocol_detail.contains("AHVzZXIAc2VjcmV0"));
}

// ---------------------------------------------------------------------------
// Message size policy
// ---------------------------------------------------------------------------

TEST(EMailAccountModelTest, TheSizeCeilingIsOneSharedPolicyValue) {
  // Applied identically to opening a file and to fetching over IMAP, so the
  // two can never disagree about what is too big.
  EXPECT_GT(kMailMaxMessageSize, 0);
  EXPECT_GE(kMailMaxMessageSize, 32LL * 1024 * 1024);
}
