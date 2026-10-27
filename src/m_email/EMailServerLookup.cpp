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

#include "EMailServerLookup.h"

#include <QDnsLookup>
#include <QHostInfo>
#include <QTimer>
#include <QUrl>

namespace {

/// Long enough for a resolver that has to go out to the domain's own name
/// servers, short enough that a missing answer does not hold the wizard up.
constexpr int kLookupTimeoutMs = 4000;

}  // namespace

EMailServerLookup::EMailServerLookup(QObject* parent)
    : QObject(parent), timeout_(new QTimer(this)) {
  timeout_->setSingleShot(true);
  timeout_->setInterval(kLookupTimeoutMs);
  connect(timeout_, &QTimer::timeout, this, &EMailServerLookup::finish);
}

auto EMailServerLookup::IsRunning() const -> bool {
  return timeout_->isActive() || !all_answered();
}

auto EMailServerLookup::all_answered() const -> bool {
  return pending_.isEmpty() && pending_names_.isEmpty();
}

void EMailServerLookup::Cancel() {
  // The generation first: abort() reports through the very handler that
  // would otherwise act on it.
  ++generation_;
  timeout_->stop();
  abandon_pending();
  records_.clear();
}

void EMailServerLookup::abandon_pending() {
  // Taken out of the member before anything is aborted, so a handler that
  // runs during abort() cannot change the list being walked.
  const auto pending = std::exchange(pending_, {});
  for (const auto& lookup : pending) {
    if (lookup == nullptr) continue;
    lookup->abort();
    lookup->deleteLater();
  }
  for (const auto id : std::exchange(pending_names_, {})) {
    QHostInfo::abortHostLookup(id);
  }
}

void EMailServerLookup::Start(const QString& address) {
  Cancel();
  const auto generation = generation_;
  domain_ = MailDomainOf(address);
  existing_names_.clear();

  // A known provider needs no question asked of anyone. Still delivered from
  // the event loop, so a caller sees one shape of answer whichever path ran.
  if (const auto preset = MailPresetForAddress(address); preset.has_value()) {
    QTimer::singleShot(0, this, [this, generation, suggestion = *preset]() {
      if (generation != generation_) return;
      emit SignalFinished(suggestion);
    });
    return;
  }

  const auto ace = QString::fromLatin1(QUrl::toAce(domain_));
  if (ace.isEmpty()) {
    QTimer::singleShot(0, this, [this, generation]() {
      if (generation != generation_) return;
      emit SignalFinished(MailGuessServers(domain_));
    });
    return;
  }

  for (const auto& service : MailSrvServices()) {
    auto* lookup = new QDnsLookup(QDnsLookup::SRV, service + "." + ace, this);
    connect(lookup, &QDnsLookup::finished, this,
            [this, lookup, service, generation]() {
              lookup->deleteLater();
              if (generation != generation_) return;

              // A failed lookup is simply no records: NXDOMAIN is the normal
              // answer for a domain that publishes none.
              if (lookup->error() == QDnsLookup::NoError) {
                for (const auto& record : lookup->serviceRecords()) {
                  records_.append({service, record.target(), record.port(),
                                   record.priority(), record.weight()});
                }
              }
              pending_.removeAll(lookup);
              if (all_answered()) finish();
            });
    pending_.append(lookup);
    lookup->lookup();
  }

  // Which conventional names exist. Only existence is asked: the address a
  // name resolves to is not used, the connection check makes its own.
  QStringList names;
  for (const bool imap : {true, false}) {
    for (const auto& name : MailHostCandidates(domain_, imap)) {
      if (!names.contains(name)) names.append(name);
    }
  }
  for (const auto& name : names) {
    const auto ace_name = QString::fromLatin1(QUrl::toAce(name));
    // Qt delivers the answer through the event loop, even from its cache, so
    // the id is recorded before any answer can arrive. Were one ever lost,
    // the timeout still ends the lookup.
    const auto id = QHostInfo::lookupHost(
        ace_name, this, [this, name, generation](const QHostInfo& info) {
          if (generation != generation_) return;
          if (info.error() == QHostInfo::NoError &&
              !info.addresses().isEmpty()) {
            existing_names_.append(name);
          }
          pending_names_.removeAll(info.lookupId());
          if (all_answered()) finish();
        });
    pending_names_.append(id);
  }
  if (all_answered()) {
    finish();
    return;
  }
  timeout_->start();
}

void EMailServerLookup::finish() {
  ++generation_;
  timeout_->stop();

  // Whatever answered in time is used; whatever did not is abandoned.
  abandon_pending();

  // SRV where the domain published it, then a name that exists, and only
  // then a guess.
  const auto published = MailServersFromSrv(records_);
  const auto resolved = MailServersFromResolved(domain_, existing_names_);
  records_.clear();
  existing_names_.clear();
  emit SignalFinished(MailMergeSuggestions(
      MailMergeSuggestions(published, resolved), MailGuessServers(domain_)));
}
