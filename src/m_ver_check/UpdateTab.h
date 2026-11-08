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

#include "UpdatePresentation.h"

class AppImageInstaller;
class UpdateChecker;

/**
 * @brief The update dialog's content: renders UpdateView, offers the actions.
 *
 * It decides nothing itself. The checker holds the state and PresentUpdate()
 * turns it into text; this only draws it and forwards the two buttons. An
 * in-place update of an AppImage (PresentInstall()) is the one thing it runs:
 * the installer lives as long as the page, so closing it cancels a download.
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
  void fit_window();
  void on_main_button();
  void install();

  QPointer<UpdateChecker> checker_;

  QLabel* icon_;
  QLabel* headline_;
  QLabel* detail_;
  QLabel* notice_;
  QLabel* freshness_;
  QPushButton* check_btn_;
  QPushButton* download_btn_;
  QCheckBox* beta_;  ///< "Also offer beta versions"; release builds only
  UpdateView view_;  ///< as last drawn: what the main button acts on
  InstallState install_;
  QPointer<AppImageInstaller> installer_;
  QToolButton* notes_toggle_;
  QTextBrowser* notes_;
};
