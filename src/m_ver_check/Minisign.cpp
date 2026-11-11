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

#include "Minisign.h"

#include <sodium.h>

#include <algorithm>
#include <array>

namespace {

constexpr qsizetype kKeyIdSize = 8;
constexpr qsizetype kPublicKeySize =
    2 + kKeyIdSize + crypto_sign_PUBLICKEYBYTES;
constexpr qsizetype kSignatureSize = 2 + kKeyIdSize + crypto_sign_BYTES;
constexpr auto kTrustedPrefix = "trusted comment: ";

auto Decode(const QString& base64) -> QByteArray {
  return QByteArray::fromBase64(base64.trimmed().toLatin1(),
                                QByteArray::AbortOnBase64DecodingErrors);
}

auto Bytes(const QByteArray& a) -> const unsigned char* {
  return reinterpret_cast<const unsigned char*>(a.constData());
}

/// A signature file's lines, a CR before each LF dropped: a file that went
/// through a CRLF checkout reads as minisign itself reads it.
auto Lines(const QByteArray& file) -> QList<QByteArray> {
  auto lines = file.split('\n');
  for (auto& line : lines) {
    if (line.endsWith('\r')) line.chop(1);
  }
  return lines;
}

/// Key id bytes as minisign prints them: a little-endian number, in hex.
auto KeyIdHex(const QByteArray& id) -> QString {
  QByteArray reversed(id.size(), 0);
  std::reverse_copy(id.begin(), id.end(), reversed.begin());
  return QString::fromLatin1(reversed.toHex().toUpper());
}

}  // namespace

auto MinisignSignatureKeyId(const QByteArray& signature_file) -> QString {
  const auto lines = Lines(signature_file);
  if (lines.size() < 2) return {};
  const auto signature = Decode(QString::fromLatin1(lines[1]));
  if (signature.size() != kSignatureSize || !signature.startsWith("ED")) {
    return {};
  }
  return KeyIdHex(signature.mid(2, kKeyIdSize));
}

auto MinisignPublicKeyId(const QString& public_key) -> QString {
  const auto key = Decode(public_key);
  if (key.size() != kPublicKeySize || !key.startsWith("Ed")) return {};
  return KeyIdHex(key.mid(2, kKeyIdSize));
}

auto MinisignVerify(const QStringList& public_keys, const QByteArray& message,
                    const QByteArray& signature_file)
    -> std::optional<QString> {
  if (sodium_init() < 0) return std::nullopt;

  // untrusted comment, signature, trusted comment, global signature.
  const auto lines = Lines(signature_file);
  if (lines.size() < 4) return std::nullopt;
  if (!lines[2].startsWith(kTrustedPrefix)) return std::nullopt;

  const auto signature = Decode(QString::fromLatin1(lines[1]));
  const auto trusted = lines[2].mid(qstrlen(kTrustedPrefix));
  const auto global = Decode(QString::fromLatin1(lines[3]));
  if (signature.size() != kSignatureSize ||
      global.size() != static_cast<qsizetype>(crypto_sign_BYTES)) {
    return std::nullopt;
  }

  // Prehashed only, as every signature the release tooling makes.
  if (!signature.startsWith("ED")) return std::nullopt;
  const auto key_id = signature.mid(2, kKeyIdSize);
  const auto sig = signature.mid(2 + kKeyIdSize);

  for (const auto& encoded : public_keys) {
    const auto key = Decode(encoded);
    if (key.size() != kPublicKeySize || !key.startsWith("Ed")) continue;
    if (key.mid(2, kKeyIdSize) != key_id) continue;
    const auto pk = key.mid(2 + kKeyIdSize);

    // The first signature covers the content's BLAKE2b-512 hash.
    std::array<unsigned char, crypto_generichash_BYTES_MAX> hash{};
    crypto_generichash(hash.data(), hash.size(), Bytes(message),
                       static_cast<unsigned long long>(message.size()), nullptr,
                       0);
    if (crypto_sign_verify_detached(Bytes(sig), hash.data(), hash.size(),
                                    Bytes(pk)) != 0) {
      return std::nullopt;
    }

    // The trusted comment is bound to that signature by the global one.
    const auto bound = sig + trusted;
    if (crypto_sign_verify_detached(
            Bytes(global), Bytes(bound),
            static_cast<unsigned long long>(bound.size()), Bytes(pk)) != 0) {
      return std::nullopt;
    }
    return QString::fromUtf8(trusted);
  }
  return std::nullopt;
}
