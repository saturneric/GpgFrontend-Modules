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

#include "SoftwareVersion.h"

#include <QJsonObject>
#include <QString>
#include <array>
#include <utility>

namespace {

// Names, not enum values, on disk: a reordered enum must not flip a cached
// "not found" into "confirmed".
constexpr std::array<std::pair<RemoteFact, const char*>, 3> kFactNames = {{
    {RemoteFact::kUnknown, "unknown"},
    {RemoteFact::kConfirmed, "confirmed"},
    {RemoteFact::kNotFound, "not_found"},
}};

constexpr std::array<std::pair<ListOutcome, const char*>, 3> kListNames = {{
    {ListOutcome::kFailed, "failed"},
    {ListOutcome::kNoMatch, "no_match"},
    {ListOutcome::kFound, "found"},
}};

template <typename Enum, std::size_t N>
auto NameOf(const std::array<std::pair<Enum, const char*>, N>& names,
            Enum value) -> QString {
  for (const auto& [e, name] : names) {
    if (e == value) return QString::fromLatin1(name);
  }
  return QString::fromLatin1(names.front().second);
}

template <typename Enum, std::size_t N>
auto ValueOf(const std::array<std::pair<Enum, const char*>, N>& names,
             const QString& name) -> Enum {
  for (const auto& [e, n] : names) {
    if (name == QLatin1String(n)) return e;
  }
  return names.front().first;
}

}  // namespace

auto SoftwareVersion::SameBuild(const QString& version,
                                const QString& commit) const -> bool {
  return current_version == version && local_commit_hash == commit;
}

auto SoftwareVersion::ToJson() const -> QJsonObject {
  QJsonObject latest;
  latest["version"] = list.latest.version;
  latest["published_at"] = list.latest.published_at;
  latest["notes"] = list.latest.notes;
  latest["html_url"] = list.latest.html_url;

  QJsonObject obj;
  obj["schema"] = kSchema;
  obj["current_version"] = current_version;
  obj["local_commit_hash"] = local_commit_hash;
  obj["list"] = NameOf(kListNames, list.outcome);
  obj["latest"] = latest;
  obj["tag_fact"] = NameOf(kFactNames, tag_fact);
  obj["commit_fact"] = NameOf(kFactNames, commit_fact);
  obj["complete"] = complete;
  obj["checked_at"] = checked_at.toSecsSinceEpoch();
  return obj;
}

auto SoftwareVersion::FromJson(const QJsonObject& obj)
    -> std::optional<SoftwareVersion> {
  if (obj.value("schema").toInt() != kSchema) return std::nullopt;
  if (!obj.value("checked_at").isDouble()) return std::nullopt;

  SoftwareVersion sv;
  sv.current_version = obj.value("current_version").toString();
  sv.local_commit_hash = obj.value("local_commit_hash").toString();
  sv.list.outcome = ValueOf(kListNames, obj.value("list").toString());

  const auto latest = obj.value("latest").toObject();
  sv.list.latest.version = latest.value("version").toString();
  sv.list.latest.published_at = latest.value("published_at").toString();
  sv.list.latest.notes = latest.value("notes").toString();
  sv.list.latest.html_url = latest.value("html_url").toString();

  sv.tag_fact = ValueOf(kFactNames, obj.value("tag_fact").toString());
  sv.commit_fact = ValueOf(kFactNames, obj.value("commit_fact").toString());
  sv.complete = obj.value("complete").toBool();
  sv.checked_at =
      QDateTime::fromSecsSinceEpoch(obj.value("checked_at").toInteger());

  // A "found" list without a version is not something ToJson() writes.
  if (sv.list.outcome == ListOutcome::kFound &&
      sv.list.latest.version.isEmpty()) {
    return std::nullopt;
  }
  return sv;
}

auto SoftwareVersion::VersionSeries(const QString& version) -> QString {
  if (version.isEmpty()) return {};

  auto real_version = version.startsWith('v') || version.startsWith('V')
                          ? version.mid(1)
                          : version;

  const auto parts = real_version.split('.');
  if (parts.size() < 2) return {};

  bool major_ok = false;
  bool minor_ok = false;
  parts[0].toInt(&major_ok);
  parts[1].toInt(&minor_ok);
  if (!major_ok || !minor_ok) return {};

  return parts[0] + "." + parts[1];
}

auto SoftwareVersion::SameSeries(const QString& a, const QString& b) -> bool {
  auto series_a = VersionSeries(a);
  return !series_a.isEmpty() && series_a == VersionSeries(b);
}
