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

#include "UpdateChecker.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QRegularExpression>

#include "Minisign.h"
#include "ReleaseCheck.h"
#include "SemVer.h"

namespace {

constexpr int kStorageSchema = 2;

}  // namespace

UpdateChecker::UpdateChecker(Build build, Fetcher fetch, Store store,
                             Clock clock, QObject* parent)
    : QObject(parent),
      build_(std::move(build)),
      fetch_(std::move(fetch)),
      store_(std::move(store)),
      clock_(clock ? std::move(clock)
                   : [] { return QDateTime::currentDateTime(); }) {
  nightly_ = build_.nightly >= 0 ? build_.nightly == 1
                                 : IsNightlyBuild(build_.version);
  if (store_.load) state_ = FromStorage(store_.load(), build_);
}

auto UpdateChecker::State() const -> const UpdateSnapshot& { return state_; }

auto UpdateChecker::IsFresh() const -> bool {
  const auto& good = state_.last_good;
  return good && IsAuthoritative(*good) &&
         good->SameBuild(build_.version, build_.commit) &&
         good->follow_beta == followsBeta() &&
         good->checked_at.secsTo(clock_()) < kFreshSeconds;
}

auto UpdateChecker::IsNightly() const -> bool { return nightly_; }

auto UpdateChecker::followsBeta() const -> bool {
  return !nightly_ && build_.follow_beta;
}

void UpdateChecker::SetFollowBeta(bool follow_beta) {
  build_.follow_beta = follow_beta;
}

auto UpdateChecker::Attention() const -> UpdateAttention {
  return AttentionFor(state_, clock_());
}

void UpdateChecker::Acknowledge() {
  if (Attention() == UpdateAttention::kNone) return;
  state_.last_seen_update_version = state_.last_good->list.latest.version;
  state_.last_seen_at = clock_();
  if (store_.save) store_.save(ToStorage(state_));
  emit Changed();
}

void UpdateChecker::Start(CheckMode mode) {
  if (state_.checking) return;
  if (mode == CheckMode::kIfStale && IsFresh()) return;

  fresh_ = SoftwareVersion{};
  fresh_.current_version = build_.version;
  fresh_.local_commit_hash = build_.commit;
  fresh_.nightly = nightly_;
  fresh_.follow_beta = followsBeta();
  accepted_serial_ = 0;
  descriptor_path_.clear();
  descriptor_sha256_.clear();
  notes_path_.clear();
  notes_sha256_.clear();
  startIndex();
}

void UpdateChecker::startIndex() {
  // One finish() at the end of the chain, however it ends.
  pending_ = 1;
  state_.checking = true;
  emit Changed();

  // Not built with a channel (a build of the modules on their own): nothing
  // can be verified, so nothing is claimed.
  if (!build_.channel.IsConfigured()) {
    finish();
    return;
  }
  tryIndexBase(0);
}

void UpdateChecker::tryIndexBase(qsizetype i, bool retried) {
  const auto& channel = build_.channel;
  if (i >= channel.update_bases.size()) {
    finish();  // no base served a good index: the list stays unusable
    return;
  }

  const QString url = channel.update_bases[i] + channel.product +
                      QStringLiteral("/v1/index.json");
  QPointer<UpdateChecker> self(this);
  fetch_(QUrl(url), [self, i, url, retried](int status,
                                            const QByteArray& body) {
    if (self.isNull()) return;
    if (status != 200) {
      self->tryIndexBase(i + 1);
      return;
    }
    self->fetch_(
        QUrl(url + QStringLiteral(".minisig")),
        [self, i, body, retried](int sig_status, const QByteArray& signature) {
          if (self.isNull()) return;
          if (sig_status == 200 && self->acceptIndex(body, signature)) {
            self->askDescriptor();
            return;
          }
          // The server replaces the index and then its signature: read in
          // between, the two disagree. Once more on the same base, then on.
          if (sig_status == 200 && !retried) {
            self->tryIndexBase(i, true);
            return;
          }
          self->tryIndexBase(i + 1);
        });
  });
}

auto UpdateChecker::acceptIndex(const QByteArray& body,
                                const QByteArray& signature) -> bool {
  const auto& channel = build_.channel;
  const auto comment = MinisignVerify(channel.index_keys, body, signature);
  if (!comment) return false;
  const auto facts = ParseIndex(body, *comment, channel.product, clock_());
  if (!facts) return false;
  // An older index than one already accepted is a replay, from a stale
  // mirror or an attacker: it is not believed, and another base is tried.
  if (facts->serial < state_.max_serial) return false;

  const auto choice =
      ChooseUpdate(*facts, build_.version, nightly_, fresh_.follow_beta);
  fresh_.list = choice.list;
  fresh_.other_series = choice.other_series;
  fresh_.tag_fact = nightly_ ? WithdrawnFact(*facts, build_.version)
                             : ReleaseFact(*facts, build_.version);
  if (!nightly_) {
    const auto n = NoticeFor(*facts, build_.version, build_.platform);
    fresh_.notice = {n.series,    n.status,           n.eol,
                     n.successor, n.successor_latest, n.successor_runs_here};
  }
  accepted_serial_ = facts->serial;
  descriptor_path_ = choice.descriptor;
  descriptor_sha256_ = choice.descriptor_sha256;
  notes_path_ = choice.notes;
  notes_sha256_ = choice.notes_sha256;
  return true;
}

void UpdateChecker::askDescriptor(qsizetype i) {
  // Only for the link behind "Download": whatever goes wrong here leaves it
  // to the downloads page, and changes no verdict.
  const auto& channel = build_.channel;
  if (descriptor_path_.isEmpty() || i >= channel.build_bases.size() ||
      fresh_.list.outcome != ListOutcome::kFound) {
    askNotes();
    return;
  }

  const QString base = channel.build_bases[i] + channel.product + u'/';
  QPointer<UpdateChecker> self(this);
  fetch_(
      QUrl(base + descriptor_path_),
      [self, base, i](int status, const QByteArray& body) {
        if (self.isNull()) return;
        // The signed index names this exact descriptor by its hash.
        const auto digest =
            QCryptographicHash::hash(body, QCryptographicHash::Sha256).toHex();
        if (status != 200 ||
            QString::fromLatin1(digest) != self->descriptor_sha256_) {
          self->askDescriptor(i + 1);
          return;
        }
        const auto choice = SelectArtifact(body, self->build_.platform);
        if (!choice.path.isEmpty()) {
          auto& latest = self->fresh_.list.latest;
          latest.html_url = base + choice.path;
          latest.download_sha256 = choice.sha256;
          latest.download_size = choice.size;
        }
        self->askNotes();
      });
}

void UpdateChecker::askNotes(qsizetype i) {
  // The offer's release notes, as reviewed and signed: kept only when they
  // are exactly what the signed index states, otherwise the page shows the
  // version and a link, and no verdict changes.
  const auto& channel = build_.channel;
  if (notes_path_.isEmpty() || i >= channel.build_bases.size() ||
      fresh_.list.outcome != ListOutcome::kFound) {
    askCommitMarker();
    return;
  }

  QPointer<UpdateChecker> self(this);
  fetch_(
      QUrl(channel.build_bases[i] + channel.product + u'/' + notes_path_),
      [self, i](int status, const QByteArray& body) {
        if (self.isNull()) return;
        const auto digest =
            QCryptographicHash::hash(body, QCryptographicHash::Sha256).toHex();
        if (status != 200 || body.size() > kMaxNotesBytes ||
            QString::fromLatin1(digest) != self->notes_sha256_) {
          self->askNotes(i + 1);
          return;
        }
        self->fresh_.list.latest.notes = QString::fromUtf8(body);
        self->askCommitMarker();
      });
}

void UpdateChecker::askCommitMarker() {
  // Asked only once an index verified: the marker's absence then means
  // something, where a broken connection would only mean nothing.
  static const QRegularExpression kCommit(QStringLiteral("^[0-9a-f]{40}$"));
  const auto commit = build_.commit.trimmed().toLower();
  const auto& channel = build_.channel;
  if (!kCommit.match(commit).hasMatch() || channel.build_bases.isEmpty()) {
    finish();
    return;
  }

  QPointer<UpdateChecker> self(this);
  fetch_(QUrl(channel.build_bases.first() + channel.product +
              QStringLiteral("/commits/") + commit),
         [self](int status, const QByteArray& /*body*/) {
           if (self.isNull()) return;
           self->fresh_.commit_fact = ParseCommitMarker(status);
           self->finish();
         });
}

void UpdateChecker::finish() {
  if (--pending_ > 0) return;

  const auto now = clock_();
  fresh_.checked_at = now;
  fresh_.complete = IsComplete(fresh_);

  state_.checking = false;
  state_.last_attempt = now;
  state_.last_attempt_failed = !IsUsable(fresh_);
  state_.last_good = Merge(state_.last_good, fresh_);
  state_.max_serial = std::max(state_.max_serial, accepted_serial_);

  if (store_.save) store_.save(ToStorage(state_));
  emit Changed();
}

auto UpdateChecker::ToStorage(const UpdateSnapshot& s) -> QByteArray {
  QJsonObject obj;
  obj["schema"] = kStorageSchema;
  if (s.last_good) obj["last_good"] = s.last_good->ToJson();
  if (s.last_attempt.isValid()) {
    obj["last_attempt"] = s.last_attempt.toSecsSinceEpoch();
  }
  obj["last_attempt_failed"] = s.last_attempt_failed;
  if (!s.last_seen_update_version.isEmpty()) {
    obj["last_seen_update_version"] = s.last_seen_update_version;
  }
  // Additive: a build that does not know the field keeps it untouched.
  if (s.max_serial > 0) obj["max_serial"] = s.max_serial;
  if (s.last_seen_at.isValid()) {
    obj["last_seen_at"] = s.last_seen_at.toSecsSinceEpoch();
  }
  return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

auto UpdateChecker::FromStorage(const QByteArray& data, const Build& build)
    -> UpdateSnapshot {
  UpdateSnapshot s;
  const auto doc = QJsonDocument::fromJson(data);
  if (!doc.isObject()) return s;

  const auto obj = doc.object();
  if (obj.value("schema").toInt() != kStorageSchema) return s;

  // Which update the user has seen is not a fact about this build: an
  // upgrade, or a rebuild, must not advertise it again.
  s.last_seen_update_version = obj.value("last_seen_update_version").toString();
  s.max_serial = obj.value("max_serial").toInteger();
  if (obj.value("last_seen_at").isDouble()) {
    s.last_seen_at =
        QDateTime::fromSecsSinceEpoch(obj.value("last_seen_at").toInteger());
  }

  auto good = SoftwareVersion::FromJson(obj.value("last_good").toObject());
  // What was learned about another build says nothing about this one, and
  // neither does how asking about it went.
  if (!good || !good->SameBuild(build.version, build.commit)) return s;

  s.last_good = std::move(good);
  if (obj.value("last_attempt").isDouble()) {
    s.last_attempt =
        QDateTime::fromSecsSinceEpoch(obj.value("last_attempt").toInteger());
  }
  s.last_attempt_failed = obj.value("last_attempt_failed").toBool();
  return s;
}

auto AttentionFor(const UpdateSnapshot& s, const QDateTime& now)
    -> UpdateAttention {
  if (!s.last_good || Decide(*s.last_good) != Verdict::kUpdateAvailable) {
    return UpdateAttention::kNone;
  }
  const auto& latest = s.last_good->list.latest.version;
  const auto& seen = s.last_seen_update_version;
  if (!seen.isEmpty() && SemVerCompare(latest, seen) <= 0) {
    return UpdateAttention::kNone;
  }
  // A nightly's channel moves with every push: after one offer was seen, the
  // next waits a day. The dialog still shows the newest whenever opened.
  if (now.isValid() && s.last_seen_at.isValid() && s.last_good->nightly &&
      s.last_seen_at.secsTo(now) < kNightlyQuietSeconds) {
    return UpdateAttention::kNone;
  }
  return UpdateAttention::kUpdateAvailable;
}
