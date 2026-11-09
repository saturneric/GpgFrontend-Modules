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

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimeZone>
#include <QUrl>
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

/// A build an index entry names: its version, and its descriptor pinned by
/// hash. False, and nothing taken, unless all three are sound.
auto ReadBuild(const QJsonValue& value, QString& version, QString& descriptor,
               QString& sha256) -> bool {
  const auto entry = value.toObject();
  const auto v = entry.value("version").toString();
  const auto path = entry.value("descriptor").toString();
  const auto digest = entry.value("sha256").toString();
  if (!SemVer::Parse(v) || !IsSafeRelativePath(path) || !IsSha256(digest)) {
    return false;
  }
  version = v;
  descriptor = path;
  sha256 = digest;
  return true;
}

}  // namespace

auto IsSha256(const QString& hex) -> bool {
  static const QRegularExpression kSha256(QStringLiteral("^[0-9a-f]{64}\\z"));
  return kSha256.match(hex).hasMatch();
}

auto Sha256Hex(const QByteArray& data) -> QString {
  return QString::fromLatin1(
      QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

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
  if (!f.generated_at.isValid() || !f.expires.isValid()) return std::nullopt;
  if (f.generated_at.secsTo(f.expires) > kMaxIndexLifetimeSeconds) {
    return std::nullopt;
  }
  if (f.expires.secsTo(now) > kExpirySkewSeconds) return std::nullopt;

  const auto nightly = obj.value("channels").toObject().value("nightly");
  if (nightly.isString()) {
    if (!SemVer::Parse(nightly.toString())) return std::nullopt;
    f.nightly_version = nightly.toString();
  } else if (!nightly.isNull() && !nightly.isUndefined()) {
    return std::nullopt;
  }

  // The stable channel is each series' `latest` (below); channels.stable
  // is for people reading the index. It must still be what it says it is.
  const auto stable = obj.value("channels").toObject().value("stable");
  if (stable.isString() ? !SemVer::Parse(stable.toString())
                        : !stable.isNull() && !stable.isUndefined()) {
    return std::nullopt;
  }

  for (const auto& w : obj.value("withdrawn").toArray()) {
    f.withdrawn << w.toObject().value("version").toString();
  }

  // An index from before each series named its own beta names one for all:
  // as channels.beta, and (to fetch its files) as the top-level entry.
  QString old_beta;
  QString old_beta_descriptor;
  QString old_beta_sha256;
  const auto channel_beta = obj.value("channels").toObject().value("beta");
  if (channel_beta.isString() && SemVer::Parse(channel_beta.toString())) {
    old_beta = channel_beta.toString();
    QString listed;
    if (!ReadBuild(obj.value("beta"), listed, old_beta_descriptor,
                   old_beta_sha256) ||
        listed != old_beta) {
      old_beta_descriptor.clear();
      old_beta_sha256.clear();
    }
  }

  // The series and the releases: what a release build is offered. A
  // malformed entry costs that entry only, never the rest of the index; a
  // beta that cannot be read is no beta (it is an opt-in extra).
  const auto series = obj.value("series").toObject();
  for (auto it = series.begin(); it != series.end(); ++it) {
    const auto s = it.value().toObject();
    SeriesFacts facts;
    facts.status = s.value("status").toString();
    const auto latest = s.value("latest").toString();
    if (!latest.isEmpty() && SemVer::Parse(latest)) facts.latest = latest;
    facts.successor = s.value("successor").toString();
    facts.eol = s.value("eol").toString();
    const auto platforms = s.value("platforms").toObject();
    for (auto p = platforms.begin(); p != platforms.end(); ++p) {
      facts.platforms.insert(p.key(), p.value().isDouble()
                                          ? QString::number(p.value().toInt())
                                          : p.value().toString());
    }
    if (s.contains("beta")) {
      QString version;
      QString descriptor;
      QString sha256;
      if (ReadBuild(s.value("beta"), version, descriptor, sha256) &&
          SoftwareVersion::VersionSeries(version) == it.key()) {
        facts.beta = version;
        facts.beta_descriptor = descriptor;
        facts.beta_descriptor_sha256 = sha256;
      }
    } else if (!old_beta.isEmpty() &&
               SoftwareVersion::VersionSeries(old_beta) == it.key()) {
      facts.beta = old_beta;
      facts.beta_descriptor = old_beta_descriptor;
      facts.beta_descriptor_sha256 = old_beta_sha256;
    }
    f.series.insert(it.key(), facts);
  }
  const auto releases = obj.value("releases").toObject();
  for (auto it = releases.begin(); it != releases.end(); ++it) {
    const auto r = it.value().toObject();
    ReleaseEntry entry;
    entry.descriptor = r.value("descriptor").toString();
    entry.descriptor_sha256 = r.value("sha256").toString();
    if (!SemVer::Parse(it.key()) || !IsSafeRelativePath(entry.descriptor) ||
        !IsSha256(entry.descriptor_sha256)) {
      continue;
    }
    const auto notes = r.value("notes").toString();
    const auto notes_sha256 = r.value("notes_sha256").toString();
    if (IsSafeRelativePath(notes) && IsSha256(notes_sha256)) {
      entry.notes = notes;
      entry.notes_sha256 = notes_sha256;
    }
    f.releases.insert(it.key(), entry);
  }
  // Where the nightly's files are listed. Optional: an index without it still
  // says whether there is an update, only not which file to fetch. It counts
  // only for the version the channel names.
  QString listed;
  if (!f.nightly_version.isEmpty() &&
      ReadBuild(obj.value("nightly"), listed, f.nightly_descriptor,
                f.nightly_descriptor_sha256) &&
      listed != f.nightly_version) {
    f.nightly_descriptor.clear();
    f.nightly_descriptor_sha256.clear();
  }
  return f;
}

auto IsSafeRelativePath(const QString& path) -> bool {
  // \z, not $: "$" also matches before a trailing newline.
  static const QRegularExpression kSegment(
      QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._+-]*\\z"));
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

auto SelectArtifact(const QByteArray& descriptor, const Platform& platform)
    -> DownloadChoice {
  // Install kind -> the artifact kind and flavor that replace it. An empty
  // kind matches any kind of the installed flavor.
  struct Want {
    QString kind;
    QString flavor;
  };
  static const QHash<QString, Want> kWants{
      {QStringLiteral("appimage"),
       {QStringLiteral("appimage"), QStringLiteral("installed")}},
      {QStringLiteral("appimage-portable"),
       {QStringLiteral("appimage"), QStringLiteral("portable")}},
      {QStringLiteral("msi"),
       {QStringLiteral("msi"), QStringLiteral("installed")}},
      {QStringLiteral("portable-zip"),
       {QStringLiteral("zip"), QStringLiteral("portable")}},
      {QStringLiteral("dmg"),
       {QStringLiteral("dmg"), QStringLiteral("installed")}},
      {QStringLiteral("source"), {QString(), QStringLiteral("installed")}},
      {QString(), {QString(), QStringLiteral("installed")}},
  };
  // msix, mas, flatpak, distro, android: their own updater, not a file.
  const auto want = kWants.constFind(platform.install_kind);
  if (want == kWants.constEnd()) return {};

  const auto doc = QJsonDocument::fromJson(descriptor);
  if (!doc.isObject()) return {};

  const auto running = QVersionNumber::fromString(platform.os_version);
  DownloadChoice best;
  QVersionNumber best_min;
  for (const auto& value : doc.object().value("artifacts").toArray()) {
    const auto a = value.toObject();
    if (a.value("os").toString() != platform.os ||
        NormalizeArch(a.value("arch").toString()) !=
            NormalizeArch(platform.arch) ||
        a.value("flavor").toString() != want->flavor ||
        (!want->kind.isEmpty() && a.value("kind").toString() != want->kind)) {
      continue;
    }
    const auto path = a.value("path").toString();
    if (!IsSafeRelativePath(path)) continue;

    const auto min = QVersionNumber::fromString(
        a.value("min_os").toObject().value(platform.os).toString());
    if (!min.isNull() && (running.isNull() || running < min)) continue;
    if (best.path.isEmpty() || min > best_min) {
      const auto sha256 = a.value("sha256").toString();
      const auto size = a.value("size").toInteger();
      best = {path, IsSha256(sha256) ? sha256 : QString(), size > 0 ? size : 0};
      best_min = min;
    }
  }
  return best;
}

auto ChooseUpdate(const IndexFacts& facts, const QString& current, bool nightly,
                  bool follow_beta) -> ChannelChoice {
  ChannelChoice choice;
  choice.list.outcome = ListOutcome::kNoMatch;
  const auto found = [&](const QString& version) {
    choice.list.outcome = ListOutcome::kFound;
    choice.list.latest.version = version;
    choice.list.latest.published_at = facts.generated_at.toString(Qt::ISODate);
  };

  // The series this build is of: its beta, and its releases.
  const auto series =
      facts.series.value(SoftwareVersion::VersionSeries(current));
  const auto offer_beta = [&] {
    found(series.beta);
    choice.descriptor = series.beta_descriptor;
    choice.descriptor_sha256 = series.beta_descriptor_sha256;
    return choice;
  };

  if (nightly) {
    // A release branch's nightly (X.Y.Z-alpha.N, Z > 0) is a fix build of
    // its own series: it follows that branch's builds, its series' beta, and
    // never main's channel, which is another series' nightly.
    const auto v = SemVer::Parse(current);
    if (v && v->patch > 0) {
      if (series.beta.isEmpty()) {
        choice.other_series = true;
        return choice;
      }
      return offer_beta();
    }
    if (facts.nightly_version.isEmpty()) return choice;
    found(facts.nightly_version);
    choice.descriptor = facts.nightly_descriptor;
    choice.descriptor_sha256 = facts.nightly_descriptor_sha256;
    return choice;
  }

  // Asked for: its series' beta, when newer than what this build runs and
  // than the release it would be offered otherwise.
  if (follow_beta && !series.beta.isEmpty() &&
      SemVerCompare(series.beta, current) > 0 &&
      (series.latest.isEmpty() ||
       SemVerCompare(series.beta, series.latest) > 0)) {
    return offer_beta();
  }
  if (series.latest.isEmpty()) return choice;
  found(series.latest);
  const auto entry = facts.releases.value(series.latest);
  choice.descriptor = entry.descriptor;
  choice.descriptor_sha256 = entry.descriptor_sha256;
  choice.notes = entry.notes;
  choice.notes_sha256 = entry.notes_sha256;
  return choice;
}

auto MeetsPlatform(const QHash<QString, QString>& required,
                   const Platform& platform) -> bool {
  const auto at_least = [](const QString& have, const QString& need) {
    const auto h = QVersionNumber::fromString(have);
    return !h.isNull() && h >= QVersionNumber::fromString(need);
  };
  if (platform.os == u"windows") {
    return !required.contains("windows") ||
           at_least(platform.os_version, required.value("windows"));
  }
  if (platform.os == u"macos") {
    return !required.contains("macos") ||
           at_least(platform.os_version, required.value("macos"));
  }
  if (platform.os == u"linux") {
    return !required.contains("linux_glibc") ||
           at_least(platform.glibc, required.value("linux_glibc"));
  }
  return false;
}

auto NoticeFor(const IndexFacts& facts, const QString& current,
               const Platform& platform) -> SeriesNotice {
  SeriesNotice notice;
  const auto key = SoftwareVersion::VersionSeries(current);
  const auto series = facts.series.value(key);
  if (series.status != u"legacy" && series.status != u"eol") return notice;
  notice.series = key;
  notice.status = series.status;
  notice.eol = series.eol;
  notice.successor = series.successor;
  const auto successor = facts.series.value(series.successor);
  notice.successor_latest = successor.latest;
  notice.successor_runs_here = !successor.latest.isEmpty() &&
                               MeetsPlatform(successor.platforms, platform);
  return notice;
}

auto ReleaseFact(const IndexFacts& facts, const QString& current)
    -> RemoteFact {
  if (WithdrawnFact(facts, current) == RemoteFact::kNotFound) {
    return RemoteFact::kNotFound;
  }
  for (auto it = facts.releases.begin(); it != facts.releases.end(); ++it) {
    if (SemVerCompare(it.key(), current) == 0) return RemoteFact::kConfirmed;
  }
  return RemoteFact::kUnknown;
}

auto IsAllowedNotesLink(const QUrl& url, const QStringList& hosts) -> bool {
  if (!url.isValid() || url.scheme() != u"https" || url.host().isEmpty() ||
      !url.userInfo().isEmpty()) {
    return false;
  }
  const auto host = url.host().toLower();
  const auto path = url.path();
  for (const auto& entry : hosts) {
    const auto slash = entry.indexOf(u'/');
    const auto want = (slash < 0 ? entry : entry.left(slash)).toLower();
    const bool host_ok =
        want.startsWith(u"*.")
            ? host.endsWith(want.mid(1)) && host.size() > want.size() - 1
            : host == want;
    if (!host_ok) continue;
    if (slash < 0) return true;
    // "github.com/owner/repo": that repository's pages only, whole segments.
    const auto prefix = entry.mid(slash);
    if (path.compare(prefix, Qt::CaseInsensitive) == 0 ||
        path.startsWith(prefix + u'/', Qt::CaseInsensitive)) {
      return true;
    }
  }
  return false;
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
