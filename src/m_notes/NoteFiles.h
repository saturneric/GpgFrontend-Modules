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

#include <QByteArray>
#include <QString>

/**
 * @file NoteFiles.h
 * @brief Writing a note's file. SDK-free.
 *
 * The one way bytes reach the disk. It takes ciphertext, made only from an
 * encryption result -- or, when the user saves a note they never encrypted,
 * a PlainTextPack, which says so at every call site.
 */

namespace gf::notes {

/// Bytes that came out of an OpenPGP encryption, and nothing else.
class EncryptedPackage {
 public:
  /// Only from the output of an encryption (see NoteCrypto).
  static auto FromCiphertext(QByteArray ciphertext) -> EncryptedPackage {
    return EncryptedPackage(std::move(ciphertext));
  }
  [[nodiscard]] auto Bytes() const -> const QByteArray& { return bytes_; }

 private:
  explicit EncryptedPackage(QByteArray bytes) : bytes_(std::move(bytes)) {}
  QByteArray bytes_;
};

/// A TextPack the user chose to save without encryption, and nothing else.
class PlainTextPack {
 public:
  /// Only for a note that was never encrypted, saved as such on request.
  static auto FromUnencryptedNote(QByteArray zip) -> PlainTextPack {
    return PlainTextPack(std::move(zip));
  }
  [[nodiscard]] auto Bytes() const -> const QByteArray& { return bytes_; }

 private:
  explicit PlainTextPack(QByteArray bytes) : bytes_(std::move(bytes)) {}
  QByteArray bytes_;
};

/**
 * @brief Replace @p path with @p package, atomically: a temporary file in
 * the same directory, owner-only, then a rename. A symbolic link at @p path
 * is refused rather than followed. On any failure the old file is untouched
 * and no temporary is left behind.
 */
auto WriteNoteFile(const QString& path, const EncryptedPackage& package,
                   QString* why) -> bool;
/// The same, for a note saved without encryption.
auto WriteNoteFile(const QString& path, const PlainTextPack& pack, QString* why)
    -> bool;

/// Read a note file; refused when it is a link or larger than @p max_bytes.
auto ReadNoteFile(const QString& path, qint64 max_bytes, QByteArray* out,
                  QString* why) -> bool;

}  // namespace gf::notes
