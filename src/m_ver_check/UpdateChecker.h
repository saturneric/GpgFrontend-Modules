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

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QUrl>
#include <array>
#include <functional>
#include <optional>

#include "ReleaseIndex.h"
#include "RootTrust.h"
#include "SoftwareVersion.h"

/// The steps of a check, in the order it runs them.
enum class CheckStepId {
  kRoot,        ///< the trust root, walked to the one the index names
  kIndexKeys,   ///< the list of keys that may sign the index
  kIndex,       ///< the signed update index
  kDescriptor,  ///< the offer's release details, by the index's hash
  kNotes,       ///< its release notes, by the index's hash
  kCommit,      ///< this build's commit marker
};
inline constexpr std::size_t kCheckSteps = 6;

enum class StepStatus { kNotRun, kVerified, kFailed, kNotApplicable };

/// Why a step failed; UpdatePresentation words each.
enum class StepReason {
  kNone,
  kUnreachable,     ///< no answer, or none usable
  kBadSignature,    ///< the index's signature does not verify
  kKeyNotListed,    ///< signed by a key the trusted list does not hold
  kExpired,         ///< the root, list or index is past its expiry
  kRollback,        ///< an index older than one accepted before
  kRootWalkFailed,  ///< the next root was refused
  kListRejected,    ///< the index-keys list was refused
  kHashMismatch,    ///< not what the signed index pins
  kOversized,       ///< larger than any such answer may be
  kMalformed,       ///< signed, but not a readable index for this product
  kNotFound,        ///< no commit marker: not an official build
};

struct CheckStep {
  StepStatus status = StepStatus::kNotRun;
  StepReason reason = StepReason::kNone;
  /// What goes with the reason: an expiry (ISO 8601), a key id, the serials
  /// of a rollback, an HTTP status. Never a path or a URL.
  QString detail;
};

/**
 * @brief How the last check went, step by step, and what it verified: what
 * the update dialog's details show. Filled where the checker already warns;
 * it decides nothing.
 */
struct CheckRecord {
  std::array<CheckStep, kCheckSteps> steps;

  [[nodiscard]] auto Step(CheckStepId id) const -> const CheckStep& {
    return steps.at(static_cast<std::size_t>(id));
  }
  auto Step(CheckStepId id) -> CheckStep& {
    return steps.at(static_cast<std::size_t>(id));
  }

  // The trust held at the end of the check.
  qint64 root_version = 0;
  qint64 root_built_in = 0;  ///< the root this build was made with
  int root_keys = 0;         ///< how many root keys it names
  QStringList root_signers;  ///< those whose signatures on it verified
  QDateTime root_expires;
  qint64 list_version = 0;
  QString list_signer;  ///< the key id that signed the list
  int list_keys = 0;
  QDateTime list_expires;

  // The index accepted; serial 0 when none was.
  qint64 index_serial = 0;
  qint64 index_floor = 0;  ///< the highest serial seen before under its key
  QString index_key;
  QString mirror;  ///< the host it came from
  QDateTime index_expires;
  QString series_status;  ///< this build's series in it: "stable", ...
  /// A newer beta of this build's series, not offered since the user did
  /// not ask for betas; empty otherwise.
  QString newer_beta;

  QString descriptor_sha256;  ///< as pinned by the index, once matched
  QString notes_sha256;
  QString marker_base;  ///< the host the commit marker was asked of

  // What the release host checks before it publishes (TrustRoot).
  QString repository;
  QString workflow;
  QStringList pgp_fingerprints;
  QString authenticode_subject;
};

/**
 * @brief Everything the update UI needs to know, readable at any moment.
 *
 * A failed attempt never replaces what was known: `last_good` is the last
 * result whose release list came back, and the attempt fields say how the
 * most recent try went.
 */
struct UpdateSnapshot {
  bool checking = false;
  std::optional<SoftwareVersion> last_good;
  QDateTime last_attempt;
  bool last_attempt_failed = false;

  /// The newest update the user has already been shown. Kept across builds
  /// and failed checks: seeing it once is enough.
  QString last_seen_update_version;

  /// When the user last saw an update offered. A nightly's next offer waits a
  /// day after it, so frequent pushes to main do not become nagging.
  QDateTime last_seen_at;

  /// The highest serial of a signed update index this installation has
  /// accepted, by the id of the key that signed it. An index with a lower
  /// one is a replay of an older state and is refused (anti-rollback); kept
  /// across builds, like the field above. A key that leaves the index-keys
  /// list takes its floor with it: a key rotated in starts afresh, and one
  /// index with a huge serial under one key blocks no other.
  QHash<QString, qint64> serial_floor;

  /// The newest root and index-keys list this installation took beyond the
  /// ones it was built with (RootTrust.h): kept across builds.
  SignedFile root;
  SignedFile index_keys;
  /// When `root` was taken; invalid for the built-in one.
  QDateTime root_taken_at;

  /// The last check of this build, step by step; none before the first one
  /// (or from a store written before it was kept).
  std::optional<CheckRecord> last_check;
};

/// Whether the update entry point asks for attention. Derived from the
/// state, never stored: the UI does not read network results itself.
enum class UpdateAttention {
  kNone,
  kUpdateAvailable,  ///< a confirmed newer release the user has not seen
};

enum class CheckMode {
  kIfStale,  ///< skip when a recent authoritative result for this build exists
  kForce,    ///< always ask the server (the user pressed the button)
};

/**
 * @brief The one update check of the process.
 *
 * Every caller shares it: a Start() while a check is running joins that check
 * instead of starting another. The transport, the store and the clock are
 * handed in, so the whole state machine runs in a test without a network, a
 * Host or an event loop.
 */
class UpdateChecker : public QObject {
  Q_OBJECT

 public:
  /// Answers a GET: the HTTP status (0 when there was none) and the body.
  using Reply = std::function<void(int http_status, const QByteArray& body)>;
  using Fetcher = std::function<void(const QUrl& url, Reply reply)>;
  using Clock = std::function<QDateTime()>;

  struct Store {
    std::function<QByteArray()> load;
    std::function<void(const QByteArray&)> save;
  };

  struct Build {
    QString version;
    QString commit;
    /// Where every build reads its signed update index: the nightly identity
    /// for the nightly channel, a release build for its series' releases.
    /// Without one (the modules built on their own) nothing is claimed.
    ReleaseChannel channel;
    /// The machine, to pick the file a "Download" leads to.
    Platform platform;
    /// Whether this is the nightly application: 1 or 0 as the Host says, -1
    /// when it is too old to say, which leaves it to the version's shape.
    int nightly = -1;
    /// A release build is offered the beta channel too ("Also offer beta
    /// versions"). The nightly identity ignores it: main's nightlies follow
    /// the nightly channel, a release branch's the beta one, either way.
    bool follow_beta = false;
    /// Where to say why the update trust or an index was refused (a log at
    /// WARNING): nothing a user sees changes.
    std::function<void(const QString&)> warn;
  };

  static constexpr qint64 kFreshSeconds = 24 * 60 * 60;
  /// Release notes are a page of text; anything larger is not taken.
  static constexpr qsizetype kMaxNotesBytes = 256 * 1024;
  /// The most any answer may weigh: the index, a signature, a descriptor.
  /// The fetcher stops reading past it (MakeFetcher); the checker refuses a
  /// larger body whatever transport it was handed. The host refuses to
  /// publish anything larger (gfrelease's SMALL_OBJECT_LIMIT).
  static constexpr qsizetype kMaxFetchBytes = 8 * 1024 * 1024;

  UpdateChecker(Build build, Fetcher fetch, Store store, Clock clock = {},
                QObject* parent = nullptr);

  [[nodiscard]] auto State() const -> const UpdateSnapshot&;

  /// Starts a check, joins the running one, or (kIfStale) does nothing.
  void Start(CheckMode mode);

  /// A recent authoritative result for this build: no startup check needed.
  [[nodiscard]] auto IsFresh() const -> bool;

  /// The user has seen the update page: whatever update it advertises no
  /// longer asks for attention. A no-op when there is none.
  void Acknowledge();

  /// AttentionFor() the current state, at the checker's own clock.
  [[nodiscard]] auto Attention() const -> UpdateAttention;

  /// What this checker was made for (the dialog shows this build).
  [[nodiscard]] auto GetBuild() const -> const Build&;

  /// Whether this build follows the nightly identity's channels, where the
  /// beta setting means nothing.
  [[nodiscard]] auto IsNightly() const -> bool;

  /// The user's "Also offer beta versions", for the next check. A result
  /// made under the other answer is no longer fresh (IsFresh()), so the next
  /// kIfStale check asks again; a check running under the other answer is
  /// followed by one under this answer.
  void SetFollowBeta(bool follow_beta);

  /// The state as stored; FromStorage() reads it back for @p build only.
  [[nodiscard]] static auto ToStorage(const UpdateSnapshot& s) -> QByteArray;
  [[nodiscard]] static auto FromStorage(const QByteArray& data,
                                        const Build& build) -> UpdateSnapshot;

 signals:
  /// Any change of State(), including the start of a check.
  void Changed();

 private:
  void finish();

  // The signed index: each update base in turn, until one serves an index
  // that verifies; then the offer's descriptor and notes, then the commit
  // marker.
  void startIndex();
  void tryIndexBase(qsizetype i, bool retried = false);
  // Before an index's signature is checked: the roots, and the index-keys
  // list, it says it is signed under, from the same base.
  using Then = std::function<void()>;
  void checkIndex(qsizetype i, const QByteArray& body,
                  const QByteArray& signature, bool retried);
  void walkRoots(qsizetype i, qint64 target, int steps, const Then& then);
  void fetchSignatures(const QString& prefix, QStringList ids,
                       QHash<QString, QByteArray> found,
                       const std::function<void(QHash<QString, QByteArray>)>&
                           done);
  void takeIndexKeys(qsizetype i, qint64 version, const Then& then);
  auto acceptIndex(const QByteArray& body, const QByteArray& signature) -> bool;
  /// The built-in trust, or what was taken since: the newer of each.
  void loadTrust();
  void keepTrust();
  void warn(const QString& what) const;
  /// @p id failed. A refusal is kept over a later "unreachable": a mirror
  /// that refused and one that was down read as refused.
  void fail(CheckStepId id, StepReason reason, const QString& detail = {});
  /// A fetch for @p id with no usable answer: none at all, too large, or
  /// an error status (read as @p refused).
  void failFetch(CheckStepId id, int status, bool oversized,
                 StepReason refused);
  [[nodiscard]] auto trustBase(qsizetype i) const -> QString;
  void askDescriptor(qsizetype i = 0);
  void askNotes(qsizetype i = 0);
  void askCommitMarker();
  /// The beta setting as it applies to this build: never for a nightly.
  [[nodiscard]] auto followsBeta() const -> bool;

  Build build_;
  bool nightly_ = false;
  Fetcher fetch_;
  Store store_;
  Clock clock_;

  UpdateSnapshot state_;
  SoftwareVersion fresh_;
  bool again_ = false;          ///< check again once the running check finishes
  bool marker_failed_ = false;  ///< the commit marker was asked, not answered
  std::optional<TrustRoot> root_;
  std::optional<IndexKeys> keys_;
  qint64 accepted_serial_ = 0;
  QString accepted_key_;  ///< the id of the key that signed it
  QString descriptor_path_;    ///< of the release the accepted index offers
  QString descriptor_sha256_;  ///< what the signed index says it hashes to
  QString notes_path_;         ///< that release's notes, when listed
  QString notes_sha256_;
  qint64 built_in_root_ = 0;  ///< the version of the root built in
  CheckRecord record_;        ///< the running check's
};

/// How long a nightly build stays quiet after an update was seen: its channel
/// moves with every push to main, and a badge a few times a day is noise.
inline constexpr qint64 kNightlyQuietSeconds = 24 * 60 * 60;

/// kUpdateAvailable while the last known result offers a release newer than
/// the last one the user has seen; a failed check changes nothing. For a
/// nightly build, given @p now, not within kNightlyQuietSeconds of the last
/// update the user saw.
auto AttentionFor(const UpdateSnapshot& s, const QDateTime& now = {})
    -> UpdateAttention;
