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

class QLabel;
class QTimer;
class QPlainTextEdit;
class QPushButton;

/**
 * @brief The Instant Messaging page of the application's Settings dialog.
 *
 * Holds the shared "Message Book Phrase", the secret that whitens every
 * instant-messaging token so it cannot be detected as a GpgFrontend/PGP
 * message. Both sides must set the same phrase; an empty phrase uses a shared
 * default that only hides the format from naive scanners.
 *
 * Edits are staged and written only on ApplySettings(), to the module's
 * secure cache, never to the settings file.
 */
class ImSettingsPage : public QWidget, public gf::ui::SettingsWidget {
 public:
  explicit ImSettingsPage(QWidget* parent = nullptr);

  void LoadSettings() override;
  auto ApplySettings() -> bool override;

 private:
  void set_revealed(bool revealed);
  void set_phrase(const QString& phrase);
  void schedule_fingerprint_update();
  void start_fingerprint_update();
  void set_fingerprint(const QString& fingerprint);

  QPlainTextEdit* phrase_edit_{};   ///< shows the phrase, or its mask
  QPushButton* reveal_button_{};    ///< toggles between the phrase and the mask
  QLabel* fingerprint_label_{};     ///< our fingerprint, e.g. "3F9A-1C4E"
  QLabel* phrase_state_label_{};    ///< "no phrase set" / phrase length
  QTimer* fingerprint_timer_{};     ///< debounce for typing
  QString phrase_;                  ///< the edited phrase, source of truth
  QString fingerprint_;             ///< last computed fingerprint, may be empty
  bool revealed_{false};            ///< whether the phrase is on screen
  bool syncing_{false};             ///< guards programmatic edits of the view
  quint64 fingerprint_request_{0};  ///< discards results of stale derivations
};

/// The configured book phrase, or empty for the default book.
auto ImBookPhrase() -> QString;

/// Replace the configured book phrase; empty clears it.
void ImSetBookPhrase(const QString& phrase);
