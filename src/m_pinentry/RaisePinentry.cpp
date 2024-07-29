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

#include "RaisePinentry.h"

#include <qapplication.h>

#include "GpgPassphraseContext.h"
#include "pinentrydialog.h"

auto FindTopMostWindow(QWidget* fallback) -> QWidget* {
  QList<QWidget*> top_widgets = QApplication::topLevelWidgets();
  foreach (QWidget* widget, top_widgets) {
    if (widget->isActiveWindow()) {
      return widget;
    }
  }
  return fallback;
}

RaisePinentry::RaisePinentry(QWidget* parent,
                             QSharedPointer<GpgPassphraseContext> context)
    : QWidget(parent), context_(std::move(context)) {}

auto RaisePinentry::Exec() -> int {
  bool ask_for_new = context_->IsAskForNew() &&
                     context_->GetPassphraseInfo().isEmpty() &&
                     context_->GetUidsInfo().isEmpty();

  auto* pinentry =
      new PinEntryDialog(FindTopMostWindow(this), 0, 15, true, ask_for_new,
                         ask_for_new ? tr("Repeat Passphrase:") : QString(),
                         tr("Show passphrase"), tr("Hide passphrase"));

  if (context_->IsPreWasBad()) {
    pinentry->setError(tr("Given Passphrase was wrong. Please retry."));
  }

  pinentry->setPrompt(tr("Passphrase:"));

  if (!context_->GetUidsInfo().isEmpty()) {
    pinentry->setDescription(QString("Please provide Passphrase of Key:\n%1\n")
                                 .arg(context_->GetUidsInfo()));
  }

  struct pinentry pinentry_info;
  pinentry->setPinentryInfo(pinentry_info);

  pinentry->setRepeatErrorText(tr("Passphrases do not match"));
  pinentry->setGenpinLabel(QString(""));
  pinentry->setGenpinTT(QString(""));
  pinentry->setCapsLockHint(tr("Caps Lock is on"));
  pinentry->setFormattedPassphrase({false, QString()});
  pinentry->setConstraintsOptions({false, QString(), QString(), QString()});

  pinentry->setWindowTitle(tr("Bundled Pinentry"));

  /* If we reuse the same dialog window.  */
  pinentry->setPin(QString());
  pinentry->setOkText(tr("Confirm"));
  pinentry->setCancelText(tr("Cancel"));

  connect(pinentry, &PinEntryDialog::finished, this,
          [pinentry, this](int result) {
            bool ret = result != 0;

            if (!ret) {
              emit SignalUserInputPassphraseCallback({});
              return -1;
            }

            auto pin = pinentry->pin().toUtf8();

            context_->SetPassphrase(pin);
            emit SignalUserInputPassphraseCallback(context_);
            return 0;
          });
  connect(pinentry, &PinEntryDialog::finished, this, &QWidget::deleteLater);

  pinentry->open();
  return 0;
}
