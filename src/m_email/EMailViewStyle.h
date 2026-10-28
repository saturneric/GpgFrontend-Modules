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

#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPalette>
#include <QSettings>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>
#include <array>
#include <functional>

#include "GFModule.h"
#include "GFModuleTheme.h"
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
inline auto EMailAccentColor(QWidget* w, bool positive) -> QColor {
  return gf::ui::ThemeColor(
      w, positive ? GF_UI_COLOR_ACCENT_POSITIVE : GF_UI_COLOR_ACCENT_NEGATIVE);
}

/// A size written the way the rest of the application writes it.
///
/// Pure: it formats into a local buffer and needs no context at all.
inline auto EMailHumanSize(qint64 bytes) -> QString {
  std::array<char, 64> text{};
  const auto written = GFUIHumanSize(bytes, text.data(), text.size());
  return written < 0 ? QString() : QString::fromUtf8(text.data(), written);
}

/**
 * @brief Holds the wait cursor for as long as it is in scope.
 *
 * Several things this module does on the GUI thread are not quick: serializing
 * a message re-encodes every attachment, and verifying its signed regions can
 * end up waiting on the agent. Done silently, the window simply stops
 * responding and the user is left deciding whether the program has hung.
 *
 * Scoped rather than a matched pair of calls because the work between them
 * returns early in several places, and an override cursor that is not restored
 * is left over the whole application.
 */
class EMailBusyCursor {
 public:
  EMailBusyCursor() { QApplication::setOverrideCursor(Qt::WaitCursor); }
  ~EMailBusyCursor() { QApplication::restoreOverrideCursor(); }

  EMailBusyCursor(const EMailBusyCursor&) = delete;
  auto operator=(const EMailBusyCursor&) -> EMailBusyCursor& = delete;
  EMailBusyCursor(EMailBusyCursor&&) = delete;
  auto operator=(EMailBusyCursor&&) -> EMailBusyCursor& = delete;
};

/// Recolours @p rule to the application's border colour.
///
/// Split out from EMailRule because this is the half a theme change has to run
/// again, and because a QFrame line takes its colour from WindowText rather
/// than from a role of its own -- written inline, that palette entry reads
/// like a mistake.
inline void EMailPaintRule(QFrame* rule) {
  auto palette = rule->palette();
  palette.setColor(QPalette::WindowText, gf::ui::BorderColor(rule));
  rule->setPalette(palette);
}

/// A one-pixel rule dividing two groups of things.
///
/// Pinned to one pixel rather than left to the frame's own metric: the default
/// line is a two-pixel bevel, which on a flat theme reads as a groove cut into
/// the page rather than as a division drawn on it.
inline auto EMailRule(QWidget* parent,
                      Qt::Orientation orientation = Qt::Horizontal) -> QFrame* {
  auto* rule = new QFrame(parent);
  rule->setFrameShape(orientation == Qt::Horizontal ? QFrame::HLine
                                                    : QFrame::VLine);
  rule->setFrameShadow(QFrame::Plain);
  if (orientation == Qt::Horizontal) {
    rule->setFixedHeight(1);
  } else {
    rule->setFixedWidth(1);
  }
  EMailPaintRule(rule);
  return rule;
}

/// A label that says something about the thing above it rather than being it.
///
/// Colour only, so it is safe to call again from a re-apply path.
inline void EMailMakeMuted(QLabel* label) {
  gf::ui::SetLabelColor(label, gf::ui::MutedColor(label));
}

/// Muted, and a shade smaller. The secondary line under a primary one.
///
/// Call it once, when the widget is built: it scales the font it is given, so
/// a second call scales an already-scaled font. See gf::ui::IsRestyle.
inline void EMailMakeSecondary(QLabel* label) {
  auto font = label->font();
  font.setPointSizeF(font.pointSizeF() * 0.92);
  label->setFont(font);
  EMailMakeMuted(label);
}

/// A heading: the loudest thing on its surface, and still the platform font.
///
/// Like EMailMakeSecondary, call once: it scales the font it is given.
inline void EMailMakeTitle(QLabel* label) {
  auto font = label->font();
  font.setPointSizeF(font.pointSizeF() * 1.15);
  font.setBold(true);
  label->setFont(font);
}

/**
 * @brief A sentence standing in for a table that has nothing in it.
 *
 * An empty list is a ruled box with column headings and no rows, which reads
 * as a table that failed to load rather than as "there is nothing here".
 * Saying which one it is takes a sentence, and the sentence takes the list's
 * place rather than sitting above it: a caption beside the blank box it is
 * explaining leaves the blank box on screen.
 */
inline auto EMailEmptyNotice(QWidget* parent, const QString& text = {})
    -> QLabel* {
  auto* label = new QLabel(text, parent);
  label->setAlignment(Qt::AlignCenter);
  label->setWordWrap(true);
  label->setMargin(24);
  EMailMakeSecondary(label);
  return label;
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
    const auto fill = gf::ui::Blend(palette().color(QPalette::Window),
                                    palette().color(QPalette::Base), 0.35);
    painter.setBrush(fill);
    painter.setPen(QPen(gf::ui::BorderColor(this), 1));

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
/// (Re)tints @p banner towards @p tint.
///
/// Mixed from the PARENT's window colour rather than from the banner's own.
/// The banner is already carrying the last mix, so mixing its own colour again
/// would take it one further step towards the tint on every call -- which,
/// once a theme change starts calling this, it does.
inline void EMailTintBanner(QFrame* banner, const QColor& tint,
                            double strength = 0.12) {
  const auto base =
      banner->parentWidget() != nullptr
          ? banner->parentWidget()->palette().color(QPalette::Window)
          : QApplication::palette().color(QPalette::Window);

  auto palette = banner->palette();
  palette.setColor(QPalette::Window, gf::ui::Blend(base, tint, strength));
  banner->setPalette(palette);
}

inline auto EMailTintedBanner(QWidget* parent, const QColor& tint,
                              double strength = 0.12) -> QFrame* {
  auto* banner = new QFrame(parent);
  banner->setAutoFillBackground(true);
  banner->setFrameShape(QFrame::NoFrame);
  EMailTintBanner(banner, tint, strength);
  return banner;
}

/// One of this module's own settings -- the Host keeps them in the module's
/// group -- or @p fallback when it was never stored.
inline auto EMailViewSetting(const char* key, const QVariant& fallback)
    -> QVariant {
  return gf::sdk::Setting(GFModuleSdkContext(), GF_SETTING_MODULE, key,
                          fallback);
}

inline void SetEMailViewSetting(const char* key, const QVariant& value) {
  gf::sdk::SetSetting(GFModuleSdkContext(), GF_SETTING_MODULE, key, value);
}

/**
 * @brief Which of this module's colours a tree cell is painted in.
 *
 * A cell is given a tone, not a colour. The tone is the DECISION -- "this is
 * subordinate", "this is a caveat" -- and the QColor is only the current
 * theme's answer to it, which is what lets the answer be asked for again after
 * the theme changes.
 *
 * Storing the decision is not merely tidier than recolouring by hand: the
 * Security view builds its rows out of arguments it does not keep and so
 * cannot rebuild them, and rebuilding the attachment list would silently drop
 * whatever the user had selected.
 */
enum class EMailTone : uint8_t {
  kDEFAULT,  ///< the palette's own text colour
  kMUTED,    ///< describes the cell beside it rather than being a value
  kGOOD,     ///< evidence of something having happened
  kWARN,     ///< a caveat; NOT a failure, and never red
  kDANGER,   ///< actually wrong, or built to deceive
};

/// Where the tone lives on an item. Far past Qt::UserRole, which these trees
/// already use for their own per-column payloads.
constexpr int kEMailToneRole = Qt::UserRole + 100;

inline auto EMailToneColor(QWidget* owner, EMailTone tone) -> QColor {
  switch (tone) {
    case EMailTone::kMUTED:
      return gf::ui::MutedColor(owner);
    case EMailTone::kGOOD:
      return EMailAccentColor(owner, true);
    case EMailTone::kWARN:
      return gf::ui::WarningColor(owner);
    case EMailTone::kDANGER:
      return gf::ui::ThemeColor(owner, GF_UI_COLOR_DANGER);
    case EMailTone::kDEFAULT:
      break;
  }
  return owner->palette().color(QPalette::WindowText);
}

/// Paints one cell, and records why, so it can be painted again later.
inline void EMailSetCellTone(QTreeWidgetItem* item, int column, EMailTone tone,
                             QWidget* owner) {
  item->setData(column, kEMailToneRole, static_cast<int>(tone));
  item->setForeground(column, EMailToneColor(owner, tone));
}

/// Repaints every cell that was given a tone, in the colours of the palette as
/// it now is.
///
/// Cells that were never toned are left alone: they follow the palette
/// already, and giving them an explicit brush would opt them out of the
/// selection and alternating-row colours the style draws for them.
inline void EMailRepaintTree(QTreeWidget* tree) {
  std::function<void(QTreeWidgetItem*)> repaint = [&](QTreeWidgetItem* item) {
    for (int column = 0; column < tree->columnCount(); ++column) {
      const auto tone = item->data(column, kEMailToneRole);
      if (!tone.isValid()) continue;
      item->setForeground(
          column, EMailToneColor(tree, static_cast<EMailTone>(tone.toInt())));
    }
    for (int i = 0; i < item->childCount(); ++i) repaint(item->child(i));
  };

  for (int i = 0; i < tree->topLevelItemCount(); ++i) {
    repaint(tree->topLevelItem(i));
  }
}

/// The palette-dependent half of this module's list styling.
///
/// A column header names a column; it is not a row of buttons. So it is given
/// the same muted colour as every other caption here. Header text is drawn in
/// ButtonText by most styles and in WindowText by some, so both are set rather
/// than guessed at.
///
/// Separate from EMailPolishTree because this is the half a theme change runs
/// again, and it must not touch the font -- the font it would scale has been
/// scaled already.
inline void EMailPaintTreeHeader(QTreeWidget* tree) {
  auto* header = tree->header();
  const auto muted = gf::ui::MutedColor(tree);

  auto palette = header->palette();
  palette.setColor(QPalette::ButtonText, muted);
  palette.setColor(QPalette::WindowText, muted);
  palette.setColor(QPalette::Text, muted);
  header->setPalette(palette);
}

/// The once-only half: everything about a list that is not a colour.
///
/// Losing the frame does most of the work. These lists sit inside a tab page
/// that is already a bordered surface, so a sunken box around them draws the
/// same edge twice a few pixels apart, and that doubled edge is most of what
/// makes these pages read as a stack of boxes.
inline void EMailPolishTree(QTreeWidget* tree) {
  tree->setFrameShape(QFrame::NoFrame);
  tree->setIndentation(14);

  auto* header = tree->header();
  header->setHighlightSections(false);
  header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  auto font = header->font();
  font.setPointSizeF(font.pointSizeF() * 0.92);
  header->setFont(font);

  EMailPaintTreeHeader(tree);
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
          return gf::ui::WarningColor(this);
        case EMailStatusState::kPENDING:
        case EMailStatusState::kUNKNOWN:
          break;
      }
      return gf::ui::MutedColor(this);
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
