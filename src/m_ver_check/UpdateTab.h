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
 * turns it into text; this only draws it and forwards the buttons. An
 * in-place update of an AppImage (PresentInstall()) is the one thing it runs:
 * the installer lives as long as the page, so closing it cancels a download.
 *
 * Top to bottom: the status, this build and the offer side by side, the
 * release notes (a few lines, the rest in a window of their own), one short
 * line on the update information with its details on request, and one row
 * of buttons.
 */
class UpdateTab : public QWidget {
  Q_OBJECT

 public:
  explicit UpdateTab(QWidget* parent = nullptr);

 protected:
  void showEvent(QShowEvent* event) override;
  void changeEvent(QEvent* event) override;

 private:
  void render();
  void apply(const UpdateView& view);
  void fill_details(const UpdateView& view);
  void fit_window();
  void fit_notes();
  void show_full_notes();
  void on_main_button();
  void install();

  QPointer<UpdateChecker> checker_;

  QLabel* icon_;
  QLabel* headline_;
  QLabel* detail_;
  QLabel* notice_;
  QLabel* build_caption_;
  QFormLayout* build_form_;
  QWidget* offer_column_;  ///< only with an offer
  QFormLayout* offer_form_;
  QLabel* beta_hint_;    ///< "A newer beta, X, is available"
  QTextBrowser* notes_;  ///< a few lines; full_notes_ opens the rest
  QLabel* full_notes_;
  QLabel* status_icon_;
  QLabel* status_text_;
  QToolButton* details_toggle_;
  QWidget* details_;  ///< rebuilt on each apply(): the rows change
  QVBoxLayout* details_layout_;
  QPushButton* copy_report_;
  QPushButton* download_btn_;
  QPushButton* check_btn_;
  UpdateView view_;  ///< as last drawn: what the buttons act on
  InstallState install_;
  QPointer<AppImageInstaller> installer_;
};
