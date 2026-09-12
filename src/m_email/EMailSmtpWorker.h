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

#include <QDateTime>
#include <QMetaType>
#include <QObject>
#include <QString>

#include "EMailAccountModel.h"
#include "EMailCancelToken.h"
#include "EMailNetError.h"
#include "EMailOutgoing.h"

/**
 * @brief What is actually known after a submission attempt.
 *
 * The four states the UI must keep apart. In particular, "the outgoing server
 * accepted this" is not "this was delivered" and is certainly not "this was
 * read", and there is no field here that could be mistaken for either.
 */
struct EMailSendReceipt {
  /// The outgoing server took responsibility. Nothing more than that.
  bool accepted{false};

  /// The connection failed after the message body was fully written, so
  /// whether the server took it is genuinely unknown. Never retried
  /// automatically: doing so is how duplicates happen.
  bool ambiguous{false};

  QString message_id;
  QDateTime submitted_at;
  QString host;
  /// Human description of the security actually used, e.g. "TLS" or
  /// "STARTTLS" -- not what was requested.
  QString security;

  int reply_code{0};
  QString reply_text;

  MailError error;
};
Q_DECLARE_METATYPE(EMailSendReceipt)

/**
 * @brief Submits one message and forgets everything.
 *
 * There is no queue, no Outbox and no retry loop, by design: this is an output
 * adapter on a single message, not the start of a mail-sending subsystem.
 */
class EMailSmtpWorker : public QObject {
  Q_OBJECT

 public:
  explicit EMailSmtpWorker(QObject* parent = nullptr);
  ~EMailSmtpWorker() override;

 public slots:
  /**
   * @brief Connect, authenticate and submit. @p password is wiped by the call.
   *
   * Always emits SignalFinished exactly once, including on failure -- a send
   * that vanished silently would be the worst possible outcome here.
   */
  void Submit(quint64 seq, const MailAccountConfig& account, QString password,
              const EMailOutgoingMessage& message);

  /**
   * @brief Connect and authenticate, then hang up without sending anything.
   *
   * A separate entry point rather than a submission with an empty message:
   * Submit() rejects an incomplete message before it opens a socket, so
   * reusing it would report success or failure without ever having reached
   * the server.
   *
   * Emits SignalFinished with an empty receipt whose error says what happened.
   */
  void TestConnection(quint64 seq, const MailAccountConfig& account,
                      QString password);

 signals:
  void SignalFinished(quint64 seq, const EMailSendReceipt& receipt);

 public:
  [[nodiscard]] auto Token() const -> EMailCancelTokenPtr { return token_; }

 private:
  EMailCancelTokenPtr token_;
};
