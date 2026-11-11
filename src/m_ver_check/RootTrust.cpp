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
#include <QRegularExpression>
#include <QTimeZone>

#include "Minisign.h"

auto ReadCount(const QJsonValue& value) -> qint64 {
  if (!value.isDouble() ||
      value.toDouble() != static_cast<double>(value.toInteger())) {
    return 0;
  }
  return std::max<qint64>(0, value.toInteger());
}

auto ReadUtcTime(const QJsonValue& value) -> QDateTime {
  static const QRegularExpression kShape(QStringLiteral(
      R"(\A[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z\z)"));
  const auto text = value.toString();
  if (!kShape.match(text).hasMatch()) return {};
  return QDateTime::fromString(text, Qt::ISODate).toUTC();
}

auto TrustedComment(const QString& product, const QString& what, qint64 n)
    -> QString {
  return QStringLiteral("product=%1 %2=%3").arg(product, what).arg(n);
}

namespace {

auto Fail(QString* why, const QString& what) -> std::nullopt_t {
  if (why != nullptr) *why = what;
  return std::nullopt;
}

/// A key id as the trust files spell it.
auto IsKeyId(const QString& id) -> bool {
  static const QRegularExpression kKeyId(QStringLiteral(R"(\A[0-9A-F]{16}\z)"));
  return kKeyId.match(id).hasMatch();
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
  version = ReadCount(obj.value("version"));
  expires = ReadUtcTime(obj.value("expires"));
  const auto listed = obj.value("keys").toObject();
  for (auto it = listed.begin(); it != listed.end(); ++it) {
    if (!IsKeyId(it.key()) ||
        MinisignPublicKeyId(it.value().toString()) != it.key()) {
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

/// @p ids of a role, each at most once: one key listed twice would count
/// twice toward its threshold.
auto Distinct(const QStringList& ids) -> bool {
  auto once = ids;
  return once.removeDuplicates() == 0;
}

/// Which of @p ids (keys of @p keys) signed @p file with @p comment.
auto Signers(const SignedFile& file, const QHash<QString, QString>& keys,
             const QStringList& ids, const QString& comment) -> QStringList {
  QStringList signers;
  for (const auto& id : ids) {
    const auto sig = file.signatures.value(id);
    if (sig.isEmpty() || MinisignSignatureKeyId(sig) != id) continue;
    if (MinisignVerify({keys.value(id)}, file.data, sig) == comment) {
      signers << id;
    }
  }
  return signers;
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
  root.threshold = static_cast<int>(std::min<qint64>(
      ReadCount(roles.value("root").toObject().value("threshold")),
      kMaxRootKeys + 1));
  root.index_list_keyids = Ids(roles.value("index_list"));
  for (const auto& id : root.root_keyids + root.index_list_keyids) {
    if (!root.keys.contains(id)) {
      return Fail(why, QStringLiteral("a role names key %1, not listed")
                           .arg(id));
    }
  }
  // As trust.py reads a root: distinct keys in each role, at least one, a
  // threshold they can meet, and G alone signing the list.
  if (!Distinct(root.root_keyids) || !Distinct(root.index_list_keyids) ||
      root.root_keyids.size() > kMaxRootKeys || root.threshold < 1 ||
      root.threshold > root.root_keyids.size() ||
      root.index_list_keyids.isEmpty() ||
      ReadCount(roles.value("index_list").toObject().value("threshold")) != 1) {
    return Fail(why, QStringLiteral("root %1 has no usable roles")
                         .arg(root.version));
  }
  for (const auto& id : root.index_list_keyids) {
    if (root.root_keyids.contains(id)) {
      return Fail(why, QStringLiteral("the index-keys list's key is a root "
                                      "key"));
    }
  }
  const auto comment =
      TrustedComment(product, QStringLiteral("root"), root.version);
  root.signed_by = Signers(file, root.keys, root.root_keyids, comment);
  if (root.signed_by.size() < root.threshold) {
    return Fail(why, QStringLiteral("root %1: too few of its own keys signed "
                                    "it")
                         .arg(root.version));
  }
  // What the release host checks before it publishes: shown, never acted on,
  // but there, as the host requires it.
  const auto policy = obj.value("host_policy").toObject();
  root.repository = policy.value("repository").toString();
  root.workflow = policy.value("workflow").toString();
  if (root.repository.isEmpty() || root.workflow.isEmpty()) {
    return Fail(why,
                QStringLiteral("root %1 has no host_policy").arg(root.version));
  }
  root.authenticode_subject = policy.value("authenticode_subject").toString();
  for (const auto& fpr :
       policy.value("stable_pgp").toObject().value("fingerprints").toArray()) {
    root.pgp_fingerprints << fpr.toString();
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
  const auto comment =
      TrustedComment(product, QStringLiteral("root"), next->version);
  if (Signers(file, trusted.keys, trusted.root_keyids, comment).size() <
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
      TrustedComment(product, QStringLiteral("index-keys"), keys.version);
  if (Signers(by_g, root.keys, root.index_list_keyids, comment).isEmpty()) {
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
  return {ReadCount(obj.value("root_version")),
          ReadCount(obj.value("index_keys_version"))};
}

auto NamedRootKeys(const QByteArray& root) -> QStringList {
  QStringList ids;
  for (const auto& id : Ids(QJsonDocument::fromJson(root)
                                .object()
                                .value("roles")
                                .toObject()
                                .value("root"))) {
    if (ids.size() >= kMaxRootKeys) break;
    if (IsKeyId(id) && !ids.contains(id)) ids << id;
  }
  return ids;
}
