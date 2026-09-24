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

enum class UpdateTone { kNeutral, kGood, kAttention };

struct UpdateView {
  UpdateTone tone = UpdateTone::kNeutral;
  QString headline;
  QString detail;
  QString notice;     ///< a refresh failed while older results are shown
  QString freshness;  ///< "Last checked ... · GitHub Releases"

  bool busy = false;

  bool show_download = false;
  QString download_label;
  QString download_version;

  QString check_label;  ///< "Check again", "Try again" or "Check now"
  bool check_enabled = true;

  bool show_notes = false;
  QString notes;  ///< markdown
};

auto PresentUpdate(const UpdateSnapshot& s, const QDateTime& now) -> UpdateView;

/// "just now", "5 minutes ago", "3 hours ago", then the date.
auto RelativeTime(const QDateTime& then, const QDateTime& now) -> QString;
