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
#include <QFrame>
#include <QLabel>
#include <QPainter>
#include <QPalette>
#include <QVBoxLayout>
#include <QWidget>

#include "GFModuleCommonUtils.hpp"
#include "GFSDKUI.h"

/**
 * @brief The application's own colours, reached through the SDK.
 *
 * Shared by every view in this module so the e-mail tab follows the user's
 * theme and reads as part of the program rather than as a plug-in that picked
 * its own greys. A getter returning 0 means the widget was not usable, so the
 * palette's own colour is the honest fallback.
 *
 * Note the convention the host documents in UIStyle.h: a negative state is
 * de-emphasised rather than red. Red belongs to irreversible acts and to
 * secrets travelling in the clear, not to "this part is unsigned".
 */
inline auto EMailThemeColor(QWidget* w, uint32_t (*getter)(void*)) -> QColor {
  const auto rgba = getter(w);
  return rgba == 0 ? w->palette().color(QPalette::WindowText)
                   : QColor::fromRgba(rgba);
}

inline auto EMailMutedColor(QWidget* w) -> QColor {
  return EMailThemeColor(w, &GFUIMutedTextColor);
}

inline auto EMailWarningColor(QWidget* w) -> QColor {
  return EMailThemeColor(w, &GFUIWarningColor);
}

inline auto EMailAccentColor(QWidget* w, bool positive) -> QColor {
  const auto rgba = GFUIAccentColor(w, positive ? 1 : 0);
  return rgba == 0 ? w->palette().color(QPalette::WindowText)
                   : QColor::fromRgba(rgba);
}

/// A size written the way the rest of the application writes it.
inline auto EMailHumanSize(qint64 bytes) -> QString {
  return UnStrDup(GFUIHumanSize(bytes));
}

/// The application's own border colour, for the one-pixel rules around cards.
inline auto EMailBorderColor(QWidget* w) -> QColor {
  return EMailThemeColor(w, &GFUIBorderColor);
}

/// @p a mixed with @p b, @p t of the way towards @p b.
///
/// Mixed rather than given an alpha: these colours are painted onto widgets
/// that fill their own background, where a translucent brush composites
/// against whatever Qt last drew there instead of against the page.
inline auto EMailBlend(const QColor& a, const QColor& b, double t) -> QColor {
  return QColor::fromRgbF(a.redF() * (1 - t) + b.redF() * t,
                          a.greenF() * (1 - t) + b.greenF() * t,
                          a.blueF() * (1 - t) + b.blueF() * t);
}

/// Paints @p label in @p color without a stylesheet.
///
/// Through the palette rather than QSS, which is the convention everywhere
/// else in this module: a stylesheet on one label silently opts that widget
/// out of the host's theme for every other property too.
inline void EMailSetLabelColor(QLabel* label, const QColor& color) {
  auto palette = label->palette();
  palette.setColor(QPalette::WindowText, color);
  palette.setColor(QPalette::Text, color);
  label->setPalette(palette);
}

/// A label that says something about the thing above it rather than being it.
inline void EMailMakeMuted(QLabel* label) {
  EMailSetLabelColor(label, EMailMutedColor(label));
}

/// Muted, and a shade smaller. The secondary line under a primary one.
inline void EMailMakeSecondary(QLabel* label) {
  auto font = label->font();
  font.setPointSizeF(font.pointSizeF() * 0.92);
  label->setFont(font);
  EMailMakeMuted(label);
}

/// A heading: the loudest thing on its surface, and still the platform font.
inline void EMailMakeTitle(QLabel* label) {
  auto font = label->font();
  font.setPointSizeF(font.pointSizeF() * 1.15);
  font.setBold(true);
  label->setFont(font);
}

/**
 * @brief A bordered surface holding one group of related things.
 *
 * Drawn rather than stylesheeted, for the reason given above EMailMakeMuted:
 * a QSS border would also freeze the frame's colours against the host theme.
 * The border and the fill are both derived from the live palette, so the card
 * follows a theme change without anything having to be rebuilt.
 */
class EMailCardFrame : public QFrame {
 public:
  explicit EMailCardFrame(QWidget* parent = nullptr) : QFrame(parent) {
    setFrameShape(QFrame::NoFrame);
  }

 protected:
  void paintEvent(QPaintEvent* event) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Far enough from the page to read as a separate material, not far enough
    // to compete with the text sitting on it.
    const auto fill = EMailBlend(palette().color(QPalette::Window),
                                 palette().color(QPalette::Base), 0.35);
    painter.setBrush(fill);
    painter.setPen(QPen(EMailBorderColor(this), 1));

    // Inset by half the pen so the stroke lands inside the widget instead of
    // being clipped in half by its own edge.
    const auto box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.drawRoundedRect(box, 8, 8);

    QFrame::paintEvent(event);
  }
};

/// A card, with an optional bold heading as its first row. The returned
/// layout is the card's own, so callers add their rows straight to it.
inline auto EMailCard(QWidget* parent, const QString& title = {})
    -> EMailCardFrame* {
  auto* card = new EMailCardFrame(parent);
  auto* layout = new QVBoxLayout(card);
  layout->setContentsMargins(14, 12, 14, 12);
  layout->setSpacing(8);

  if (!title.isEmpty()) {
    auto* heading = new QLabel(title, card);
    auto font = heading->font();
    font.setBold(true);
    heading->setFont(font);
    layout->addWidget(heading);
  }
  return card;
}

/**
 * @brief A tinted strip carrying one sentence about the state of things.
 *
 * The tint is a mix towards @p tint rather than @p tint itself: a banner is a
 * surface the text has to stay readable on, not a coloured block.
 */
inline auto EMailTintedBanner(QWidget* parent, const QColor& tint,
                              double strength = 0.12) -> QFrame* {
  auto* banner = new QFrame(parent);
  banner->setAutoFillBackground(true);
  banner->setFrameShape(QFrame::NoFrame);

  auto palette = banner->palette();
  palette.setColor(QPalette::Window,
                   EMailBlend(palette.color(QPalette::Window), tint, strength));
  banner->setPalette(palette);
  return banner;
}

/// What one line of a result is currently known to be.
enum class EMailStatusState : uint8_t {
  kPENDING,  ///< still happening
  kGOOD,     ///< happened, and we have evidence of it
  kUNKNOWN,  ///< not observable from here; NOT a failure
  kBAD,      ///< it did not happen
};

/**
 * @brief A small disc standing for one EMailStatusState.
 *
 * Never red: per the convention above, red belongs to irreversible acts and to
 * secrets in the clear. A failed send is de-emphasised in warning colour, and
 * "unknown" is muted rather than dressed up as either outcome.
 */
class EMailStatusDot : public QWidget {
 public:
  explicit EMailStatusDot(QWidget* parent = nullptr) : QWidget(parent) {
    setFixedSize(kDiameter + 4, kDiameter + 4);
  }

  void SetState(EMailStatusState state) {
    if (state_ == state) return;
    state_ = state;
    update();
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto color = [this]() -> QColor {
      switch (state_) {
        case EMailStatusState::kGOOD:
          return EMailAccentColor(this, true);
        case EMailStatusState::kBAD:
          return EMailWarningColor(this);
        case EMailStatusState::kPENDING:
        case EMailStatusState::kUNKNOWN:
          break;
      }
      return EMailMutedColor(this);
    }();

    painter.setPen(Qt::NoPen);
    // Pending is drawn hollow: something that has not resolved yet must not
    // look like something that resolved to "nothing to report".
    if (state_ == EMailStatusState::kPENDING) {
      painter.setBrush(Qt::NoBrush);
      painter.setPen(QPen(color, 1.5));
    } else {
      painter.setBrush(color);
    }

    const auto inset = state_ == EMailStatusState::kPENDING ? 1.0 : 0.0;
    painter.drawEllipse(QRectF(2 + inset, 2 + inset, kDiameter - 2 * inset,
                               kDiameter - 2 * inset));
  }

 private:
  static constexpr int kDiameter = 10;
  EMailStatusState state_{EMailStatusState::kPENDING};
};
