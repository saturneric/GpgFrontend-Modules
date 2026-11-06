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

#include "ReleaseIndex.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimeZone>
#include <QVersionNumber>

#include "SemVer.h"

auto IsNightlyBuild(const QString& version) -> bool {
  const auto v = SemVer::Parse(version);
  if (!v || !v->IsPrerelease()) return false;
  return v->prerelease.first() == QStringLiteral("alpha") ||
         v->prerelease.contains(QStringLiteral("dev"));
}

namespace {

auto ParseTime(const QJsonValue& value) -> QDateTime {
  auto t = QDateTime::fromString(value.toString(), Qt::ISODate);
  if (t.isValid()) t.setTimeZone(QTimeZone::UTC);
  return t;
}

}  // namespace

auto ParseIndex(const QByteArray& json, const QString& trusted_comment,
                const QString& product, const QDateTime& now)
    -> std::optional<IndexFacts> {
  const auto doc = QJsonDocument::fromJson(json);
  if (!doc.isObject()) return std::nullopt;
  const auto obj = doc.object();

  if (obj.value("schema").toInt() != 1) return std::nullopt;
  if (product.isEmpty() || obj.value("product").toString() != product) {
    return std::nullopt;
  }

  IndexFacts f;
  if (!obj.value("serial").isDouble()) return std::nullopt;
  f.serial = obj.value("serial").toInteger();
  if (f.serial <= 0) return std::nullopt;
  // The comment is what the signature binds; the document must agree.
  if (trusted_comment !=
      QStringLiteral("product=%1 serial=%2").arg(product).arg(f.serial)) {
    return std::nullopt;
  }

  f.generated_at = ParseTime(obj.value("generated_at"));
  f.expires = ParseTime(obj.value("expires"));
  if (!f.expires.isValid()) return std::nullopt;
  if (f.expires.secsTo(now) > kExpirySkewSeconds) return std::nullopt;

  const auto nightly = obj.value("channels").toObject().value("nightly");
  if (nightly.isString()) {
    if (!SemVer::Parse(nightly.toString())) return std::nullopt;
    f.nightly_version = nightly.toString();
  } else if (!nightly.isNull() && !nightly.isUndefined()) {
    return std::nullopt;
  }

  for (const auto& w : obj.value("withdrawn").toArray()) {
    f.withdrawn << w.toObject().value("version").toString();
  }

  // Where the nightly's files are listed. Optional: an index without it still
  // says whether there is an update, only not which file to fetch.
  static const QRegularExpression kSha256(QStringLiteral("^[0-9a-f]{64}$"));
  const auto entry = obj.value("nightly").toObject();
  const auto descriptor = entry.value("descriptor").toString();
  const auto sha256 = entry.value("sha256").toString();
  if (!f.nightly_version.isEmpty() &&
      entry.value("version").toString() == f.nightly_version &&
      IsSafeRelativePath(descriptor) && kSha256.match(sha256).hasMatch()) {
    f.nightly_descriptor = descriptor;
    f.nightly_descriptor_sha256 = sha256;
  }
  return f;
}

auto IsSafeRelativePath(const QString& path) -> bool {
  static const QRegularExpression kSegment(
      QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._+-]*$"));
  if (path.isEmpty() || path.contains(u'%') || path.contains(u'\\')) {
    return false;
  }
  for (const auto& part : path.split(u'/')) {
    if (part == u"." || part == u".." || !kSegment.match(part).hasMatch()) {
      return false;
    }
  }
  return true;
}

namespace {

auto NormalizeArch(const QString& arch) -> QString {
  const auto a = arch.toLower();
  if (a == u"amd64" || a == u"x86_64") return QStringLiteral("x86_64");
  if (a == u"aarch64" || a == u"arm64" || a == u"arm64-v8a") {
    return QStringLiteral("arm64");
  }
  return a;
}

}  // namespace

auto SelectDownload(const QByteArray& descriptor, const Platform& platform)
    -> QString {
  const auto doc = QJsonDocument::fromJson(descriptor);
  if (!doc.isObject()) return {};

  const auto running = QVersionNumber::fromString(platform.os_version);
  QString best;
  QVersionNumber best_min;
  for (const auto& value : doc.object().value("artifacts").toArray()) {
    const auto a = value.toObject();
    if (a.value("os").toString() != platform.os ||
        NormalizeArch(a.value("arch").toString()) !=
            NormalizeArch(platform.arch) ||
        a.value("flavor").toString() != u"installed") {
      continue;
    }
    const auto path = a.value("path").toString();
    if (!IsSafeRelativePath(path)) continue;

    const auto min = QVersionNumber::fromString(
        a.value("min_os").toObject().value(platform.os).toString());
    if (!min.isNull() && (running.isNull() || running < min)) continue;
    if (best.isEmpty() || min > best_min) {
      best = path;
      best_min = min;
    }
  }
  return best;
}

auto NightlyList(const IndexFacts& facts) -> ListResult {
  ListResult result;
  if (facts.nightly_version.isEmpty()) {
    result.outcome = ListOutcome::kNoMatch;
    return result;
  }
  result.outcome = ListOutcome::kFound;
  result.latest.version = facts.nightly_version;
  result.latest.published_at = facts.generated_at.toString(Qt::ISODate);
  return result;
}

auto WithdrawnFact(const IndexFacts& facts, const QString& current)
    -> RemoteFact {
  for (const auto& w : facts.withdrawn) {
    if (SemVerCompare(w, current) == 0) return RemoteFact::kNotFound;
  }
  return RemoteFact::kConfirmed;
}

auto ParseCommitMarker(int http_status) -> RemoteFact {
  if (http_status == 200) return RemoteFact::kConfirmed;
  if (http_status == 404) return RemoteFact::kNotFound;
  return RemoteFact::kUnknown;
}
