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

#include "ReleaseCheck.h"

#include <GFSDKApp.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {

auto Compare(const QString& a, const QString& b) -> int {
  return GFCompareSoftwareVersion(a.toUtf8().constData(),
                                  b.toUtf8().constData());
}

auto ParseObject(const QByteArray& body) -> std::optional<QJsonObject> {
  const auto doc = QJsonDocument::fromJson(body);
  if (!doc.isObject()) return std::nullopt;
  return doc.object();
}

}  // namespace

auto ExtractVersionFromRawTag(const QString& raw_tag) -> QString {
  static const QRegularExpression kVersionRe(
      R"(^[vV](\d+\.)?(\d+\.)?(\*|\d+))");
  auto match = kVersionRe.match(raw_tag);
  return match.hasMatch() ? match.captured(0) : QString{};
}

auto IsPrereleaseVersion(const QString& version) -> bool {
  static const QRegularExpression kSuffixRe(R"(^[vV]?\d+(\.\d+)*[-+~_a-zA-Z])");
  return kSuffixRe.match(version).hasMatch();
}

auto ParseReleaseList(int http_status, const QByteArray& body,
                      const QString& current) -> ListResult {
  ListResult result;
  if (http_status != 200) return result;

  const auto doc = QJsonDocument::fromJson(body);
  if (!doc.isArray()) return result;

  const bool accept_prerelease = IsPrereleaseVersion(current);
  result.outcome = ListOutcome::kNoMatch;

  for (const auto& value : doc.array()) {
    if (!value.isObject()) continue;
    const auto release = value.toObject();

    if (release["draft"].toBool()) continue;
    if (release["prerelease"].toBool() && !accept_prerelease) continue;

    const auto version =
        ExtractVersionFromRawTag(release["tag_name"].toString());
    if (version.isEmpty()) continue;
    if (!SoftwareVersion::SameSeries(version, current)) continue;

    if (result.outcome == ListOutcome::kFound &&
        Compare(version, result.latest.version) <= 0) {
      continue;
    }

    result.outcome = ListOutcome::kFound;
    result.latest.version = version;
    result.latest.published_at = release["published_at"].toString();
    result.latest.notes = release["body"].toString();
    result.latest.html_url = release["html_url"].toString();
  }

  return result;
}

auto ClassifyLookup(int http_status, bool body_valid) -> RemoteFact {
  if (http_status == 200) {
    return body_valid ? RemoteFact::kConfirmed : RemoteFact::kUnknown;
  }
  if (http_status == 404) return RemoteFact::kNotFound;
  return RemoteFact::kUnknown;
}

auto ParseTagLookup(int http_status, const QByteArray& body) -> RemoteFact {
  const auto obj = http_status == 200 ? ParseObject(body) : std::nullopt;
  const bool valid = obj && !obj->value("tag_name").toString().isEmpty();
  return ClassifyLookup(http_status, valid);
}

auto ParseCommitLookup(int http_status, const QByteArray& body,
                       const QString& local_commit) -> RemoteFact {
  const auto commit = local_commit.trimmed();
  if (commit.isEmpty()) return RemoteFact::kUnknown;

  if (http_status == 422) {
    const auto obj = ParseObject(body);
    const bool no_commit =
        obj && obj->value("message").toString().startsWith("No commit found");
    return no_commit ? RemoteFact::kNotFound : RemoteFact::kUnknown;
  }

  // GitHub resolves a short SHA to the full one, so the answer must start
  // with ours; any other SHA answers a different question.
  const auto obj = http_status == 200 ? ParseObject(body) : std::nullopt;
  const bool valid = obj && obj->value("sha").toString().startsWith(
                                commit, Qt::CaseInsensitive);
  return ClassifyLookup(http_status, valid);
}

auto IsUsable(const SoftwareVersion& r) -> bool {
  return r.list.outcome != ListOutcome::kFailed;
}

auto IsComplete(const SoftwareVersion& fresh) -> bool {
  return IsUsable(fresh) && fresh.tag_fact != RemoteFact::kUnknown &&
         fresh.commit_fact != RemoteFact::kUnknown;
}

auto IsAuthoritative(const SoftwareVersion& r) -> bool {
  return IsUsable(r) && r.complete;
}

auto Merge(const std::optional<SoftwareVersion>& prev,
           const SoftwareVersion& fresh) -> std::optional<SoftwareVersion> {
  if (!IsUsable(fresh)) return prev;

  auto merged = fresh;
  if (prev && prev->SameBuild(fresh.current_version, fresh.local_commit_hash)) {
    if (merged.tag_fact == RemoteFact::kUnknown) {
      merged.tag_fact = prev->tag_fact;
    }
    if (merged.commit_fact == RemoteFact::kUnknown) {
      merged.commit_fact = prev->commit_fact;
    }
  }
  return merged;
}

auto Decide(const SoftwareVersion& r) -> Verdict {
  if (!IsUsable(r)) return Verdict::kUnknown;
  if (r.list.outcome == ListOutcome::kFound &&
      Compare(r.current_version, r.list.latest.version) < 0) {
    return Verdict::kUpdateAvailable;
  }
  if (r.tag_fact == RemoteFact::kNotFound) {
    return Verdict::kWithdrawnOrUnreleased;
  }
  if (r.commit_fact == RemoteFact::kNotFound) return Verdict::kUnofficialBuild;
  return Verdict::kUpToDate;
}
