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

#include <QDateTime>
#include <QString>
#include <QStringList>

#include "UpdateChecker.h"

/**
 * @file UpdatePresentation.h
 * @brief What the update dialog shows, decided without a widget.
 *
 * The checker keeps a precise model; the user gets a headline, a line of
 * detail, at most two buttons, and on request the steps of the last check.
 * A request that got no answer is not guessed at (offline, a mirror down, a
 * server error); a refusal (a bad signature, a rollback) is named.
 */

/// kNeutral informs, kGood is all well, kUpdate good news (a newer
/// version), kAttention a warning (withdrawn, unofficial, the server not
/// reached), kError a refusal (update information that does not verify).
enum class UpdateTone { kNeutral, kGood, kUpdate, kAttention, kError };

/// One row of the update dialog's details.
struct DetailRow {
  /// kNotChecked: shown, not checked (a download the browser fetches, a
  /// build without trust); kNone: what the release server checked, never
  /// this device.
  enum class Mark { kVerified, kFailed, kNotRun, kNotChecked, kNone };
  Mark mark = Mark::kNone;
  QString title;
  QString value;          ///< in the secondary colour, after the title
  QString mono;           ///< key ids, short hashes: in the fixed-width font
  QString full;           ///< the full values behind `mono`, for its tooltip
  QString copy;           ///< a value the row offers to copy, or empty
  bool download = false;  ///< the downloaded file's (PresentInstall())
};

struct UpdateView {
  UpdateTone tone = UpdateTone::kNeutral;
  QString headline;
  QString detail;
  /// A refresh that failed while an older result is shown, a series notice,
  /// a failed install, a package source.
  QString notice;

  // "This build"
  QString build_version;
  QString build_kind;    ///< "Release · series 2.2 (stable)" or "Nightly"
  QString build_commit;  ///< shortened; empty without one
  QString build_official;
  QString newest_label;  ///< "Newest in 2.2" when nothing is offered
  QString newest;        ///< "2.2.4"
  /// "A newer beta, X, is available": a beta not offered while "Also offer
  /// beta versions" (Settings > Updates) is off.
  QString newer_beta;

  // "Offered"
  bool show_offer = false;
  QString offer_version;
  QString offer_file;    ///< the file's name; empty when not known
  QString offer_size;    ///< "2.0 MiB"; empty when not known
  QString offer_sha256;  ///< full; shown shortened

  /// One short line: verified and when, or that it was not. Why not is the
  /// header's to say, once.
  QString verification;
  UpdateTone verification_tone = UpdateTone::kNeutral;

  /// Checked on this device, in the order the check runs: a step that does
  /// not apply is left out, and what did not run after a failure is one
  /// row. Empty before a check recorded them.
  QList<DetailRow> device_rows;
  /// Checked by the release server before it published: never a mark.
  QList<DetailRow> host_rows;
  QString security_url;     ///< how to verify a download by hand
  QStringList report_head;  ///< the build and when it was checked

  bool show_download = false;
  QString download_label;
  QString download_version;
  /// The file itself, when known (from the release's signed descriptor);
  /// empty sends "Download" to the downloads page.
  QString download_url;
  /// What the descriptor says of that file, for an in-place update.
  QString download_sha256;
  qint64 download_size = 0;
  /// The update can replace this build in place: a nightly whose file the
  /// signed descriptor states by hash and size. Whether this machine allows
  /// it (an AppImage in a place the user can write) is the dialog's to add.
  bool installable = false;
  /// What the main button does: open download_url (or the downloads page),
  /// install the update in place, or restart into the installed one.
  enum class Action { kOpen, kInstall, kRestart } action = Action::kOpen;
  bool download_enabled = true;

  /// "Check again" only where asking again can help: not for a build
  /// without trust.
  bool show_check = true;
  QString check_label;  ///< "Check now", "Check again" or "Checking…"
  bool check_enabled = true;

  bool show_notes = false;
  QString notes;  ///< markdown
};

/// @p build is what the checker was made for (UpdateChecker::GetBuild()):
/// this build's block, its package source and platform.
auto PresentUpdate(const UpdateSnapshot& s, const QDateTime& now,
                   const UpdateChecker::Build& build = {}) -> UpdateView;

/// The details as plain text, one line per step, for a bug report: no
/// paths, nothing about the user.
auto ReportText(const UpdateView& view) -> QString;

/// "0123abcd…89abcdef": a hash as a person compares it.
auto ShortHash(const QString& hash) -> QString;

/// Where an in-place update of the running AppImage stands.
struct InstallState {
  enum class Stage { kIdle, kDownloading, kInstalled, kFailed };
  Stage stage = Stage::kIdle;
  qint64 received = 0;
  qint64 total = 0;         ///< 0 when the server did not say
  QString error;            ///< kFailed: why, as AppImageInstaller put it
  bool can_restart = true;  ///< false once the Host could not restart
};

/// @p view with the in-place update laid over it: "Install update", its
/// progress, then "Restart now". Unchanged unless @p view is installable.
auto PresentInstall(UpdateView view, const InstallState& install) -> UpdateView;

/// A version as a person reads it: no leading "v", no build metadata
/// ("v2.3.0-alpha.415+gbe5ff859f457" -> "2.3.0-alpha.415"). The commit says
/// nothing to a user and makes a headline wrap.
auto DisplayVersion(const QString& version) -> QString;

/// "just now", "5 minutes ago", "3 hours ago", then the date.
auto RelativeTime(const QDateTime& then, const QDateTime& now) -> QString;

/// What the page says when the build's series is no longer the one to be
/// on (SoftwareVersion::notice); empty for a supported series.
auto SeriesNoticeText(const SoftwareVersion::Notice& notice) -> QString;

class QTextDocument;

/// Release notes as shown: a link is kept only when IsAllowedNotesLink()
/// takes its target; any other reads as plain text and opens nothing.
void RestrictNotesLinks(QTextDocument* document, const QStringList& hosts);
