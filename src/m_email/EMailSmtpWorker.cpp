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

#include "EMailSmtpWorker.h"

#include <vmime/net/smtp/SMTPExceptions.hpp>
#include <vmime/security/defaultAuthenticator.hpp>

#include "EMailTlsSetup.h"
#include "GFModuleCommonUtils.hpp"

namespace {

/// See the IMAP worker: the password is only produced on a secured link, so a
/// policy failure becomes a failed login rather than a leaked credential.
class SecureAuthenticator : public vmime::security::defaultAuthenticator {
 public:
  SecureAuthenticator(QString username, QString password, bool allow_cleartext)
      : username_(std::move(username)),
        password_(std::move(password)),
        allow_cleartext_(allow_cleartext) {}

  ~SecureAuthenticator() override { password_.fill(QChar('\0')); }

  auto getUsername() const -> const vmime::string override {
    return username_.toStdString();
  }

  auto getPassword() const -> const vmime::string override {
    if (!allow_cleartext_) {
      auto service = getService().lock();
      if (!EMailTlsSetup::IsSecured(service)) {
        LOG_ERROR("refusing to send mail password over an unencrypted link");
        throw vmime::exceptions::authentication_error(
            "refusing to authenticate over an unencrypted connection");
      }
    }
    return password_.toStdString();
  }

 private:
  QString username_;
  mutable QString password_;
  bool allow_cleartext_;
};

/**
 * @brief Watches the body leave, so an ambiguous failure can be recognised.
 *
 * The distinction this exists for: if the connection dies *before* the body is
 * fully written, nothing was delivered and saying so is safe. If it dies
 * *after*, the server may or may not have accepted the message, and claiming
 * either outcome would be a guess. vmime drives this listener from the stream
 * copy that writes the body, so completion here is the dividing line.
 */
class BodyProgress : public vmime::utility::progressListener {
 public:
  void start(const size_t predicted_total) override {
    total_ = predicted_total;
  }

  void progress(const size_t current, const size_t current_total) override {
    total_ = current_total;
    if (current >= current_total && current_total > 0) written_ = true;
  }

  void stop(const size_t total) override {
    if (total > 0 && total >= total_) written_ = true;
  }

  [[nodiscard]] auto BodyFullyWritten() const -> bool { return written_; }

 private:
  size_t total_{0};
  bool written_{false};
};

auto SecurityLabel(MailTlsMode mode) -> QString {
  switch (mode) {
    case MailTlsMode::kIMPLICIT:
      return "TLS";
    case MailTlsMode::kSTARTTLS:
      return "STARTTLS";
    case MailTlsMode::kNONE:
      return "unencrypted";
  }
  return {};
}

}  // namespace

EMailSmtpWorker::EMailSmtpWorker(QObject* parent)
    : QObject(parent), token_(std::make_shared<EMailCancelToken>()) {}

EMailSmtpWorker::~EMailSmtpWorker() = default;

void EMailSmtpWorker::TestConnection(quint64 seq,
                                     const MailAccountConfig& account,
                                     QString password) {
  token_->Reset();
  EMailTlsSetup::ClearLastSeen();

  const auto& config = account.smtp;

  EMailSendReceipt receipt;
  receipt.host =
      QString("%1:%2").arg(config.host).arg(config.EffectivePort(false));
  receipt.security = SecurityLabel(config.tls);

  const auto cleartext = config.tls == MailTlsMode::kNONE;
  if (cleartext && !MailHostAllowsCleartext(config.host)) {
    password.fill(QChar('\0'));
    receipt.error = MailTlsRequiredError(config.host);
    emit SignalFinished(seq, receipt);
    return;
  }

  auto timeouts = vmime::make_shared<EMailTimeoutHandlerFactory>(token_, 30);

  try {
    auto session = vmime::net::session::create();
    const auto protocol = EMailTlsSetup::ProtocolName(false, config.tls);

    vmime::utility::url url(protocol.toStdString(), config.host.toStdString());
    url.setPort(config.EffectivePort(false));

    auto transport = session->getTransport(url);
    transport->setTimeoutHandlerFactory(timeouts);

    EMailTlsSetup::Apply(session, transport,
                         QString("transport.%1").arg(protocol), config);

    transport->setAuthenticator(vmime::make_shared<SecureAuthenticator>(
        config.username, password, cleartext));
    password.fill(QChar('\0'));

    transport->connect();

    if (!cleartext && !EMailTlsSetup::IsSecured(transport)) {
      receipt.error = MailTlsRequiredError(config.host);
    } else {
      // Reaching here means the server accepted the credentials, which is the
      // whole question a connection test asks.
      receipt.accepted = true;
    }

    try {
      transport->disconnect();
    } catch (...) {
    }

    emit SignalFinished(seq, receipt);
  } catch (const vmime::exception& e) {
    password.fill(QChar('\0'));
    receipt.error = ClassifyVmimeException(e, MailStage::kCONNECT,
                                           timeouts->LastWasCancelled());
    emit SignalFinished(seq, receipt);
  } catch (const std::exception& e) {
    password.fill(QChar('\0'));
    receipt.error = MailInternalError(QString::fromUtf8(e.what()));
    emit SignalFinished(seq, receipt);
  }
}

void EMailSmtpWorker::Submit(quint64 seq, const MailAccountConfig& account,
                             QString password,
                             const EMailOutgoingMessage& message) {
  token_->Reset();
  EMailTlsSetup::ClearLastSeen();

  const auto& config = account.smtp;

  EMailSendReceipt receipt;
  receipt.message_id = message.message_id;
  receipt.host =
      QString("%1:%2").arg(config.host).arg(config.EffectivePort(false));
  receipt.security = SecurityLabel(config.tls);

  const auto cleartext = config.tls == MailTlsMode::kNONE;
  if (cleartext && !MailHostAllowsCleartext(config.host)) {
    password.fill(QChar('\0'));
    receipt.error = MailTlsRequiredError(config.host);
    emit SignalFinished(seq, receipt);
    return;
  }

  if (!message.IsValid()) {
    password.fill(QChar('\0'));
    receipt.error = MailInternalError("outgoing message is incomplete");
    emit SignalFinished(seq, receipt);
    return;
  }

  auto timeouts = vmime::make_shared<EMailTimeoutHandlerFactory>(token_, 60);
  BodyProgress progress;
  auto stage = MailStage::kCONNECT;

  vmime::shared_ptr<vmime::net::transport> transport;

  try {
    auto session = vmime::net::session::create();
    const auto protocol = EMailTlsSetup::ProtocolName(false, config.tls);

    vmime::utility::url url(protocol.toStdString(), config.host.toStdString());
    url.setPort(config.EffectivePort(false));

    transport = session->getTransport(url);
    transport->setTimeoutHandlerFactory(timeouts);

    EMailTlsSetup::Apply(session, transport,
                         QString("transport.%1").arg(protocol), config);

    transport->setAuthenticator(vmime::make_shared<SecureAuthenticator>(
        config.username, password, cleartext));
    password.fill(QChar('\0'));

    stage = MailStage::kCONNECT;
    transport->connect();

    if (!cleartext && !EMailTlsSetup::IsSecured(transport)) {
      receipt.error = MailTlsRequiredError(config.host);
      emit SignalFinished(seq, receipt);
      return;
    }

    vmime::mailbox expeditor(message.envelope_from.toStdString());
    vmime::mailboxList recipients;
    for (const auto& address : message.envelope_rcpt) {
      recipients.appendMailbox(
          vmime::make_shared<vmime::mailbox>(address.toStdString()));
    }

    const std::string data(message.eml.constData(),
                           static_cast<size_t>(message.eml.size()));
    vmime::utility::inputStreamStringAdapter input(data);

    receipt.submitted_at = QDateTime::currentDateTime();

    // The stream overload, deliberately. The message-taking overload
    // regenerates the header on a clone -- inventing a Message-ID the caller
    // never learns and re-serializing the body -- which would both break a
    // signature and make the sent copy impossible to find afterwards.
    stage = MailStage::kSUBMIT_BODY;
    transport->send(expeditor, recipients, input,
                    static_cast<size_t>(message.eml.size()), &progress);

    // Reached only when the server's final reply arrived and was positive.
    receipt.accepted = true;
    receipt.reply_code = 250;

    try {
      transport->disconnect();
    } catch (...) {
      // A failure closing the connection says nothing about the message: it
      // has already been accepted, and reporting it would be alarming noise.
    }

    emit SignalFinished(seq, receipt);
    return;
  } catch (const vmime::exception& e) {
    password.fill(QChar('\0'));

    // The heart of the ambiguity handling. Once the body has been fully
    // written, a socket-level failure means we do not know whether the server
    // took the message -- so the stage handed to the classifier changes, and
    // with it the answer the user is given.
    if (progress.BodyFullyWritten() && stage == MailStage::kSUBMIT_BODY) {
      stage = MailStage::kSUBMIT_FINAL;
    }

    if (const auto* smtp =
            dynamic_cast<const vmime::net::smtp::SMTPCommandError*>(&e);
        smtp != nullptr) {
      receipt.reply_code = smtp->statusCode();
      receipt.reply_text = QString::fromStdString(smtp->response()).trimmed();
    }

    receipt.error =
        ClassifyVmimeException(e, stage, timeouts->LastWasCancelled());
    receipt.ambiguous =
        receipt.error.category == MailErrorCategory::kSMTP_AMBIGUOUS;

    try {
      if (transport && transport->isConnected()) transport->disconnect();
    } catch (...) {
    }

    emit SignalFinished(seq, receipt);
    return;
  } catch (const std::exception& e) {
    password.fill(QChar('\0'));
    receipt.error = MailInternalError(QString::fromUtf8(e.what()));
    emit SignalFinished(seq, receipt);
    return;
  }
}
