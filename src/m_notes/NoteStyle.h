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

#include <QColor>
#include <QIcon>
#include <QWidget>

class QPainter;

namespace gf::notes {

/// @p label at @p factor times its font size, optionally heavier.
void NoteScaleFont(QWidget* w, double factor, int weight = -1);

/// "#rrggbb", what the editor page accepts.
auto NoteHex(const QColor& c) -> QString;

/**
 * @brief A small drawing, painted rather than loaded, so it is sharp at any
 * scale and takes the theme's colours.
 */
class NoteGlyph : public QWidget {
 public:
  enum class Kind : uint8_t {
    kLOCK,
    kKEY,
    kPASSPHRASE,
    kPERSON,
    kHIDDEN,
    kWARNING,
    kINFO,
    kCLOSE,
  };

  /// A glyph @p size pixels square; on a tinted disc when @p disc is set.
  NoteGlyph(Kind kind, int size, bool disc = false, QWidget* parent = nullptr);

  void SetKind(Kind kind);
  /// An explicit colour; invalid follows the palette's accent.
  void SetColor(const QColor& color);

  /// The same drawing as an icon, for buttons.
  static auto Icon(Kind kind, const QColor& color, int size) -> QIcon;
  /// Draws @p kind into @p r.
  static void Paint(QPainter& p, const QRectF& r, Kind kind,
                    const QColor& color, const QColor& cutout);

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  Kind kind_;
  bool disc_;
  QColor color_;
};

/// A rounded panel on the base colour, with the application's border.
class NoteCard : public QWidget {
 public:
  explicit NoteCard(QWidget* parent = nullptr);

 protected:
  void paintEvent(QPaintEvent* event) override;
};

}  // namespace gf::notes
