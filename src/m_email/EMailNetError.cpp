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

#include "EMailNetError.h"

#include <QCoreApplication>

// The test target defines this on the command line; the module build does
// not, so it is set here and guarded rather than assumed either way.
#ifndef VMIME_STATIC
#define VMIME_STATIC
#endif
#include <vmime/exception.hpp>
#include <vmime/net/smtp/SMTPExceptions.hpp>
#include <vmime/security/cert/certificateException.hpp>
#include <vmime/security/cert/certificateExpiredException.hpp>
#include <vmime/security/cert/certificateIssuerVerificationException.hpp>
#include <vmime/security/cert/certificateNotTrustedException.hpp>
#include <vmime/security/cert/certificateNotYetValidException.hpp>
#include <vmime/security/cert/serverIdentityException.hpp>
#include <vmime/security/cert/unsupportedCertificateTypeException.hpp>

namespace {

auto Tr(const char* text) -> QString {
  return QCoreApplication::translate("EMailTransport", text);
}

/// Whatever the library said, trimmed of anything that could be a secret.
///
/// vmime puts the failing command into command_error, and for SMTP that text
/// is the literal line we sent -- which for AUTH contains the base64 of the
/// password. Nothing downstream is allowed to see that.
auto SafeProtocolText(const std::string& command, const std::string& response)
    -> QString {
  auto cmd = QString::fromStdString(command).trimmed();
  if (cmd.startsWith("AUTH", Qt::CaseInsensitive)) cmd = "AUTH";

  auto resp = QString::fromStdString(response).trimmed();
  if (cmd.isEmpty()) return resp;
  if (resp.isEmpty()) return cmd;
  return QString("%1: %2").arg(cmd, resp);
}

/// The SMTP verb a command_error names, upper-cased, first word only.
auto CommandVerb(const std::string& command) -> QString {
  const auto text = QString::fromStdString(command).trimmed().toUpper();
  const auto space = text.indexOf(' ');
  return space < 0 ? text : text.left(space);
}

/// The address inside a "RCPT TO:<someone@example.org>" command line.
auto AddressInCommand(const std::string& command) -> QString {
  const auto text = QString::fromStdString(command);
  const auto open = text.indexOf('<');
  const auto close = text.lastIndexOf('>');
  if (open < 0 || close <= open) return {};
  return text.mid(open + 1, close - open - 1);
}

auto ClassifyCertificate(const vmime::exception& e, MailError& out) -> bool {
  using namespace vmime::security::cert;

  if (dynamic_cast<const serverIdentityException*>(&e) != nullptr) {
    out.category = MailErrorCategory::kTLS_HOSTNAME;
    out.title = Tr("The server's certificate is for a different host");
    out.detail =
        Tr("The certificate the server presented does not name the host you "
           "configured. This is what an interception looks like, so the "
           "connection was refused. Check the host name for a typo.");
    return true;
  }

  if (dynamic_cast<const certificateExpiredException*>(&e) != nullptr ||
      dynamic_cast<const certificateNotYetValidException*>(&e) != nullptr) {
    out.category = MailErrorCategory::kTLS_EXPIRED;
    out.title = Tr("The server's certificate is not currently valid");
    out.detail = Tr(
        "The certificate has expired or is not valid yet. If your computer's "
        "clock is wrong, fix that first; otherwise the server's certificate "
        "needs renewing. This cannot be bypassed by trusting the certificate.");
    return true;
  }

  if (dynamic_cast<const certificateNotTrustedException*>(&e) != nullptr ||
      dynamic_cast<const certificateIssuerVerificationException*>(&e) !=
          nullptr) {
    out.category = MailErrorCategory::kTLS_UNTRUSTED;
    out.title = Tr("The server's certificate is not trusted");
    out.detail = Tr(
        "The certificate was not issued by an authority your system trusts. "
        "That is expected for a self-hosted server with its own certificate. "
        "You can examine it and choose to trust this exact certificate for "
        "this account.");
    return true;
  }

  if (dynamic_cast<const unsupportedCertificateTypeException*>(&e) != nullptr) {
    out.category = MailErrorCategory::kTLS_HANDSHAKE;
    out.title = Tr("The server's certificate could not be read");
    out.detail =
        Tr("The certificate is of a type this application cannot "
           "verify, so the connection was refused.");
    return true;
  }

  if (dynamic_cast<const certificateException*>(&e) != nullptr) {
    out.category = MailErrorCategory::kTLS_HANDSHAKE;
    out.title = Tr("The server's certificate was rejected");
    out.detail =
        Tr("The certificate could not be verified, so the connection "
           "was refused.");
    return true;
  }

  return false;
}

auto ClassifySmtpCommand(const vmime::exceptions::command_error& e,
                         MailError& out) -> void {
  const auto verb = CommandVerb(e.command());

  int status = 0;
  if (const auto* smtp =
          dynamic_cast<const vmime::net::smtp::SMTPCommandError*>(&e);
      smtp != nullptr) {
    status = smtp->statusCode();
  }
  out.status_code = status;

  // A 4xx is the server saying "not now"; a 5xx is "not ever". Only the first
  // is worth trying again unchanged.
  out.transient = status / 100 == 4;

  if (verb == "MAIL") {
    out.category = MailErrorCategory::kSMTP_SENDER_REJECTED;
    out.title = Tr("The server rejected the sender address");
    out.detail =
        Tr("The outgoing server would not accept mail from this address. It "
           "usually means the address does not match the account you signed in "
           "with.");
    return;
  }

  if (verb == "RCPT") {
    out.category = MailErrorCategory::kSMTP_RECIPIENT_REJECTED;
    const auto address = AddressInCommand(e.command());
    out.title = address.isEmpty()
                    ? Tr("The server rejected a recipient")
                    : Tr("The server rejected the recipient %1").arg(address);

    // Stated plainly because the intuition is wrong: vmime abandons the whole
    // submission at the first bad RCPT, so this is not a partial delivery.
    out.detail = Tr(
        "The message was not sent to anyone. The server refused this "
        "recipient, and sending stops at the first refusal. Correct or remove "
        "the address and send again.");
    return;
  }

  if (verb == "DATA") {
    out.category = MailErrorCategory::kSMTP_DATA_REJECTED;
    out.title = Tr("The server rejected the message");
    out.detail = Tr(
        "The outgoing server accepted the sender and recipients but refused "
        "the message itself. Its reply is shown below and usually says why -- "
        "commonly a size limit or a content policy.");
    return;
  }

  if (verb == "STARTTLS") {
    out.category = MailErrorCategory::kTLS_HANDSHAKE;
    out.title = Tr("The server refused to start an encrypted connection");
    out.detail =
        Tr("The connection was closed rather than continued unencrypted. Check "
           "whether this server expects a different security setting or port.");
    return;
  }

  if (verb == "LOGIN" || verb == "AUTH") {
    out.category = MailErrorCategory::kAUTH;
    out.title = Tr("Sign-in was refused");
    out.detail = Tr("The server did not accept the username and password.");
    return;
  }

  if (verb == "LIST" || verb == "SELECT" || verb == "EXAMINE") {
    out.category = MailErrorCategory::kFOLDER;
    out.title = Tr("The folder could not be opened");
    return;
  }

  if (verb == "SEARCH" || verb == "UID") {
    out.category = MailErrorCategory::kLISTING;
    out.title = Tr("The search could not be completed");
    return;
  }

  if (verb == "FETCH") {
    out.category = MailErrorCategory::kFETCH;
    out.title = Tr("The message could not be retrieved");
    return;
  }

  out.category = MailErrorCategory::kLISTING;
  out.title = Tr("The server rejected a command");
}

}  // namespace

auto ClassifyVmimeException(const vmime::exception& e, MailStage stage,
                            bool cancelled) -> MailError {
  namespace ex = vmime::exceptions;

  MailError out;
  out.protocol_detail = QString::fromUtf8(e.what()).trimmed();

  // Cancellation first: vmime cannot tell our timeout handler firing on a user
  // request apart from a server that stopped answering, and reporting a
  // deliberate stop as a server fault would be a lie.
  if (cancelled) {
    out.category = MailErrorCategory::kCANCELLED;
    out.title = Tr("Stopped");
    return out;
  }

  if (ClassifyCertificate(e, out)) return out;

  // Nested causes: a certificate failure raised inside a handshake can arrive
  // wrapped, and the wrapper says far less than the cause does.
  for (const auto* cause = e.other(); cause != nullptr;
       cause = cause->other()) {
    if (ClassifyCertificate(*cause, out)) return out;
  }

  if (dynamic_cast<const ex::operation_timed_out*>(&e) != nullptr) {
    out.category = MailErrorCategory::kTIMEOUT;
    out.title = Tr("The server stopped responding");
    out.transient = true;

    // The body is already on the wire; whether the server took it is unknown.
    if (stage == MailStage::kSUBMIT_FINAL) {
      out.category = MailErrorCategory::kSMTP_AMBIGUOUS;
      out.title = Tr("The outcome is unknown");
      out.transient = false;
    }
    return out;
  }

  if (const auto* cmd = dynamic_cast<const ex::command_error*>(&e);
      cmd != nullptr) {
    out.protocol_detail = SafeProtocolText(cmd->command(), cmd->response());
    ClassifySmtpCommand(*cmd, out);
    return out;
  }

  if (dynamic_cast<const ex::authentication_error*>(&e) != nullptr) {
    out.category = MailErrorCategory::kAUTH;
    out.title = Tr("Sign-in was refused");
    out.detail = Tr(
        "The server did not accept the username and password. If this account "
        "belongs to a provider that requires signing in through its own web "
        "page, an ordinary password will not work and you will need an "
        "app-specific password instead.");
    return out;
  }

  if (dynamic_cast<
          const vmime::net::smtp::SMTPMessageSizeExceedsMaxLimitsException*>(
          &e) != nullptr ||
      dynamic_cast<
          const vmime::net::smtp::SMTPMessageSizeExceedsCurLimitsException*>(
          &e) != nullptr) {
    out.category = MailErrorCategory::kSMTP_DATA_REJECTED;
    out.title = Tr("The message is too large for this server");
    return out;
  }

  if (dynamic_cast<const ex::folder_not_found*>(&e) != nullptr ||
      dynamic_cast<const ex::invalid_folder_name*>(&e) != nullptr ||
      dynamic_cast<const ex::folder_already_open*>(&e) != nullptr) {
    out.category = MailErrorCategory::kFOLDER;
    out.title = Tr("The folder could not be opened");
    return out;
  }

  if (dynamic_cast<const ex::message_not_found*>(&e) != nullptr ||
      dynamic_cast<const ex::unfetched_object*>(&e) != nullptr ||
      dynamic_cast<const ex::partial_fetch_not_supported*>(&e) != nullptr) {
    out.category = MailErrorCategory::kFETCH;
    out.title = Tr("The message could not be retrieved");
    return out;
  }

  if (dynamic_cast<const ex::invalid_response*>(&e) != nullptr ||
      dynamic_cast<const ex::connection_greeting_error*>(&e) != nullptr) {
    out.category = stage == MailStage::kCONNECT ? MailErrorCategory::kCONNECT
                                                : MailErrorCategory::kLISTING;
    out.title = Tr("The server sent an unexpected response");
    return out;
  }

  if (dynamic_cast<const ex::connection_error*>(&e) != nullptr ||
      dynamic_cast<const ex::socket_exception*>(&e) != nullptr) {
    // A failure once the body is on the wire is the one case that must never
    // be reported as either success or failure.
    if (stage == MailStage::kSUBMIT_FINAL) {
      out.category = MailErrorCategory::kSMTP_AMBIGUOUS;
      out.title = Tr("The outcome is unknown");
      return out;
    }

    // vmime raises the same exception type for "cannot resolve" and "cannot
    // connect", and only the message distinguishes them. Matching on text is
    // fragile, so an unrecognised message falls through to the connect case,
    // which is the more general of the two.
    const auto text = out.protocol_detail.toLower();
    if (text.contains("resolve")) {
      out.category = MailErrorCategory::kDNS;
      out.title = Tr("The server could not be found");
      out.detail = Tr(
          "The host name did not resolve. Check it for a typo and check that "
          "this computer is online.");
      return out;
    }

    out.category = MailErrorCategory::kCONNECT;
    out.title = Tr("The server could not be reached");
    out.detail =
        Tr("The host resolved but refused or ignored the connection. Check the "
           "port and security settings, and whether a firewall is in the way.");
    out.transient = true;
    return out;
  }

  if (dynamic_cast<const ex::tls_exception*>(&e) != nullptr) {
    out.category = MailErrorCategory::kTLS_HANDSHAKE;
    out.title = Tr("The encrypted connection could not be established");
    out.detail = Tr(
        "The server and this application could not agree on how to secure the "
        "connection. A server that only offers outdated encryption will fail "
        "here, which is deliberate.");
    return out;
  }

  if (dynamic_cast<const ex::not_connected*>(&e) != nullptr ||
      dynamic_cast<const ex::already_connected*>(&e) != nullptr ||
      dynamic_cast<const ex::illegal_state*>(&e) != nullptr ||
      dynamic_cast<const ex::illegal_operation*>(&e) != nullptr) {
    return MailInternalError(out.protocol_detail);
  }

  out.category = stage == MailStage::kCONNECT ? MailErrorCategory::kCONNECT
                                              : MailErrorCategory::kLISTING;
  out.title = Tr("The operation failed");
  return out;
}

auto MailTlsRequiredError(const QString& host) -> MailError {
  MailError out;
  out.category = MailErrorCategory::kTLS_REQUIRED;
  out.title = Tr("%1 would not start an encrypted connection").arg(host);
  out.detail = Tr(
      "The connection was closed rather than continued unencrypted, so your "
      "password was never sent. Check whether this server expects a different "
      "security setting or a different port.");
  return out;
}

auto MailInternalError(const QString& what) -> MailError {
  MailError out;
  out.category = MailErrorCategory::kINTERNAL;
  out.title = Tr("Something went wrong inside this application");
  out.protocol_detail = what;
  return out;
}
