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

#include <QNetworkReply>
#include <QObject>
#include <QString>

class QNetworkAccessManager;

/**
 * @brief What a single protocol probe concluded.
 */
enum class ProbeVerdict {
  kConforms,       ///< answered the way the protocol says it should
  kNotAKeyServer,  ///< answered, but not as a key server
  kUnreachable,    ///< never got far enough to tell
};

/**
 * @brief Judge an HKP probe response.
 *
 * Split out from the request so the rules can be read, and reasoned about,
 * without a network in the picture.
 *
 * @param network_error QNetworkReply::NetworkError as an int
 * @param http_status HTTP status code, 0 when the request never got a response
 * @param content_type the Content-Type header, may be empty
 * @param body the response body, only the start of it is examined
 * @return ProbeVerdict
 */
auto ClassifyHkpProbe(int network_error, int http_status,
                      const QString& content_type, const QByteArray& body)
    -> ProbeVerdict;

/**
 * @brief Judge a VKS probe response. @see ClassifyHkpProbe
 */
auto ClassifyVksProbe(int network_error, int http_status,
                      const QString& content_type, const QByteArray& body)
    -> ProbeVerdict;

/**
 * @brief Ask a URL whether it speaks the key server protocols.
 *
 * Sends the HKP and VKS probes together and reports once both have settled, so
 * the caller learns everything about a server from a single round trip.
 */
class KeyServerProber : public QObject {
  Q_OBJECT

 public:
  struct Result {
    QString url;
    bool hkp{false};
    bool vks{false};
    QString detail;  ///< why it failed, empty when either protocol answered

    [[nodiscard]] auto Conforms() const -> bool { return hkp || vks; }
  };

  explicit KeyServerProber(QObject* parent = nullptr);

  /**
   * @brief Probe @p url. Emits SignalProbeFinished exactly once.
   *
   * @param url the key server base URL
   */
  void Probe(const QString& url);

 signals:
  void SignalProbeFinished(const KeyServerProber::Result& result);

 private:
  /**
   * @brief Record one protocol's verdict and report when both are in.
   */
  void settle(bool is_hkp, ProbeVerdict verdict);

  void send(const QString& url, bool is_hkp);

  QNetworkAccessManager* manager_;
  Result result_;
  ProbeVerdict hkp_verdict_{ProbeVerdict::kUnreachable};
  ProbeVerdict vks_verdict_{ProbeVerdict::kUnreachable};
  int outstanding_{0};
};
