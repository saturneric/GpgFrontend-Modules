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

#include <QMetaType>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief A password, held somewhere it can actually be erased.
 *
 * QString and QByteArray are implicitly shared, and their mutating methods
 * detach before writing. So `password.fill('\0')` on a string that anything
 * else still references zeroes a FRESH copy and drops the reference to the
 * original, which is then freed with the plaintext still in it. Every wipe in
 * this module used to work that way: they read as protection and performed
 * none. One comment in the settings page had already worked this out and
 * declined to wipe for exactly that reason, while every other site went on
 * doing it.
 *
 * std::vector<char> has no sharing, so overwriting it overwrites the only
 * copy. The buffer is wiped on destruction and on every early exit, and the
 * type is move-only so a copy cannot be made by accident.
 *
 * What this does NOT claim: vmime's authenticator interface hands back a
 * std::string by value, and vmime then builds further copies of its own while
 * encoding AUTH. Those are outside anything this module owns and are not
 * wiped. StdStringCopy() is named to say so at the call site.
 */
class EMailSecret {
 public:
  EMailSecret() = default;
  ~EMailSecret() { Wipe(); }

  /// Takes the bytes of a NUL-terminated C string, then wipes the source.
  /// For the SDK's secure buffers, which arrive as char*.
  static auto AdoptCString(char* source) -> std::shared_ptr<EMailSecret>;

  /// Copies from a QString, for text the user typed into a field. The QString
  /// itself cannot be erased -- see the class note -- so the shorter its life
  /// the better; this exists to end it early.
  static auto CopyFrom(const QString& text) -> std::shared_ptr<EMailSecret>;

  EMailSecret(const EMailSecret&) = delete;
  auto operator=(const EMailSecret&) -> EMailSecret& = delete;

  EMailSecret(EMailSecret&& other) noexcept
      : bytes_(std::move(other.bytes_)) {
    other.bytes_.clear();
  }

  auto operator=(EMailSecret&& other) noexcept -> EMailSecret& {
    if (this != &other) {
      Wipe();
      bytes_ = std::move(other.bytes_);
      other.bytes_.clear();
    }
    return *this;
  }

  [[nodiscard]] auto IsEmpty() const -> bool { return bytes_.empty(); }
  [[nodiscard]] auto Size() const -> size_t { return bytes_.size(); }

  /// A copy for a third-party API that will not take anything else. The copy
  /// is the caller's problem and cannot be wiped by this class.
  [[nodiscard]] auto StdStringCopy() const -> std::string {
    return std::string(bytes_.data(), bytes_.size());
  }

  /// Overwrites the buffer and releases it. Safe to call more than once.
  void Wipe();

 private:
  std::vector<char> bytes_;
};

/// Shared so it can cross a queued connection without duplicating the secret:
/// Qt copies a queued argument, and copying the pointer is not copying the
/// password. The bytes are wiped when the last holder goes.
using EMailSecretPtr = std::shared_ptr<EMailSecret>;

Q_DECLARE_METATYPE(EMailSecretPtr)
