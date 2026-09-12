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

#include <QWidget>

#include "EMailModel.h"

class QLineEdit;
class QTreeWidget;
class QLabel;

/**
 * @brief The message's headers, as one searchable list.
 *
 * Rows come from the ORIGINAL bytes rather than from a reserialization, so
 * order, folding, duplicates and encoding are the sender's and not vmime's.
 * That distinction is the whole point of the view: a header list fed by a
 * round trip shows the parser's idea of the headers -- reordered, refolded and
 * re-encoded -- while claiming to show the sender's, which is precisely the
 * evidence header forensics depends on.
 *
 * Each row therefore carries both readings: the decoded value, which is what
 * the field means, and the exact bytes it was written as, which is what it
 * says. They are shown together rather than behind a mode switch, because the
 * question "do these two disagree?" is the one worth being able to ask.
 */
class EMailHeaderView : public QWidget {
  Q_OBJECT

 public:
  explicit EMailHeaderView(QWidget* parent = nullptr);

  /**
   * @brief Shows the headers of @p root.
   *
   * @param root the parsed tree; its header_fields supply the decoded reading
   * @param raw the ORIGINAL message bytes, which supply the rows themselves
   */
  void SetMessage(const EMailPart& root, const QByteArray& raw);

  /// Drops everything shown.
  void Clear();

 private:
  void build_ui();
  void refresh();
  /// Hides the rows that do not match the filter, and shows the placeholder
  /// when that leaves nothing. Never rebuilds: filtering is a view choice and
  /// must not be able to change what is in the list.
  void apply_filter();
  void copy_selected(bool whole_field);

  QLineEdit* filter_{};
  QTreeWidget* tree_{};
  QLabel* empty_notice_{};

  EMailPart root_;
  QByteArray raw_;
  bool loaded_{false};
};
