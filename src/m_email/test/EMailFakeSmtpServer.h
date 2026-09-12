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
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include <QStringList>
#include <QTcpServer>

/**
 * @brief An SMTP submission server that speaks just enough to be argued with.
 *
 * It exists to answer one question the application could not otherwise ask:
 * does a wrong password actually fail? That question only has meaning over a
 * secured link, because the client refuses to authenticate over anything else
 * -- so this server serves implicit TLS with a self-signed certificate, which
 * the test trusts by pinning its fingerprint.
 *
 * Every command line is recorded so a test can assert on what was SENT rather
 * than only on what came back. "No AUTH command was ever issued" is not
 * observable from the client's return value, which is precisely how sending
 * mail unauthenticated went unnoticed.
 */
class FakeSmtpServer : public QTcpServer {
 public:
  explicit FakeSmtpServer(QObject* parent = nullptr) : QTcpServer(parent) {}

  /// Mechanisms to advertise after EHLO. Empty advertises no AUTH extension.
  QStringList mechanisms{"PLAIN", "LOGIN"};

  /// Whether to serve implicit TLS. A cleartext server exists to prove the
  /// client refuses to authenticate over one.
  bool use_tls{true};

  QString expect_username{"user"};
  QString expect_password{"correct-horse"};

  QStringList Commands() const {
    QMutexLocker locker(&mutex_);
    return commands_;
  }

  /// Credentials the client actually presented, however it presented them.
  QString seen_username;
  QString seen_password;

  bool authenticated{false};

  /// What the server does once the message body has been fully received --
  /// i.e. after the terminating ".", at the exact moment the client is waiting
  /// for the 250 that says the message was accepted.
  ///
  /// This is the only interesting moment in an SMTP submission. Everything
  /// before it is safe to report as "not sent"; after it, whether the message
  /// was delivered is genuinely unknown, and every one of these behaviours has
  /// to end up saying so rather than claiming a clean failure.
  enum class AfterBody {
    kAccept,     ///< answer 250: the ordinary success
    kDropConnection,  ///< close the socket without answering
    kStall,      ///< read the body, then never answer at all
  };
  AfterBody after_body{AfterBody::kAccept};

  /// True once the terminating "." has been seen, so a test can wait for the
  /// body to be fully on the wire before doing anything else.
  auto BodyComplete() const -> bool {
    QMutexLocker locker(&mutex_);
    return body_complete_;
  }

  static auto CertificatePem() -> QByteArray {
    return
      "-----BEGIN CERTIFICATE-----\n"
      "MIIDHDCCAgSgAwIBAgIUdAThcYxVDbKBZHXo/AONasLzm/gwDQYJKoZIhvcNAQEL\n"
      "BQAwFDESMBAGA1UEAwwJMTI3LjAuMC4xMCAXDTI2MDkxMjE0MjYyMFoYDzIwNTYw\n"
      "OTA0MTQyNjIwWjAUMRIwEAYDVQQDDAkxMjcuMC4wLjEwggEiMA0GCSqGSIb3DQEB\n"
      "AQUAA4IBDwAwggEKAoIBAQDcHPyKuKQvl9pCsZQiTd04e2x8RbTxouxazeWgHTvS\n"
      "bvl1Gh7/aMZIFB/isYsI72eCLGGDJSNlsrlBE6PSpUJCwIl2IkLHfc3hUNj0lW5a\n"
      "7zzxF/s2iZ/VVondd7mZe19H7M/mNSrhD0ggu9B9cBAlRYkrtuRcNgYDFYYFXue4\n"
      "6imLhivR5oXSS4NrXjbJH+gyQYuoqHstUUCEFWgjl9FMdb2sJKzWhyxEEEhePYFP\n"
      "GpVDteXO7E6RxLmDTSxzkoXkn+mAQBJYwVfG98mvZcBk0Ov6qvQi1yw45M2CCXvi\n"
      "fovOIlz7lG1sjLg1YH96DuSorOCt6XxD39M4RpDtdq69AgMBAAGjZDBiMB0GA1Ud\n"
      "DgQWBBTxcmUXPeF/wG2sg8XHmP2WfHoZVDAfBgNVHSMEGDAWgBTxcmUXPeF/wG2s\n"
      "g8XHmP2WfHoZVDAPBgNVHRMBAf8EBTADAQH/MA8GA1UdEQQIMAaHBH8AAAEwDQYJ\n"
      "KoZIhvcNAQELBQADggEBAE+9KLl23YtCD3uAWteMZSY4VVAaLkw8E4QO3+U4TciM\n"
      "o1Wy9tAAgU3zgB0bNvjb/o1/5C+2DOxRDK5CPn3uqVzIfUusMfjBNdAfVeuU4QQA\n"
      "hK2y7XDR0sKWy9gKGsgSi5S2NQi4n6Lw5wG8zlC2RF1JsuzLpPYQgebNGSGvYotr\n"
      "JjoO7JvLWEbt4CpgT3c3XGHaGX0iZnHqtUT/YE8MCNNyqppu0H4eGdN/MLeoVYen\n"
      "kFIqM6ZTmyrS2Hkz+j3UpQ7kGUK7cazarRhbLTr2e7yvC2kxw/2q51X3fTCG7u5U\n"
      "D19b4zXVLKDPsNWU1UezVv0FhVBVdQRym7on69qDiLg=\n"
      "-----END CERTIFICATE-----\n";
  }

  static auto PrivateKeyPem() -> QByteArray {
    return
      "-----BEGIN PRIVATE KEY-----\n"
      "MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDcHPyKuKQvl9pC\n"
      "sZQiTd04e2x8RbTxouxazeWgHTvSbvl1Gh7/aMZIFB/isYsI72eCLGGDJSNlsrlB\n"
      "E6PSpUJCwIl2IkLHfc3hUNj0lW5a7zzxF/s2iZ/VVondd7mZe19H7M/mNSrhD0gg\n"
      "u9B9cBAlRYkrtuRcNgYDFYYFXue46imLhivR5oXSS4NrXjbJH+gyQYuoqHstUUCE\n"
      "FWgjl9FMdb2sJKzWhyxEEEhePYFPGpVDteXO7E6RxLmDTSxzkoXkn+mAQBJYwVfG\n"
      "98mvZcBk0Ov6qvQi1yw45M2CCXvifovOIlz7lG1sjLg1YH96DuSorOCt6XxD39M4\n"
      "RpDtdq69AgMBAAECggEAAh73YgDQeDLu4/ihFEmKyaoKzBHB4hgvTdqxDT9q3aa7\n"
      "GaFETjK8wmILQpxyWgYwiaUS1XgspKS5u0JMsFFuV4VSbcR7vz9blgG47XZpJ1Ap\n"
      "1qrj/U0LMLuSPYOTUsLJEsDZ6f52KNAEYmhc5HCJf0pxvW2YOrk8ipgRAAk/k10N\n"
      "sQhVp9x8fDWG9PjsKpD3xGwbHd6uQVeu+GOeWsK1oFQAukdvCO/ZucGHt/r0mh+9\n"
      "hnqLS+VKhywiTpE0k9IE4yYpFzUjmTko9m/GD7fLXV90gphWyUZirxeUXgoDqXN6\n"
      "g0uC0mOdb+n3rnrA25B8N428f44bBDvuf0gRZGpWYwKBgQDyd+uiVY1Yc/z5Xp50\n"
      "U9lOG3V08CU3a9qJgWnuX62ClOC9l8fnEJaV+9lyO3hcFbf6eafAYccxmyjYujUr\n"
      "jFfuXVlgQlKLniiohjGGAXWpmYHrrhitDuEgOs5YrI22IVatvzKL//8fij/nNjSw\n"
      "MqpCaYCNwbjtXHsj8rxmjeqJwwKBgQDoZa7uwJkWYo7tkl841bPrnaOI2J7HsuUO\n"
      "I3hMaUk2lN6ODWfLGHtdfDPObeXyuL1ZouhVfR9EGQA4EG+15ZF2a+SJ50kE2++Q\n"
      "Z5h0798HhkN+wjGkRSVKbkWzNwa82Z/gLVS1qOkWIcgDsPVZ/nzXvd/jUumWRAto\n"
      "ClPGDCXdfwKBgQDonMWRalP8zOGf9vc7EIoEFfG8Kvr5TV5N1rritiWGhf+JxAhC\n"
      "k7Zz0zbMEWOprChhgr69oNBxtbIdIZ8K1UwyYJny8A2y5huJeZwfPF0+RQfQK6h+\n"
      "tiyiN+hoR7p4RUmbzDbY+tIt1vrxfR5U/3Y52m87D+Oyy6tTVHcMSA/+8wKBgAWg\n"
      "qLrJWSusmU9xcvLaYe/7skEXFck6MMfF3hzjk81Jj5YbBv9pCVu7LTn7eU0GYjdw\n"
      "dXXUgNRSUqoI49ugwoP+mtsoCaGffc6eY5e5U0pIWwwPwcn7jqqdvvxXAcfC8Vcp\n"
      "YdrAS1yo1sIYH+jc+8LxqyFDGbr0zN3pGhw6oRQbAoGAHZ164Y//m22YgBA0dd+c\n"
      "u+QZgQQ29TP51iBHuNbmuoMdpwYyihE6qy3pqfrK4kVvsHUBAteiqCNv6l2q+5of\n"
      "/p/NXoG1+bPvCXJJ8kbelHUdu/yasP+5Bi+7yXQIlNOvYzOKiuAzo17CuO7uuVAj\n"
      "DZxo78ePW/SGcgmAkaz+Wa8=\n"
      "-----END PRIVATE KEY-----\n";
  }

  /// Lowercase hex SHA-256 of the certificate above, for pinning.
  static auto Fingerprint() -> QString {
    const auto certs = QSslCertificate::fromData(CertificatePem(), QSsl::Pem);
    if (certs.isEmpty()) return {};
    return QString::fromLatin1(
               certs.first().digest(QCryptographicHash::Sha256).toHex())
        .toLower();
  }

 protected:
  void incomingConnection(qintptr descriptor) override {
    if (!use_tls) {
      auto* plain = new QTcpSocket(this);
      plain->setSocketDescriptor(descriptor);
      connect(plain, &QTcpSocket::readyRead, this,
              [this, plain]() { handle(plain); });
      plain->write("220 fake ESMTP ready\r\n");
      plain->flush();
      return;
    }

    auto* socket = new QSslSocket(this);
    socket->setSocketDescriptor(descriptor);

    const auto certs = QSslCertificate::fromData(CertificatePem(), QSsl::Pem);
    socket->setLocalCertificate(certs.isEmpty() ? QSslCertificate()
                                                : certs.first());
    socket->setPrivateKey(QSslKey(PrivateKeyPem(), QSsl::Rsa));
    socket->setPeerVerifyMode(QSslSocket::VerifyNone);

    connect(socket, &QSslSocket::encrypted, this, [this, socket]() {
      socket->write("220 fake ESMTP ready\r\n");
      socket->flush();
    });
    connect(socket, &QTcpSocket::readyRead, this,
            [this, socket]() { handle(socket); });

    socket->startServerEncryption();
  }

 private:
  mutable QMutex mutex_;
  QStringList commands_;

  /// What a bare base64 line arriving next should be read as. AUTH LOGIN is a
  /// three-round-trip conversation, so the line alone does not say.
  enum class Expect { kCommand, kLoginUser, kLoginPassword, kBody };
  Expect expect_{Expect::kCommand};
  bool body_complete_{false};

  void handle(QTcpSocket* socket) {
    while (socket->canReadLine()) {
      const auto line = QString::fromUtf8(socket->readLine()).trimmed();
      if (line.isEmpty()) continue;
      {
        QMutexLocker locker(&mutex_);
        commands_.append(line);
      }

      QByteArray out;

      if (expect_ == Expect::kBody) {
        // In DATA mode every line is body content until the lone ".".
        if (line != ".") continue;

        {
          QMutexLocker locker(&mutex_);
          body_complete_ = true;
        }
        expect_ = Expect::kCommand;

        if (after_body == AfterBody::kDropConnection) {
          socket->abort();
          return;
        }
        if (after_body == AfterBody::kStall) {
          // Deliberately no reply and no close: the client sits waiting for a
          // 250 that never comes, which is what a hung server looks like.
          continue;
        }
        socket->write("250 2.0.0 Ok: queued\r\n");
        socket->flush();
        continue;
      }

      if (expect_ == Expect::kLoginUser) {
        seen_username = QString::fromUtf8(QByteArray::fromBase64(line.toUtf8()));
        expect_ = Expect::kLoginPassword;
        out = "334 UGFzc3dvcmQ6\r\n";
      } else if (expect_ == Expect::kLoginPassword) {
        seen_password = QString::fromUtf8(QByteArray::fromBase64(line.toUtf8()));
        expect_ = Expect::kCommand;
        out = finish_auth();
      } else {
        out = command(line);
      }

      socket->write(out);
      socket->flush();
    }
  }

  auto finish_auth() -> QByteArray {
    if (seen_username == expect_username && seen_password == expect_password) {
      authenticated = true;
      return "235 2.7.0 Authentication successful\r\n";
    }
    return "535 5.7.8 Authentication credentials invalid\r\n";
  }

  auto command(const QString& line) -> QByteArray {
    const auto verb = line.section(' ', 0, 0).toUpper();

    if (verb == "EHLO") {
      QByteArray out = "250-fake hello\r\n250-8BITMIME\r\n";
      if (!mechanisms.isEmpty()) {
        out += QString("250-AUTH %1\r\n").arg(mechanisms.join(' ')).toUtf8();
      }
      out += "250 SIZE 35882577\r\n";
      return out;
    }
    if (verb == "HELO") return "250 fake hello\r\n";

    if (verb == "AUTH") {
      const auto mechanism = line.section(' ', 1, 1).toUpper();
      if (!mechanisms.contains(mechanism)) {
        return "504 5.5.4 Unrecognized authentication type\r\n";
      }
      if (mechanism == "PLAIN") {
        const auto blob = QByteArray::fromBase64(line.section(' ', 2).toUtf8());
        const auto parts = blob.split('\0');
        if (parts.size() >= 3) {
          seen_username = QString::fromUtf8(parts.at(1));
          seen_password = QString::fromUtf8(parts.at(2));
        }
        return finish_auth();
      }
      if (mechanism == "LOGIN") {
        expect_ = Expect::kLoginUser;
        return "334 VXNlcm5hbWU6\r\n";
      }
    }

    // Submission commands. A server that requires authentication refuses
    // these outright when none happened -- which is how the missing AUTH
    // would eventually have surfaced, as a confusing error on MAIL FROM.
    if (verb == "MAIL" || verb == "RCPT") {
      if (!authenticated) return "530 5.7.0 Authentication required\r\n";
      return "250 2.1.0 Ok\r\n";
    }
    if (verb == "DATA") {
      if (!authenticated) return "530 5.7.0 Authentication required\r\n";
      expect_ = Expect::kBody;
      return "354 End data with <CR><LF>.<CR><LF>\r\n";
    }
    if (verb == "QUIT") return "221 2.0.0 Bye\r\n";

    return "250 2.0.0 Ok\r\n";
  }
};
