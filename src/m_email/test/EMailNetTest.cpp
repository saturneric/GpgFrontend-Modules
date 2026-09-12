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

/**
 * @file Transport tests that need a server to talk to.
 *
 * The rest of the e-mail tests are pure: they reason about bytes and policy
 * with no socket in sight. These cannot be, because what they check is what
 * the application PUTS ON THE WIRE -- that folders are opened read-only, that
 * bodies are peeked rather than read, and that a wrong password is actually
 * refused. None of that is observable from a return value, which is precisely
 * how sending mail unauthenticated went unnoticed for as long as it did.
 *
 * Both fake servers run on loopback. For IMAP that means cleartext, which the
 * policy permits for loopback and nowhere else. SMTP has to be encrypted --
 * the client refuses to authenticate over anything else, which is the whole
 * point -- so that server serves implicit TLS with a self-signed certificate
 * the test trusts by pinning its fingerprint.
 */

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QThread>
#include <atomic>
#include <chrono>
#include <thread>

#include "EMailFakeImapServer.h"
#include "EMailFakeSmtpServer.h"
#include "EMailImapWorker.h"
#include "EMailSmtpWorker.h"

namespace {

/// Runs one server on its own thread for the duration of a scope.
///
/// The worker blocks its own thread inside vmime's socket calls, so a server
/// sharing that thread would deadlock waiting for a client that is waiting
/// for it.
template <typename Server>
class ServerThread {
 public:
  explicit ServerThread(Server* server) : server_(server) {
    server_->moveToThread(&thread_);
    thread_.start();
  }

  ~ServerThread() {
    thread_.quit();
    thread_.wait(5000);
  }

 private:
  Server* server_;
  QThread thread_;
};

auto ImapAccount(quint16 port) -> MailAccountConfig {
  MailAccountConfig account;
  account.id = "test";
  account.address = "me@example.org";
  account.imap.enabled = true;
  account.imap.host = "127.0.0.1";
  account.imap.port = port;
  account.imap.tls = MailTlsMode::kNONE;
  account.imap.username = "user";
  return account;
}

auto SentAuthCommand(const QStringList& commands) -> bool {
  for (const auto& command : commands) {
    if (command.startsWith("AUTH", Qt::CaseInsensitive)) return true;
  }
  return false;
}

/// One SMTP connection test against a server configured for the occasion.
struct SmtpAttempt {
  bool accepted{};
  MailError error;
  QStringList commands;
  QString seen_username;
  QString seen_password;
};

auto TrySmtp(const QStringList& mechanisms, const QString& password,
             bool secure = true) -> SmtpAttempt {
  FakeSmtpServer server;
  server.mechanisms = mechanisms;
  server.use_tls = secure;
  if (!server.listen(QHostAddress::LocalHost, 0)) return {};

  ServerThread<FakeSmtpServer> running(&server);

  MailAccountConfig account;
  account.id = "test";
  account.address = "me@example.org";
  account.smtp.enabled = true;
  account.smtp.host = "127.0.0.1";
  account.smtp.port = server.serverPort();
  account.smtp.tls = secure ? MailTlsMode::kIMPLICIT : MailTlsMode::kNONE;
  account.smtp.username = "user";
  // Self-signed, which is exactly the case a pin exists for.
  account.smtp.pinned_cert_sha256 = FakeSmtpServer::Fingerprint();

  EMailSmtpWorker worker;
  SmtpAttempt attempt;
  QObject::connect(&worker, &EMailSmtpWorker::SignalFinished, &worker,
                   [&attempt](quint64, const EMailSendReceipt& receipt) {
                     attempt.accepted = receipt.accepted;
                     attempt.error = receipt.error;
                   });

  worker.TestConnection(1, account, password);

  attempt.commands = server.Commands();
  attempt.seen_username = server.seen_username;
  attempt.seen_password = server.seen_password;
  return attempt;
}


/// The outcome of a real submission against the fake server.
struct SubmitAttempt {
  bool finished{false};
  EMailSendReceipt receipt;
  QStringList commands;
  bool body_complete{false};
};

/// Submits a small message and returns what the worker reported.
///
/// @param after_body what the server does at the one moment that matters --
///        once the body is fully received and the client is waiting for 250
/// @param cancel_at_body cancel the operation as soon as the body is on the
///        wire, i.e. exactly when the outcome stops being knowable
auto Submit(FakeSmtpServer::AfterBody after_body, bool cancel_at_body = false,
            bool cancel_immediately = false) -> SubmitAttempt {
  FakeSmtpServer server;
  server.after_body = after_body;
  if (!server.listen(QHostAddress::LocalHost, 0)) return {};

  ServerThread<FakeSmtpServer> running(&server);

  MailAccountConfig account;
  account.id = "test";
  account.address = "me@example.org";
  account.smtp.enabled = true;
  account.smtp.host = "127.0.0.1";
  account.smtp.port = server.serverPort();
  account.smtp.tls = MailTlsMode::kIMPLICIT;
  account.smtp.username = "user";
  account.smtp.pinned_cert_sha256 = FakeSmtpServer::Fingerprint();

  EMailOutgoingMessage message;
  message.envelope_from = "me@example.org";
  message.envelope_rcpt = QStringList{"you@example.org"};
  message.message_id = "test-message-id@example.org";
  message.eml =
      "From: me@example.org\r\n"
      "To: you@example.org\r\n"
      "Subject: submission\r\n"
      "Message-ID: <test-message-id@example.org>\r\n"
      "\r\n"
      "body\r\n";

  EMailSmtpWorker worker;
  SubmitAttempt attempt;
  QObject::connect(&worker, &EMailSmtpWorker::SignalFinished, &worker,
                   [&attempt](quint64, const EMailSendReceipt& receipt) {
                     attempt.finished = true;
                     attempt.receipt = receipt;
                   });

  // Aimed at the very operation about to be submitted, and issued BEFORE it
  // starts -- the case a token that cleared itself at slot entry used to lose.
  if (cancel_immediately) worker.Token()->Cancel(1);

  // Watches for the body to land and cancels at that instant. A thread rather
  // than a timer because Submit() blocks this one inside vmime.
  std::thread canceller;
  std::atomic<bool> stop_watching{false};
  if (cancel_at_body) {
    canceller = std::thread([&server, &worker, &stop_watching]() {
      while (!stop_watching.load()) {
        if (server.BodyComplete()) {
          worker.Token()->Cancel(1);
          return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    });
  }

  worker.Submit(1, account, "correct-horse", message);

  stop_watching.store(true);
  if (canceller.joinable()) canceller.join();

  attempt.commands = server.Commands();
  attempt.body_complete = server.BodyComplete();
  return attempt;
}

}  // namespace

// ---------------------------------------------------------------------------
// SMTP authentication
// ---------------------------------------------------------------------------

TEST(EMailSmtpNetTest, AWrongPasswordIsRefused) {
  // The regression this file exists for. vmime treats SMTP authentication as
  // optional and defaults it off, so connect() used to return success without
  // sending any AUTH command at all -- and every password, including no
  // password, was reported as a working account.
  const auto attempt = TrySmtp({"PLAIN", "LOGIN"}, "wrong-password");

  ASSERT_FALSE(attempt.commands.isEmpty()) << "the server was never reached";
  EXPECT_TRUE(SentAuthCommand(attempt.commands))
      << "no AUTH command was sent, so nothing was actually verified";
  EXPECT_FALSE(attempt.accepted);
  EXPECT_EQ(attempt.error.category, MailErrorCategory::kAUTH);
}

TEST(EMailSmtpNetTest, TheRightPasswordIsAccepted) {
  const auto attempt = TrySmtp({"PLAIN", "LOGIN"}, "correct-horse");

  EXPECT_TRUE(attempt.accepted);
  EXPECT_EQ(attempt.seen_username, "user");
  EXPECT_EQ(attempt.seen_password, "correct-horse");
}

TEST(EMailSmtpNetTest, APlainOnlyServerAuthenticates) {
  EXPECT_TRUE(TrySmtp({"PLAIN"}, "correct-horse").accepted);
}

TEST(EMailSmtpNetTest, ALoginOnlyServerAuthenticates) {
  // Without SASL, vmime implements only AUTH PLAIN; LOGIN-only servers are
  // common enough that our fork adds the fallback this covers.
  const auto attempt = TrySmtp({"LOGIN"}, "correct-horse");

  EXPECT_TRUE(attempt.accepted);
  EXPECT_EQ(attempt.seen_password, "correct-horse");
}

TEST(EMailSmtpNetTest, NoCredentialIsSentOverACleartextLink) {
  // Loopback may be spoken to in the clear, but that is not licence to hand a
  // password to it. Encoding is not encryption.
  const auto attempt = TrySmtp({"PLAIN", "LOGIN"}, "correct-horse", false);

  ASSERT_FALSE(attempt.commands.isEmpty());
  EXPECT_TRUE(attempt.seen_password.isEmpty());
  EXPECT_TRUE(attempt.seen_username.isEmpty());
}

// ---------------------------------------------------------------------------
// SMTP submission: what may be claimed about a message, and when
//
// There is exactly one moment that matters. Before the body is fully written
// nothing was delivered and saying so is safe. After it, the server may
// already have queued the message, and NOTHING -- not a timeout, not a dropped
// socket, not the user pressing Stop -- can tell the difference. Reporting any
// of those as a clean failure is what produces a duplicate, because the user
// sends again.
// ---------------------------------------------------------------------------

TEST(EMailSmtpNetTest, AnAcceptedSubmissionIsReportedAsAccepted) {
  const auto attempt = Submit(FakeSmtpServer::AfterBody::kAccept);

  ASSERT_TRUE(attempt.finished);
  EXPECT_TRUE(attempt.receipt.accepted);
  EXPECT_FALSE(attempt.receipt.ambiguous)
      << "a clean 250 is not an ambiguous outcome";
  EXPECT_TRUE(attempt.body_complete);
}

TEST(EMailSmtpNetTest, ADroppedConnectionAfterTheBodyIsAmbiguous) {
  const auto attempt = Submit(FakeSmtpServer::AfterBody::kDropConnection);

  ASSERT_TRUE(attempt.finished);
  ASSERT_TRUE(attempt.body_complete) << "the body never reached the server";
  EXPECT_FALSE(attempt.receipt.accepted);
  EXPECT_TRUE(attempt.receipt.ambiguous)
      << "the message may have been queued; claiming failure invites a "
         "duplicate";
  EXPECT_EQ(attempt.receipt.error.category, MailErrorCategory::kSMTP_AMBIGUOUS);
}

TEST(EMailSmtpNetTest, ACancelAfterTheBodyIsAmbiguousNotACleanStop) {
  // The case the classifier used to get wrong: cancellation was checked before
  // the stage was, so stopping a submission whose body was already on the wire
  // was reported as "Stopped" -- a clean not-sent -- and the user resent it.
  const auto attempt =
      Submit(FakeSmtpServer::AfterBody::kStall, /*cancel_at_body=*/true);

  ASSERT_TRUE(attempt.finished);
  ASSERT_TRUE(attempt.body_complete);
  EXPECT_FALSE(attempt.receipt.accepted);
  EXPECT_TRUE(attempt.receipt.ambiguous)
      << "a user cancel after DATA was written was reported as a clean stop";
  EXPECT_NE(attempt.receipt.error.category, MailErrorCategory::kCANCELLED);
}

TEST(EMailSmtpNetTest, AStalledServerAfterTheBodyIsAmbiguous) {
  // Same instant, reached by a hung server rather than by the user. Cancelled
  // here only to keep the test bounded -- the 60 s timeout would say the same.
  const auto attempt =
      Submit(FakeSmtpServer::AfterBody::kStall, /*cancel_at_body=*/true);

  ASSERT_TRUE(attempt.finished);
  EXPECT_EQ(attempt.receipt.error.category, MailErrorCategory::kSMTP_AMBIGUOUS);
}

TEST(EMailSmtpNetTest, ACancelBeforeAnythingIsSentIsACleanStop) {
  // The other side of the rule. Nothing was written, so there is nothing to be
  // ambiguous about, and the user is told plainly that it stopped.
  const auto attempt = Submit(FakeSmtpServer::AfterBody::kAccept,
                              /*cancel_at_body=*/false,
                              /*cancel_immediately=*/true);

  ASSERT_TRUE(attempt.finished);
  EXPECT_FALSE(attempt.receipt.accepted);
  EXPECT_FALSE(attempt.receipt.ambiguous)
      << "nothing was on the wire, so the outcome is not in doubt";
  EXPECT_FALSE(attempt.body_complete);
}

TEST(EMailSmtpNetTest, AnAmbiguousOutcomeIsNeverMarkedRetryable) {
  // Whatever else happens, nothing may invite an automatic resend after the
  // body is on the wire.
  for (const auto after :
       {FakeSmtpServer::AfterBody::kDropConnection,
        FakeSmtpServer::AfterBody::kStall}) {
    const auto attempt =
        Submit(after, /*cancel_at_body=*/after ==
                          FakeSmtpServer::AfterBody::kStall);
    ASSERT_TRUE(attempt.finished);
    if (attempt.receipt.ambiguous) {
      EXPECT_FALSE(attempt.receipt.error.transient)
          << "an ambiguous outcome was marked retryable";
    }
  }
}

// ---------------------------------------------------------------------------
// IMAP: the invariants, asserted on the wire rather than on the API
// ---------------------------------------------------------------------------

class EMailImapNetTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(server_.listen(QHostAddress::LocalHost, 0));
    running_ = std::make_unique<ServerThread<FakeImapServer>>(&server_);

    QObject::connect(
        &worker_, &EMailImapWorker::SignalFailed, &worker_,
        [this](quint64, const MailError& error) { last_error_ = error; });
    QObject::connect(&worker_, &EMailImapWorker::SignalConnected, &worker_,
                     [this](quint64) { connected_ = true; });
    QObject::connect(&worker_, &EMailImapWorker::SignalFolders, &worker_,
                     [this](quint64, const QList<EMailFolderInfo>& folders) {
                       folders_ = folders;
                     });
    QObject::connect(
        &worker_, &EMailImapWorker::SignalMessages, &worker_,
        [this](quint64, const EMailMessagePage& page) { page_ = page; });

    worker_.Connect(1, ImapAccount(server_.serverPort()), "password");
    ASSERT_TRUE(connected_) << last_error_.title.toStdString();
  }

  void TearDown() override {
    worker_.Disconnect();
    running_.reset();
  }

  FakeImapServer server_;
  std::unique_ptr<ServerThread<FakeImapServer>> running_;
  EMailImapWorker worker_;
  MailError last_error_;
  bool connected_{false};
  QList<EMailFolderInfo> folders_;
  EMailMessagePage page_;
};

// A cancellation applies to the operation it was aimed at, not to the session.
// Pressing Stop used to leave the token set forever -- the only Reset() in the
// module was in Connect() -- so every later request failed as kCANCELLED and
// the browser silently went dead until the account was switched.
TEST_F(EMailImapNetTest, ACancelledOperationDoesNotPoisonTheSession) {
  // Aimed at request 2 specifically, and issued before it runs.
  worker_.Token()->Cancel(2);
  worker_.ListMessages(2, "INBOX", 0, 50, 0);
  EXPECT_EQ(last_error_.category, MailErrorCategory::kCANCELLED);

  page_ = {};
  last_error_ = {};
  worker_.ListMessages(3, "INBOX", 0, 50, 0);

  EXPECT_NE(last_error_.category, MailErrorCategory::kCANCELLED);
  EXPECT_FALSE(page_.rows.isEmpty());
}

TEST_F(EMailImapNetTest, AStopBeforeARequestStartsStillStopsIt) {
  // The request is cancelled while it is still queued -- nothing has begun.
  // The old token cleared itself at slot entry, so this stop was silently
  // discarded and the request ran to its full timeout.
  worker_.Token()->Cancel(5);
  worker_.ListMessages(5, "INBOX", 0, 50, 0);

  EXPECT_EQ(last_error_.category, MailErrorCategory::kCANCELLED);
  EXPECT_TRUE(page_.rows.isEmpty()) << "a stopped request still fetched rows";
}

TEST_F(EMailImapNetTest, AStopDoesNotReachPastTheRequestItNamed) {
  worker_.Token()->Cancel(5);

  // A LATER request is untouched, even though the stop has never been cleared
  // by anything.
  worker_.ListMessages(6, "INBOX", 0, 50, 0);

  EXPECT_NE(last_error_.category, MailErrorCategory::kCANCELLED);
  EXPECT_FALSE(page_.rows.isEmpty());
}

TEST_F(EMailImapNetTest, AStopWhileIdleDoesNotCostTheNextRequest) {
  // Nothing is running, so this names an operation that has already finished.
  worker_.Token()->Cancel(1);

  worker_.ListMessages(2, "INBOX", 0, 50, 0);

  EXPECT_NE(last_error_.category, MailErrorCategory::kCANCELLED)
      << "a stop pressed while idle was charged to the next request";
  EXPECT_FALSE(page_.rows.isEmpty());
}

TEST_F(EMailImapNetTest, FoldersAreOpenedReadOnly) {
  worker_.ListFolders(2);
  worker_.ListMessages(3, "INBOX", 0, 50, 0);

  bool examined = false;
  bool selected = false;
  for (const auto& command : server_.Commands()) {
    const auto verb = command.section(' ', 1, 1).toUpper();
    if (verb == "EXAMINE") examined = true;
    if (verb == "SELECT") selected = true;
  }

  EXPECT_TRUE(examined);
  // Read-only is an invariant, not a setting: SELECT would make writing a
  // flag possible, and possible is enough to eventually happen.
  EXPECT_FALSE(selected);
}

TEST_F(EMailImapNetTest, NoFlagIsEverWritten) {
  worker_.ListFolders(2);
  worker_.ListMessages(3, "INBOX", 0, 50, 0);
  worker_.FetchMessage(4, "INBOX", 1);

  for (const auto& command : server_.Commands()) {
    EXPECT_NE(command.section(' ', 1, 1).toUpper(), "STORE");
  }
}

TEST_F(EMailImapNetTest, TheBodyFetchPeeksSoNothingIsMarkedRead) {
  worker_.ListFolders(2);
  worker_.FetchMessage(3, "INBOX", 1);

  bool peeked = false;
  bool read_without_peeking = false;
  for (const auto& command : server_.Commands()) {
    if (command.contains("BODY.PEEK[]", Qt::CaseInsensitive)) peeked = true;
    if (command.contains("BODY[]", Qt::CaseInsensitive)) {
      read_without_peeking = true;
    }
  }

  EXPECT_TRUE(peeked);
  EXPECT_FALSE(read_without_peeking);
}

TEST_F(EMailImapNetTest, FetchedBytesAreTheMessageItself) {
  QByteArray fetched;
  QObject::connect(
      &worker_, &EMailImapWorker::SignalMessageFetched, &worker_,
      [&fetched](quint64, const QByteArray& raw) { fetched = raw; });

  worker_.ListFolders(2);
  worker_.FetchMessage(3, "INBOX", 1);

  // Byte preservation is what lets an imported PGP/MIME signature still
  // verify, so this compares octets rather than parsed fields.
  EXPECT_EQ(fetched, server_.message_body);
}

TEST_F(EMailImapNetTest, AListedFolderCanStillBeOpened) {
  // vmime refuses to open a folder while ANY other live folder object shares
  // its path, so holding the result of a recursive LIST once made every
  // listed folder unopenable.
  worker_.ListFolders(2);

  last_error_ = {};
  worker_.ListMessages(3, "INBOX", 0, 50, 0);
  EXPECT_NE(last_error_.category, MailErrorCategory::kFOLDER);
  EXPECT_FALSE(last_error_.IsError());

  last_error_ = {};
  worker_.ListMessages(4, "Sent Mail", 0, 50, 0);
  EXPECT_FALSE(last_error_.IsError());

  last_error_ = {};
  worker_.ListMessages(5, "INBOX", 0, 50, 0);
  EXPECT_FALSE(last_error_.IsError()) << "switching back to a folder failed";
}

TEST_F(EMailImapNetTest, DownloadingAMessageReportsProgress) {
  // A download is the one operation whose size is known in advance, so it is
  // the one that can show a real fraction rather than a marquee. Without a
  // progress channel the window can only say "Downloading..." and hope.
  QList<QPair<qint64, qint64>> reports;
  QObject::connect(&worker_, &EMailImapWorker::SignalFetchProgress, &worker_,
                   [&reports](quint64, qint64 current, qint64 total) {
                     reports.append({current, total});
                   });

  worker_.ListFolders(2);
  worker_.FetchMessage(3, "INBOX", 1);

  ASSERT_FALSE(reports.isEmpty()) << "the download reported no progress";

  // It must end having accounted for the whole message, and never claim to
  // have moved more bytes than there are.
  const auto last = reports.last();
  EXPECT_GT(last.second, 0);
  EXPECT_LE(last.first, last.second);
  EXPECT_EQ(last.first, static_cast<qint64>(server_.message_body.size()));
}

TEST_F(EMailImapNetTest, ProgressDoesNotFloodTheGuiThread) {
  // Every progress report is a queued event delivered to the window. A large
  // message must not turn one download into tens of thousands of them: the
  // GUI would spend its time draining the queue instead of drawing, which
  // looks exactly like a hang that never recovers.
  QByteArray big =
      "From: someone@example.org\r\nSubject: Big\r\n"
      "Content-Type: text/html\r\n\r\n<html><body>";
  big += QByteArray(600 * 1024, 'x');
  big += "</body></html>\r\n";
  server_.message_body = big;

  int reports = 0;
  QObject::connect(&worker_, &EMailImapWorker::SignalFetchProgress, &worker_,
                   [&reports](quint64, qint64, qint64) { ++reports; });

  worker_.ListFolders(2);
  worker_.FetchMessage(3, "INBOX", 1);

  EXPECT_GT(reports, 0) << "a 600 KiB download reported nothing";
  EXPECT_LT(reports, 300)
      << "a single download produced " << reports
      << " progress events; that floods the window's event queue";
}

TEST_F(EMailImapNetTest, SubjectsAndSendersAreDecodedForDisplay) {
  // A listing shows what the server put in the ENVELOPE, and real mail puts
  // RFC 2047 encoded-words there. Re-emitting the field in its wire form gives
  // the user "=?utf-8?Q?Caf=C3=A9?=" where the subject should be.
  EMailMessagePage page;
  QObject::connect(&worker_, &EMailImapWorker::SignalMessages, &worker_,
                   [&page](quint64, const EMailMessagePage& p) { page = p; });

  worker_.ListFolders(2);
  worker_.ListMessages(3, "INBOX", 0, 50, 0);

  ASSERT_FALSE(page.rows.isEmpty()) << last_error_.title.toStdString();
  EXPECT_EQ(page.rows.first().subject, QString::fromUtf8("Café résumé"));
  EXPECT_FALSE(page.rows.first().subject.contains("=?"));

  // The display name in an address is encoded the same way and needs the same
  // treatment, while the address itself must stay exactly as it arrived.
  EXPECT_TRUE(page.rows.first().from.contains(QString::fromUtf8("Björn")));
  EXPECT_TRUE(page.rows.first().from.contains("someone@example.org"));
}

TEST_F(EMailImapNetTest, TheSentFolderComesFromSpecialUse) {
  worker_.ListFolders(2);

  bool resolved = false;
  QString folder;
  QObject::connect(&worker_, &EMailImapWorker::SignalSentLookup, &worker_,
                   [&](quint64, bool r, bool, const QString& f) {
                     resolved = r;
                     folder = f;
                   });

  worker_.FindInSentFolder(3, "abc123@example.org");

  EXPECT_TRUE(resolved);
  // Resolved from the \Sent attribute, never guessed from the name.
  EXPECT_EQ(folder, "Sent Mail");
}

/// The bytes a send would hand to SaveToSentFolder.
auto SentCopyEml() -> QByteArray {
  return "From: me@example.org\r\n"
         "To: someone@example.org\r\n"
         "Subject: Filed\r\n"
         "Message-ID: <filed-1@example.org>\r\n"
         "\r\n"
         "body text\r\n";
}

TEST_F(EMailImapNetTest, ASentMessageIsAppendedToTheSentFolder) {
  worker_.ListFolders(2);
  // Nothing is filed yet, which is the normal case: most servers do not keep
  // a copy of what they send for you.
  server_.search_finds = false;

  MailSentSaveOutcome outcome{};
  QString folder;
  QObject::connect(
      &worker_, &EMailImapWorker::SignalSentSaved, &worker_,
      [&](quint64, MailSentSaveOutcome o, const QString& f, const MailError&) {
        outcome = o;
        folder = f;
      });

  worker_.SaveToSentFolder(3, "filed-1@example.org", SentCopyEml());

  EXPECT_EQ(outcome, MailSentSaveOutcome::kSAVED);
  EXPECT_EQ(folder, "Sent Mail");
  EXPECT_TRUE(server_.appended);
  EXPECT_EQ(server_.appended_mailbox, "Sent Mail");

  // Byte for byte. These octets may be exactly what a signature covers, so a
  // copy that has been parsed and re-rendered is a copy that no longer
  // verifies.
  EXPECT_EQ(server_.appended_data, SentCopyEml());
}

TEST_F(EMailImapNetTest, TheFiledCopyIsMarkedSeen) {
  worker_.ListFolders(2);
  server_.search_finds = false;

  worker_.SaveToSentFolder(3, "filed-1@example.org", SentCopyEml());

  // A message the user just sent has been read by definition. Leaving it
  // unread puts an unread badge on a folder nobody reads.
  EXPECT_TRUE(server_.appended_flags.contains("\\Seen"));
}

TEST_F(EMailImapNetTest, AnAlreadyFiledMessageIsNotAppendedTwice) {
  worker_.ListFolders(2);
  // A server that files its own copy -- Gmail does. Appending here would leave
  // the user two copies to tidy up by hand.
  server_.search_finds = true;

  MailSentSaveOutcome outcome{};
  QObject::connect(&worker_, &EMailImapWorker::SignalSentSaved, &worker_,
                   [&](quint64, MailSentSaveOutcome o, const QString&,
                       const MailError&) { outcome = o; });

  worker_.SaveToSentFolder(3, "filed-1@example.org", SentCopyEml());

  EXPECT_EQ(outcome, MailSentSaveOutcome::kALREADY_THERE);
  EXPECT_FALSE(server_.appended);
}

TEST_F(EMailImapNetTest, AMessageWithNoIdIsFiledButReportedUnverified) {
  worker_.ListFolders(2);
  server_.search_finds = false;

  MailSentSaveOutcome outcome{};
  QObject::connect(&worker_, &EMailImapWorker::SignalSentSaved, &worker_,
                   [&](quint64, MailSentSaveOutcome o, const QString&,
                       const MailError&) { outcome = o; });

  worker_.SaveToSentFolder(3, {}, SentCopyEml());

  // The copy is written, because that is what the user wanted. It cannot be
  // looked up afterwards, and saying it was confirmed would be a claim we
  // never checked.
  EXPECT_TRUE(server_.appended);
  EXPECT_EQ(outcome, MailSentSaveOutcome::kSAVED_UNVERIFIED);
}

TEST_F(EMailImapNetTest, FilingACopyIsTheOnlyThingOpenedForWriting) {
  worker_.ListFolders(2);
  server_.search_finds = false;
  worker_.SaveToSentFolder(3, "filed-1@example.org", SentCopyEml());

  // Everything else in this class opens folders with EXAMINE, which the server
  // cannot let a client write through. Exactly one SELECT is expected, and it
  // belongs to the append.
  int selects = 0;
  for (const auto& command : server_.Commands()) {
    if (command.section(' ', 1, 1).toUpper() == "SELECT") ++selects;
  }
  EXPECT_EQ(selects, 1);
}

/**
 * @brief A main of our own, because Qt's socket layer needs an application.
 *
 * gtest_main would do, except that QTcpSocket and QThread require a
 * QCoreApplication to exist before either is touched -- without one the first
 * connection never completes and the suite hangs on a timeout rather than
 * failing with a reason.
 */
auto main(int argc, char** argv) -> int {
  QCoreApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

// A search used to fetch its results one UID at a time -- a full server round
// trip per row, fifty of them for a full page -- while the plain listing
// beside it had always fetched its whole range in one go. On anything but a
// local server that is the difference between a search feeling instant and
// feeling broken.
//
// Asserted as "the cost does not grow with the answer" rather than as an exact
// command count: how many round trips a batched fetch takes is vmime's
// business, but it must be the same number for one result as for five.
TEST_F(EMailImapNetTest, ASearchCostsTheSameWhateverItFinds) {
  const auto fetches_for = [this](const QList<int>& uids) {
    server_.ResetCommands();
    server_.search_uids = uids;
    worker_.SearchMessages(2, "INBOX", "report", 50);

    int fetches = 0;
    for (const auto& command : server_.Commands()) {
      auto verb = command.section(' ', 1, 1).toUpper();
      if (verb == "UID") verb = command.section(' ', 2, 2).toUpper();
      if (verb == "FETCH") fetches++;
    }
    return fetches;
  };

  const auto one = fetches_for({1});
  const auto five = fetches_for({1, 2, 3, 4, 5});

  EXPECT_GT(one, 0);
  EXPECT_EQ(one, five) << "round trips must not grow with the result count";
}

// Every match comes back, not just the first.
TEST_F(EMailImapNetTest, ASearchReturnsEveryMatchItFetched) {
  server_.search_uids = {1, 2, 3, 4, 5};

  worker_.SearchMessages(2, "INBOX", "report", 50);

  ASSERT_TRUE(last_error_.title.isEmpty()) << last_error_.title.toStdString();
  EXPECT_EQ(page_.rows.size(), 5);
}

// The order the search asked for is the order the user sees. A server is free
// to answer a batched fetch in whatever order suits it, so the rows are put
// back into the requested order rather than taken as they arrive.
TEST_F(EMailImapNetTest, SearchResultsAreNewestFirst) {
  server_.search_uids = {1, 2, 3, 4, 5};

  worker_.SearchMessages(2, "INBOX", "report", 50);
  ASSERT_EQ(page_.rows.size(), 5);

  QList<quint64> seen;
  seen.reserve(page_.rows.size());
  for (const auto& row : page_.rows) seen.append(row.uid);

  auto sorted = seen;
  std::sort(sorted.begin(), sorted.end(), std::greater<>());
  EXPECT_EQ(seen, sorted);
}
