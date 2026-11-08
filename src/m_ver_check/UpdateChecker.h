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
#include <functional>
#include <optional>

#include "ReleaseIndex.h"
#include "SoftwareVersion.h"

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
  /// accepted. An index with a lower one is a replay of an older state and is
  /// refused (anti-rollback); kept across builds, like the field above.
  qint64 max_serial = 0;
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
  };

  static constexpr qint64 kFreshSeconds = 24 * 60 * 60;
  /// Release notes are a page of text; anything larger is not taken.
  static constexpr qsizetype kMaxNotesBytes = 256 * 1024;

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
  auto acceptIndex(const QByteArray& body, const QByteArray& signature) -> bool;
  void askDescriptor(qsizetype i = 0);
  void askNotes(qsizetype i = 0);
  void askCommitMarker();

  Build build_;
  bool nightly_ = false;
  Fetcher fetch_;
  Store store_;
  Clock clock_;

  UpdateSnapshot state_;
  SoftwareVersion fresh_;
  int pending_ = 0;
  qint64 accepted_serial_ = 0;
  QString descriptor_path_;    ///< of the release the accepted index offers
  QString descriptor_sha256_;  ///< what the signed index says it hashes to
  QString notes_path_;         ///< that release's notes, when listed
  QString notes_sha256_;
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
