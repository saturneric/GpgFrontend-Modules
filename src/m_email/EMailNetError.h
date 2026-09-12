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

#include <QMetaType>
#include <QString>

namespace vmime {
class exception;
}

/**
 * @brief What went wrong, precisely enough to tell the user something useful.
 *
 * The point of this enum is that nothing in it means "network error". Every
 * value corresponds to a different thing for the user to do about it, which is
 * the only justification for distinguishing them at all.
 */
enum class MailErrorCategory : uint8_t {
  kNONE = 0,

  kDNS,            ///< the host name does not resolve
  kCONNECT,        ///< reached the network, not the server
  kTLS_HANDSHAKE,  ///< TLS itself failed
  kTLS_UNTRUSTED,  ///< chain does not reach a trusted root (pinnable)
  kTLS_EXPIRED,    ///< certificate expired or not yet valid (never pinnable)
  kTLS_HOSTNAME,   ///< certificate is for a different host (never pinnable)
  kTLS_REQUIRED,   ///< the server would not start TLS and we refuse to go on
  kAUTH,           ///< the server rejected the credentials
  kFOLDER,         ///< a folder could not be listed or opened
  kLISTING,        ///< listing or searching failed
  kFETCH,          ///< a message could not be retrieved
  kTIMEOUT,        ///< the server stopped answering
  kCANCELLED,      ///< the user stopped it; not a failure

  kSMTP_SENDER_REJECTED,     ///< MAIL FROM refused
  kSMTP_RECIPIENT_REJECTED,  ///< RCPT TO refused -- nobody received it
  kSMTP_DATA_REJECTED,       ///< the message itself was refused
  kSMTP_AMBIGUOUS,           ///< sent, then the link died before the reply

  kINTERNAL,  ///< our bug, not theirs
};

/**
 * @brief One failure, carrying enough context to be acted on.
 *
 * Split into a translated explanation for the user and the server's own words
 * for a bug report, because collapsing the two loses whichever the reader
 * needed.
 */
struct MailError {
  MailErrorCategory category{MailErrorCategory::kNONE};

  /// Short, translated, addressed to the user.
  QString title;
  /// Translated, says what to do about it where there is anything to do.
  QString detail;
  /// The server's or library's own words. Untranslated, safe to paste into a
  /// bug report, and never contains a credential.
  QString protocol_detail;

  /// Protocol status where there is one: an SMTP reply code. 0 when there is
  /// none.
  int status_code{0};

  /// Whether trying again unchanged could plausibly work. A 4xx is transient;
  /// a wrong password is not.
  bool transient{false};

  [[nodiscard]] auto IsError() const -> bool {
    return category != MailErrorCategory::kNONE;
  }

  /// Whether offering to pin a certificate is a legitimate response to this.
  /// True only for an untrusted or self-signed chain -- never for expiry and
  /// never for a hostname mismatch, because a pin is not a way to ignore TLS.
  [[nodiscard]] auto IsPinnable() const -> bool {
    return category == MailErrorCategory::kTLS_UNTRUSTED;
  }
};

Q_DECLARE_METATYPE(MailError)

/**
 * @brief Which operation was running when it failed.
 *
 * The same exception means different things at different points -- a socket
 * error while connecting is not the same event as one after a message body has
 * been written -- so the stage is an input to classification, not decoration.
 */
enum class MailStage : uint8_t {
  kCONNECT = 0,
  kTLS,
  kAUTH,
  kFOLDER,
  kLISTING,
  kFETCH,
  kSUBMIT_ENVELOPE,
  kSUBMIT_BODY,   ///< body not yet fully written: a failure here is safe
  kSUBMIT_FINAL,  ///< body fully written: a failure here is AMBIGUOUS
};

/**
 * @brief Turn a vmime exception into something the user can act on.
 *
 * Pure: no socket, no widgets, no SDK. Every branch is reachable by
 * constructing the corresponding vmime exception, which is what makes the
 * whole taxonomy testable without a server.
 *
 * @param e the exception, already caught
 * @param stage what was being attempted
 * @param cancelled whether the user asked to stop, which makes an otherwise
 *                  indistinguishable timeout a cancellation instead
 * @return the classified error
 */
auto ClassifyVmimeException(const vmime::exception& e, MailStage stage,
                            bool cancelled) -> MailError;

/**
 * @brief The error for a server that refused to start TLS.
 *
 * Its own constructor because it is not a vmime failure at all: vmime is
 * perfectly willing to continue in the clear, and this is us declining.
 */
auto MailTlsRequiredError(const QString& host) -> MailError;

/// An error for something that is our fault rather than the server's.
auto MailInternalError(const QString& what) -> MailError;
