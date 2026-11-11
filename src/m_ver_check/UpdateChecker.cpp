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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QRegularExpression>
#include <algorithm>
#include <utility>

#include "Minisign.h"
#include "ReleaseCheck.h"
#include "SemVer.h"

namespace {

// 3 adds the last check's record; a store of 2 reads as one without it.
constexpr int kStorageSchema = 3;

constexpr std::array<std::pair<CheckStepId, const char*>, kCheckSteps>
    kStepNames = {{
        {CheckStepId::kRoot, "root"},
        {CheckStepId::kIndexKeys, "index_keys"},
        {CheckStepId::kIndex, "index"},
        {CheckStepId::kDescriptor, "descriptor"},
        {CheckStepId::kNotes, "notes"},
        {CheckStepId::kCommit, "commit"},
    }};

constexpr std::array<std::pair<StepStatus, const char*>, 4> kStatusNames = {{
    {StepStatus::kNotRun, "not_run"},
    {StepStatus::kVerified, "verified"},
    {StepStatus::kFailed, "failed"},
    {StepStatus::kNotApplicable, "not_applicable"},
}};

constexpr std::array<std::pair<StepReason, const char*>, 13> kReasonNames = {{
    {StepReason::kNone, ""},
    {StepReason::kUnreachable, "unreachable"},
    {StepReason::kBadSignature, "bad_signature"},
    {StepReason::kKeyNotListed, "key_not_listed"},
    {StepReason::kExpired, "expired"},
    {StepReason::kRollback, "rollback"},
    {StepReason::kRootWalkFailed, "root_walk_failed"},
    {StepReason::kListRejected, "list_rejected"},
    {StepReason::kHashMismatch, "hash_mismatch"},
    {StepReason::kOversized, "oversized"},
    {StepReason::kMalformed, "malformed"},
    {StepReason::kNotFound, "not_found"},
    {StepReason::kNoTrust, "no_trust"},
}};

auto TimeToJson(const QDateTime& t) -> QJsonValue {
  return t.isValid() ? QJsonValue(t.toSecsSinceEpoch()) : QJsonValue();
}

auto TimeFromJson(const QJsonValue& v) -> QDateTime {
  return v.isDouble() ? QDateTime::fromSecsSinceEpoch(v.toInteger())
                      : QDateTime();
}

auto RecordToJson(const CheckRecord& r) -> QJsonObject {
  QJsonObject steps;
  for (const auto& [id, name] : kStepNames) {
    const auto& step = r.Step(id);
    steps[QString::fromLatin1(name)] =
        QJsonObject{{"status", NameOf(kStatusNames, step.status)},
                    {"reason", NameOf(kReasonNames, step.reason)},
                    {"detail", step.detail}};
  }
  return {
      {"steps", steps},
      {"root_version", r.root_version},
      {"root_built_in", r.root_built_in},
      {"root_keys", r.root_keys},
      {"root_signers", QJsonArray::fromStringList(r.root_signers)},
      {"root_expires", TimeToJson(r.root_expires)},
      {"list_version", r.list_version},
      {"list_signer", r.list_signer},
      {"list_keys", r.list_keys},
      {"list_expires", TimeToJson(r.list_expires)},
      {"index_serial", r.index_serial},
      {"index_key", r.index_key},
      {"mirror", r.mirror},
      {"index_expires", TimeToJson(r.index_expires)},
      {"series_status", r.series_status},
      {"newer_beta", r.newer_beta},
      {"descriptor_sha256", r.descriptor_sha256},
      {"notes_sha256", r.notes_sha256},
      {"marker_base", r.marker_base},
      {"repository", r.repository},
      {"workflow", r.workflow},
      {"pgp_fingerprints", QJsonArray::fromStringList(r.pgp_fingerprints)},
      {"authenticode_subject", r.authenticode_subject},
  };
}

auto RecordFromJson(const QJsonObject& obj) -> CheckRecord {
  CheckRecord r;
  const auto steps = obj.value("steps").toObject();
  for (const auto& [id, name] : kStepNames) {
    const auto step = steps.value(QString::fromLatin1(name)).toObject();
    r.Step(id) = {ValueOf(kStatusNames, step.value("status").toString()),
                  ValueOf(kReasonNames, step.value("reason").toString()),
                  step.value("detail").toString()};
  }
  const auto strings = [&](const char* key) {
    return obj.value(QLatin1StringView(key)).toVariant().toStringList();
  };
  r.root_version = obj.value("root_version").toInteger();
  r.root_built_in = obj.value("root_built_in").toInteger();
  r.root_keys = obj.value("root_keys").toInt();
  r.root_signers = strings("root_signers");
  r.root_expires = TimeFromJson(obj.value("root_expires"));
  r.list_version = obj.value("list_version").toInteger();
  r.list_signer = obj.value("list_signer").toString();
  r.list_keys = obj.value("list_keys").toInt();
  r.list_expires = TimeFromJson(obj.value("list_expires"));
  r.index_serial = obj.value("index_serial").toInteger();
  r.index_key = obj.value("index_key").toString();
  r.mirror = obj.value("mirror").toString();
  r.index_expires = TimeFromJson(obj.value("index_expires"));
  r.series_status = obj.value("series_status").toString();
  r.newer_beta = obj.value("newer_beta").toString();
  r.descriptor_sha256 = obj.value("descriptor_sha256").toString();
  r.notes_sha256 = obj.value("notes_sha256").toString();
  r.marker_base = obj.value("marker_base").toString();
  r.repository = obj.value("repository").toString();
  r.workflow = obj.value("workflow").toString();
  r.pgp_fingerprints = strings("pgp_fingerprints");
  r.authenticode_subject = obj.value("authenticode_subject").toString();
  return r;
}

/// An expiry as a step's detail.
auto IsoDetail(const QDateTime& t) -> QString {
  return t.toUTC().toString(Qt::ISODate);
}

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

void UpdateChecker::fail(CheckStepId id, StepReason reason,
                         const QString& detail) {
  auto& step = record_.Step(id);
  if (reason == StepReason::kUnreachable &&
      step.status == StepStatus::kFailed &&
      step.reason != StepReason::kUnreachable) {
    return;
  }
  step = {StepStatus::kFailed, reason, detail};
}

void UpdateChecker::failFetch(CheckStepId id, int status, bool oversized,
                              StepReason refused) {
  if (oversized || status == kStatusOversized) {
    fail(id, StepReason::kOversized);
  } else if (status == 0) {
    fail(id, StepReason::kUnreachable);
  } else {
    fail(id, refused, QStringLiteral("HTTP %1").arg(status));
  }
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
  built_in_root_ = root_->version;
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

void UpdateChecker::fetchBounded(
    const QUrl& url, qsizetype limit, CheckStepId id, StepReason refused,
    const std::function<void(const QByteArray&)>& ok, const Then& failed) {
  QPointer<UpdateChecker> self(this);
  fetch_(url, [=](int status, const QByteArray& body) {
    if (self.isNull()) return;
    if (status != 200 || body.size() > limit) {
      self->warn(QStringLiteral("%1: HTTP %2").arg(url.toString()).arg(status));
      self->failFetch(id, status, body.size() > limit, refused);
      failed();
      return;
    }
    ok(body);
  });
}

auto UpdateChecker::trustBase(qsizetype i) const -> QString {
  const auto& channel = build_.channel;
  return channel.update_bases[i] + channel.product + QStringLiteral("/v1/");
}

auto UpdateChecker::buildBase(qsizetype i) const -> QString {
  const auto& channel = build_.channel;
  return channel.build_bases[i] + channel.product + u'/';
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

auto UpdateChecker::GetBuild() const -> const Build& { return build_; }

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
  record_ = {};
  startIndex();
}

void UpdateChecker::startIndex() {
  // One finish() at the end of the chain, however it ends (finish()).
  state_.checking = true;
  emit Changed();

  // Not built with a channel (a build of the modules on their own): nothing
  // can be verified, so nothing is claimed.
  if (!build_.channel.IsConfigured()) {
    fail(CheckStepId::kRoot, StepReason::kNoTrust);
    finish();
    return;
  }
  // What loadTrust() verified; the walk may take newer ones.
  if (root_) {
    record_.Step(CheckStepId::kRoot).status = StepStatus::kVerified;
  } else {
    fail(CheckStepId::kRoot, StepReason::kRootWalkFailed);
  }
  if (keys_) {
    record_.Step(CheckStepId::kIndexKeys).status = StepStatus::kVerified;
  } else {
    fail(CheckStepId::kIndexKeys, StepReason::kListRejected);
  }
  tryIndexBase(0);
}

void UpdateChecker::tryIndexBase(qsizetype i, bool retried) {
  const auto& channel = build_.channel;
  if (i >= channel.update_bases.size()) {
    finish();  // no base served a good index: the list stays unusable
    return;
  }

  const QString url = trustBase(i) + QStringLiteral("index.json");
  const auto next = [this, i] { tryIndexBase(i + 1); };
  fetchBounded(
      QUrl(url), kMaxFetchBytes, CheckStepId::kIndex, StepReason::kUnreachable,
      [this, i, url, retried, next](const QByteArray& body) {
        fetchBounded(
            QUrl(url + QStringLiteral(".minisig")), kMaxFetchBytes,
            CheckStepId::kIndex, StepReason::kUnreachable,
            [this, i, body, retried](const QByteArray& signature) {
              checkIndex(i, body, signature, retried);
            },
            next);
      },
      next);
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
        self->record_.mirror =
            QUrl(self->build_.channel.update_bases[i]).host();
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
  fetchBounded(
      QUrl(url), kMaxFetchBytes, CheckStepId::kRoot,
      StepReason::kRootWalkFailed,
      [=](const QByteArray& data) {
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
                self->fail(CheckStepId::kRoot, StepReason::kRootWalkFailed,
                           why);
                then();
                return;
              }
              // Another G: what the old one signed counts no more.
              if (next->index_list_keyids != self->root_->index_list_keyids) {
                self->keys_.reset();
              }
              self->root_ = next;
              self->state_.root_taken_at = self->clock_();
              self->record_.Step(CheckStepId::kRoot) = {StepStatus::kVerified};
              self->keepTrust();  // each root kept before the next is asked
              self->walkRoots(i, target, steps - 1, then);
            });
      },
      then);
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
  fetchBounded(
      QUrl(url), kMaxFetchBytes, CheckStepId::kIndexKeys,
      StepReason::kListRejected,
      [this, url, then](const QByteArray& data) {
        fetchBounded(
            QUrl(url + QStringLiteral(".minisig")), kMaxFetchBytes,
            CheckStepId::kIndexKeys, StepReason::kListRejected,
            [this, url, data, then](const QByteArray& sig) {
              QString why;
              auto keys = ParseIndexKeys(*root_, {data, {{QString(), sig}}},
                                         build_.channel.product,
                                         keys_ ? keys_->version : 0, &why);
              if (keys) {
                keys_ = keys;
                record_.Step(CheckStepId::kIndexKeys) = {StepStatus::kVerified};
                keepTrust();
              } else {
                warn(url + QStringLiteral(": ") + why);
                fail(CheckStepId::kIndexKeys, StepReason::kListRejected, why);
              }
              then();
            },
            then);
      },
      then);
}

auto UpdateChecker::acceptIndex(const QByteArray& body,
                                const QByteArray& signature) -> bool {
  const auto& channel = build_.channel;
  const auto now = clock_();
  if (!root_ || !keys_) {  // said when it was found out
    if (!keys_ &&
        record_.Step(CheckStepId::kIndexKeys).status != StepStatus::kFailed) {
      fail(CheckStepId::kIndexKeys, StepReason::kListRejected);
    }
    return false;
  }
  const bool root_expired = IsTrustExpired(root_->expires, now);
  const bool keys_expired = IsTrustExpired(keys_->expires, now);
  if (root_expired || keys_expired) {
    warn(QStringLiteral("root %1 or index-keys %2 has expired")
             .arg(root_->version)
             .arg(keys_->version));
    if (root_expired) {
      fail(CheckStepId::kRoot, StepReason::kExpired, IsoDetail(root_->expires));
    }
    if (keys_expired) {
      fail(CheckStepId::kIndexKeys, StepReason::kExpired,
           IsoDetail(keys_->expires));
    }
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
    fail(CheckStepId::kIndex, StepReason::kKeyNotListed, key_id);
    return false;
  }
  const auto comment =
      MinisignVerify({keys_->keys.value(key_id)}, body, signature);
  if (!comment) {
    warn(QStringLiteral("the index's signature does not verify"));
    fail(CheckStepId::kIndex, StepReason::kBadSignature, key_id);
    return false;
  }
  QDateTime expired;
  const auto facts = ParseIndex(body, *comment, channel.product, now, &expired);
  if (!facts) {
    warn(QStringLiteral("the index is expired, malformed or another's"));
    if (expired.isValid()) {
      fail(CheckStepId::kIndex, StepReason::kExpired, IsoDetail(expired));
    } else {
      fail(CheckStepId::kIndex, StepReason::kMalformed);
    }
    return false;
  }
  // An older index than one already accepted under this key is a replay,
  // from a stale mirror or an attacker: not believed, another base is tried.
  if (facts->serial < state_.serial_floor.value(key_id)) {
    warn(QStringLiteral("index serial %1 is older than %2, seen before")
             .arg(facts->serial)
             .arg(state_.serial_floor.value(key_id)));
    fail(CheckStepId::kIndex, StepReason::kRollback,
         QStringLiteral("%1 %2")
             .arg(state_.serial_floor.value(key_id))
             .arg(facts->serial));
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

  // A mirror refused or down before this one: this one verified.
  for (const auto id :
       {CheckStepId::kRoot, CheckStepId::kIndexKeys, CheckStepId::kIndex}) {
    record_.Step(id) = {StepStatus::kVerified};
  }
  record_.index_serial = facts->serial;
  record_.index_key = key_id;
  record_.index_expires = facts->expires;
  record_.series_status =
      facts->series.value(SoftwareVersion::VersionSeries(build_.version))
          .status;
  record_.newer_beta = choice.newer_beta;
  return true;
}

void UpdateChecker::askPinned(const Pinned& pinned, qsizetype i) {
  if (pinned.path.isEmpty() || fresh_.list.outcome != ListOutcome::kFound) {
    record_.Step(pinned.step).status = StepStatus::kNotApplicable;
    pinned.next();
    return;
  }
  if (i >= build_.channel.build_bases.size()) {
    pinned.next();
    return;
  }
  const auto base = buildBase(i);
  fetchBounded(
      QUrl(base + pinned.path), pinned.limit, pinned.step,
      StepReason::kUnreachable,
      [this, pinned, base, i](const QByteArray& body) {
        // The signed index names this exact file by its hash.
        if (Sha256Hex(body) != pinned.sha256) {
          fail(pinned.step, StepReason::kHashMismatch);
          askPinned(pinned, i + 1);
          return;
        }
        record_.Step(pinned.step) = {StepStatus::kVerified};
        pinned.take(base, body);
        pinned.next();
      },
      [this, pinned, i] { askPinned(pinned, i + 1); });
}

void UpdateChecker::askDescriptor() {
  // Only for the link behind "Download": whatever goes wrong here leaves it
  // to the downloads page, and changes no verdict.
  askPinned({CheckStepId::kDescriptor, descriptor_path_, descriptor_sha256_,
             kMaxFetchBytes,
             [this](const QString& base, const QByteArray& body) {
               record_.descriptor_sha256 = descriptor_sha256_;
               const auto choice = SelectArtifact(body, build_.platform);
               if (!choice.path.isEmpty()) {
                 auto& latest = fresh_.list.latest;
                 latest.html_url = base + choice.path;
                 latest.download_sha256 = choice.sha256;
                 latest.download_size = choice.size;
               }
             },
             [this] { askNotes(); }});
}

void UpdateChecker::askNotes() {
  // The offer's release notes, as reviewed and signed: kept only when they
  // are exactly what the signed index states, otherwise the page shows the
  // version and a link, and no verdict changes.
  askPinned({CheckStepId::kNotes, notes_path_, notes_sha256_, kMaxNotesBytes,
             [this](const QString& /*base*/, const QByteArray& body) {
               record_.notes_sha256 = notes_sha256_;
               fresh_.list.latest.notes = QString::fromUtf8(body);
             },
             [this] { askCommitMarker(); }});
}

void UpdateChecker::askCommitMarker() {
  // Asked only once an index verified: the marker's absence then means
  // something, where a broken connection would only mean nothing.
  static const QRegularExpression kCommit(QStringLiteral("^[0-9a-f]{40}$"));
  const auto commit = build_.commit.trimmed().toLower();
  const auto& channel = build_.channel;
  if (!kCommit.match(commit).hasMatch() || channel.build_bases.isEmpty()) {
    record_.Step(CheckStepId::kCommit).status = StepStatus::kNotApplicable;
    finish();
    return;
  }

  record_.marker_base = QUrl(channel.build_bases.first()).host();
  QPointer<UpdateChecker> self(this);
  fetch_(QUrl(buildBase(0) + QStringLiteral("commits/") + commit),
         [self](int status, const QByteArray& /*body*/) {
           if (self.isNull()) return;
           self->fresh_.commit_fact = ParseCommitMarker(status);
           // 200 or 404 answer; anything else is a lookup that failed.
           self->marker_failed_ =
               self->fresh_.commit_fact == RemoteFact::kUnknown;
           if (status == 200) {
             self->record_.Step(CheckStepId::kCommit) = {StepStatus::kVerified};
           } else if (status == 404) {
             self->fail(CheckStepId::kCommit, StepReason::kNotFound);
           } else {
             self->failFetch(CheckStepId::kCommit, status, false,
                             StepReason::kUnreachable);
           }
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
  {
    // The trust held now, whatever this check took or was refused.
    record_.root_built_in = built_in_root_;
    if (root_) {
      record_.root_version = root_->version;
      record_.root_keys = static_cast<int>(root_->root_keyids.size());
      record_.root_signers = root_->signed_by;
      record_.root_expires = root_->expires;
      record_.repository = root_->repository;
      record_.workflow = root_->workflow;
      record_.pgp_fingerprints = root_->pgp_fingerprints;
      record_.authenticode_subject = root_->authenticode_subject;
    }
    if (keys_) {
      record_.list_version = keys_->version;
      record_.list_keys = static_cast<int>(keys_->keys.size());
      record_.list_expires = keys_->expires;
      for (const auto& sig : keys_->file.signatures) {
        record_.list_signer = MinisignSignatureKeyId(sig);
      }
    }
    state_.last_check = record_;
  }
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
  if (s.root_taken_at.isValid()) {
    obj["root_taken_at"] = s.root_taken_at.toSecsSinceEpoch();
  }
  if (s.last_check) obj["last_check"] = RecordToJson(*s.last_check);
  return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

auto UpdateChecker::FromStorage(const QByteArray& data, const Build& build)
    -> UpdateSnapshot {
  UpdateSnapshot s;
  const auto doc = QJsonDocument::fromJson(data);
  if (!doc.isObject()) return s;

  const auto obj = doc.object();
  // Schema 2 is this one without the last check's record.
  const auto schema = obj.value("schema").toInt();
  if (schema != 2 && schema != kStorageSchema) return s;

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
  s.last_seen_at = TimeFromJson(obj.value("last_seen_at"));
  s.root_taken_at = TimeFromJson(obj.value("root_taken_at"));

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
  if (obj.value("last_check").isObject()) {
    s.last_check = RecordFromJson(obj.value("last_check").toObject());
  }
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
  // next waits a day. The dialog still shows the newest whenever opened. One
  // seen "in the future" was seen under a clock that was ahead: how long ago
  // cannot be told, and it keeps nothing quiet for ever (as IsFresh()).
  if (now.isValid() && s.last_seen_at.isValid() && s.last_good->nightly) {
    const auto since = s.last_seen_at.secsTo(now);
    if (since >= 0 && since < kNightlyQuietSeconds) {
      return UpdateAttention::kNone;
    }
  }
  return UpdateAttention::kUpdateAvailable;
}
