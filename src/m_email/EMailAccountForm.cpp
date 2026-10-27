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

#include "EMailAccountForm.h"

#include <QComboBox>
#include <QCoreApplication>

void EMailFillSecurityCombo(QComboBox* combo, const QString& host) {
  const auto previous = combo->currentData();

  const QSignalBlocker blocker(combo);
  combo->clear();
  combo->addItem(QCoreApplication::translate("EMailAccountSettingsPage",
                                             "TLS (recommended)"),
                 static_cast<int>(MailTlsMode::kIMPLICIT));
  combo->addItem(
      QCoreApplication::translate("EMailAccountSettingsPage", "STARTTLS"),
      static_cast<int>(MailTlsMode::kSTARTTLS));

  if (MailHostAllowsCleartext(host)) {
    combo->addItem(QCoreApplication::translate("EMailAccountSettingsPage",
                                               "None (this machine only)"),
                   static_cast<int>(MailTlsMode::kNONE));
  }

  const auto index = combo->findData(previous);
  combo->setCurrentIndex(index < 0 ? 0 : index);
}

auto EMailSecurityOf(const QComboBox* combo) -> MailTlsMode {
  return static_cast<MailTlsMode>(combo->currentData().toInt());
}

void EMailSelectSecurity(QComboBox* combo, MailTlsMode mode) {
  const auto index = combo->findData(static_cast<int>(mode));

  // A cleartext account whose host is no longer loopback has no row to select:
  // the safest offered choice is the honest answer, and the mode only changes
  // if the user goes on to edit something.
  combo->setCurrentIndex(index < 0 ? 0 : index);
}

void EMailRefreshSecurityCombo(QComboBox* combo, const QString& host) {
  const auto current = EMailSecurityOf(combo);
  EMailFillSecurityCombo(combo, host.trimmed());
  EMailSelectSecurity(combo, current);
}

auto EMailServerSummary(const QString& host, MailTlsMode mode, bool imap)
    -> QString {
  if (host.trimmed().isEmpty()) {
    return QCoreApplication::translate("EMailAccountSettingsPage",
                                       "No server set");
  }

  QString security;
  switch (mode) {
    case MailTlsMode::kIMPLICIT:
      security = QCoreApplication::translate("EMailAccountSettingsPage", "TLS");
      break;
    case MailTlsMode::kSTARTTLS:
      security =
          QCoreApplication::translate("EMailAccountSettingsPage", "STARTTLS");
      break;
    case MailTlsMode::kNONE:
      security = QCoreApplication::translate("EMailAccountSettingsPage",
                                             "No encryption");
      break;
  }
  return QCoreApplication::translate("EMailAccountSettingsPage",
                                     "%1 · %2 · port %3")
      .arg(host.trimmed(), security)
      .arg(MailDefaultPort(imap, mode));
}
