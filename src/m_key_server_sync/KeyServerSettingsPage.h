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

#include <GFModule.h>

#include <QWidget>
#include <memory>

#include "KeyServerList.h"
#include "KeyServerProbe.h"

class Ui_KeyServerSettingsPage;

/**
 * @brief The Key Servers page of the application's Settings dialog.
 *
 * Owned by this module, in a page the application's Settings dialog owns:
 * LoadSettings() and ApplySettings() are the typed SettingsWidget interface,
 * so edits here are staged and only written when the user accepts.
 */
class KeyServerSettingsPage : public QWidget, public gf::ui::SettingsWidget {
  Q_OBJECT

 public:
  explicit KeyServerSettingsPage(QWidget* parent = nullptr);

 public:
  /**
   * @brief Load the stored servers, discarding anything staged.
   *
   * Called by the Host both when the page is built and when the user
   * cancels out of a restart confirmation.
   */
  void LoadSettings() override;

  /// Persist the staged servers. Called by the Host on OK.
  auto ApplySettings() -> bool override;

 private slots:
  void slot_add();
  void slot_remove();
  void slot_set_default();
  void slot_test_selected();

 private:
  void refresh_table();
  void start_probe(const QString& url);
  void apply_probe_result(const KeyServerProber::Result& result);
  void switch_ui_busy(bool busy);

  /**
   * @brief The row the user has selected, or -1.
   */
  [[nodiscard]] auto selected_row() const -> int;

  std::shared_ptr<Ui_KeyServerSettingsPage> ui_;

  /// Staged, not stored: the dialog's Cancel has to be able to throw all of
  /// this away, which it does by calling LoadSettings() again.
  QList<KeyServerEntry> entries_;
  QString default_url_;
  int pending_probes_{0};
};

