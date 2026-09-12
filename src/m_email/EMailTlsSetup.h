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
 * @brief Apply this application's security policy to a session and service.
 *
 * Sets the transport properties that make TLS mandatory, installs the
 * certificate verifier, and applies any certificate the user has pinned for
 * this transport.
 *
 * @param session the vmime session whose properties are being set
 * @param service the store or transport to attach the verifier to
 * @param prefix property prefix, e.g. "store.imaps" or "transport.smtp"
 * @param config the transport being connected
 */
void Apply(const vmime::shared_ptr<vmime::net::session>& session,
           const vmime::shared_ptr<vmime::net::service>& service,
           const QString& prefix, const MailTransportConfig& config);

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

/**
 * @brief The SHA-256 fingerprint of the certificate a server presented.
 *
 * Only meaningful after a verification failure, and only used to offer a pin.
 *
 * @return lowercase hex fingerprint, or empty when none was captured
 */
auto LastSeenFingerprint() -> QString;

/**
 * @brief A human description of the certificate a server presented.
 *
 * Shown before the user is asked to trust it, because a fingerprint alone is
 * not something anyone can make a decision about.
 */
auto LastSeenCertificateSummary() -> QString;

/// Forget the captured certificate. Called before each connection attempt so a
/// stale one can never be offered for the wrong server.
void ClearLastSeen();

}  // namespace EMailTlsSetup
