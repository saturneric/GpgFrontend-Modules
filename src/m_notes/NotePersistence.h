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

#include <QList>
#include <QString>
#include <functional>
#include <memory>
#include <optional>

/**
 * @file NotePersistence.h
 * @brief One note's encryptions and saves, in order, without ever marking
 * newer edits clean.
 *
 * SDK-free and synchronous: the work itself (pack, encrypt, write) is the
 * caller's `run`, which reports back through Finished(). At most one job runs
 * at a time, in the order asked.
 *
 *  - Encrypt makes a revision: the note as it was, encrypted, in a temporary
 *    file, with the readers it was encrypted for. Save writes the latest
 *    revision -- as it is when the save runs -- to the note's file, and keeps
 *    it. A note without one is saved as it is instead.
 *  - The tab is clean only while the current generation is exactly what the
 *    last successful save wrote: saving an older revision leaves newer edits
 *    unsaved.
 *  - Removing the encryption drops the revision; one still being made is
 *    dropped when it finishes.
 *  - Separately, it says when the in-memory recovery record (which the Host
 *    keeps with its other unsaved tabs) is due to be refreshed.
 */

#include "NoteProtection.h"

namespace gf::notes {

struct Snapshot {
  quint64 edit_gen = 0;
  ProtectionConfig readers;  ///< kREVISION: who it is encrypted for
  /// What the run needs (the note, the readers); opaque here.
  std::shared_ptr<const void> payload;
};

/// kREVISION: the user's Encrypt -- the note packed and encrypted into a
/// temporary file. kSAVE: the latest revision copied to where it belongs; it
/// stays the note's encrypted snapshot. kSAVE_PLAIN: a note without one,
/// packed and written as it is.
enum class JobKind { kREVISION, kSAVE, kSAVE_PLAIN };

struct Job {
  quint64 id = 0;
  JobKind kind = JobKind::kSAVE;
  Snapshot snapshot;    ///< kSAVE: the revision's generation, set when it runs
  QString destination;  ///< kREVISION: the temporary file; else where to
  QString source;       ///< kSAVE: the revision file, set when it runs
  quint64 epoch = 0;    ///< kREVISION: dropped if the revision was since
};

/// The latest Encrypt, in a temporary file: what Save writes.
struct RevisionSlot {
  bool exists = false;
  quint64 edit_gen = 0;
  ProtectionConfig readers;
  QString path;
};

class NotePersistence {
 public:
  /// Start @p job; Finished() is called when it ends, never from inside.
  using Run = std::function<void(const Job& job)>;
  /// How a job ended, after the state above was updated for it.
  using Done = std::function<void(const Job& job, bool ok, const QString& why)>;

  /// The recovery record follows the editing this closely.
  static constexpr qint64 kCaptureDebounceMs = 2000;
  static constexpr qint64 kCaptureMaxLatencyMs = 10000;

  explicit NotePersistence(Run run, Done done = {});

  /* --- what the editor tells it --- */

  /// The page's document is now at @p gen (edited at @p now, ms).
  void Edited(quint64 gen, qint64 now);
  /// An existing note was opened: its state on disk, a plain TextPack or
  /// not (an encrypted one, decrypted to be edited).
  void Opened(quint64 gen, bool plain_file);
  /// The recovery record now holds generation @p gen.
  void Captured(quint64 gen);

  /* --- requests --- */

  /// Encrypt the note as it is in @p snapshot, into @p path.
  auto Revise(Snapshot snapshot, const QString& path) -> quint64;
  /// Write the latest revision -- as it is when this runs, so one revised
  /// just before is the one written -- to @p destination. It is kept.
  auto Save(const QString& destination) -> quint64;
  /// Write the note in @p snapshot, unencrypted, to @p destination.
  auto SavePlain(Snapshot snapshot, const QString& destination) -> quint64;
  /// The encryption is taken off: forget the revision, and drop one still
  /// being made when it finishes. @return the revision file to remove.
  auto DropRevision() -> QString;
  /// Drop everything not started yet.
  void CancelQueued();
  /// The run for @p id ended.
  void Finished(quint64 id, bool ok, const QString& why = {});

  /* --- state --- */

  [[nodiscard]] auto EditGen() const -> quint64 { return edit_gen_; }
  [[nodiscard]] auto SavedGen() const -> quint64 { return saved_gen_; }
  [[nodiscard]] auto IsClean() const -> bool {
    return has_saved_ && edit_gen_ == saved_gen_;
  }
  /// The note's file is what Save would write now, as far as encryption
  /// goes: the current revision, or a plain TextPack when there is none.
  [[nodiscard]] auto WrittenIsCurrent() const -> bool {
    if (!has_saved_) return false;
    return revision_.exists ? written_revision_ == revision_.path
                            : written_plain_;
  }
  [[nodiscard]] auto Revision() const -> RevisionSlot { return revision_; }
  /// The latest revision has everything: no edit since it was made.
  [[nodiscard]] auto RevisionCurrent() const -> bool {
    return revision_.exists && revision_.edit_gen == edit_gen_;
  }
  [[nodiscard]] auto Busy() const -> bool {
    return running_.has_value() || !queue_.isEmpty();
  }
  [[nodiscard]] auto SaveInFlight() const -> bool;
  /// Whether the recovery record should be refreshed now.
  [[nodiscard]] auto CaptureDue(qint64 now) const -> bool;

 private:
  void pump();
  void enqueue(Job job);

  Run run_;
  Done done_;
  quint64 next_id_ = 1;
  std::optional<Job> running_;
  QList<Job> queue_;

  RevisionSlot revision_;
  quint64 edit_gen_ = 0;
  quint64 epoch_ = 0;  ///< bumped when the revision is dropped
  bool has_saved_ = false;
  quint64 saved_gen_ = 0;
  bool written_plain_ = false;  ///< the last file written is a plain TextPack
  QString written_revision_;    ///< or this revision, encrypted

  // When the edits not yet in the recovery record began, and the last edit.
  qint64 uncaptured_since_ = -1;
  qint64 last_edit_ = -1;
  quint64 captured_gen_ = 0;
};

}  // namespace gf::notes
