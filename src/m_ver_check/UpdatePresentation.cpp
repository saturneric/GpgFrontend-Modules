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

#include "UpdatePresentation.h"

#include <QCoreApplication>
#include <QLocale>

#include "ReleaseCheck.h"

// Every string here uses the literal context "UpdateTab", shared with the
// dialog's own tr(): lupdate files them together, and a named constant would
// hide them from it.

auto RelativeTime(const QDateTime& then, const QDateTime& now) -> QString {
  const auto minutes = then.secsTo(now) / 60;
  if (minutes < 1) return QCoreApplication::translate("UpdateTab", "just now");

  const int m = static_cast<int>(minutes);
  if (minutes < 60) {
    return QCoreApplication::translate("UpdateTab", "%n minute(s) ago", "", m);
  }

  const int h = static_cast<int>(minutes / 60);
  if (h < 24) {
    return QCoreApplication::translate("UpdateTab", "%n hour(s) ago", "", h);
  }

  return QLocale().toString(then, QLocale::ShortFormat);
}

auto PresentUpdate(const UpdateSnapshot& s, const QDateTime& now)
    -> UpdateView {
  UpdateView v;
  v.busy = s.checking;
  v.check_enabled = !s.checking;

  if (!s.last_good) {
    if (s.checking) {
      v.headline =
          QCoreApplication::translate("UpdateTab", "Checking for updates…");
      v.check_label = QCoreApplication::translate("UpdateTab", "Check now");
    } else if (s.last_attempt_failed) {
      v.headline = QCoreApplication::translate("UpdateTab",
                                               "Couldn't check for updates");
      v.detail =
          QCoreApplication::translate("UpdateTab", "Please try again later.");
      v.check_label = QCoreApplication::translate("UpdateTab", "Try again");
    } else {
      v.headline =
          QCoreApplication::translate("UpdateTab", "No update information yet");
      v.detail = QCoreApplication::translate(
          "UpdateTab",
          "Check now to see whether a newer GpgFrontend is available.");
      v.check_label = QCoreApplication::translate("UpdateTab", "Check now");
    }
    return v;
  }

  const auto& r = *s.last_good;
  const auto& latest = r.list.latest.version;

  switch (Decide(r)) {
    case Verdict::kUpdateAvailable:
      v.tone = UpdateTone::kAttention;
      v.headline = QCoreApplication::translate("UpdateTab",
                                               "GpgFrontend %1 is available")
                       .arg(latest);
      v.detail = QCoreApplication::translate("UpdateTab", "You are using %1.")
                     .arg(r.current_version);
      v.show_download = true;
      v.download_version = latest;
      v.download_label =
          QCoreApplication::translate("UpdateTab", "Download %1").arg(latest);
      v.notes = r.list.latest.notes.trimmed();
      v.show_notes = !v.notes.isEmpty();
      break;
    case Verdict::kWithdrawnOrUnreleased:
      v.tone = UpdateTone::kAttention;
      v.headline = QCoreApplication::translate(
          "UpdateTab", "This version isn't listed on GitHub Releases");
      v.detail = QCoreApplication::translate(
          "UpdateTab",
          "It may be a pre-release build or a withdrawn version. "
          "The latest official release is recommended.");
      break;
    case Verdict::kUnofficialBuild:
      v.tone = UpdateTone::kAttention;
      v.headline = QCoreApplication::translate(
          "UpdateTab", "This build isn't from the official repository");
      v.detail = QCoreApplication::translate(
          "UpdateTab",
          "Its source commit wasn't found upstream. That is expected "
          "for self-built or modified versions.");
      break;
    case Verdict::kUpToDate:
      v.tone = UpdateTone::kGood;
      v.headline =
          QCoreApplication::translate("UpdateTab", "You're up to date");
      v.detail = r.list.outcome == ListOutcome::kFound
                     ? QCoreApplication::translate("UpdateTab",
                                                   "%1 is the latest release.")
                           .arg(r.current_version)
                     : QCoreApplication::translate(
                           "UpdateTab", "No newer release was found for %1.")
                           .arg(r.current_version);
      break;
    case Verdict::kUnknown:
      // last_good is usable by construction; kept for completeness.
      v.headline = QCoreApplication::translate("UpdateTab",
                                               "Couldn't check for updates");
      break;
  }

  v.freshness = QCoreApplication::translate("UpdateTab",
                                            "Last checked %1 · GitHub Releases")
                    .arg(RelativeTime(r.checked_at, now));

  if (s.last_attempt_failed && !s.checking) {
    v.notice = QCoreApplication::translate(
        "UpdateTab",
        "Couldn't check for updates just now. Showing the last "
        "known result.");
    v.check_label = QCoreApplication::translate("UpdateTab", "Try again");
  } else {
    v.check_label = QCoreApplication::translate("UpdateTab", "Check again");
  }
  return v;
}
