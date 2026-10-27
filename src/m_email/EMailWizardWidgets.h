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

#include <QAbstractButton>
#include <QFrame>
#include <QWizard>
#include <QWizardPage>
#include <functional>

class QLabel;
class QVariantAnimation;
class QVBoxLayout;

/**
 * @file
 * @brief The application's wizard look, owned by this module.
 *
 * The Host's GeneralWizard, ChoiceTile and ToggleSwitch are not reachable from
 * a module -- nothing of the Host's UI library crosses into one -- so these are
 * this module's own copies of them, kept to the same measurements so the Add
 * Account wizard reads as one family with the key generation wizard. Colours
 * come from EMailViewStyle, which asks the Host for its theme through the SDK.
 */

/// A wizard page whose Continue and validation are plain functions, so a page
/// is built where it is laid out instead of in a subclass of its own.
class EMailWizardPage : public QWizardPage {
 public:
  using QWizardPage::QWizardPage;

  std::function<bool()> complete;  ///< enables Continue, or the final action
  std::function<bool()> validate;  ///< runs when it is pressed

  /// Where the step's own content goes, under the shared header. Scrolls
  /// rather than growing the window.
  QVBoxLayout* body{};

  [[nodiscard]] auto isComplete() const -> bool override;
  auto validatePage() -> bool override;

  /// Re-asks complete(). Public so the wizard can call it when something the
  /// page does not own changes.
  void Recheck() { emit completeChanged(); }
};

/**
 * @brief Where the user is among the steps, said quietly: small marks joined
 * by a hairline, each named, the current one in the accent.
 *
 * It sits above the page title and must not compete with it. It never asks
 * for more width than it is given: a name that does not fit is elided, so a
 * long translation cannot widen the window.
 */
class EMailStepIndicator : public QWidget {
 public:
  EMailStepIndicator(QStringList names, int current, QWidget* parent);

  [[nodiscard]] auto sizeHint() const -> QSize override;
  [[nodiscard]] auto minimumSizeHint() const -> QSize override;

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  QStringList names_;
  int current_;
};

/**
 * @brief The frame every step sits in.
 *
 * One header for all steps -- the progress marks, the step's title and one
 * sentence about it -- and a body under it. The window does not change size
 * from step to step: it opens at one size, and a body taller than that
 * scrolls.
 */
class EMailWizard : public QWizard {
  Q_OBJECT

 public:
  /**
   * @param name the key the size is remembered under
   * @param finish_text what the last step's button does, never just "Finish"
   * @param steps the steps' short names, for the progress marks
   * @param parent parent widget
   */
  EMailWizard(QString name, const QString& finish_text, QStringList steps,
              QWidget* parent);

  void setVisible(bool visible) override;
  void done(int result) override;

 protected:
  /// The page for step @p index, with its header built and its body ready.
  auto AddStep(int index, const QString& title, const QString& description)
      -> EMailWizardPage*;

 private:
  QString name_;
  QStringList steps_;
  bool first_show_handled_{false};
};

/// An on/off switch, for a setting that changes what a window does.
class EMailToggleSwitch : public QAbstractButton {
  Q_OBJECT

 public:
  explicit EMailToggleSwitch(const QString& text, QWidget* parent = nullptr);

  [[nodiscard]] auto sizeHint() const -> QSize override;

 protected:
  void paintEvent(QPaintEvent* event) override;
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;

 private:
  QVariantAnimation* slide_;
  qreal position_{0};
  bool hovered_{false};

  [[nodiscard]] auto track_rect() const -> QRectF;
};

/**
 * @brief Widgets side by side while they fit, one under another once they do
 * not.
 *
 * For a short row of choices: a long translation turns the row into a column
 * instead of widening the window or cutting the last choice off.
 */
class EMailWrapRow : public QWidget {
 public:
  explicit EMailWrapRow(QWidget* parent = nullptr);

  void AddWidget(QWidget* widget);

  [[nodiscard]] auto minimumSizeHint() const -> QSize override;

 protected:
  void resizeEvent(QResizeEvent* event) override;

 private:
  class QBoxLayout* layout_;
  QList<QWidget*> widgets_;
  static constexpr int kSpacing = 18;

  /// The width the widgets need side by side.
  [[nodiscard]] auto row_width() const -> int;
};

/**
 * @brief A small drawn mark for one EMailStatusState: a tick, an exclamation
 * mark, a dash, or an open ring while something is still happening.
 *
 * Drawn from the palette, so it follows the theme, and shaped as well as
 * coloured, so it reads without colour. Its accessible name is the state in
 * words; the text beside it says the rest.
 */
class EMailStatusIcon : public QWidget {
 public:
  explicit EMailStatusIcon(QWidget* parent = nullptr);

  void SetState(int state, const QString& accessible_name);

  [[nodiscard]] auto sizeHint() const -> QSize override;

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  int state_{0};
};

/**
 * @brief @p content behind a flat "▸ title" button, closed at first.
 *
 * For what most people never need to see. @p on_toggle runs after the content
 * is shown or hidden, so a wizard can refit itself around it.
 */
auto EMailDisclosure(const QString& title, QWidget* content, QWidget* parent,
                     const std::function<void(bool)>& on_toggle = {})
    -> QWidget*;
