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

#include <QString>
#include <memory>

// The test target defines this on the command line; the module build does
// not, so it is set here and guarded rather than assumed either way.
#ifndef VMIME_STATIC
#define VMIME_STATIC
#endif
#include <vmime/vmime.hpp>

#include "EMailAccountModel.h"

/**
 * @brief Everything that decides how secure a mail connection is.
 *
 * Collected in one place on purpose. vmime's defaults are wrong for our
 * purposes in two ways that are easy to miss and expensive to get wrong:
 *
 *  - its certificate verifier trusts nothing, because its root list starts
 *    empty, so verification has to be supplied rather than merely left on;
 *  - `connection.tls.required` defaults to false, so a server that refuses
 *    STARTTLS silently yields a cleartext session that then sends the
 *    password.
 *
 * Neither is configurable here, and neither should become configurable.
 */
namespace EMailTlsSetup {

/**
 * @brief The certificate ONE connection was offered.
 *
 * Per connection, deliberately. This used to be a single process-global slot
 * written by every verification in the process, so two connections in flight
 * -- a settings-page probe and a mailbox browser, say -- overwrote each
 * other's. A mutex made that free of data races and no less wrong: the
 * "trust this certificate?" dialog could show one server's fingerprint and
 * then pin it to a different account.
 */
struct SeenCertificate {
  /// Lowercase hex SHA-256, or empty when nothing was captured.
  QString fingerprint;
  /// Validity dates, subject, and whether it names the host asked for.
  QString summary;
};

using SeenCertificatePtr = std::shared_ptr<SeenCertificate>;

/**
 * @brief Apply this application's security policy to a session and service.
 *
 * Sets the transport properties that make TLS mandatory, requires
 * authentication where the protocol treats it as optional, installs the
 * certificate verifier, and applies any certificate the user has pinned for
 * this transport.
 *
 * @param session the vmime session whose properties are being set
 * @param service the store or transport to attach the verifier to
 * @param prefix property prefix, e.g. "store.imaps" or "transport.smtp"
 * @param config the transport being connected
 * @param imap true for IMAP, false for SMTP. Only SMTP treats authentication
 *   as optional, so only SMTP needs it demanded; see the property below.
 * @return the slot this connection's verifier records what it saw in. Keep it
 *   for as long as the connection attempt lasts and read it if the attempt
 *   fails on a certificate.
 */
auto Apply(const vmime::shared_ptr<vmime::net::session>& session,
           const vmime::shared_ptr<vmime::net::service>& service,
           const QString& prefix, const MailTransportConfig& config, bool imap)
    -> SeenCertificatePtr;

/**
 * @brief The vmime protocol name for a transport.
 *
 * @param imap true for IMAP, false for SMTP
 * @param mode the chosen security
 * @return "imaps"/"imap" or "smtps"/"smtp"
 */
auto ProtocolName(bool imap, MailTlsMode mode) -> QString;

/**
 * @brief Whether a connection actually ended up encrypted.
 *
 * Asked before a credential is handed over, so that a policy failure can never
 * become a leaked password. vmime reports a secured connection by the concrete
 * type of its connection infos rather than by a flag.
 *
 * @param service a connected store or transport
 * @return true when the link is TLS-protected
 */
auto IsSecured(const vmime::shared_ptr<vmime::net::service>& service) -> bool;

}  // namespace EMailTlsSetup
