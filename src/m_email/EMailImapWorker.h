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
#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <QString>

#include "EMailAccountModel.h"
#include "EMailCancelToken.h"
#include "EMailNetError.h"

/**
 * @brief One folder, as far as a picker needs to know.
 */
struct EMailFolderInfo {
  QString path;          ///< server path, the handle for everything else
  QString display_name;  ///< last component, for showing
  bool selectable{true};
  bool is_sent{false};  ///< advertised \Sent via RFC 6154 SPECIAL-USE
  bool is_inbox{false};
};
Q_DECLARE_METATYPE(EMailFolderInfo)

/**
 * @brief One row of the message list.
 *
 * Everything here comes from an envelope fetch. There is deliberately no body,
 * no structure and no attachment information: a picker that needed those would
 * be downloading mail rather than listing it.
 */
struct EMailMessageSummary {
  quint64 uid{0};
  QString message_id;
  QString subject;
  QString from;
  QDateTime date;
  qint64 size{0};

  /// Whether the message is larger than this application will open. Decided
  /// from the listed size, so an oversized message is refused without ever
  /// being downloaded.
  [[nodiscard]] auto TooLarge() const -> bool {
    return size > kMailMaxMessageSize;
  }
};
Q_DECLARE_METATYPE(EMailMessageSummary)

/**
 * @brief A page of message rows, and whether there are more behind it.
 */
struct EMailMessagePage {
  QList<EMailMessageSummary> rows;
  /// Rows still available further back, ignoring the session cap.
  int remaining{0};
  /// True when the session cap stopped the load rather than the mailbox
  /// running out. The picker says so and asks the user to search.
  bool capped{false};
};
Q_DECLARE_METATYPE(EMailMessagePage)

/**
 * @brief What became of the attempt to put a sent message in Sent.
 *
 * Five answers rather than a bool, because "we could not check" and "it is not
 * there" are different facts, and because a copy the SERVER filed and a copy
 * WE filed are both fine but not the same event. None of these says anything
 * about whether the message was sent: that was settled over SMTP before any
 * of this ran, and nothing here may cast doubt on it.
 */
enum class MailSentSaveOutcome : uint8_t {
  kALREADY_THERE,     ///< the server had filed a copy itself; nothing appended
  kSAVED,             ///< appended by us, and found afterwards
  kSAVED_UNVERIFIED,  ///< appended, and we cannot prove it: no Message-ID
  kUNRESOLVED,        ///< no Sent folder could be identified
  kFAILED,            ///< the append itself failed; see the error
};
Q_DECLARE_METATYPE(MailSentSaveOutcome)

/**
 * @brief Owns an IMAP session, on its own thread.
 *
 * The only file besides the SMTP worker that includes vmime's networking. It
 * exists on a dedicated thread and every vmime object it owns is created
 * inside one of its slots, so the whole graph has a single thread's affinity
 * from birth -- which matters because none of it is thread-safe and an
 * IMAP connection is a stateful, strictly sequential thing.
 *
 * Commands in, values out. No vmime type ever crosses a signal.
 *
 * Four behaviours here are invariants rather than options, and must stay that
 * way: folders are opened read-only, fetches use PEEK, no flag is ever
 * written, and nothing about a mailbox is persisted.
 */
class EMailImapWorker : public QObject {
  Q_OBJECT

 public:
  explicit EMailImapWorker(QObject* parent = nullptr);
  ~EMailImapWorker() override;

 public slots:
  /**
   * @brief Open a session. @p password is wiped by this call.
   *
   * Emits SignalConnected or SignalFailed.
   */
  void Connect(quint64 seq, const MailAccountConfig& account, QString password);

  /// List every selectable folder. Emits SignalFolders or SignalFailed.
  void ListFolders(quint64 seq);

  /**
   * @brief Fetch one page of the newest messages in @p folder.
   *
   * @param before_uid page backwards from this UID, or 0 for the newest page
   * @param page_size rows to fetch; clamped
   * @param retained how many rows the picker already holds, for the cap
   */
  void ListMessages(quint64 seq, const QString& folder, quint64 before_uid,
                    int page_size, int retained);

  /**
   * @brief Search @p folder, bounded the same way a listing is.
   *
   * The query is constrained server-side; an over-large match set is truncated
   * and reported as narrowed rather than silently trimmed.
   */
  void SearchMessages(quint64 seq, const QString& folder, const QString& query,
                      int page_size);

  /// Fetch one complete message, byte-exact and without setting \Seen.
  void FetchMessage(quint64 seq, const QString& folder, quint64 uid);

  /**
   * @brief Look for @p message_id in the account's Sent folder.
   *
   * Used only by Send & Confirm. Emits SignalSentLookup.
   */
  void FindInSentFolder(quint64 seq, const QString& message_id);

  /**
   * @brief Put a just-sent message in the account's Sent folder.
   *
   * The ONE operation in this class that writes to a mailbox, and the only
   * reason the read-only invariant below has an exception. It runs after the
   * message has already been accepted over SMTP, so nothing it does or fails
   * to do changes whether the message was sent.
   *
   * Checks before it writes: a server that filed its own copy -- Gmail does,
   * most do not -- must not end up with two. That check needs a Message-ID, so
   * a message carrying none is appended without one and reported as
   * kSAVED_UNVERIFIED rather than silently risking a duplicate quietly.
   *
   * @p eml is appended byte for byte, through the stream overload, so a
   * signature over those octets survives being filed.
   *
   * Emits SignalSentSaved.
   */
  void SaveToSentFolder(quint64 seq, const QString& message_id,
                        const QByteArray& eml);

  /// Close the session and release every vmime object, on this thread.
  void Disconnect();

 signals:
  void SignalConnected(quint64 seq);
  void SignalFolders(quint64 seq, const QList<EMailFolderInfo>& folders);
  void SignalMessages(quint64 seq, const EMailMessagePage& page);
  void SignalMessageFetched(quint64 seq, const QByteArray& raw_eml);

  /**
   * @brief How much of a message body has arrived.
   *
   * Emitted only while fetching one message, which is the one operation here
   * whose length is known in advance: the listing already reported the size.
   * Everything else IMAP does reports completion and nothing in between, so
   * this is deliberately not a general progress channel.
   *
   * @param total may be 0 if the server never said, in which case the caller
   *   should fall back to an indeterminate indicator rather than inventing a
   *   denominator.
   */
  void SignalFetchProgress(quint64 seq, qint64 current, qint64 total);

  /**
   * @brief Result of a Sent-folder lookup.
   *
   * @param found whether a copy carrying the Message-ID was there
   * @param folder the folder that was searched, empty when none was resolved
   * @param resolved whether a Sent folder could be identified at all --
   *                 which is a different answer from "not found"
   */
  void SignalSentLookup(quint64 seq, bool resolved, bool found,
                        const QString& folder);

  /**
   * @brief Result of filing a copy in Sent.
   *
   * @param folder the folder written to, empty when none was resolved
   * @param error meaningful only for MailSentSaveOutcome::kFAILED
   */
  void SignalSentSaved(quint64 seq, MailSentSaveOutcome outcome,
                       const QString& folder, const MailError& error);

  void SignalFailed(quint64 seq, const MailError& error);

 public:
  /// The token that stops whatever is running. Held by the controller, set
  /// from the GUI thread while this worker is blocked.
  [[nodiscard]] auto Token() const -> EMailCancelTokenPtr { return token_; }

 private:
  /**
   * @brief Open the Sent folder so one message can be appended to it.
   *
   * The single, deliberate exception to the read-only rule below, and scoped
   * to exactly that: it is called only by SaveToSentFolder, it is never
   * cached, and the folder it returns is closed as soon as the append is done.
   * Nothing else in this class may use it.
   *
   * Releases the cached read-only folder first -- vmime refuses to open a path
   * that another live folder object already holds.
   */
  auto OpenSentFolderForWrite(const QString& path)
      -> vmime::shared_ptr<vmime::net::folder>;

  /**
   * @brief Open a folder for reading and nothing else.
   *
   * Read-only is not a preference: a folder opened this way cannot have its
   * flags written even by a bug, so the "importing never marks a message read"
   * rule is enforced by the server rather than by our own care.
   *
   * Reuses the currently open folder when the path matches, because reopening
   * costs a round trip per page.
   */
  auto OpenFolderReadOnly(const QString& path)
      -> vmime::shared_ptr<vmime::net::folder>;

  /**
   * @brief Work out which folder holds sent mail.
   *
   * In order: the account's explicit override, then a folder the server
   * advertises as \Sent through RFC 6154 SPECIAL-USE. There is deliberately
   * no third step guessing at the name "Sent" -- a wrong guess would report a
   * copy that is not there, or miss one that is.
   *
   * @return the folder path, or empty when it could not be determined
   */
  auto ResolveSentFolder() -> QString;

  struct Impl;
  Impl* impl_;
  EMailCancelTokenPtr token_;
};
