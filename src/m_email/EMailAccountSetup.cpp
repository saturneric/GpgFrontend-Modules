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

#include "EMailAccountSetup.h"

#include <array>

#include "EMailOutgoing.h"

namespace {

struct ProviderPreset {
  const char* domains;  ///< space-separated, every one an exact match
  const char* name;
  /// A host name, or a pattern in which "*" stands for the address's own
  /// domain: "imap.*" is imap.gmail.com for gmail.com.
  const char* imap_host;
  MailTlsMode imap_tls;
  const char* smtp_host;
  MailTlsMode smtp_tls;
  MailProviderNote note;
};

/// Providers people actually type into a mail client, and only the settings
/// their own help pages publish. Being listed is what spares the address a DNS
/// check, so the providers on the conventional names are here as well, as
/// patterns; the rest are spelled out because those names would be wrong for
/// them -- another host, another domain, or STARTTLS where the guess says TLS.
/// Tests keep each entry honest about which kind it is. Providers on ports
/// other than the policy ones (a local bridge, say) cannot be reached and are
/// left out.
constexpr std::array<ProviderPreset, 15> kPresets{{
    // The conventional names, TLS from the first byte.
    {"gmail.com", "Gmail", "imap.*", MailTlsMode::kIMPLICIT, "smtp.*",
     MailTlsMode::kIMPLICIT, MailProviderNote::kAPP_PASSWORD},
    {"aol.com", "AOL Mail", "imap.*", MailTlsMode::kIMPLICIT, "smtp.*",
     MailTlsMode::kIMPLICIT, MailProviderNote::kAPP_PASSWORD},
    {"fastmail.com", "Fastmail", "imap.*", MailTlsMode::kIMPLICIT, "smtp.*",
     MailTlsMode::kIMPLICIT, MailProviderNote::kAPP_PASSWORD},
    {"mailbox.org", "mailbox.org", "imap.*", MailTlsMode::kIMPLICIT, "smtp.*",
     MailTlsMode::kIMPLICIT, MailProviderNote::kNONE},
    {"zoho.com", "Zoho Mail", "imap.*", MailTlsMode::kIMPLICIT, "smtp.*",
     MailTlsMode::kIMPLICIT, MailProviderNote::kAPP_PASSWORD},

    // Everyone else, spelled out.
    {"googlemail.com", "Gmail", "imap.gmail.com", MailTlsMode::kIMPLICIT,
     "smtp.gmail.com", MailTlsMode::kIMPLICIT, MailProviderNote::kAPP_PASSWORD},
    {"outlook.com hotmail.com live.com msn.com", "Outlook.com",
     "outlook.office365.com", MailTlsMode::kIMPLICIT, "smtp-mail.outlook.com",
     MailTlsMode::kSTARTTLS, MailProviderNote::kWEB_SIGN_IN},
    {"icloud.com me.com mac.com", "iCloud Mail", "imap.mail.me.com",
     MailTlsMode::kIMPLICIT, "smtp.mail.me.com", MailTlsMode::kSTARTTLS,
     MailProviderNote::kAPP_PASSWORD},
    {"yahoo.com ymail.com", "Yahoo Mail", "imap.mail.yahoo.com",
     MailTlsMode::kIMPLICIT, "smtp.mail.yahoo.com", MailTlsMode::kIMPLICIT,
     MailProviderNote::kAPP_PASSWORD},
    {"fastmail.fm", "Fastmail", "imap.fastmail.com", MailTlsMode::kIMPLICIT,
     "smtp.fastmail.com", MailTlsMode::kIMPLICIT,
     MailProviderNote::kAPP_PASSWORD},
    {"posteo.de posteo.net", "Posteo", "posteo.de", MailTlsMode::kIMPLICIT,
     "posteo.de", MailTlsMode::kIMPLICIT, MailProviderNote::kNONE},
    {"gmx.de gmx.net gmx.at gmx.ch", "GMX", "imap.gmx.net",
     MailTlsMode::kIMPLICIT, "mail.gmx.net", MailTlsMode::kIMPLICIT,
     MailProviderNote::kNONE},
    {"gmx.com", "GMX", "imap.gmx.com", MailTlsMode::kIMPLICIT, "mail.gmx.com",
     MailTlsMode::kIMPLICIT, MailProviderNote::kNONE},
    {"web.de", "WEB.DE", "imap.web.de", MailTlsMode::kIMPLICIT, "smtp.web.de",
     MailTlsMode::kSTARTTLS, MailProviderNote::kNONE},
    {"zohomail.com", "Zoho Mail", "imap.zoho.com", MailTlsMode::kIMPLICIT,
     "smtp.zoho.com", MailTlsMode::kIMPLICIT, MailProviderNote::kAPP_PASSWORD},
}};

/// @p host with "*" replaced by @p domain.
auto ExpandHost(const char* host, const QString& domain) -> QString {
  return QString::fromLatin1(host).replace('*', domain);
}

auto DomainListed(const char* domains, const QString& domain) -> bool {
  return QString::fromLatin1(domains)
      .split(' ', Qt::SkipEmptyParts)
      .contains(domain);
}

/// Which transport and connection type one SRV name stands for.
struct SrvService {
  const char* name;
  bool imap;
  MailTlsMode tls;
};

/// In order of preference within each transport (RFC 8314: TLS from the first
/// byte over STARTTLS).
constexpr std::array<SrvService, 4> kSrvServices{{
    {"_imaps._tcp", true, MailTlsMode::kIMPLICIT},
    {"_imap._tcp", true, MailTlsMode::kSTARTTLS},
    {"_submissions._tcp", false, MailTlsMode::kIMPLICIT},
    {"_submission._tcp", false, MailTlsMode::kSTARTTLS},
}};

/// The best usable record of one service, or nullopt. A "." target is the
/// domain saying it does not offer the service, so it is never a candidate.
auto BestRecord(const QList<MailSrvRecord>& records, const SrvService& service)
    -> std::optional<MailSrvRecord> {
  std::optional<MailSrvRecord> best;
  for (const auto& record : records) {
    if (record.service.compare(QLatin1String(service.name),
                               Qt::CaseInsensitive) != 0) {
      continue;
    }

    auto target = record.target.trimmed();
    if (target.endsWith('.')) target.chop(1);
    if (target.isEmpty()) continue;
    if (record.port != MailDefaultPort(service.imap, service.tls)) continue;

    if (!best || record.priority < best->priority ||
        (record.priority == best->priority && record.weight > best->weight)) {
      best = record;
      best->target = target.toLower();
    }
  }
  return best;
}

}  // namespace

auto MailDomainOf(const QString& address) -> QString {
  const auto trimmed = address.trimmed();
  const auto at = trimmed.lastIndexOf('@');
  if (at < 0) return {};
  return trimmed.mid(at + 1).toLower();
}

auto MailProviderInfoFor(const QString& address)
    -> std::optional<MailProviderInfo> {
  const auto domain = MailDomainOf(address);
  if (domain.isEmpty()) return std::nullopt;

  for (const auto& preset : kPresets) {
    if (!DomainListed(preset.domains, domain)) continue;
    return MailProviderInfo{QString::fromLatin1(preset.name), preset.note};
  }
  return std::nullopt;
}

auto MailPresetForAddress(const QString& address)
    -> std::optional<MailServerSuggestion> {
  const auto domain = MailDomainOf(address);
  if (domain.isEmpty()) return std::nullopt;

  for (const auto& preset : kPresets) {
    if (!DomainListed(preset.domains, domain)) continue;

    MailServerSuggestion suggestion;
    suggestion.imap = {ExpandHost(preset.imap_host, domain), preset.imap_tls,
                       MailServerSource::kPRESET};
    suggestion.smtp = {ExpandHost(preset.smtp_host, domain), preset.smtp_tls,
                       MailServerSource::kPRESET};
    suggestion.note = preset.note;
    suggestion.provider = QString::fromLatin1(preset.name);
    return suggestion;
  }
  return std::nullopt;
}

/// Whether the preset for @p domain is written as a pattern.
auto MailPresetIsPattern(const QString& domain) -> bool {
  for (const auto& preset : kPresets) {
    if (!DomainListed(preset.domains, domain)) continue;
    return QString::fromLatin1(preset.imap_host).contains('*') ||
           QString::fromLatin1(preset.smtp_host).contains('*');
  }
  return false;
}

auto MailPresetDomains() -> QStringList {
  QStringList domains;
  for (const auto& preset : kPresets) {
    domains.append(
        QString::fromLatin1(preset.domains).split(' ', Qt::SkipEmptyParts));
  }
  return domains;
}

auto MailNeedsDnsLookup(const QString& address) -> bool {
  const auto domain = MailDomainOf(address);
  return domain.contains('.') && !MailPresetForAddress(address).has_value();
}

auto MailHostCandidates(const QString& domain, bool imap) -> QStringList {
  const auto d = domain.trimmed().toLower();
  if (d.isEmpty()) return {};
  return {(imap ? QStringLiteral("imap.") : QStringLiteral("smtp.")) + d,
          QStringLiteral("mail.") + d};
}

auto MailServersFromResolved(const QString& domain, const QStringList& existing)
    -> MailServerSuggestion {
  MailServerSuggestion suggestion;
  for (const bool imap : {true, false}) {
    for (const auto& candidate : MailHostCandidates(domain, imap)) {
      if (!existing.contains(candidate, Qt::CaseInsensitive)) continue;
      (imap ? suggestion.imap : suggestion.smtp) = {
          candidate, MailTlsMode::kIMPLICIT, MailServerSource::kRESOLVED};
      break;
    }
  }
  return suggestion;
}

auto MailGuessServers(const QString& domain) -> MailServerSuggestion {
  MailServerSuggestion suggestion;
  const auto d = domain.trimmed().toLower();
  if (d.isEmpty()) return suggestion;

  suggestion.imap = {"imap." + d, MailTlsMode::kIMPLICIT,
                     MailServerSource::kGUESS};
  suggestion.smtp = {"smtp." + d, MailTlsMode::kIMPLICIT,
                     MailServerSource::kGUESS};
  return suggestion;
}

auto MailSrvServices() -> QStringList {
  QStringList names;
  for (const auto& service : kSrvServices) {
    names.append(QString::fromLatin1(service.name));
  }
  return names;
}

auto MailServersFromSrv(const QList<MailSrvRecord>& records)
    -> MailServerSuggestion {
  MailServerSuggestion suggestion;

  for (const bool imap : {true, false}) {
    auto& choice = imap ? suggestion.imap : suggestion.smtp;

    // In preference order, and the first service that answers decides. A
    // refusal of the TLS service does not rule out STARTTLS -- a domain may
    // well offer only that -- so the search goes on past it.
    for (const auto& service : kSrvServices) {
      if (service.imap != imap) continue;

      const auto best = BestRecord(records, service);
      if (!best) continue;

      choice = {best->target, service.tls, MailServerSource::kDNS};
      break;
    }
  }
  return suggestion;
}

auto MailMergeSuggestions(const MailServerSuggestion& primary,
                          const MailServerSuggestion& fallback)
    -> MailServerSuggestion {
  auto merged = primary;
  if (!merged.imap.IsKnown()) merged.imap = fallback.imap;
  if (!merged.smtp.IsKnown()) merged.smtp = fallback.smtp;
  if (merged.note == MailProviderNote::kNONE) merged.note = fallback.note;
  if (merged.provider.isEmpty()) merged.provider = fallback.provider;
  return merged;
}

auto MailNewAccount(const QString& id, const QString& display_name,
                    const QString& address, const MailServerSuggestion& servers,
                    MailAccountUsage usage) -> MailAccountConfig {
  MailAccountConfig account;
  account.id = id;
  account.display_name = display_name.trimmed();
  account.address = address.trimmed();

  account.imap.enabled = usage != MailAccountUsage::kSEND_ONLY;
  account.imap.host = servers.imap.host.trimmed();
  account.imap.tls = servers.imap.tls;
  account.imap.username = account.address;

  account.smtp.enabled = usage != MailAccountUsage::kRECEIVE_ONLY;
  account.smtp.host = servers.smtp.host.trimmed();
  account.smtp.tls = servers.smtp.tls;
  account.smtp.username = account.address;
  return account;
}

void MailFollowAddress(MailAccountConfig& account, const QString& old_address) {
  const auto old_trimmed = old_address.trimmed();
  if (old_trimmed.isEmpty()) return;

  for (auto* transport : {&account.imap, &account.smtp}) {
    if (transport->username.compare(old_trimmed, Qt::CaseInsensitive) == 0) {
      transport->username = account.address.trimmed();
    }
  }
}

auto MailAccountProblems(const MailAccountConfig& account,
                         const QList<MailAccountConfig>& others)
    -> QList<MailAccountProblem> {
  QList<MailAccountProblem> problems;
  const auto address = account.address.trimmed();

  if (address.isEmpty()) {
    problems.append(MailAccountProblem::kNO_ADDRESS);
  } else if (!MailIsPlausibleAddress(address)) {
    problems.append(MailAccountProblem::kBAD_ADDRESS);
  }

  if (account.imap.enabled && account.imap.host.trimmed().isEmpty()) {
    problems.append(MailAccountProblem::kRECEIVE_WITHOUT_SERVER);
  }
  if (account.smtp.enabled && account.smtp.host.trimmed().isEmpty()) {
    problems.append(MailAccountProblem::kSEND_WITHOUT_SERVER);
  }
  if (!account.imap.enabled && !account.smtp.enabled) {
    problems.append(MailAccountProblem::kNOTHING_ENABLED);
  }

  // Two accounts with the same address are indistinguishable in every list
  // that offers them, and the one that gets picked is whichever comes first.
  if (!address.isEmpty()) {
    for (const auto& other : others) {
      if (!account.id.isEmpty() && other.id == account.id) continue;
      if (other.address.trimmed().compare(address, Qt::CaseInsensitive) != 0) {
        continue;
      }
      problems.append(MailAccountProblem::kDUPLICATE_ADDRESS);
      break;
    }
  }
  return problems;
}

auto MailSetupChecksPassed(const MailSetupState& state) -> bool {
  const auto& account = state.account;
  if (!account.imap.enabled && !account.smtp.enabled) return false;
  if (account.imap.enabled && state.imap_check != MailCheckState::kPASSED) {
    return false;
  }
  if (account.smtp.enabled && state.smtp_check != MailCheckState::kPASSED) {
    return false;
  }
  return true;
}

auto MailSetupCanAdvance(MailSetupStep step, const MailSetupState& state)
    -> bool {
  const auto problems = MailAccountProblems(state.account, state.others);
  const auto has = [&problems](MailAccountProblem p) {
    return problems.contains(p);
  };

  switch (step) {
    case MailSetupStep::kACCOUNT:
      // A password is required here, unlike in the editor: an account added
      // without one could never connect, and nothing else would ask for it.
      return state.has_password && !has(MailAccountProblem::kNO_ADDRESS) &&
             !has(MailAccountProblem::kBAD_ADDRESS) &&
             !has(MailAccountProblem::kDUPLICATE_ADDRESS);
    case MailSetupStep::kSERVERS:
      return !has(MailAccountProblem::kRECEIVE_WITHOUT_SERVER) &&
             !has(MailAccountProblem::kSEND_WITHOUT_SERVER) &&
             !has(MailAccountProblem::kNOTHING_ENABLED);
    case MailSetupStep::kCHECK: {
      // Never while a test is still running: what it finds is what the review
      // step reports.
      const auto running = state.imap_check == MailCheckState::kRUNNING ||
                           state.smtp_check == MailCheckState::kRUNNING;
      if (running) return false;
      return MailSetupChecksPassed(state) || state.accept_unverified;
    }
    case MailSetupStep::kREVIEW:
      return problems.isEmpty();
  }
  return false;
}
