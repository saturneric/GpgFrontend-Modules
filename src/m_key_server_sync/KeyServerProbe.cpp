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

#include "KeyServerProbe.h"

#include <GFSDKExtra.h>

#include <QCoreApplication>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <optional>

#include "GFModuleCommonUtils.hpp"

namespace {

/// A handle that cannot belong to any key, so a conforming server is guaranteed
/// to take its "no such key" path rather than returning something real.
constexpr auto kAbsentHandle = "0000000000000000000000000000000000000000";

/// Long enough for any keyserver's "not found" note, short enough that a real
/// document does not slip through as one.
constexpr int kMaxNotFoundBodySize = 512;

/// Only the head of a response tells us anything; the rest is not worth
/// holding.
constexpr int kMaxInspectedBodySize = 4096;

/// Shorter than a real lookup's 15s: this runs while the user waits on a
/// dialog.
constexpr int kProbeTimeoutMs = 8000;

/**
 * @brief Whether the response body is an actual HTML document.
 *
 * Judged on the body rather than the Content-Type header, because the header
 * cannot be trusted here: keys.openpgp.org answers its "No key found for
 * fingerprint ..." with `text/html`, even though the body is a single line of
 * plain text. Rejecting on the header alone would throw out one of the most
 * widely used key servers there is.
 *
 * A mislabelled one-liner and a real 404 page are still easy to tell apart —
 * the page is a document, and it says so in its first bytes.
 */
auto LooksLikeHtml(const QString& content_type, const QByteArray& body)
    -> bool {
  const auto head = QString::fromLatin1(body.left(64)).trimmed().toLower();
  if (head.startsWith("<!doctype html") || head.startsWith("<html")) {
    return true;
  }

  // Claims to be a page, and is long enough to actually be one: markup that
  // opens with a comment or a stray BOM would slip past the check above.
  return content_type.contains("text/html") &&
         body.trimmed().size() >= kMaxNotFoundBodySize;
}

auto ParsesAsJsonObject(const QByteArray& body) -> bool {
  return QJsonDocument::fromJson(body.trimmed()).isObject();
}

/**
 * @brief The verdicts that do not depend on which protocol was probed.
 *
 * @return std::optional<ProbeVerdict> set when the shared rules already decide
 */
auto ClassifyShared(int network_error, int http_status,
                    const QString& content_type, const QByteArray& body)
    -> std::optional<ProbeVerdict> {
  // No status at all means we never spoke HTTP with anyone: DNS failure,
  // refused connection, TLS handshake, or our own transfer timeout firing.
  if (http_status == 0) {
    return network_error == QNetworkReply::NoError
               ? std::optional<ProbeVerdict>{}
               : ProbeVerdict::kUnreachable;
  }

  // An HTML document is what separates a website's 404 from a key server's. It
  // also catches captive portals and intercepting proxies, which happily answer
  // 200 with a login page for any URL you ask them about.
  if (LooksLikeHtml(content_type, body)) return ProbeVerdict::kNotAKeyServer;

  return {};
}

}  // namespace

auto ClassifyHkpProbe(int network_error, int http_status,
                      const QString& content_type, const QByteArray& body)
    -> ProbeVerdict {
  const auto lower_type = content_type.toLower();
  if (auto shared =
          ClassifyShared(network_error, http_status, lower_type, body)) {
    return *shared;
  }

  const auto trimmed = body.trimmed();

  if (http_status == 200) {
    // The machine-readable index starts with an info: header, and each key it
    // found is a pub: line.
    static const QRegularExpression kInfoLine(R"(^info:\s*\d)");
    const auto text = QString::fromUtf8(trimmed.left(kMaxInspectedBodySize));
    if (kInfoLine.match(text).hasMatch()) return ProbeVerdict::kConforms;
    if (text.startsWith("pub:") || text.contains("\npub:")) {
      return ProbeVerdict::kConforms;
    }
    // Some deployments answer an empty index rather than a 404.
    if (trimmed.isEmpty()) return ProbeVerdict::kConforms;
    return ProbeVerdict::kNotAKeyServer;
  }

  // "No keys found" is the expected answer here, and HKP servers say it with a
  // 404 (or a 501 for an op they do not implement) and a one-line body.
  if ((http_status == 404 || http_status == 501) &&
      trimmed.size() < kMaxNotFoundBodySize) {
    return ProbeVerdict::kConforms;
  }

  return ProbeVerdict::kNotAKeyServer;
}

auto ClassifyVksProbe(int network_error, int http_status,
                      const QString& content_type, const QByteArray& body)
    -> ProbeVerdict {
  const auto lower_type = content_type.toLower();
  if (auto shared =
          ClassifyShared(network_error, http_status, lower_type, body)) {
    return *shared;
  }

  const auto trimmed = body.trimmed();

  if (http_status == 200) {
    return trimmed.startsWith("-----BEGIN PGP PUBLIC KEY BLOCK-----")
               ? ProbeVerdict::kConforms
               : ProbeVerdict::kNotAKeyServer;
  }

  // VKS reports both a missing key and a malformed handle as a JSON error
  // object; keys.openpgp.org answers {"code":404,"message":"..."}.
  if (http_status == 404 || http_status == 400) {
    if (lower_type.contains("application/json") ||
        ParsesAsJsonObject(trimmed)) {
      return ProbeVerdict::kConforms;
    }
    // A plain-text "not found" is still a key server answering; only a 404 with
    // a whole document behind it is something else.
    if (http_status == 404 && trimmed.size() < kMaxNotFoundBodySize) {
      return ProbeVerdict::kConforms;
    }
  }

  return ProbeVerdict::kNotAKeyServer;
}

KeyServerProber::KeyServerProber(QObject* parent)
    : QObject(parent), manager_(new QNetworkAccessManager(this)) {}

void KeyServerProber::Probe(const QString& url) {
  result_ = Result{};
  result_.url = url;
  outstanding_ = 2;

  send(url, true);
  send(url, false);
}

void KeyServerProber::send(const QString& url, bool is_hkp) {
  const auto target =
      is_hkp ? QString("%1/pks/lookup?op=index&options=mr&search=0x%2")
                   .arg(url, kAbsentHandle)
             : QString("%1/vks/v1/by-fingerprint/%2").arg(url, kAbsentHandle);

  QNetworkRequest request{QUrl(target)};
  request.setHeader(QNetworkRequest::UserAgentHeader,
                    UDUP(GFHttpRequestUserAgent()));
  request.setTransferTimeout(kProbeTimeoutMs);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);

  auto* reply = manager_->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, is_hkp]() {
    const auto status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto content_type =
        reply->header(QNetworkRequest::ContentTypeHeader).toString();
    const auto body = reply->read(kMaxInspectedBodySize);

    FLOG_DEBUG("key server probe reply, hkp: %1, status: %2, error: %3",
               static_cast<int>(is_hkp), status,
               static_cast<int>(reply->error()));

    settle(is_hkp, is_hkp ? ClassifyHkpProbe(static_cast<int>(reply->error()),
                                             status, content_type, body)
                          : ClassifyVksProbe(static_cast<int>(reply->error()),
                                             status, content_type, body));
    reply->deleteLater();
  });
}

void KeyServerProber::settle(bool is_hkp, ProbeVerdict verdict) {
  if (is_hkp) {
    hkp_verdict_ = verdict;
    result_.hkp = verdict == ProbeVerdict::kConforms;
  } else {
    vks_verdict_ = verdict;
    result_.vks = verdict == ProbeVerdict::kConforms;
  }

  if (--outstanding_ > 0) return;

  if (!result_.Conforms()) {
    // Say which of the two things went wrong. Being unreachable is worth
    // separating from answering wrongly: one is worth retrying later, the other
    // means the address is not a key server at all.
    const auto unreachable = hkp_verdict_ == ProbeVerdict::kUnreachable &&
                             vks_verdict_ == ProbeVerdict::kUnreachable;
    result_.detail =
        unreachable ? QCoreApplication::translate(
                          "GTrC", "The server could not be reached.")
                    : QCoreApplication::translate(
                          "GTrC",
                          "The server responded, but not as a key server: it "
                          "supports neither the HKP nor the VKS interface.");
  }

  emit SignalProbeFinished(result_);
}
