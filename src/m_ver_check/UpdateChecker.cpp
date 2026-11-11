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

#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <QPointer>
#include <QRegularExpression>
#include <utility>

#include "Minisign.h"
#include "ReleaseCheck.h"
#include "SemVer.h"

namespace {

constexpr int kStorageSchema = 2;

auto FileToJson(const SignedFile& f) -> QJsonObject {
  QJsonObject sigs;
  for (auto it = f.signatures.begin(); it != f.signatures.end(); ++it) {
    sigs[it.key()] = QString::fromLatin1(it.value().toBase64());
  }
  return {{"data", QString::fromLatin1(f.data.toBase64())},
          {"signatures", sigs}};
}

auto FileFromJson(const QJsonValue& v) -> SignedFile {
  const auto obj = v.toObject();
  SignedFile f{QByteArray::fromBase64(obj.value("data").toString().toLatin1()),
               {}};
  const auto sigs = obj.value("signatures").toObject();
  for (auto it = sigs.begin(); it != sigs.end(); ++it) {
    f.signatures.insert(it.key(), QByteArray::fromBase64(
                                      it.value().toString().toLatin1()));
  }
  return f;
}

/// A serial as stored: a whole number above zero, never through a double.
auto StoredSerial(const QJsonValue& v) -> qint64 {
  if (!v.isDouble() || v.toInteger() <= 0 ||
      v.toVariant().typeId() != QMetaType::LongLong) {
    return 0;
  }
  return v.toInteger();
}

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
  loadTrust();
}

void UpdateChecker::warn(const QString& what) const {
  if (build_.warn) build_.warn(QStringLiteral("update check: ") + what);
}

void UpdateChecker::loadTrust() {
  const auto& channel = build_.channel;
  if (!channel.IsConfigured()) return;
  QString why;
  root_ = ParseRoot(channel.trust.root, channel.product, &why);
  if (!root_) {
    warn(QStringLiteral("the built-in trust root does not hold: ") + why);
    return;
  }
  if (!state_.root.data.isEmpty()) {
    // Taken through the chain before; its own signatures still hold.
    auto kept = ParseRoot(state_.root, channel.product);
    if (kept && kept->version > root_->version) root_ = kept;
  }
  for (const SignedFile* file :
       std::initializer_list<const SignedFile*>{&state_.index_keys,
                                                &channel.trust.index_keys}) {
    if (file->data.isEmpty()) continue;
    auto keys = ParseIndexKeys(*root_, *file, channel.product, 0);
    if (keys && (!keys_ || keys->version > keys_->version)) keys_ = keys;
  }
  if (!keys_) {
    warn(QStringLiteral("no index-keys list that root %1 signs")
             .arg(root_->version));
  }
  // A store from before the floor was kept per key: its one floor was the
  // floor of the key every index was signed with then, a listed one.
  if (state_.serial_floor.contains(QString()) && keys_) {
    const auto floor = state_.serial_floor.take(QString());
    for (const auto& id : keys_->keys.keys()) {
      state_.serial_floor[id] = std::max(state_.serial_floor.value(id), floor);
    }
  }
}

void UpdateChecker::keepTrust() {
  if (root_) state_.root = root_->file;
  state_.index_keys = keys_ ? keys_->file : SignedFile{};
  if (store_.save) store_.save(ToStorage(state_));
}

auto UpdateChecker::trustBase(qsizetype i) const -> QString {
  const auto& channel = build_.channel;
  return channel.update_bases[i] + channel.product + QStringLiteral("/v1/");
}

auto UpdateChecker::State() const -> const UpdateSnapshot& { return state_; }

auto UpdateChecker::IsFresh() const -> bool {
  const auto& good = state_.last_good;
  if (!good || !IsAuthoritative(*good) ||
      !good->SameBuild(build_.version, build_.commit) ||
      good->follow_beta != followsBeta()) {
    return false;
  }
  // A result from the future is from a clock that was set back since: how
  // old it really is cannot be told, and it is not fresh for ever.
  const auto age = good->checked_at.secsTo(clock_());
  return age >= 0 && age < kFreshSeconds;
}

auto UpdateChecker::IsNightly() const -> bool { return nightly_; }

auto UpdateChecker::followsBeta() const -> bool {
  return !nightly_ && build_.follow_beta;
}

void UpdateChecker::SetFollowBeta(bool follow_beta) {
  build_.follow_beta = follow_beta;
  // The running check asked under the old answer: once it is done, another
  // asks under the new one, rather than the next tick an hour later.
  if (state_.checking && fresh_.follow_beta != followsBeta()) {
    again_ = true;
  }
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
  again_ = false;
  marker_failed_ = false;
  accepted_serial_ = 0;
  accepted_key_.clear();
  descriptor_path_.clear();
  descriptor_sha256_.clear();
  notes_path_.clear();
  notes_sha256_.clear();
  startIndex();
}

void UpdateChecker::startIndex() {
  // One finish() at the end of the chain, however it ends (finish()).
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
    if (status != 200 || body.size() > kMaxFetchBytes) {
      self->tryIndexBase(i + 1);
      return;
    }
    self->fetch_(
        QUrl(url + QStringLiteral(".minisig")),
        [self, i, body, retried](int sig_status, const QByteArray& signature) {
          if (self.isNull()) return;
          if (sig_status == 200 && signature.size() <= kMaxFetchBytes) {
            self->checkIndex(i, body, signature, retried);
            return;
          }
          self->tryIndexBase(i + 1);
        });
  });
}

void UpdateChecker::checkIndex(qsizetype i, const QByteArray& body,
                               const QByteArray& signature, bool retried) {
  // What the index says it is signed under is only where to look: the
  // roots and the list hold on their own signatures, and the index on the
  // list's keys.
  const auto hints = ReadTrustHints(body);
  QPointer<UpdateChecker> self(this);
  walkRoots(i, hints.root_version, kMaxRootSteps, [=] {
    if (self.isNull()) return;
    self->takeIndexKeys(i, hints.index_keys_version, [=] {
      if (self.isNull()) return;
      if (self->acceptIndex(body, signature)) {
        self->askDescriptor();
        return;
      }
      // The server replaces the index and then its signature: read in
      // between, the two disagree. Once more on the same base, then on.
      self->tryIndexBase(retried ? i + 1 : i, !retried);
    });
  });
}

void UpdateChecker::walkRoots(qsizetype i, qint64 target, int steps,
                              const Then& then) {
  if (!root_ || root_->version >= target || steps <= 0) {
    then();
    return;
  }
  const auto n = root_->version + 1;
  const auto url = trustBase(i) + QStringLiteral("root/%1.root.json").arg(n);
  QPointer<UpdateChecker> self(this);
  fetch_(QUrl(url), [=](int status, const QByteArray& data) {
    if (self.isNull()) return;
    if (status != 200 || data.size() > kMaxFetchBytes) {
      self->warn(QStringLiteral("%1: HTTP %2").arg(url).arg(status));
      then();
      return;
    }
    // Signed by root keys of the trusted root, and of its own.
    auto ids = self->root_->root_keyids + NamedRootKeys(data);
    ids.removeDuplicates();
    self->fetchSignatures(
        url + u'.', ids, {}, [=](const QHash<QString, QByteArray>& sigs) {
          if (self.isNull()) return;
          QString why;
          auto next = AcceptNextRoot(*self->root_, {data, sigs},
                                     self->build_.channel.product, &why);
          if (!next) {
            self->warn(url + QStringLiteral(": ") + why);
            then();
            return;
          }
          // Another G: what the old one signed counts no more.
          if (next->index_list_keyids != self->root_->index_list_keyids) {
            self->keys_.reset();
          }
          self->root_ = next;
          self->keepTrust();  // each root kept before the next is asked
          self->walkRoots(i, target, steps - 1, then);
        });
  });
}

void UpdateChecker::fetchSignatures(
    const QString& prefix, QStringList ids, QHash<QString, QByteArray> found,
    const std::function<void(QHash<QString, QByteArray>)>& done) {
  if (ids.isEmpty()) {
    done(found);
    return;
  }
  const auto id = ids.takeFirst();
  QPointer<UpdateChecker> self(this);
  fetch_(QUrl(prefix + id + QStringLiteral(".minisig")),
         [=](int status, const QByteArray& sig) mutable {
           if (self.isNull()) return;
           // A missing one is no error: only those that verify count.
           if (status == 200 && sig.size() <= kMaxFetchBytes) found[id] = sig;
           self->fetchSignatures(prefix, ids, found, done);
         });
}

void UpdateChecker::takeIndexKeys(qsizetype i, qint64 version,
                                  const Then& then) {
  if (!root_ || version <= 0 || (keys_ && version <= keys_->version)) {
    then();
    return;
  }
  const auto url = trustBase(i) + QStringLiteral("index-keys/%1.json")
                                      .arg(version);
  QPointer<UpdateChecker> self(this);
  fetch_(QUrl(url), [=](int status, const QByteArray& data) {
    if (self.isNull()) return;
    if (status != 200 || data.size() > kMaxFetchBytes) {
      self->warn(QStringLiteral("%1: HTTP %2").arg(url).arg(status));
      then();
      return;
    }
    self->fetch_(
        QUrl(url + QStringLiteral(".minisig")),
        [=](int sig_status, const QByteArray& sig) {
          if (self.isNull()) return;
          QString why;
          auto keys =
              sig_status == 200
                  ? ParseIndexKeys(*self->root_, {data, {{QString(), sig}}},
                                   self->build_.channel.product,
                                   self->keys_ ? self->keys_->version : 0,
                                   &why)
                  : std::nullopt;
          if (keys) {
            self->keys_ = keys;
            self->keepTrust();
          } else {
            self->warn(url + QStringLiteral(": ") +
                       (why.isEmpty() ? QStringLiteral("no signature") : why));
          }
          then();
        });
  });
}

auto UpdateChecker::acceptIndex(const QByteArray& body,
                                const QByteArray& signature) -> bool {
  const auto& channel = build_.channel;
  const auto now = clock_();
  if (!root_ || !keys_) return false;  // said when it was found out
  if (IsTrustExpired(root_->expires, now) ||
      IsTrustExpired(keys_->expires, now)) {
    warn(QStringLiteral("root %1 or index-keys %2 has expired")
             .arg(root_->version)
             .arg(keys_->version));
    return false;
  }
  // Only a key the trusted list names signs an index; anything else is not
  // one, however it verifies.
  const auto key_id = MinisignSignatureKeyId(signature);
  if (!keys_->keys.contains(key_id)) {
    warn(QStringLiteral("the index is signed by %1, which index-keys %2 does "
                        "not list")
             .arg(key_id.isEmpty() ? QStringLiteral("no key it knows")
                                   : key_id)
             .arg(keys_->version));
    return false;
  }
  const auto comment =
      MinisignVerify({keys_->keys.value(key_id)}, body, signature);
  if (!comment) {
    warn(QStringLiteral("the index's signature does not verify"));
    return false;
  }
  const auto facts = ParseIndex(body, *comment, channel.product, now);
  if (!facts) {
    warn(QStringLiteral("the index is expired, malformed or another's"));
    return false;
  }
  // An older index than one already accepted under this key is a replay,
  // from a stale mirror or an attacker: not believed, another base is tried.
  if (facts->serial < state_.serial_floor.value(key_id)) {
    warn(QStringLiteral("index serial %1 is older than %2, seen before")
             .arg(facts->serial)
             .arg(state_.serial_floor.value(key_id)));
    return false;
  }

  const auto choice =
      ChooseUpdate(*facts, build_.version, nightly_, fresh_.follow_beta);
  fresh_.list = choice.list;
  fresh_.other_series = choice.other_series;
  fresh_.tag_fact = nightly_ ? WithdrawnFact(*facts, build_.version)
                             : ReleaseFact(*facts, build_.version);
  if (!nightly_) {
    fresh_.notice = NoticeFor(*facts, build_.version, build_.platform);
  }
  accepted_serial_ = facts->serial;
  accepted_key_ = key_id;
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
        if (status != 200 || body.size() > kMaxFetchBytes ||
            Sha256Hex(body) != self->descriptor_sha256_) {
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
  fetch_(QUrl(channel.build_bases[i] + channel.product + u'/' + notes_path_),
         [self, i](int status, const QByteArray& body) {
           if (self.isNull()) return;
           if (status != 200 || body.size() > kMaxNotesBytes ||
               Sha256Hex(body) != self->notes_sha256_) {
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
           // 200 or 404 answer; anything else is a lookup that failed.
           self->marker_failed_ =
               self->fresh_.commit_fact == RemoteFact::kUnknown;
           self->finish();
         });
}

void UpdateChecker::finish() {
  if (!state_.checking) return;  // finished already: a reply answered twice

  const auto now = clock_();
  fresh_.checked_at = now;
  fresh_.complete = IsComplete(fresh_);
  fresh_.settled = IsUsable(fresh_) && accepted_serial_ > 0 && !marker_failed_;

  state_.checking = false;
  state_.last_attempt = now;
  state_.last_attempt_failed = !IsUsable(fresh_);
  state_.last_good = Merge(state_.last_good, fresh_);
  if (accepted_serial_ > 0) {
    state_.serial_floor[accepted_key_] =
        std::max(state_.serial_floor.value(accepted_key_), accepted_serial_);
  }
  if (keys_) {
    // A key no longer listed takes its floor with it.
    state_.serial_floor.removeIf(
        [this](const auto& it) { return !keys_->keys.contains(it.key()); });
  }

  if (store_.save) store_.save(ToStorage(state_));
  emit Changed();
  if (std::exchange(again_, false)) Start(CheckMode::kIfStale);
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
  // Additive: a build that does not know these fields leaves them out.
  if (!s.serial_floor.isEmpty()) {
    QJsonObject floors;
    for (auto it = s.serial_floor.begin(); it != s.serial_floor.end(); ++it) {
      floors[it.key()] = QJsonValue(it.value());  // an integer, exactly
    }
    obj["serial_floor"] = floors;
  }
  if (!s.root.data.isEmpty()) obj["root"] = FileToJson(s.root);
  if (!s.index_keys.data.isEmpty()) {
    obj["index_keys"] = FileToJson(s.index_keys);
  }
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
  // Never below zero: a damaged store only forgets what was seen.
  const auto floors = obj.value("serial_floor").toObject();
  for (auto it = floors.begin(); it != floors.end(); ++it) {
    if (const auto serial = StoredSerial(it.value()); serial > 0) {
      s.serial_floor[it.key()] = serial;
    }
  }
  // A store from before: one floor, for whichever key is listed (loadTrust).
  if (const auto serial = StoredSerial(obj.value("max_serial")); serial > 0) {
    s.serial_floor[QString()] = serial;
  }
  s.root = FileFromJson(obj.value("root"));
  s.index_keys = FileFromJson(obj.value("index_keys"));
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
