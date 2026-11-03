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

#include <QString>
#include <QStringList>

/**
 * @file NoteProtection.h
 * @brief Who an encrypted snapshot of a note is for.
 *
 * SDK-free. Recorded when the snapshot is made, so what the note says about
 * its readers describes the ciphertext it holds, not whatever the Key ToolBox
 * has checked since.
 */

namespace gf::notes {

/// The readers of one encrypted snapshot: keys by primary fingerprint, or a
/// passphrase when no key was checked.
struct ProtectionConfig {
  QStringList recipients;  ///< primary-key fingerprints, upper case, sorted
  bool passphrase = false;

  /// What Encrypt uses for the keys checked now: those keys, or a
  /// passphrase when there are none.
  static auto ForCheckedKeys(const QStringList& fingerprints)
      -> ProtectionConfig;

  friend auto operator==(const ProtectionConfig& a, const ProtectionConfig& b)
      -> bool {
    return a.recipients == b.recipients && a.passphrase == b.passphrase;
  }
  void Normalise();
};

/// What is known of a key here (GFGpgKeyPrimaryUid).
struct KeyFacts {
  QString fingerprint;
  QString label;
  bool found = false;
  bool can_encrypt = false;
  bool has_secret = false;
  bool expired = false;
  bool revoked = false;
  bool disabled = false;
  [[nodiscard]] auto Usable() const -> bool {
    return found && can_encrypt && !expired && !revoked && !disabled;
  }
  /// The label, or the fingerprint for a key that has none here.
  [[nodiscard]] auto DisplayName() const -> QString {
    return label.isEmpty() ? fingerprint : label;
  }
};

}  // namespace gf::notes
