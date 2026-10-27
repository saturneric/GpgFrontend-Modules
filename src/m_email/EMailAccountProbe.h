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

#include <QObject>
#include <QWidget>
#include <memory>
#include <mutex>

#include "EMailAccountModel.h"
#include "EMailCancelToken.h"
#include "EMailNetError.h"
#include "EMailSecret.h"
#include "EMailViewStyle.h"
#include "EMailWizardWidgets.h"

class QLabel;
class QThread;

/**
 * @brief One connection test at a time, for the settings page and the wizard.
 *
 * Connects to one transport of an account with the given password and says
 * whether it worked. The work runs on a thread of its own and is reported back
 * on this object's thread; a probe destroyed while it runs cancels it, so
 * neither a live socket nor a live credential outlives the window that asked.
 */
class EMailAccountProbe : public QObject {
  Q_OBJECT

 public:
  enum class Outcome : uint8_t { kCONNECTED, kSTOPPED, kFAILED };

  explicit EMailAccountProbe(QObject* parent = nullptr);
  ~EMailAccountProbe() override;

  /**
   * @brief Starts a test of one transport of @p account.
   *
   * @return false, and nothing started, when a test is already running
   */
  auto Test(const MailAccountConfig& account, const EMailSecretPtr& password,
            bool imap) -> bool;

  /// Cancels the running test, if there is one. It still reports, as
  /// stopped.
  void Stop();

  [[nodiscard]] auto IsRunning() const -> bool;

  /// Which transport the running test is for. Meaningless when none is.
  [[nodiscard]] auto RunningImap() const -> bool;

  /// What @p error means and what can be done about it, as a few lines.
  static auto Describe(const MailError& error) -> QString;

  /**
   * @brief Asks whether to trust the certificate @p error was refused over.
   *
   * @return the fingerprint to pin, lower-case, or empty when the user said no
   *         or there is nothing to pin
   */
  static auto AskToPin(QWidget* parent, const MailError& error) -> QString;

 signals:
  void SignalFinished(bool imap, EMailAccountProbe::Outcome outcome,
                      const MailError& error);

 private:
  struct Run {
    bool imap{false};
    bool connected{false};
    bool cancelled{false};
    MailError error;
    /// Published by the worker thread once its worker exists, so Stop can
    /// reach it. Guarded because the GUI thread reads it concurrently.
    std::mutex mutex;
    EMailCancelTokenPtr token;
  };

  std::shared_ptr<Run> run_;
  QThread* thread_{};

  void finish(const std::shared_ptr<Run>& run);
};

/**
 * @brief A status icon and the sentence beside it, for one connection test.
 *
 * Remembers what it is saying rather than the colour it said it in, so a theme
 * change repaints it correctly.
 */
class EMailProbeStatus : public QWidget {
 public:
  explicit EMailProbeStatus(QWidget* parent = nullptr);

  /// Says nothing and shows nothing.
  void Clear();

  /// @param tooltip the raw protocol detail, kept out of the sentence
  void Set(EMailStatusState state, const QString& text,
           const QString& tooltip = {});

  /// A probe's outcome, worded.
  void SetOutcome(EMailAccountProbe::Outcome outcome, const MailError& error);

  /**
   * @brief Keeps the line in place and one line tall, whatever it says.
   *
   * For a form that must not move when a result arrives: the line is always
   * shown -- @p idle_text when there is nothing to report -- and a result too
   * long for it is shortened, with the whole of it in the tooltip.
   */
  void SetFixedLine(const QString& idle_text);

 protected:
  void changeEvent(QEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

 private:
  EMailStatusIcon* icon_;
  QLabel* label_;
  EMailStatusState state_{EMailStatusState::kUNKNOWN};
  /// Set in fixed-line mode: what is shown when there is no result.
  QString idle_text_;
  bool fixed_line_{false};
  /// The whole text, of which a fixed line may show only the start.
  QString text_;
  QString tooltip_;

  void recolor();
  /// Puts text_ into the label, shortened to one line in fixed-line mode.
  void lay_out_text();
};

/**
 * @brief Label-and-value rows, each with room for a status line.
 *
 * The one way this module lists an account's settings -- the server summary,
 * the connection check, the review -- so a server and its state look the same
 * wherever they appear. Labels are muted and share one column; values wrap and
 * can be selected; a status line, when there is one, sits under its value.
 */
class EMailSummaryRows : public QWidget {
 public:
  explicit EMailSummaryRows(QWidget* parent = nullptr);

  /// Removes every row.
  void Clear();

  /// Adds a row.
  /// @return the row's status line, hidden until something is said in it
  auto AddRow(const QString& label, const QString& value) -> EMailProbeStatus*;

 protected:
  void changeEvent(QEvent* event) override;

 private:
  class QGridLayout* grid_;
  QList<QLabel*> labels_;
};
