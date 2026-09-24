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

#include <GFSDKApp.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>

#include "ReleaseCheck.h"

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
  if (store_.load) state_ = FromStorage(store_.load(), build_);
}

auto UpdateChecker::State() const -> const UpdateSnapshot& { return state_; }

auto UpdateChecker::IsFresh() const -> bool {
  const auto& good = state_.last_good;
  return good && IsAuthoritative(*good) &&
         good->SameBuild(build_.version, build_.commit) &&
         good->checked_at.secsTo(clock_()) < kFreshSeconds;
}

void UpdateChecker::Acknowledge() {
  if (AttentionFor(state_) == UpdateAttention::kNone) return;
  state_.last_seen_update_version = state_.last_good->list.latest.version;
  if (store_.save) store_.save(ToStorage(state_));
  emit Changed();
}

void UpdateChecker::Start(CheckMode mode) {
  if (state_.checking) return;
  if (mode == CheckMode::kIfStale && IsFresh()) return;

  fresh_ = SoftwareVersion{};
  fresh_.current_version = build_.version;
  fresh_.local_commit_hash = build_.commit;

  const bool ask_commit = !build_.commit.trimmed().isEmpty();
  // Counted before anything is sent: a fetcher may answer synchronously.
  pending_ = ask_commit ? 3 : 2;
  state_.checking = true;
  emit Changed();

  const QString base = QString::fromLatin1(kRepoApi);
  // Replies can outlive the checker: the module deletes it on deactivation.
  QPointer<UpdateChecker> self(this);

  // The list, not /releases/latest: with parallel stable and mainline tracks
  // the newest release in the running build's own series is wanted, which a
  // single global "latest" cannot express.
  fetch_(QUrl(base + "/releases?per_page=100"), [self](int status,
                                                       const QByteArray& body) {
    if (self.isNull()) return;
    self->fresh_.list = ParseReleaseList(status, body, self->build_.version);
    self->finish();
  });

  fetch_(QUrl(base + "/releases/tags/" + build_.version),
         [self](int status, const QByteArray& body) {
           if (self.isNull()) return;
           self->fresh_.tag_fact = ParseTagLookup(status, body);
           self->finish();
         });

  if (ask_commit) {
    fetch_(QUrl(base + "/commits/" + build_.commit.trimmed()),
           [self](int status, const QByteArray& body) {
             if (self.isNull()) return;
             self->fresh_.commit_fact =
                 ParseCommitLookup(status, body, self->build_.commit);
             self->finish();
           });
  }
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

auto AttentionFor(const UpdateSnapshot& s) -> UpdateAttention {
  if (!s.last_good || Decide(*s.last_good) != Verdict::kUpdateAvailable) {
    return UpdateAttention::kNone;
  }
  const auto& latest = s.last_good->list.latest.version;
  const auto& seen = s.last_seen_update_version;
  if (!seen.isEmpty() &&
      GFCompareSoftwareVersion(latest.toUtf8().constData(),
                               seen.toUtf8().constData()) <= 0) {
    return UpdateAttention::kNone;
  }
  return UpdateAttention::kUpdateAvailable;
}
