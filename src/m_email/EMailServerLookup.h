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

#include <QList>
#include <QObject>
#include <QPointer>

#include "EMailAccountSetup.h"

class QDnsLookup;
class QTimer;

/**
 * @brief Works out which servers an address probably uses.
 *
 * The built-in provider list first, which needs nothing from the network.
 * Only when the provider is not on it is DNS asked, two things at once: the
 * domain's SRV records (RFC 6186, and RFC 8314 for TLS from the first byte),
 * and which of the conventional names -- imap., smtp., mail. under the domain
 * -- exist at all. SRV wins where it answers, a name that exists comes next,
 * and only what neither found is guessed.
 *
 * Nothing here runs on its own: a lookup happens when the user presses
 * Continue with an address typed, never while it is being typed. And nothing
 * found is used without being shown -- the next step of the wizard puts every
 * suggestion in front of the user, and the connection test then checks it
 * against the server's certificate.
 */
class EMailServerLookup : public QObject {
  Q_OBJECT

 public:
  explicit EMailServerLookup(QObject* parent = nullptr);

  /// Starts over for @p address, abandoning any lookup still running.
  void Start(const QString& address);

  /// Abandons the running lookup. It does not report.
  void Cancel();

  [[nodiscard]] auto IsRunning() const -> bool;

 signals:
  /// Always delivered after Start() returns, even for a known provider.
  void SignalFinished(const MailServerSuggestion& suggestion);

 private:
  quint64 generation_{0};
  QString domain_;
  QList<QPointer<QDnsLookup>> pending_;
  QList<MailSrvRecord> records_;
  /// Conventional names still being resolved, by lookup id, and those found.
  QList<int> pending_names_;
  QStringList existing_names_;
  QTimer* timeout_;

  void finish();
  void abandon_pending();
  /// Whether every question asked has been answered.
  [[nodiscard]] auto all_answered() const -> bool;
};
