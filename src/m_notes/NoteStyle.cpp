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

#include "NoteStyle.h"

#include <GFModule.h>
#include <GFModuleTheme.h>
#include <GFSDKUI.h>

#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace gf::notes {

void NoteScaleFont(QWidget* w, double factor, int weight) {
  auto f = w->font();
  if (f.pointSizeF() > 0) {
    f.setPointSizeF(f.pointSizeF() * factor);
  } else {
    f.setPixelSize(qRound(f.pixelSize() * factor));
  }
  if (weight > 0) f.setWeight(static_cast<QFont::Weight>(weight));
  w->setFont(f);
}

auto NoteHex(const QColor& c) -> QString { return c.name(QColor::HexRgb); }

// --- glyphs ------------------------------------------------------------------

NoteGlyph::NoteGlyph(Kind kind, int size, bool disc, QWidget* parent)
    : QWidget(parent), kind_(kind), disc_(disc) {
  setFixedSize(size, size);
  setAttribute(Qt::WA_TranslucentBackground);
}

void NoteGlyph::SetKind(Kind kind) {
  kind_ = kind;
  update();
}

void NoteGlyph::SetColor(const QColor& color) {
  color_ = color;
  update();
}

auto NoteGlyph::Icon(Kind kind, const QColor& color, int size) -> QIcon {
  const qreal dpr = qApp->devicePixelRatio();
  QPixmap pm(QSize(size, size) * dpr);
  pm.setDevicePixelRatio(dpr);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  Paint(p, QRectF(0, 0, size, size), kind, color, Qt::transparent);
  p.end();
  return QIcon(pm);
}

void NoteGlyph::paintEvent(QPaintEvent* /*event*/) {
  QPainter p(this);
  const auto accent =
      color_.isValid() ? color_ : palette().color(QPalette::Highlight);
  QRectF r = rect();
  QColor cutout = palette().color(QPalette::Window);
  if (disc_) {
    p.setRenderHint(QPainter::Antialiasing);
    const auto fill =
        gf::ui::Blend(palette().color(QPalette::Window), accent, 0.16);
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawEllipse(r);
    cutout = fill;
    const qreal inset = r.width() * 0.24;
    r = r.adjusted(inset, inset, -inset, -inset);
  }
  Paint(p, r, kind_, accent, cutout);
}

void NoteGlyph::Paint(QPainter& p, const QRectF& r, Kind kind,
                      const QColor& color, const QColor& cutout) {
  p.save();
  p.setRenderHint(QPainter::Antialiasing);
  p.translate(r.topLeft());
  const qreal s = r.width();
  auto at = [s](qreal x, qreal y) { return QPointF(x * s, y * s); };
  QPen pen(color, qMax<qreal>(1.25, s * 0.085), Qt::SolidLine, Qt::RoundCap,
           Qt::RoundJoin);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);

  // The cut-outs (keyhole, dots) are drawn by clearing when there is no
  // background colour to paint them in.
  auto cut = [&p, &cutout](const QPainterPath& path) {
    p.save();
    p.setPen(Qt::NoPen);
    if (cutout.alpha() == 0) {
      p.setCompositionMode(QPainter::CompositionMode_Clear);
      p.setBrush(Qt::black);
    } else {
      p.setBrush(cutout);
    }
    p.drawPath(path);
    p.restore();
  };

  switch (kind) {
    case Kind::kLOCK: {
      QPainterPath shackle;
      shackle.moveTo(at(0.31, 0.46));
      shackle.lineTo(at(0.31, 0.33));
      shackle.arcTo(QRectF(at(0.31, 0.12), at(0.69, 0.5)), 180, -180);
      shackle.lineTo(at(0.69, 0.46));
      p.drawPath(shackle);
      p.setPen(Qt::NoPen);
      p.setBrush(color);
      p.drawRoundedRect(QRectF(at(0.17, 0.44), at(0.83, 0.9)), s * 0.09,
                        s * 0.09);
      QPainterPath hole;
      hole.addEllipse(at(0.5, 0.62), s * 0.065, s * 0.065);
      hole.addRoundedRect(QRectF(at(0.47, 0.63), at(0.53, 0.76)), s * 0.02,
                          s * 0.02);
      cut(hole);
      break;
    }
    case Kind::kKEY: {
      p.drawEllipse(at(0.3, 0.5), s * 0.16, s * 0.16);
      p.drawLine(at(0.46, 0.5), at(0.88, 0.5));
      p.drawLine(at(0.74, 0.5), at(0.74, 0.64));
      p.drawLine(at(0.86, 0.5), at(0.86, 0.6));
      break;
    }
    case Kind::kPASSPHRASE: {
      p.drawRoundedRect(QRectF(at(0.08, 0.28), at(0.92, 0.72)), s * 0.1,
                        s * 0.1);
      p.setPen(Qt::NoPen);
      p.setBrush(color);
      for (const qreal x : {0.3, 0.5, 0.7})
        p.drawEllipse(at(x, 0.5), s * 0.055, s * 0.055);
      break;
    }
    case Kind::kPERSON:
    case Kind::kHIDDEN: {
      if (kind == Kind::kHIDDEN) {
        pen.setStyle(Qt::DotLine);
        p.setPen(pen);
      }
      p.drawEllipse(at(0.5, 0.33), s * 0.17, s * 0.17);
      QPainterPath body;
      body.moveTo(at(0.16, 0.9));
      body.cubicTo(at(0.16, 0.66), at(0.32, 0.58), at(0.5, 0.58));
      body.cubicTo(at(0.68, 0.58), at(0.84, 0.66), at(0.84, 0.9));
      p.drawPath(body);
      break;
    }
    case Kind::kWARNING: {
      QPainterPath tri;
      tri.moveTo(at(0.5, 0.1));
      tri.lineTo(at(0.93, 0.86));
      tri.lineTo(at(0.07, 0.86));
      tri.closeSubpath();
      p.drawPath(tri);
      p.drawLine(at(0.5, 0.38), at(0.5, 0.58));
      p.drawPoint(at(0.5, 0.72));
      break;
    }
    case Kind::kINFO: {
      p.drawEllipse(at(0.5, 0.5), s * 0.42, s * 0.42);
      p.drawLine(at(0.5, 0.46), at(0.5, 0.7));
      p.drawPoint(at(0.5, 0.31));
      break;
    }
    case Kind::kCLOSE: {
      p.drawLine(at(0.27, 0.27), at(0.73, 0.73));
      p.drawLine(at(0.73, 0.27), at(0.27, 0.73));
      break;
    }
  }
  p.restore();
}

// --- the card ----------------------------------------------------------------

NoteCard::NoteCard(QWidget* parent) : QWidget(parent) {}

void NoteCard::paintEvent(QPaintEvent* /*event*/) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(QPen(gf::ui::BorderColor(this), 1));
  p.setBrush(palette().color(QPalette::Base));
  p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
}

}  // namespace gf::notes
