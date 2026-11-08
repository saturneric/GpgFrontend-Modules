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
 * detail and at most two buttons. Failure wording stays generic: the cause of
 * a failed request (offline, rate limit, server) is not known for certain,
 * so it is not guessed.
 */

/// kUpdate is good news (a newer version); kAttention is something wrong
/// with this one (withdrawn, unofficial).
enum class UpdateTone { kNeutral, kGood, kUpdate, kAttention };

struct UpdateView {
  UpdateTone tone = UpdateTone::kNeutral;
  QString headline;
  QString detail;
  QString notice;     ///< a refresh failed while older results are shown
  QString freshness;  ///< "Last checked ... · <where the answer came from>"

  bool busy = false;

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

  QString check_label;  ///< "Check again", "Try again" or "Check now"
  bool check_enabled = true;

  bool show_notes = false;
  QString notes;  ///< markdown
};

auto PresentUpdate(const UpdateSnapshot& s, const QDateTime& now) -> UpdateView;

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
