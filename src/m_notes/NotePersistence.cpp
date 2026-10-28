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

#include "NotePersistence.h"

#include <algorithm>

namespace gf::notes {

NotePersistence::NotePersistence(Run run, Done done)
    : run_(std::move(run)), done_(std::move(done)) {}

void NotePersistence::Edited(quint64 gen, qint64 now) {
  last_edit_ = now;
  if (gen <= edit_gen_ && gen != 0) return;
  edit_gen_ = gen;
  if (uncaptured_since_ < 0) uncaptured_since_ = now;
}

void NotePersistence::ProtectionChanged(quint64 rev) { rev_ = rev; }

void NotePersistence::Opened(quint64 gen, quint64 rev) {
  edit_gen_ = gen;
  rev_ = rev;
  has_saved_ = true;
  saved_gen_ = gen;
  saved_rev_ = rev;
  captured_gen_ = gen;
  uncaptured_since_ = -1;
}

void NotePersistence::Captured(quint64 gen) {
  captured_gen_ = std::max(captured_gen_, gen);
  if (edit_gen_ <= captured_gen_) uncaptured_since_ = -1;
}

auto NotePersistence::Revise(Snapshot snapshot, const QString& path)
    -> quint64 {
  Job job;
  job.id = next_id_++;
  job.kind = JobKind::kREVISION;
  job.snapshot = std::move(snapshot);
  job.destination = path;
  enqueue(job);
  return job.id;
}

auto NotePersistence::Save(const QString& destination) -> quint64 {
  Job job;
  job.id = next_id_++;
  job.kind = JobKind::kSAVE;
  job.destination = destination;
  enqueue(job);
  return job.id;
}

auto NotePersistence::SavePlain(Snapshot snapshot, const QString& destination)
    -> quint64 {
  Job job;
  job.id = next_id_++;
  job.kind = JobKind::kSAVE_PLAIN;
  job.snapshot = std::move(snapshot);
  job.destination = destination;
  enqueue(job);
  return job.id;
}

void NotePersistence::CancelQueued() { queue_.clear(); }

void NotePersistence::enqueue(Job job) {
  queue_.append(std::move(job));
  pump();
}

void NotePersistence::pump() {
  while (!running_.has_value() && !queue_.isEmpty()) {
    auto job = queue_.takeFirst();
    if (job.kind == JobKind::kSAVE) {
      // The revision as it is now: one made just before this is the one.
      if (!revision_.exists) {
        if (done_)
          done_(job, false, QStringLiteral("there is no revision to save"));
        continue;
      }
      job.source = revision_.path;
      job.snapshot = {revision_.edit_gen, revision_.protection_rev, nullptr};
    }
    running_ = job;
    run_(job);
    return;  // Finished() continues the queue
  }
}

void NotePersistence::Finished(quint64 id, bool ok, const QString& why) {
  if (!running_.has_value() || running_->id != id) return;
  const auto job = *running_;
  running_.reset();

  if (ok) {
    if (job.kind == JobKind::kREVISION) {
      revision_ = {true, job.snapshot.edit_gen, job.snapshot.protection_rev,
                   job.destination};
    } else {
      // Moved: the temporary file is gone.
      if (job.kind == JobKind::kSAVE && revision_.path == job.source) {
        revision_ = {};
      }
      has_saved_ = true;
      saved_gen_ = job.snapshot.edit_gen;
      saved_rev_ = job.snapshot.protection_rev;
    }
  }
  if (done_) done_(job, ok, why);
  pump();
}

auto NotePersistence::SaveInFlight() const -> bool {
  return running_.has_value() || !queue_.isEmpty();
}

auto NotePersistence::CaptureDue(qint64 now) const -> bool {
  if (uncaptured_since_ < 0 || edit_gen_ <= captured_gen_) return false;
  return now - last_edit_ >= kCaptureDebounceMs ||
         now - uncaptured_since_ >= kCaptureMaxLatencyMs;
}

}  // namespace gf::notes
