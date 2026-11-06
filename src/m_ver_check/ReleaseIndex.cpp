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
#include <QTimeZone>

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
  return f;
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
