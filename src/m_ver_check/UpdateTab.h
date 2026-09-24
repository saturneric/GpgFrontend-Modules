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

#include <QPointer>
#include <QtWidgets/QtWidgets>

class UpdateChecker;
struct UpdateView;

/**
 * @brief The update dialog's content: renders UpdateView, offers the actions.
 *
 * It decides nothing itself. The checker holds the state and PresentUpdate()
 * turns it into text; this only draws it and forwards the two buttons.
 */
class UpdateTab : public QWidget {
  Q_OBJECT

 public:
  explicit UpdateTab(QWidget* parent = nullptr);

 protected:
  void showEvent(QShowEvent* event) override;

 private:
  void render();
  void apply(const UpdateView& view);

  QPointer<UpdateChecker> checker_;

  QLabel* icon_;
  QLabel* headline_;
  QLabel* detail_;
  QLabel* notice_;
  QLabel* freshness_;
  QProgressBar* busy_;
  QPushButton* check_btn_;
  QPushButton* download_btn_;
  QToolButton* notes_toggle_;
  QTextBrowser* notes_;
};
