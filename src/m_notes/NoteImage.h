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
#include <optional>

#include "NoteModel.h"

/**
 * @file NoteImage.h
 * @brief An image pasted into a note: checked, then stored as a fresh
 * encoding of its pixels -- never the bytes as they came.
 *
 * SDK-free (Qt Gui). Re-encoding drops every metadata block a file can carry
 * (EXIF location, camera, comments) and anything trailing the image data.
 */

namespace gf::notes {

inline constexpr qint64 kMaxImageBytes = Budgets{}.max_asset_bytes;
inline constexpr qint64 kMaxImagePixels = 40LL * 1000 * 1000;

struct StoredImage {
  QString path;  ///< "assets/<sha256 prefix>.<png|jpg>"
  QString mime;
  QByteArray data;
};

/// The image to store for @p bytes, or nullopt and why not.
auto PrepareImage(const QByteArray& bytes, QString* why)
    -> std::optional<StoredImage>;

/// Whether @p bytes decode as an image of an allowed format, within limits:
/// for assets a note already carries, which are kept as they are.
auto IsDisplayableImage(const QByteArray& bytes, QString* mime) -> bool;

}  // namespace gf::notes
