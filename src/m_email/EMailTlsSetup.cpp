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

#include "EMailTlsSetup.h"

#include <QMutex>
#include <QMutexLocker>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <vmime/net/tls/TLSSecuredConnectionInfos.hpp>
#include <vmime/security/cert/defaultCertificateVerifier.hpp>

#include "GFModuleCommonUtils.hpp"

namespace cert = vmime::security::cert;

namespace {

Q_GLOBAL_STATIC(QMutex, tls_mutex)

/// Parsed once per process. Turning the system trust store into vmime
/// certificates costs real time -- there are typically well over a hundred --
/// and it cannot change while we are running.
auto RootCertificates()
    -> const std::vector<vmime::shared_ptr<cert::X509Certificate>>& {
  static std::vector<vmime::shared_ptr<cert::X509Certificate>> roots;
  static bool loaded = false;

  QMutexLocker locker(tls_mutex());
  if (loaded) return roots;
  loaded = true;

  // Qt's system store is the only source that works on all three platforms:
  // the Windows and macOS stores have no file to read, and a bundled CA list
  // would make us responsible for distrust events we cannot react to quickly.
  const auto system_certs = QSslConfiguration::systemCaCertificates();

  // vmime's batch import is PEM-only in practice -- it loops on
  // PEM_read_bio_X509 and stops at the first failure -- despite its comment
  // claiming it accepts DER as well.
  QByteArray bundle;
  for (const auto& certificate : system_certs) {
    bundle.append(certificate.toPem());
  }

  if (bundle.isEmpty()) {
    LOG_ERROR("system certificate store is empty; mail TLS will fail closed");
    return roots;
  }

  cert::X509Certificate::import(
      reinterpret_cast<const vmime::byte_t*>(bundle.constData()),
      static_cast<size_t>(bundle.size()), roots);

  // %1, not %d: FormatString() is built on QString::arg, so a printf-style
  // placeholder is left in the text and the argument is dropped with a
  // runtime warning.
  FLOG_DEBUG("loaded %1 root certificates for mail TLS",
             static_cast<int>(roots.size())); 
  return roots;
}

/// The certificate the last failed verification saw, so it can be shown and
/// offered for pinning. Guarded because a worker thread writes it and the GUI
/// thread reads it.
struct LastSeen {
  QString fingerprint;
  QString summary;
};

Q_GLOBAL_STATIC(LastSeen, last_seen)

auto FingerprintOf(const vmime::shared_ptr<cert::X509Certificate>& certificate)
    -> QString {
  if (!certificate) return {};

  const auto digest =
      certificate->getFingerprint(cert::X509Certificate::DIGEST_SHA256);
  QByteArray bytes;
  for (const auto byte : digest) bytes.append(static_cast<char>(byte));
  return QString::fromLatin1(bytes.toHex()).toLower();
}

/**
 * @brief The application's verifier: the default one, plus a memory.
 *
 * It deliberately does NOT relax any rule. Pinning is expressed by handing the
 * pinned certificate to the base class as a trusted certificate, which the
 * base class checks only after validity and before hostname -- so a pinned
 * certificate that has expired, or that names another host, still fails. That
 * ordering is what keeps a pin from becoming "ignore TLS errors", and there is
 * a test over it.
 */
class RememberingVerifier : public cert::defaultCertificateVerifier {
 public:
  void verify(const vmime::shared_ptr<cert::certificateChain>& chain,
              const vmime::string& hostname) override {
    remember(chain);
    cert::defaultCertificateVerifier::verify(chain, hostname);
  }

 private:
  static void remember(const vmime::shared_ptr<cert::certificateChain>& chain) {
    if (!chain || chain->getCount() == 0) return;

    auto leaf = vmime::dynamicCast<cert::X509Certificate>(chain->getAt(0));
    if (!leaf) return;

    QMutexLocker locker(tls_mutex());
    last_seen->fingerprint = FingerprintOf(leaf);
    last_seen->summary =
        QString::fromStdString(leaf->getIssuerString()).trimmed();
  }
};

}  // namespace

namespace EMailTlsSetup {

auto ProtocolName(bool imap, MailTlsMode mode) -> QString {
  // Implicit TLS is a different vmime protocol; STARTTLS is the plain protocol
  // plus a property. kNONE is the plain protocol with that property off, and
  // is only ever reached for a loopback host.
  if (mode == MailTlsMode::kIMPLICIT) return imap ? "imaps" : "smtps";
  return imap ? "imap" : "smtp";
}

void Apply(const vmime::shared_ptr<vmime::net::session>& session,
           const vmime::shared_ptr<vmime::net::service>& service,
           const QString& prefix, const MailTransportConfig& config) {
  if (!session || !service) return;

  auto& properties = session->getProperties();

  const auto tls_key = QString("%1.connection.tls").arg(prefix).toStdString();
  const auto required_key =
      QString("%1.connection.tls.required").arg(prefix).toStdString();

  if (config.tls == MailTlsMode::kNONE) {
    // Only ever reached for loopback; the caller has already refused this for
    // any other host.
    properties.setProperty(tls_key, false);
    properties.setProperty(required_key, false);
    return;
  }

  properties.setProperty(tls_key, true);

  // The load-bearing line of this whole file. vmime defaults this to false,
  // and with it false a server that refuses STARTTLS gets a cleartext session
  // that then carries the password. Setting it makes that failure fatal.
  properties.setProperty(required_key, true);

  auto verifier = vmime::make_shared<RememberingVerifier>();
  verifier->setX509RootCAs(RootCertificates());

  if (!config.pinned_cert_sha256.isEmpty()) {
    // A pin is one certificate for one transport. It is given to the base
    // class as a trusted certificate, which excuses an untrusted chain and
    // nothing else: expiry is checked before this is consulted and the
    // hostname after it.
    std::vector<vmime::shared_ptr<cert::X509Certificate>> pinned;
    for (const auto& root : RootCertificates()) {
      if (FingerprintOf(root) == config.pinned_cert_sha256)
        pinned.push_back(root);
    }
    if (!pinned.empty()) verifier->setX509TrustedCerts(pinned);
  }

  service->setCertificateVerifier(verifier);
}

auto IsSecured(const vmime::shared_ptr<vmime::net::service>& service) -> bool {
  if (!service) return false;

  auto infos = service->getConnectionInfos();
  if (!infos) return false;

  // vmime says "this link is encrypted" by the concrete type of the infos
  // object rather than with a flag, so this is the only honest way to ask.
  return vmime::dynamicCast<const vmime::net::tls::TLSSecuredConnectionInfos>(
             infos) != nullptr;
}

auto LastSeenFingerprint() -> QString {
  QMutexLocker locker(tls_mutex());
  return last_seen->fingerprint;
}

auto LastSeenCertificateSummary() -> QString {
  QMutexLocker locker(tls_mutex());
  return last_seen->summary;
}

void ClearLastSeen() {
  QMutexLocker locker(tls_mutex());
  last_seen->fingerprint.clear();
  last_seen->summary.clear();
}

}  // namespace EMailTlsSetup
