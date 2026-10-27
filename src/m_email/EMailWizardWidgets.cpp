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

#include "EMailWizardWidgets.h"

#include <QAbstractButton>
#include <QBoxLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include "EMailViewStyle.h"

namespace {

/// Word-wrapped labels ask for almost no width, so a wizard would open as a
/// tall ribbon; stating the size once makes everything wrap against it.
constexpr int kWizardMinimumWidth = 520;
constexpr int kWizardMinimumHeight = 480;
/// What it opens at when nothing is remembered: room for every step's
/// ordinary content at the default font, so most people never see a scroll
/// bar, and not so much that a short step floats in empty space.
constexpr int kWizardDefaultWidth = 620;
constexpr int kWizardDefaultHeight = 540;

/// One rhythm for every step: between the header and the body, and between
/// the sections of a body.
constexpr int kHeaderSpacing = 20;
constexpr int kSectionSpacing = 18;

/// Around each step's content, the same on every page.
constexpr int kPageMarginH = 16;
constexpr int kPageMarginV = 12;

/// The diameter of one progress mark: small, so the marks stay below the
/// title in weight.
constexpr int kStepMark = 8;

/// The side of a status icon.
constexpr int kStatusIcon = 16;

auto AvailableGeometry(QWidget* widget) -> QRect {
  auto* screen = widget->screen();
  if (screen == nullptr) screen = QGuiApplication::primaryScreen();
  if (screen == nullptr) return {0, 0, 1024, 768};
  return screen->availableGeometry();
}

/// Moved and, only if it must be, shrunk to fit on @p available.
auto ClampToAvailable(QRect rect, const QRect& available) -> QRect {
  rect.setWidth(std::min(rect.width(), available.width()));
  rect.setHeight(std::min(rect.height(), available.height()));
  if (rect.right() > available.right()) rect.moveRight(available.right());
  if (rect.bottom() > available.bottom()) rect.moveBottom(available.bottom());
  if (rect.left() < available.left()) rect.moveLeft(available.left());
  if (rect.top() < available.top()) rect.moveTop(available.top());
  return rect;
}

constexpr int kTrackWidth = 34;
constexpr int kTrackHeight = 18;
constexpr int kSwitchGap = 10;

}  // namespace

auto EMailWizardPage::isComplete() const -> bool {
  return complete ? complete() : QWizardPage::isComplete();
}

auto EMailWizardPage::validatePage() -> bool {
  return validate ? validate() : QWizardPage::validatePage();
}

EMailStepIndicator::EMailStepIndicator(QStringList names, int current,
                                       QWidget* parent)
    : QWidget(parent), names_(std::move(names)), current_(current) {
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setAccessibleName(EMailWizard::tr("Step %1 of %2: %3")
                        .arg(current_ + 1)
                        .arg(names_.size())
                        .arg(names_.value(current_)));
}

auto EMailStepIndicator::sizeHint() const -> QSize {
  return {kWizardMinimumWidth, fontMetrics().height() + 6};
}

auto EMailStepIndicator::minimumSizeHint() const -> QSize {
  return {static_cast<int>(names_.size()) * (kStepMark + 8),
          fontMetrics().height() + 6};
}

void EMailStepIndicator::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const auto count = static_cast<int>(names_.size());
  if (count == 0) return;

  const auto accent = palette().color(QPalette::Highlight);
  const auto border = EMailBorderColor(this);
  const auto muted = EMailMutedColor(this);
  const auto text = palette().color(QPalette::WindowText);
  const auto slot = width() / count;
  const auto mid = height() / 2.0;
  const QFontMetrics metrics(font());

  for (int i = 0; i < count; ++i) {
    const auto left = i * slot;
    const auto done = i < current_;
    const auto here = i == current_;
    const QRectF mark(left, mid - (kStepMark / 2.0), kStepMark, kStepMark);

    // Done and current are filled, what is still ahead is outlined: the
    // difference reads without colour too.
    p.setPen(QPen(here || done ? accent : border, 1.2));
    if (here || done) {
      p.setBrush(accent);
    } else {
      p.setBrush(Qt::NoBrush);
    }
    p.drawEllipse(mark.adjusted(0.6, 0.6, -0.6, -0.6));

    p.setPen(here ? text : muted);

    // The line to the next mark takes what the name leaves over.
    const auto text_left = static_cast<int>(mark.right()) + 6;
    const auto room = slot - (text_left - left) - 10;
    const auto name =
        metrics.elidedText(names_.at(i), Qt::ElideRight, std::max(0, room));
    const auto name_width = metrics.horizontalAdvance(name);
    p.drawText(QRect(text_left, 0, std::max(0, room), height()),
               Qt::AlignLeft | Qt::AlignVCenter, name);

    if (i + 1 < count) {
      const auto line_left = text_left + name_width + 8;
      const auto line_right = left + slot - 6;
      if (line_right > line_left) {
        p.setPen(QPen(border, 1));
        p.drawLine(QPointF(line_left, mid), QPointF(line_right, mid));
      }
    }
  }
}

EMailWrapRow::EMailWrapRow(QWidget* parent)
    : QWidget(parent), layout_(new QBoxLayout(QBoxLayout::LeftToRight, this)) {
  layout_->setContentsMargins(0, 0, 0, 0);
  layout_->setSpacing(kSpacing);
  layout_->addStretch(1);
}

void EMailWrapRow::AddWidget(QWidget* widget) {
  // Before the trailing stretch, so a row stays packed to the left.
  layout_->insertWidget(static_cast<int>(widgets_.size()), widget);
  widgets_.append(widget);
}

auto EMailWrapRow::row_width() const -> int {
  int width = 0;
  for (auto* widget : widgets_) width += widget->sizeHint().width();
  return width + (kSpacing * static_cast<int>(
                                 std::max<qsizetype>(0, widgets_.size() - 1)));
}

auto EMailWrapRow::minimumSizeHint() const -> QSize {
  // As narrow as the widest single widget: that much it can always be.
  int widest = 0;
  for (auto* widget : widgets_) {
    widest = std::max(widest, widget->sizeHint().width());
  }
  return {widest, QWidget::minimumSizeHint().height()};
}

void EMailWrapRow::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  const auto direction = row_width() <= width() ? QBoxLayout::LeftToRight
                                                : QBoxLayout::TopToBottom;
  if (layout_->direction() == direction) return;
  layout_->setDirection(direction);
  layout_->setSpacing(direction == QBoxLayout::LeftToRight ? kSpacing : 6);
  updateGeometry();
}

EMailStatusIcon::EMailStatusIcon(QWidget* parent) : QWidget(parent) {
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void EMailStatusIcon::SetState(int state, const QString& accessible_name) {
  state_ = state;
  setAccessibleName(accessible_name);
  update();
}

auto EMailStatusIcon::sizeHint() const -> QSize {
  return {kStatusIcon, kStatusIcon};
}

void EMailStatusIcon::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const auto state = static_cast<EMailStatusState>(state_);
  const QRectF box = QRectF(rect()).adjusted(1, 1, -1, -1);

  QColor color;
  switch (state) {
    case EMailStatusState::kGOOD:
      color = EMailAccentColor(this, true);
      break;
    case EMailStatusState::kBAD:
      color = EMailWarningColor(this);
      break;
    case EMailStatusState::kPENDING:
    case EMailStatusState::kUNKNOWN:
      color = EMailMutedColor(this);
      break;
  }

  // Unresolved is an open ring: it must not look like an answer.
  if (state == EMailStatusState::kPENDING) {
    p.setPen(QPen(color, 1.6));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(box.adjusted(1, 1, -1, -1));
    return;
  }

  p.setPen(Qt::NoPen);
  p.setBrush(color);
  p.drawEllipse(box);

  const auto mark = palette().color(QPalette::Base);
  p.setPen(QPen(mark, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  p.setBrush(Qt::NoBrush);
  const auto c = box.center();
  const auto r = box.width() / 2.0;
  switch (state) {
    case EMailStatusState::kGOOD: {
      // A tick.
      const std::array<QPointF, 3> tick{
          QPointF(c.x() - (r * 0.45), c.y() + (r * 0.02)),
          QPointF(c.x() - (r * 0.1), c.y() + (r * 0.38)),
          QPointF(c.x() + (r * 0.48), c.y() - (r * 0.35))};
      p.drawPolyline(tick.data(), static_cast<int>(tick.size()));
      break;
    }
    case EMailStatusState::kBAD:
      // An exclamation mark.
      p.drawLine(QPointF(c.x(), c.y() - (r * 0.5)),
                 QPointF(c.x(), c.y() + (r * 0.1)));
      p.drawPoint(QPointF(c.x(), c.y() + (r * 0.48)));
      break;
    default:
      // A dash: known not to be an answer.
      p.drawLine(QPointF(c.x() - (r * 0.42), c.y()),
                 QPointF(c.x() + (r * 0.42), c.y()));
      break;
  }
}

EMailWizard::EMailWizard(QString name, const QString& finish_text,
                         QStringList steps, QWidget* parent)
    : QWizard(parent), name_(std::move(name)), steps_(std::move(steps)) {
  setModal(true);

  // No header of QWizard's own -- each page carries the shared one, so the
  // steps look the same on every platform. Classic rather than Modern: Modern
  // paints the page in the base colour once its header is gone, which turns
  // the cards on it into white on white.
#if !defined(Q_OS_MACOS)
  setWizardStyle(QWizard::ClassicStyle);
#endif
  setPixmap(QWizard::LogoPixmap, {});
  setPixmap(QWizard::WatermarkPixmap, {});
  setPixmap(QWizard::BackgroundPixmap, {});
  setOption(QWizard::HaveHelpButton, false);
  setOption(QWizard::NoBackButtonOnStartPage);

  setButtonText(QWizard::BackButton, tr("Back"));
  setButtonText(QWizard::NextButton, tr("Continue"));
  setButtonText(QWizard::FinishButton, finish_text);

  // Cancel away from the way forward, at the left; Back and Continue together
  // at the right, Continue last and the default button -- the application's
  // treatment for the primary action.
  setButtonLayout({QWizard::CancelButton, QWizard::Stretch, QWizard::BackButton,
                   QWizard::NextButton, QWizard::FinishButton});
  for (const auto which : {QWizard::NextButton, QWizard::FinishButton}) {
    if (auto* push = qobject_cast<QPushButton*>(button(which))) {
      push->setDefault(true);
    }
  }

  setMinimumSize(kWizardMinimumWidth, kWizardMinimumHeight);
}

auto EMailWizard::AddStep(int index, const QString& title,
                          const QString& description) -> EMailWizardPage* {
  auto* page = new EMailWizardPage(this);
  page->setAccessibleName(title);

  auto* outer = new QVBoxLayout(page);
  outer->setContentsMargins(kPageMarginH, kPageMarginV, kPageMarginH, 0);
  outer->setSpacing(kHeaderSpacing);

  auto* header = new QVBoxLayout;
  header->setSpacing(4);
  header->addWidget(new EMailStepIndicator(steps_, index, page));
  header->addSpacing(12);

  // Clearly the largest thing on the page: it is what the step is.
  auto* title_label = new QLabel(title, page);
  title_label->setWordWrap(true);
  auto title_font = title_label->font();
  title_font.setPointSizeF(title_font.pointSizeF() * 1.45);
  title_font.setBold(true);
  title_label->setFont(title_font);
  header->addWidget(title_label);

  auto* description_label = new QLabel(description, page);
  description_label->setWordWrap(true);
  header->addWidget(description_label);
  outer->addLayout(header);

  // The body scrolls, vertically only, so a step that grows -- an opened
  // section, a long error -- never resizes the window under the pointer.
  auto* scroll = new QScrollArea(page);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidgetResizable(true);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto* content = new QWidget(scroll);
  page->body = new QVBoxLayout(content);
  page->body->setContentsMargins(0, 0, 0, 0);
  page->body->setSpacing(kSectionSpacing);
  scroll->setWidget(content);
  outer->addWidget(scroll, 1);

  setPage(index, page);
  return page;
}

void EMailWizard::setVisible(bool visible) {
  if (visible && !first_show_handled_) {
    first_show_handled_ = true;
    ensurePolished();
    if (currentId() == -1) restart();

    // One size for every step: remembered, or the default.
    const auto width =
        EMailViewSetting(qPrintable(name_ + "/width"), 0).toInt();
    const auto height =
        EMailViewSetting(qPrintable(name_ + "/height"), 0).toInt();
    auto size = width > 0 && height > 0
                    ? QSize(width, height)
                    : QSize(kWizardDefaultWidth, kWizardDefaultHeight);
    size = size.expandedTo(minimumSize());

    QRect rect{QPoint{}, size};
    auto* parent_widget = parentWidget();
    QWidget* anchor = parent_widget != nullptr ? parent_widget->window() : this;
    const auto available = AvailableGeometry(anchor);
    if (parent_widget != nullptr) {
      // The same nudge the Host's dialogs give, so a wizard over its parent
      // is not mistaken for part of it.
      rect.moveCenter(
          QRect(anchor->mapToGlobal(QPoint(0, 0)), anchor->size()).center());
      rect.translate(24, 24);
    } else {
      rect.moveCenter(available.center());
    }
    setGeometry(ClampToAvailable(rect, available));
  }
  QWizard::setVisible(visible);
}

void EMailWizard::done(int result) {
  if (isVisible()) {
    SetEMailViewSetting(qPrintable(name_ + "/width"), width());
    SetEMailViewSetting(qPrintable(name_ + "/height"), height());
  }
  QWizard::done(result);
}

EMailToggleSwitch::EMailToggleSwitch(const QString& text, QWidget* parent)
    : QAbstractButton(parent), slide_(new QVariantAnimation(this)) {
  setText(text);
  setCheckable(true);
  setCursor(Qt::PointingHandCursor);
  setFocusPolicy(Qt::StrongFocus);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

  slide_->setDuration(120);
  slide_->setEasingCurve(QEasingCurve::OutCubic);
  connect(slide_, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& v) {
            position_ = v.toReal();
            update();
          });
  connect(this, &QAbstractButton::toggled, this, [this](bool on) {
    slide_->stop();
    // Set in code before the window is shown, the switch is simply there.
    if (!isVisible()) {
      position_ = on ? 1 : 0;
      update();
      return;
    }
    slide_->setStartValue(position_);
    slide_->setEndValue(on ? 1.0 : 0.0);
    slide_->start();
  });
}

auto EMailToggleSwitch::sizeHint() const -> QSize {
  const auto text_width = fontMetrics().horizontalAdvance(text());
  return {text_width + kSwitchGap + kTrackWidth + 4,
          std::max(kTrackHeight + 4, fontMetrics().height() + 4)};
}

auto EMailToggleSwitch::track_rect() const -> QRectF {
  return {static_cast<qreal>(width() - kTrackWidth - 2),
          (height() - kTrackHeight) / 2.0, kTrackWidth, kTrackHeight};
}

void EMailToggleSwitch::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  const auto pal = palette();
  const auto enabled = isEnabled();

  // The label, left; muted when the switch cannot be used.
  p.setPen(enabled ? pal.color(QPalette::WindowText) : EMailMutedColor(this));
  p.drawText(QRect(0, 0, width() - kTrackWidth - kSwitchGap, height()),
             Qt::AlignLeft | Qt::AlignVCenter, text());

  // The accent when on, a hairline grey when off, blended as the knob travels
  // so the change reads as one motion.
  const auto track = track_rect();
  const auto on_color = pal.color(QPalette::Highlight);
  auto track_color = EMailBlend(EMailBorderColor(this), on_color,
                                std::clamp(position_, 0.0, 1.0));
  if (!enabled) track_color.setAlphaF(0.4F);
  if (hovered_ && enabled) track_color = track_color.lighter(108);

  p.setPen(Qt::NoPen);
  p.setBrush(track_color);
  p.drawRoundedRect(track, kTrackHeight / 2.0, kTrackHeight / 2.0);

  constexpr qreal kInset = 2;
  const auto knob = kTrackHeight - (2 * kInset);
  const auto x = track.left() + kInset +
                 ((track.width() - knob - (2 * kInset)) * position_);
  p.setBrush(enabled ? QColor(Qt::white) : pal.color(QPalette::Button));
  p.drawEllipse(QRectF(x, track.top() + kInset, knob, knob));

  if (hasFocus()) {
    auto focus = on_color;
    focus.setAlpha(110);
    p.setPen(QPen(focus, 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(track.adjusted(-2, -2, 2, 2), (kTrackHeight / 2.0) + 2,
                      (kTrackHeight / 2.0) + 2);
  }
}

void EMailToggleSwitch::enterEvent(QEnterEvent* event) {
  hovered_ = true;
  update();
  QAbstractButton::enterEvent(event);
}

void EMailToggleSwitch::leaveEvent(QEvent* event) {
  hovered_ = false;
  update();
  QAbstractButton::leaveEvent(event);
}

auto EMailDisclosure(const QString& title, QWidget* content, QWidget* parent,
                     const std::function<void(bool)>& on_toggle) -> QWidget* {
  auto* holder = new QWidget(parent);
  auto* layout = new QVBoxLayout(holder);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto* toggle = new QToolButton(holder);
  toggle->setCheckable(true);
  toggle->setChecked(false);
  toggle->setText(title);
  toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  toggle->setArrowType(Qt::RightArrow);
  toggle->setAutoRaise(true);

  content->setParent(holder);
  content->hide();

  QObject::connect(
      toggle, &QToolButton::toggled, holder,
      [toggle, content, on_toggle](bool open) {
        toggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        content->setVisible(open);
        if (on_toggle) on_toggle(open);
      });

  auto* toggle_row = new QHBoxLayout();
  toggle_row->addWidget(toggle);
  toggle_row->addStretch(1);

  layout->addLayout(toggle_row);
  layout->addWidget(content);
  return holder;
}
