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

// What every index-driven test needs: a signer that signs as minisign does,
// the index the release server publishes, built from its parts, and mirrors
// that serve it.

#include <sodium.h>

#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <array>
#include <cstdlib>
#include <map>
#include <utility>

#include "UpdateChecker.h"

namespace test_index {

/// Signs as minisign does (prehashed "ED"), so a test can sign any index.
class Signer {
 public:
  Signer() {
    if (sodium_init() < 0) std::abort();
    crypto_sign_keypair(pk_.data(), sk_.data());
    randombytes_buf(id_.data(), id_.size());
  }

  [[nodiscard]] auto PublicKey() const -> QString {
    QByteArray raw("Ed");
    raw += Raw(id_) + Raw(pk_);
    return QString::fromLatin1(raw.toBase64());
  }

  [[nodiscard]] auto Sign(const QByteArray& message,
                          const QString& comment) const -> QByteArray {
    std::array<unsigned char, crypto_generichash_BYTES_MAX> hash{};
    crypto_generichash(hash.data(), hash.size(), U(message), message.size(),
                       nullptr, 0);
    std::array<unsigned char, crypto_sign_BYTES> sig{};
    crypto_sign_detached(sig.data(), nullptr, hash.data(), hash.size(),
                         sk_.data());
    const auto trusted = comment.toUtf8();
    const auto bound = Raw(sig) + trusted;
    std::array<unsigned char, crypto_sign_BYTES> global{};
    crypto_sign_detached(global.data(), nullptr, U(bound), bound.size(),
                         sk_.data());
    QByteArray line("ED");
    line += Raw(id_) + Raw(sig);
    return "untrusted comment: test\n" + line.toBase64() +
           "\ntrusted comment: " + trusted + "\n" + Raw(global).toBase64() +
           "\n";
  }

 private:
  template <std::size_t N>
  static auto Raw(const std::array<unsigned char, N>& a) -> QByteArray {
    return {reinterpret_cast<const char*>(a.data()), static_cast<qsizetype>(N)};
  }
  static auto U(const QByteArray& a) -> const unsigned char* {
    return reinterpret_cast<const unsigned char*>(a.constData());
  }

  std::array<unsigned char, crypto_sign_PUBLICKEYBYTES> pk_{};
  std::array<unsigned char, crypto_sign_SECRETKEYBYTES> sk_{};
  std::array<unsigned char, 8> id_{};
};

inline const QDateTime kNow =
    QDateTime::fromString("2026-11-06T12:00:00Z", Qt::ISODate);

/// The index as scripts/release/gfrelease writes it, signed 14 days before it
/// @p expires. @p extra adds or replaces top-level fields: "series",
/// "releases", "channels" for a test about release builds.
inline auto Index(qint64 serial, const QString& nightly,
                  const QString& product = "gpgfrontend",
                  const QDateTime& expires = kNow.addDays(14),
                  const QStringList& withdrawn = {},
                  const QJsonObject& extra = {}) -> QByteArray {
  QJsonObject channels{
      {"stable", QJsonValue::Null},
      {"nightly",
       nightly.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(nightly)}};
  QJsonArray w;
  for (const auto& v : withdrawn)
    w.append(QJsonObject{{"version", v}, {"reason", "r"}});
  QJsonObject index{
      {"schema", 1},
      {"product", product},
      {"serial", serial},
      {"generated_at", expires.addDays(-14).toUTC().toString(Qt::ISODate)},
      {"expires", expires.toUTC().toString(Qt::ISODate)},
      {"channels", channels},
      {"withdrawn", w}};
  for (auto it = extra.begin(); it != extra.end(); ++it) {
    index[it.key()] = it.value();
  }
  return QJsonDocument(index).toJson();
}

inline auto Comment(qint64 serial, const QString& product = "gpgfrontend")
    -> QString {
  return QStringLiteral("product=%1 serial=%2").arg(product).arg(serial);
}

/// The update and build mirrors as a table of URL -> (status, body), every
/// request answered at once; anything not in it is a 404. (UpdateCheckerTest
/// has its own server that holds each request until the test answers it.)
struct Mirrors {
  std::map<QString, std::pair<int, QByteArray>> table;
  QStringList asked;

  void Put(const QString& url, const QByteArray& body, int status = 200) {
    table[url] = {status, body};
  }

  /// @p index and its @p signature, as <base>gpgfrontend/v1/ serves them.
  void Serve(const QString& base, const QByteArray& index,
             const QByteArray& signature) {
    Put(base + "gpgfrontend/v1/index.json", index);
    Put(base + "gpgfrontend/v1/index.json.minisig", signature);
  }

  auto Fetcher() -> UpdateChecker::Fetcher {
    return [this](const QUrl& url, const UpdateChecker::Reply& reply) {
      asked << url.toString();
      const auto it = table.find(url.toString());
      if (it == table.end()) {
        reply(404, {});
      } else {
        reply(it->second.first, it->second.second);
      }
    };
  }
};

}  // namespace test_index
