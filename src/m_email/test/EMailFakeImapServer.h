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
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>

/**
 * @brief A cleartext IMAP server that speaks just enough to be browsed.
 *
 * Not a mail server: it answers the handful of commands this application
 * actually sends, and records every one of them so a test can assert on what
 * was asked rather than only on what came back. That is what makes the
 * read-only and PEEK guarantees checkable.
 */
class FakeImapServer : public QTcpServer {
 public:
  explicit FakeImapServer(QObject* parent = nullptr) : QTcpServer(parent) {}

  /// Every tagged command line the client sent, in order. Written on the
  /// server's thread and read from the test's, so it is guarded.
  QStringList Commands() const {
    QMutexLocker locker(&mutex_);
    return commands_;
  }

  /// What SEARCH reports. Real servers answer "nothing matched" for a message
  /// that is not filed yet, which is the case the Sent-copy path turns on:
  /// answering "found" unconditionally would hide the append entirely.
  bool search_finds = true;

  /// Set to true by an APPEND, so a test can tell a copy was actually written
  /// rather than only that the command was sent.
  bool appended = false;

  /// The literal an APPEND carried, byte for byte. The whole point of the
  /// stream overload is that these octets are untouched, so a test has to be
  /// able to compare them.
  QByteArray appended_data;

  /// The flags the APPEND asked for, as sent.
  QString appended_flags;

  /// The mailbox the APPEND named.
  QString appended_mailbox;

  /// Raw message returned for a fetch of the whole body.
  QByteArray message_body =
      "From: someone@example.org\r\n"
      "To: me@example.org\r\n"
      "Subject: Hello\r\n"
      "Message-ID: <abc123@example.org>\r\n"
      "\r\n"
      "body text\r\n";

 protected:
  void incomingConnection(qintptr descriptor) override {
    auto* socket = new QTcpSocket(this);
    socket->setSocketDescriptor(descriptor);
    connect(socket, &QTcpSocket::readyRead, this,
            [this, socket]() { handle(socket); });
    socket->write("* OK [CAPABILITY IMAP4rev1] fake ready\r\n");
    socket->flush();
  }

 private:
  mutable QMutex mutex_;
  QStringList commands_;

  /// Bytes still owed by an APPEND literal, and the tag that is waiting on it.
  qint64 literal_remaining_ = 0;
  QString literal_tag_;

  void handle(QTcpSocket* socket) {
    while (true) {
      // An APPEND literal is binary and may contain anything, newlines
      // included, so it is read by COUNT rather than by line. Until it has all
      // arrived nothing else on this connection can be parsed.
      if (literal_remaining_ > 0) {
        const auto chunk = socket->read(literal_remaining_);
        if (chunk.isEmpty()) return;

        appended_data += chunk;
        literal_remaining_ -= chunk.size();
        if (literal_remaining_ > 0) return;

        // The client sends a bare CRLF after the literal to end the command.
        socket->readLine();

        appended = true;
        // A message that is filed IS findable afterwards, which is what lets
        // the verifying search in SaveToSentFolder mean something.
        search_finds = true;
        socket->write(
            (literal_tag_ + " OK [APPENDUID 1 2] append done\r\n").toUtf8());
        socket->flush();
        continue;
      }

      if (!socket->canReadLine()) return;

      const auto line = QString::fromUtf8(socket->readLine()).trimmed();
      if (line.isEmpty()) continue;
      {
        QMutexLocker locker(&mutex_);
        commands_.append(line);
      }

      const auto tag = line.section(' ', 0, 0);
      auto verb = line.section(' ', 1, 1).toUpper();

      // "UID FETCH" / "UID SEARCH" -- the real verb is the next word.
      if (verb == "UID") verb = line.section(' ', 2, 2).toUpper();

      QByteArray out;

      if (verb == "CAPABILITY") {
        out += "* CAPABILITY IMAP4rev1 SPECIAL-USE\r\n";
        out += (tag + " OK done\r\n").toUtf8();
      } else if (verb == "LOGIN") {
        out += (tag + " OK logged in\r\n").toUtf8();
      } else if (verb == "LIST" || verb == "LSUB") {
        out += "* LIST (\\HasNoChildren) \"/\" \"INBOX\"\r\n";
        out += "* LIST (\\HasNoChildren \\Sent) \"/\" \"Sent Mail\"\r\n";
        out += (tag + " OK done\r\n").toUtf8();
      } else if (verb == "SELECT" || verb == "EXAMINE") {
        out += "* 1 EXISTS\r\n* 0 RECENT\r\n";
        out += "* FLAGS (\\Seen \\Answered)\r\n";
        out += "* OK [UIDVALIDITY 1] ok\r\n";
        out += "* OK [UIDNEXT 2] ok\r\n";
        out += (tag + (verb == "EXAMINE" ? " OK [READ-ONLY] done\r\n"
                                         : " OK [READ-WRITE] done\r\n"))
                   .toUtf8();
      } else if (verb == "APPEND") {
        // tag APPEND "Sent Mail" (\Seen) {123}
        appended_mailbox = line.section('"', 1, 1);
        const auto open_paren = line.indexOf('(');
        const auto close_paren = line.indexOf(')');
        if (open_paren >= 0 && close_paren > open_paren) {
          appended_flags =
              line.mid(open_paren + 1, close_paren - open_paren - 1);
        }

        const auto brace = line.lastIndexOf('{');
        const auto end_brace = line.lastIndexOf('}');
        literal_remaining_ =
            brace >= 0 && end_brace > brace
                ? line.mid(brace + 1, end_brace - brace - 1).toLongLong()
                : 0;
        literal_tag_ = tag;
        appended_data.clear();

        // "+" invites the literal. Nothing is answered for this tag until all
        // of it has arrived.
        socket->write("+ ready for literal\r\n");
        socket->flush();
        continue;
      } else if (verb == "SEARCH") {
        if (search_finds) out += "* SEARCH 1\r\n";
        out += (tag + " OK done\r\n").toUtf8();
      } else if (verb == "FETCH") {
        // A whole-body fetch asks for an empty section, "[]"; anything else is
        // a metadata fetch. Distinguishing on the section rather than on the
        // word BODY matters, because a listing asks for
        // BODY.PEEK[HEADER.FIELDS (...)] and must not be answered with a body.
        const auto whole_body =
            line.contains("BODY[]") || line.contains("BODY.PEEK[]");

        if (whole_body) {
          out += QString("* 1 FETCH (UID 1 BODY[] {%1}\r\n")
                     .arg(message_body.size())
                     .toUtf8();
          out += message_body;
          out += ")\r\n";
        } else {
          const QByteArray header_fields =
              "Message-ID: <abc123@example.org>\r\n\r\n";
          out += "* 1 FETCH (UID 1 RFC822.SIZE 120 FLAGS () ";
          out +=
              // Deliberately an RFC 2047 encoded-word, and a display name
              // that is one too: this is what real mail looks like, and
              // showing it raw is a bug a plain-ASCII fixture cannot catch.
              "ENVELOPE (\"Mon, 1 Jan 2026 00:00:00 +0000\" "
              "\"=?utf-8?Q?Caf=C3=A9_r=C3=A9sum=C3=A9?=\" "
              "((\"=?utf-8?Q?Bj=C3=B6rn?=\" NIL \"someone\" "
              "\"example.org\")) "
              "((\"S\" NIL \"someone\" \"example.org\")) "
              "((\"S\" NIL \"someone\" \"example.org\")) "
              "((\"M\" NIL \"me\" \"example.org\")) NIL NIL NIL "
              "\"<abc123@example.org>\") ";
          out += QString("BODY[HEADER.FIELDS (MESSAGE-ID)] {%1}\r\n")
                     .arg(header_fields.size())
                     .toUtf8();
          out += header_fields;
          out += ")\r\n";
        }
        out += (tag + " OK done\r\n").toUtf8();
      } else if (verb == "LOGOUT") {
        out += "* BYE\r\n";
        out += (tag + " OK done\r\n").toUtf8();
      } else {
        out += (tag + " OK done\r\n").toUtf8();
      }

      socket->write(out);
      socket->flush();
    }
  }
};
