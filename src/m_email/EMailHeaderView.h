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

class QPlainTextEdit;
class QComboBox;

/**
 * @brief The message's headers, at three levels of fidelity.
 *
 * Basic and Full are decoded, human-facing renderings. Raw is the original
 * byte slice and nothing else: header order, folding, duplicates and encoding
 * exactly as the sender wrote them.
 *
 * That distinction is the whole point of the view. A "raw" mode fed by a
 * reserialization would show vmime's idea of the headers -- reordered, refolded
 * and re-encoded -- while claiming to show the sender's, which is precisely the
 * evidence header forensics depends on.
 */
class EMailHeaderView : public QWidget {
  Q_OBJECT

 public:
  explicit EMailHeaderView(QWidget* parent = nullptr);

  /**
   * @brief Shows the headers of @p root.
   *
   * @param root the parsed tree; its header_fields feed Basic and Full
   * @param raw the ORIGINAL message bytes, which feed Raw verbatim
   */
  void SetMessage(const EMailPart& root, const QByteArray& raw);

  /// Drops everything shown.
  void Clear();

 private:
  void build_ui();
  void refresh();

  QComboBox* mode_{};
  QPlainTextEdit* text_{};

  EMailPart root_;
  QByteArray raw_;
  bool loaded_{false};
};
