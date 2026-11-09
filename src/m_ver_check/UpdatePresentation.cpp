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
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QUrl>
#include <algorithm>

#include "ReleaseCheck.h"
#include "ReleaseIndex.h"

// Every string here uses the literal context "UpdateTab", shared with the
// dialog's own tr(): lupdate files them together, and a named constant would
// hide them from it.

auto PresentInstall(UpdateView view, const InstallState& install)
    -> UpdateView {
  if (!view.installable) return view;
  using Stage = InstallState::Stage;
  view.show_download = true;
  view.download_enabled = true;
  view.action = UpdateView::Action::kInstall;

  switch (install.stage) {
    case Stage::kIdle:
      view.download_label =
          QCoreApplication::translate("UpdateTab", "Install update");
      break;
    case Stage::kDownloading: {
      const auto total = install.total > 0 ? install.total : view.download_size;
      const int percent = total > 0
                              ? static_cast<int>(std::clamp<qint64>(
                                    install.received * 100 / total, 0, 100))
                              : 0;
      view.download_label =
          QCoreApplication::translate("UpdateTab", "Downloading… %1%")
              .arg(percent);
      view.download_enabled = false;
      view.check_enabled = false;
      break;
    }
    case Stage::kInstalled:
      view.tone = UpdateTone::kGood;
      view.headline = QCoreApplication::translate(
                          "UpdateTab", "Nightly build %1 is installed")
                          .arg(DisplayVersion(view.download_version));
      view.detail =
          install.can_restart
              ? QCoreApplication::translate(
                    "UpdateTab", "Restart GpgFrontend to start using it.")
              : QCoreApplication::translate(
                    "UpdateTab",
                    "Close and reopen GpgFrontend to start using it.");
      view.notice.clear();
      view.action = UpdateView::Action::kRestart;
      view.download_label =
          QCoreApplication::translate("UpdateTab", "Restart now");
      view.show_download = install.can_restart;
      view.check_enabled = false;
      break;
    case Stage::kFailed:
      view.notice = install.error;
      view.download_label =
          QCoreApplication::translate("UpdateTab", "Try again");
      break;
  }
  return view;
}

auto DisplayVersion(const QString& version) -> QString {
  auto v = version.trimmed();
  if (v.startsWith(u'v') || v.startsWith(u'V')) v.remove(0, 1);
  const auto plus = v.indexOf(u'+');
  return plus < 0 ? v : v.left(plus);
}

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

auto SeriesNoticeText(const SoftwareVersion::Notice& notice) -> QString {
  if (notice.series.isEmpty()) return {};
  const auto latest = DisplayVersion(notice.successor_latest);
  if (notice.status == u"legacy") {
    return notice.successor_runs_here
               ? QCoreApplication::translate(
                     "UpdateTab",
                     "The %1 series gets security fixes only, until %2. "
                     "GpgFrontend %3 is the current release.")
                     .arg(notice.series, notice.eol, latest)
               : QCoreApplication::translate(
                     "UpdateTab",
                     "The %1 series gets security fixes only, until %2.")
                     .arg(notice.series, notice.eol);
  }
  return notice.successor_runs_here
             ? QCoreApplication::translate(
                   "UpdateTab",
                   "The %1 series no longer gets fixes. GpgFrontend %2 is "
                   "the current release.")
                   .arg(notice.series, latest)
             : QCoreApplication::translate(
                   "UpdateTab",
                   "The %1 series no longer gets fixes, and the current "
                   "release does not run on this system.")
                   .arg(notice.series);
}

void RestrictNotesLinks(QTextDocument* document, const QStringList& hosts) {
  for (auto block = document->begin(); block.isValid(); block = block.next()) {
    for (auto it = block.begin(); !it.atEnd(); ++it) {
      const auto fragment = it.fragment();
      const auto format = fragment.charFormat();
      if (!format.isAnchor() ||
          IsAllowedNotesLink(QUrl(format.anchorHref()), hosts)) {
        continue;
      }
      // Kept as text, in the paragraph's own look: nothing to click, and
      // nothing that looks clickable.
      QTextCharFormat plain = block.charFormat();
      plain.setFontWeight(format.fontWeight());
      plain.setFontItalic(format.fontItalic());
      QTextCursor cursor(document);
      cursor.setPosition(fragment.position());
      cursor.setPosition(fragment.position() + fragment.length(),
                         QTextCursor::KeepAnchor);
      cursor.setCharFormat(plain);
    }
  }
}

auto PresentUpdate(const UpdateSnapshot& s, const QDateTime& now)
    -> UpdateView {
  UpdateView v;
  v.check_enabled = !s.checking;

  if (!s.last_good) {
    if (s.checking) {
      v.headline =
          QCoreApplication::translate("UpdateTab", "Checking for updates…");
      v.check_label = QCoreApplication::translate("UpdateTab", "Checking…");
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
  const auto latest = DisplayVersion(r.list.latest.version);
  const auto current = DisplayVersion(r.current_version);
  // A nightly follows the signed nightly channel, a release build its
  // series' releases in the same signed index; the wording says which.
  const bool nightly = r.nightly;

  switch (Decide(r)) {
    case Verdict::kUpdateAvailable:
      v.tone = UpdateTone::kUpdate;
      v.headline = nightly ? QCoreApplication::translate(
                                 "UpdateTab", "Nightly build %1 is available")
                                 .arg(latest)
                           : QCoreApplication::translate(
                                 "UpdateTab", "GpgFrontend %1 is available")
                                 .arg(latest);
      v.detail = QCoreApplication::translate("UpdateTab", "You are using %1.")
                     .arg(current);
      v.show_download = true;
      v.download_version = r.list.latest.version;
      // The version is in the headline: the button only says what it does.
      v.download_label = QCoreApplication::translate("UpdateTab", "Download");
      // Only a link the checker made from a verified descriptor. In place
      // only for a nightly: a stable update is installed by its installer.
      if (r.list.latest.html_url.startsWith(u"https://")) {
        v.download_url = r.list.latest.html_url;
        v.download_sha256 = r.list.latest.download_sha256;
        v.download_size = r.list.latest.download_size;
        v.installable =
            nightly && IsSha256(v.download_sha256) && v.download_size > 0;
      }
      v.notes = r.list.latest.notes.trimmed();
      v.show_notes = !v.notes.isEmpty();
      break;
    case Verdict::kWithdrawnOrUnreleased:
      v.tone = UpdateTone::kAttention;
      if (nightly) {
        v.headline = QCoreApplication::translate(
            "UpdateTab", "This nightly build has been withdrawn");
        v.detail = QCoreApplication::translate(
            "UpdateTab", "Please move to the newest nightly build.");
        break;
      }
      v.headline = QCoreApplication::translate(
          "UpdateTab", "This version has been withdrawn");
      v.detail = QCoreApplication::translate(
          "UpdateTab", "Please move to the latest release of its series.");
      break;
    case Verdict::kUnofficialBuild:
      v.tone = UpdateTone::kAttention;
      v.headline = QCoreApplication::translate(
          "UpdateTab", "This build isn't from the official repository");
      v.detail = nightly
                     ? QCoreApplication::translate(
                           "UpdateTab",
                           "Its source commit isn't among the published "
                           "builds. That is expected for self-built or "
                           "modified versions.")
                     : QCoreApplication::translate(
                           "UpdateTab",
                           "Its source commit wasn't found upstream. That is "
                           "expected for self-built or modified versions.");
      break;
    case Verdict::kUpToDate:
      if (r.other_series) {
        // A release branch's nightly: main's channel is another series, and
        // jumping there is not an update.
        v.headline = QCoreApplication::translate(
            "UpdateTab", "This is a release branch build");
        v.detail = QCoreApplication::translate(
                       "UpdateTab",
                       "%1 is a test build of a fix release. It is not offered "
                       "updates; install the release when it is out.")
                       .arg(current);
        break;
      }
      v.tone = UpdateTone::kGood;
      v.headline =
          QCoreApplication::translate("UpdateTab", "You're up to date");
      if (nightly) {
        v.detail = QCoreApplication::translate(
                       "UpdateTab", "%1 is the newest nightly build.")
                       .arg(current);
      } else {
        v.detail = r.list.outcome == ListOutcome::kFound
                       ? QCoreApplication::translate(
                             "UpdateTab", "%1 is the latest release.")
                             .arg(current)
                       : QCoreApplication::translate(
                             "UpdateTab", "No newer release was found for %1.")
                             .arg(current);
      }
      break;
    case Verdict::kUnknown:
      // last_good is usable by construction; kept for completeness.
      v.headline = QCoreApplication::translate("UpdateTab",
                                               "Couldn't check for updates");
      break;
  }

  v.freshness =
      nightly ? QCoreApplication::translate(
                    "UpdateTab", "Last checked %1 · Signed nightly channel")
                    .arg(RelativeTime(r.checked_at, now))
              : QCoreApplication::translate(
                    "UpdateTab", "Last checked %1 · Signed update index")
                    .arg(RelativeTime(r.checked_at, now));

  if (s.checking) {
    // The button says it is busy; nothing else moves on the page.
    v.check_label = QCoreApplication::translate("UpdateTab", "Checking…");
  } else if (s.last_attempt_failed) {
    v.notice = QCoreApplication::translate(
        "UpdateTab",
        "Couldn't check for updates just now. Showing the last "
        "known result.");
    v.check_label = QCoreApplication::translate("UpdateTab", "Try again");
  } else {
    v.check_label = QCoreApplication::translate("UpdateTab", "Check again");
  }

  const auto series_notice = SeriesNoticeText(r.notice);
  if (!series_notice.isEmpty()) {
    v.notice =
        v.notice.isEmpty() ? series_notice : v.notice + u'\n' + series_notice;
  }
  return v;
}
