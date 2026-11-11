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

#include "RootTrust.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>

#include "Minisign.h"
#include "ReleaseIndex.h"

namespace {

auto Fail(QString* why, const QString& what) -> std::nullopt_t {
  if (why != nullptr) *why = what;
  return std::nullopt;
}

auto Integer(const QJsonValue& v) -> qint64 {
  // A whole positive number, or 0.
  if (!v.isDouble() || v.toDouble() != static_cast<double>(v.toInteger())) {
    return 0;
  }
  return std::max<qint64>(0, v.toInteger());
}

auto Time(const QJsonValue& v) -> QDateTime {
  auto t = QDateTime::fromString(v.toString(), Qt::ISODate);
  if (t.isValid()) t.setTimeZone(QTimeZone::UTC);
  return t;
}

/// What a root and a list share: schema, type, product, version, expiry,
/// and keys each listed under its own id.
auto ReadCommon(const QByteArray& data, const QString& type,
                const QString& product, qint64& version, QDateTime& expires,
                QHash<QString, QString>& keys, QJsonObject& obj,
                QString* why) -> bool {
  const auto doc = QJsonDocument::fromJson(data);
  obj = doc.object();
  if (!doc.isObject() || obj.value("schema").toInt() != 1 ||
      obj.value("type").toString() != type ||
      obj.value("product").toString() != product || product.isEmpty()) {
    Fail(why, QStringLiteral("not a %1 of %2").arg(type, product));
    return false;
  }
  version = Integer(obj.value("version"));
  expires = Time(obj.value("expires"));
  const auto listed = obj.value("keys").toObject();
  for (auto it = listed.begin(); it != listed.end(); ++it) {
    if (MinisignPublicKeyId(it.value().toString()) != it.key()) {
      Fail(why, QStringLiteral("key %1 is not the key listed under it")
                    .arg(it.key()));
      return false;
    }
    keys.insert(it.key(), it.value().toString());
  }
  if (version < 1 || !expires.isValid() || keys.isEmpty()) {
    Fail(why, QStringLiteral("a %1 without version, expiry or keys").arg(type));
    return false;
  }
  return true;
}

auto Ids(const QJsonValue& role) -> QStringList {
  QStringList ids;
  for (const auto& id : role.toObject().value("keyids").toArray()) {
    ids << id.toString();
  }
  return ids;
}

/// How many of @p ids (keys of @p keys) signed @p file with @p comment.
auto Signers(const SignedFile& file, const QHash<QString, QString>& keys,
             const QStringList& ids, const QString& comment) -> int {
  int count = 0;
  for (const auto& id : ids) {
    const auto sig = file.signatures.value(id);
    if (sig.isEmpty() || MinisignSignatureKeyId(sig) != id) continue;
    if (MinisignVerify({keys.value(id)}, file.data, sig) == comment) ++count;
  }
  return count;
}

auto Comment(const QString& product, const QString& what, qint64 n)
    -> QString {
  return QStringLiteral("product=%1 %2=%3").arg(product, what).arg(n);
}

/// The root in @p file, read and signed by enough of its own keys.
auto ReadRoot(const SignedFile& file, const QString& product, QString* why)
    -> std::optional<TrustRoot> {
  TrustRoot root;
  QJsonObject obj;
  if (!ReadCommon(file.data, QStringLiteral("root"), product, root.version,
                  root.expires, root.keys, obj, why)) {
    return std::nullopt;
  }
  const auto roles = obj.value("roles").toObject();
  root.root_keyids = Ids(roles.value("root"));
  root.threshold = roles.value("root").toObject().value("threshold").toInt();
  root.index_list_keyids = Ids(roles.value("index_list"));
  for (const auto& id : root.root_keyids + root.index_list_keyids) {
    if (!root.keys.contains(id)) {
      return Fail(why, QStringLiteral("a role names key %1, not listed")
                           .arg(id));
    }
  }
  if (root.threshold < 1 || root.threshold > root.root_keyids.size() ||
      root.index_list_keyids.isEmpty()) {
    return Fail(why, QStringLiteral("root %1 has no usable roles")
                         .arg(root.version));
  }
  for (const auto& id : root.index_list_keyids) {
    if (root.root_keyids.contains(id)) {
      return Fail(why, QStringLiteral("the index-keys list's key is a root "
                                      "key"));
    }
  }
  const auto comment = Comment(product, QStringLiteral("root"), root.version);
  if (Signers(file, root.keys, root.root_keyids, comment) < root.threshold) {
    return Fail(why, QStringLiteral("root %1: too few of its own keys signed "
                                    "it")
                         .arg(root.version));
  }
  root.file = file;
  return root;
}

}  // namespace

auto ParseRoot(const SignedFile& file, const QString& product, QString* why)
    -> std::optional<TrustRoot> {
  return ReadRoot(file, product, why);
}

auto AcceptNextRoot(const TrustRoot& trusted, const SignedFile& file,
                    const QString& product, QString* why)
    -> std::optional<TrustRoot> {
  auto next = ReadRoot(file, product, why);
  if (!next) return std::nullopt;
  if (next->version != trusted.version + 1) {
    return Fail(why, QStringLiteral("root %1 does not follow root %2")
                         .arg(next->version)
                         .arg(trusted.version));
  }
  const auto comment = Comment(product, QStringLiteral("root"), next->version);
  if (Signers(file, trusted.keys, trusted.root_keyids, comment) <
      trusted.threshold) {
    return Fail(why, QStringLiteral("root %1: too few of root %2's keys "
                                    "signed it")
                         .arg(next->version)
                         .arg(trusted.version));
  }
  return next;
}

auto ParseIndexKeys(const TrustRoot& root, const SignedFile& file,
                    const QString& product, qint64 floor, QString* why)
    -> std::optional<IndexKeys> {
  IndexKeys keys;
  QJsonObject obj;
  if (!ReadCommon(file.data, QStringLiteral("index-keys"), product,
                  keys.version, keys.expires, keys.keys, obj, why)) {
    return std::nullopt;
  }
  if (keys.version < floor) {
    return Fail(why, QStringLiteral("index-keys %1 is older than %2, taken "
                                    "before")
                         .arg(keys.version)
                         .arg(floor));
  }
  // One signature file, whoever it claims: G's, or nobody's.
  SignedFile by_g{file.data, {}};
  for (const auto& sig : file.signatures) {
    by_g.signatures.insert(MinisignSignatureKeyId(sig), sig);
  }
  const auto comment =
      Comment(product, QStringLiteral("index-keys"), keys.version);
  if (Signers(by_g, root.keys, root.index_list_keyids, comment) < 1) {
    return Fail(why, QStringLiteral("index-keys %1 is not signed by root %2's "
                                    "index_list key")
                         .arg(keys.version)
                         .arg(root.version));
  }
  keys.file = file;
  return keys;
}

auto IsTrustExpired(const QDateTime& expires, const QDateTime& now) -> bool {
  return expires.secsTo(now) > kExpirySkewSeconds;
}

auto ReadTrustHints(const QByteArray& index) -> TrustHints {
  const auto obj = QJsonDocument::fromJson(index).object();
  return {Integer(obj.value("root_version")),
          Integer(obj.value("index_keys_version"))};
}

auto NamedRootKeys(const QByteArray& root) -> QStringList {
  return Ids(QJsonDocument::fromJson(root)
                 .object()
                 .value("roles")
                 .toObject()
                 .value("root"));
}
