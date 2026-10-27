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

#include "EMailAccountProbe.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QRegularExpression>
#include <QThread>

#include "EMailImapWorker.h"
#include "EMailSmtpWorker.h"

EMailAccountProbe::EMailAccountProbe(QObject* parent) : QObject(parent) {}

EMailAccountProbe::~EMailAccountProbe() {
  // A test outliving its window is not a use-after-free -- everything it
  // touches is held by value or by shared_ptr, and the result is delivered
  // with `this` as context -- but it is a live socket and a live credential
  // with nothing left that can stop it.
  if (run_ != nullptr) {
    const std::lock_guard<std::mutex> guard(run_->mutex);
    run_->cancelled = true;
    if (run_->token != nullptr) run_->token->CancelAll();
  }

  StopMailWorkerThread(thread_, "mail account test thread");
  thread_ = nullptr;
}

auto EMailAccountProbe::IsRunning() const -> bool { return run_ != nullptr; }

auto EMailAccountProbe::RunningImap() const -> bool {
  return run_ != nullptr && run_->imap;
}

auto EMailAccountProbe::Test(const MailAccountConfig& account,
                             const EMailSecretPtr& password, bool imap)
    -> bool {
  // One test at a time. Two would fight over the same status and the same
  // locked-down form.
  if (run_ != nullptr) return false;

  // Everything the test reports back, on the heap and shared with the thread:
  // this function returns long before the answer arrives.
  auto run = std::make_shared<Run>();
  run->imap = imap;
  run_ = run;

  thread_ = QThread::create([run, account, password, imap]() {
    const auto publish = [&run](const EMailCancelTokenPtr& token) {
      const std::lock_guard<std::mutex> guard(run->mutex);
      run->token = token;
    };

    if (imap) {
      // Called directly rather than through the event loop: the worker's
      // slots are synchronous, so the signal has already been delivered by
      // the time Connect() returns.
      EMailImapWorker worker;
      publish(worker.Token());
      QObject::connect(&worker, &EMailImapWorker::SignalFailed, &worker,
                       [run](quint64, const MailError& e) { run->error = e; });
      QObject::connect(&worker, &EMailImapWorker::SignalConnected, &worker,
                       [run](quint64) { run->connected = true; });
      worker.Connect(1, account, password);
      worker.Disconnect();
    } else {
      EMailSmtpWorker worker;
      publish(worker.Token());
      QObject::connect(&worker, &EMailSmtpWorker::SignalFinished, &worker,
                       [run](quint64, const EMailSendReceipt& receipt) {
                         run->error = receipt.error;
                         run->connected = receipt.accepted;
                       });
      worker.TestConnection(1, account, password);
    }
  });

  // `this` as the context object, so a probe destroyed while the test runs
  // never sees this run at all.
  connect(thread_, &QThread::finished, this, [this, run]() { finish(run); });
  connect(thread_, &QThread::finished, thread_, &QObject::deleteLater);
  thread_->start();
  return true;
}

void EMailAccountProbe::Stop() {
  auto run = run_;
  if (run == nullptr) return;

  // A Stop pressed before the worker published its token still counts:
  // `cancelled` is already set, so the outcome is reported as stopped
  // whatever the connection went on to do.
  run->cancelled = true;
  const std::lock_guard<std::mutex> guard(run->mutex);
  if (run->token != nullptr) run->token->CancelAll();
}

void EMailAccountProbe::finish(const std::shared_ptr<Run>& run) {
  if (run != run_) return;
  run_ = nullptr;
  thread_ = nullptr;

  // Judged on having actually connected, not on the absence of an error: an
  // internal failure is not a successful login.
  auto outcome = Outcome::kFAILED;
  if (run->cancelled) {
    outcome = Outcome::kSTOPPED;
  } else if (run->connected) {
    outcome = Outcome::kCONNECTED;
  }
  emit SignalFinished(run->imap, outcome, run->error);
}

auto EMailAccountProbe::Describe(const MailError& error) -> QString {
  if (!error.IsError()) {
    return tr("Could not connect, and the reason is not known.");
  }

  // The category is the part worth reading: a wrong password and an
  // unreachable host must not produce the same shape of answer.
  QString advice;
  switch (error.category) {
    case MailErrorCategory::kAUTH:
      advice = tr("Check the user name and password.");
      break;
    case MailErrorCategory::kDNS:
      advice = tr("Check the server address for a typo.");
      break;
    case MailErrorCategory::kCONNECT:
      advice =
          tr("The address resolves but nothing answered. Check the "
             "connection type, and whether the server is reachable from "
             "here.");
      break;
    case MailErrorCategory::kTLS_UNTRUSTED:
      advice =
          tr("The server's certificate is not signed by an authority this "
             "computer trusts. If you run this server yourself, you can "
             "choose to trust its certificate.");
      break;
    case MailErrorCategory::kTLS_EXPIRED:
      advice =
          tr("The server's certificate is expired or not yet valid. Check "
             "the clock on both machines before anything else.");
      break;
    case MailErrorCategory::kTLS_HOSTNAME:
      advice =
          tr("The server's certificate is for a different host. Check the "
             "server address.");
      break;
    case MailErrorCategory::kTLS_REQUIRED:
      advice =
          tr("The server would not start an encrypted session. Nothing was "
             "sent to it.");
      break;
    case MailErrorCategory::kTLS_HANDSHAKE:
      advice = tr("The encrypted connection could not be established.");
      break;
    case MailErrorCategory::kTIMEOUT:
      advice = tr("The server stopped answering. Try again.");
      break;
    case MailErrorCategory::kINTERNAL:
      advice = tr("This is a fault in GpgFrontend rather than in the server.");
      break;
    default:
      break;
  }

  // Only where it is not already implied: "try again" under a wrong password
  // would be wrong, and under a timeout it is redundant.
  if (advice.isEmpty() && error.transient) {
    advice = tr("This may be temporary. Try again.");
  }

  // What happened, then what to do. The longer explanation repeats the
  // advice more often than it adds to it, so it is shown only when there is no
  // advice; otherwise it goes in the tooltip with the protocol line.
  auto text = error.title;
  const auto& second = advice.isEmpty() ? error.detail : advice;
  if (!second.isEmpty()) text += "\n" + second;
  return text;
}

auto EMailAccountProbe::AskToPin(QWidget* parent, const MailError& error)
    -> QString {
  if (!error.IsPinnable()) return {};

  // Off the error itself, which carries what THIS connection's verifier saw,
  // never from anything shared with other connections.
  const auto fingerprint = error.cert_fingerprint;
  if (fingerprint.isEmpty()) return {};

  // The decision is the user's, so the default button is Cancel and the
  // question names exactly what is being trusted and how far that goes.
  const auto summary = error.cert_summary;
  const auto question =
      tr("This server presented a certificate that no authority vouches for. "
         "That is normal for a server you run yourself, and it is also what "
         "an interception looks like.\n\n%1\nSHA-256: %2\n\nTrust this "
         "exact certificate for this account? Any other certificate, "
         "including a later replacement of this one, will still be "
         "refused.")
          .arg(summary.isEmpty() ? tr("(no certificate details)") : summary,
               fingerprint);

  if (QMessageBox::question(parent, tr("Trust This Certificate?"), question,
                            QMessageBox::Yes | QMessageBox::Cancel,
                            QMessageBox::Cancel) != QMessageBox::Yes) {
    return {};
  }
  return fingerprint.toLower();
}

EMailProbeStatus::EMailProbeStatus(QWidget* parent)
    : QWidget(parent),
      icon_(new EMailStatusIcon(this)),
      label_(new QLabel(this)) {
  label_->setWordWrap(true);
  label_->setTextInteractionFlags(Qt::TextSelectableByMouse);

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);
  layout->addWidget(icon_, 0, Qt::AlignTop);
  layout->addWidget(label_, 1);

  Clear();
}

void EMailProbeStatus::Clear() {
  if (fixed_line_) {
    // Never hidden in this mode: the line stays where it is and says so.
    Set(EMailStatusState::kUNKNOWN, idle_text_);
    return;
  }
  text_.clear();
  tooltip_.clear();
  label_->clear();
  label_->setToolTip({});
  setVisible(false);
}

void EMailProbeStatus::Set(EMailStatusState state, const QString& text,
                           const QString& tooltip) {
  state_ = state;
  text_ = text;
  tooltip_ = tooltip;
  icon_->SetState(static_cast<int>(state), text.section('\n', 0, 0));
  lay_out_text();
  recolor();
  setVisible(true);
}

void EMailProbeStatus::SetFixedLine(const QString& idle_text) {
  fixed_line_ = true;
  idle_text_ = idle_text;
  label_->setWordWrap(false);
  // Shortened text is not worth selecting: the tooltip has all of it.
  label_->setTextInteractionFlags(Qt::NoTextInteraction);
  // As wide as it is given and no wider, so a long result cannot widen the
  // form either.
  label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  label_->setMinimumWidth(1);
  Clear();
}

void EMailProbeStatus::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  if (fixed_line_) lay_out_text();
}

void EMailProbeStatus::lay_out_text() {
  if (!fixed_line_) {
    label_->setText(text_);
    label_->setToolTip(tooltip_);
    return;
  }

  // The lines joined into one, shortened to the width there is; the tooltip
  // carries the whole text and the protocol detail under it.
  // A line that ends without a full stop gets one before the next begins,
  // or "could not be reached" runs straight into the advice after it.
  QString one_line;
  for (const auto& line : text_.split('\n', Qt::SkipEmptyParts)) {
    if (!one_line.isEmpty()) {
      if (!one_line.endsWith('.') && !one_line.endsWith('!') &&
          !one_line.endsWith('?') && !one_line.endsWith(':')) {
        one_line += '.';
      }
      one_line += ' ';
    }
    one_line += line.trimmed();
  }
  const auto shown = label_->fontMetrics().elidedText(
      one_line, Qt::ElideRight, std::max(0, label_->width()));
  label_->setText(shown);

  QStringList tip;
  if (shown != one_line) tip.append(text_);
  if (!tooltip_.isEmpty()) tip.append(tooltip_);
  label_->setToolTip(tip.join("\n\n"));
}

void EMailProbeStatus::SetOutcome(EMailAccountProbe::Outcome outcome,
                                  const MailError& error) {
  switch (outcome) {
    case EMailAccountProbe::Outcome::kCONNECTED:
      Set(EMailStatusState::kGOOD,
          EMailAccountProbe::tr("Connected successfully."));
      break;
    case EMailAccountProbe::Outcome::kSTOPPED:
      Set(EMailStatusState::kUNKNOWN, EMailAccountProbe::tr("Stopped."));
      break;
    case EMailAccountProbe::Outcome::kFAILED:
      // The raw protocol line goes where it can be copied for a bug report,
      // not into the sentence a person decides by.
      Set(EMailStatusState::kBAD, EMailAccountProbe::Describe(error),
          QStringList{error.detail, error.protocol_detail}
              .filter(QRegularExpression("\\S"))
              .join("\n\n"));
      break;
  }
}

void EMailProbeStatus::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);
  if (EMailIsRestyle(event)) recolor();
}

void EMailProbeStatus::recolor() {
  switch (state_) {
    case EMailStatusState::kGOOD:
      EMailSetLabelColor(label_, EMailAccentColor(this, true));
      break;
    case EMailStatusState::kBAD:
      EMailSetLabelColor(label_, EMailWarningColor(this));
      break;
    case EMailStatusState::kPENDING:
    case EMailStatusState::kUNKNOWN:
      EMailSetLabelColor(label_, EMailMutedColor(this));
      break;
  }
}

EMailSummaryRows::EMailSummaryRows(QWidget* parent)
    : QWidget(parent), grid_(new QGridLayout(this)) {
  grid_->setContentsMargins(0, 0, 0, 0);
  grid_->setHorizontalSpacing(16);
  grid_->setVerticalSpacing(6);
  grid_->setColumnStretch(1, 1);
}

void EMailSummaryRows::Clear() {
  while (auto* item = grid_->takeAt(0)) {
    if (auto* widget = item->widget()) widget->deleteLater();
    delete item;
  }
  labels_.clear();
}

auto EMailSummaryRows::AddRow(const QString& label, const QString& value)
    -> EMailProbeStatus* {
  const auto row = grid_->rowCount();

  auto* name = new QLabel(label, this);
  name->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  EMailMakeMuted(name);
  labels_.append(name);

  auto* cell = new QWidget(this);
  auto* cell_layout = new QVBoxLayout(cell);
  cell_layout->setContentsMargins(0, 0, 0, 0);
  cell_layout->setSpacing(3);

  // A host name has no spaces to break at, so a long one would widen the
  // window instead of wrapping. Shown with a break opportunity after every dot
  // and hyphen; the exact text is the tooltip, and nothing here is selectable,
  // so the invisible breaks cannot be copied out into a server field.
  auto breakable = value;
  breakable.replace('.', QStringLiteral(".\u200B"));
  breakable.replace('-', QStringLiteral("-\u200B"));
  auto* text = new QLabel(breakable, cell);
  text->setWordWrap(true);
  text->setTextFormat(Qt::PlainText);
  text->setToolTip(value);
  cell_layout->addWidget(text);

  auto* status = new EMailProbeStatus(cell);
  cell_layout->addWidget(status);

  grid_->addWidget(name, row, 0);
  grid_->addWidget(cell, row, 1);
  return status;
}

void EMailSummaryRows::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);
  if (!EMailIsRestyle(event)) return;
  for (auto* label : labels_) EMailMakeMuted(label);
}
