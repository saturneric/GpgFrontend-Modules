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

#include "ReleaseCheck.h"

#include "SemVer.h"

auto IsUsable(const SoftwareVersion& r) -> bool {
  return r.list.outcome != ListOutcome::kFailed;
}

auto IsComplete(const SoftwareVersion& fresh) -> bool {
  return IsUsable(fresh) && fresh.tag_fact != RemoteFact::kUnknown &&
         fresh.commit_fact != RemoteFact::kUnknown;
}

auto IsAuthoritative(const SoftwareVersion& r) -> bool {
  return IsUsable(r) && (r.complete || r.settled);
}

auto Merge(const std::optional<SoftwareVersion>& prev,
           const SoftwareVersion& fresh) -> std::optional<SoftwareVersion> {
  if (!IsUsable(fresh)) return prev;

  auto merged = fresh;
  if (prev && prev->SameBuild(fresh.current_version, fresh.local_commit_hash)) {
    if (merged.tag_fact == RemoteFact::kUnknown) {
      merged.tag_fact = prev->tag_fact;
    }
    if (merged.commit_fact == RemoteFact::kUnknown) {
      merged.commit_fact = prev->commit_fact;
    }
  }
  return merged;
}

auto Decide(const SoftwareVersion& r) -> Verdict {
  if (!IsUsable(r)) return Verdict::kUnknown;
  if (r.list.outcome == ListOutcome::kFound &&
      SemVerCompare(r.current_version, r.list.latest.version) < 0) {
    return Verdict::kUpdateAvailable;
  }
  if (r.tag_fact == RemoteFact::kNotFound) {
    return Verdict::kWithdrawnOrUnreleased;
  }
  if (r.commit_fact == RemoteFact::kNotFound) return Verdict::kUnofficialBuild;
  return Verdict::kUpToDate;
}
