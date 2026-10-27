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
#include <QString>
#include <cstdint>
#include <optional>

#include "EMailAccountModel.h"

/**
 * @file
 * @brief Setting up a mail account, with no widget behind it.
 *
 * Which servers an address probably uses, what is wrong with an account, and
 * when a step of the Add Account wizard may be left. Kept apart from the
 * wizard and the settings page for the reason EMailViewLayout is: this
 * module's tests link Qt Core only, and a rule written inline in a widget is a
 * rule nothing checks.
 */

/// Where a server suggestion came from. Shown to the user, because "we know
/// this provider" and "we guessed" deserve different amounts of trust.
enum class MailServerSource : uint8_t {
  kNONE,      ///< nothing found; the user has to say
  kPRESET,    ///< the built-in provider list
  kDNS,       ///< the domain's own SRV records (RFC 6186 / RFC 8314)
  kRESOLVED,  ///< a conventional name, imap.<domain> say, that exists in DNS
  kGUESS,     ///< imap.<domain> and smtp.<domain>, not known to exist
};

/// Something about a provider the user needs to hear before the password
/// field means what they think it does.
enum class MailProviderNote : uint8_t {
  kNONE,
  /// The account password is refused over IMAP/SMTP; an app password works.
  kAPP_PASSWORD,
  /// The provider wants a sign-in through its own web page (OAuth2), which
  /// this version cannot do. Some accounts still accept an app password.
  kWEB_SIGN_IN,
};

/// One transport's suggested server.
struct MailServerChoice {
  QString host;
  MailTlsMode tls{MailTlsMode::kIMPLICIT};
  MailServerSource source{MailServerSource::kNONE};

  [[nodiscard]] auto IsKnown() const -> bool { return !host.isEmpty(); }
};

/// What the wizard suggests for an address, per transport. The two are
/// independent: a domain may publish one and not the other.
struct MailServerSuggestion {
  MailServerChoice imap;
  MailServerChoice smtp;
  MailProviderNote note{MailProviderNote::kNONE};
  /// The provider's name when it came from the built-in list, for display.
  QString provider;
};

/// The domain of @p address, lower-cased, or empty when there is none.
auto MailDomainOf(const QString& address) -> QString;

/// A provider known by name, and what it asks of the password.
struct MailProviderInfo {
  QString name;
  MailProviderNote note{MailProviderNote::kNONE};
};

/// What is known about the provider behind @p address, servers aside.
auto MailProviderInfoFor(const QString& address)
    -> std::optional<MailProviderInfo>;

/**
 * @brief Whether finding the servers for @p address means asking DNS.
 *
 * True for any plausible domain without a preset. The wizard says so before
 * it asks, because a lookup tells the resolver which domain is being set up.
 */
auto MailNeedsDnsLookup(const QString& address) -> bool;

/**
 * @brief The built-in suggestion for a well-known provider.
 *
 * A listed provider needs no DNS check. Those on the conventional names are
 * listed as a pattern -- imap.* and smtp.*, "*" being the address's domain --
 * and the rest with their own host names.
 *
 * Matched on the exact domain, case-insensitively. A subdomain is somebody
 * else's server more often than not, so "mail.gmail.com.example" and
 * "corp.outlook.com" are not matched.
 */
auto MailPresetForAddress(const QString& address)
    -> std::optional<MailServerSuggestion>;

/// Every domain with a preset, for tests that hold the list to its rules.
auto MailPresetDomains() -> QStringList;

/// Whether @p domain's preset is a pattern rather than spelled out.
auto MailPresetIsPattern(const QString& domain) -> bool;

/// imap.<domain> and smtp.<domain>, both with TLS from the first byte. The
/// connection test is what confirms or refutes it.
auto MailGuessServers(const QString& domain) -> MailServerSuggestion;

/// The conventional names checked in DNS for one transport, in order of
/// preference: imap.<domain> or smtp.<domain>, then mail.<domain>.
auto MailHostCandidates(const QString& domain, bool imap) -> QStringList;

/**
 * @brief The first candidate per transport that exists in DNS.
 *
 * @param existing the candidate names that resolved
 * @return a suggestion with TLS from the first byte for whatever was found,
 *         and nothing for a transport none of whose names exist
 */
auto MailServersFromResolved(const QString& domain, const QStringList& existing)
    -> MailServerSuggestion;

/// One SRV answer, as fetched. Plain data so the ranking can be tested
/// without a resolver.
struct MailSrvRecord {
  QString service;  ///< e.g. "_imaps._tcp"
  QString target;
  quint16 port{0};
  quint16 priority{0};
  quint16 weight{0};
};

/// The SRV names that are asked about, in order of preference within each
/// transport: TLS from the first byte before STARTTLS (RFC 8314).
auto MailSrvServices() -> QStringList;

/**
 * @brief The best server per transport among @p records.
 *
 * Lowest priority wins, then highest weight. A target of "." means the domain
 * says it does not offer that service, so it is never suggested. A
 * record whose port is not the policy port for its connection type is not
 * used: the port is never configurable here, so a server on another port is a
 * server this account cannot reach, and suggesting it would only fail later.
 */
auto MailServersFromSrv(const QList<MailSrvRecord>& records)
    -> MailServerSuggestion;

/**
 * @brief Fills in whatever @p primary left open from @p fallback.
 *
 * Per transport, so a domain that publishes only an IMAP record still gets a
 * guessed SMTP server rather than none.
 */
auto MailMergeSuggestions(const MailServerSuggestion& primary,
                          const MailServerSuggestion& fallback)
    -> MailServerSuggestion;

/// Which directions an account is for.
enum class MailAccountUsage : uint8_t { kBOTH, kRECEIVE_ONLY, kSEND_ONLY };

/**
 * @brief A new account, as the wizard hands it to the settings page.
 *
 * Both user names start as the address, which is what nearly every provider
 * expects; the editor's Advanced section is where they can differ.
 */
auto MailNewAccount(const QString& id, const QString& display_name,
                    const QString& address, const MailServerSuggestion& servers,
                    MailAccountUsage usage) -> MailAccountConfig;

/**
 * @brief Carries a user name along with an address being edited.
 *
 * A user name that was the old address was almost certainly meant to be the
 * address, so it follows it. Anything else -- including an empty one, which
 * for SMTP means "do not authenticate" -- is the user's own and is left alone.
 */
void MailFollowAddress(MailAccountConfig& account, const QString& old_address);

/// What can be wrong with an account. The wording lives in the widgets, so
/// these stay translatable there.
enum class MailAccountProblem : uint8_t {
  kNO_ADDRESS,
  kBAD_ADDRESS,
  kRECEIVE_WITHOUT_SERVER,
  kSEND_WITHOUT_SERVER,
  kNOTHING_ENABLED,
  kDUPLICATE_ADDRESS,
};

/**
 * @brief Everything wrong with @p account, in the order it should be said.
 *
 * @param others the other accounts, for the duplicate check; @p account itself
 *        is recognised by id and skipped if it is among them
 */
auto MailAccountProblems(const MailAccountConfig& account,
                         const QList<MailAccountConfig>& others)
    -> QList<MailAccountProblem>;

/// The steps of the Add Account wizard, in order.
enum class MailSetupStep : uint8_t { kACCOUNT, kSERVERS, kCHECK, kREVIEW };

constexpr int kMailSetupStepCount = 4;

/// Where a connection check stands.
enum class MailCheckState : uint8_t {
  kNOT_RUN,
  kRUNNING,
  kPASSED,
  kFAILED,
  /// Stopped before it had an answer. Never a pass: nothing was learnt.
  kSTOPPED,
};

/// What the wizard knows when it asks whether Continue may be pressed.
struct MailSetupState {
  MailAccountConfig account;
  QList<MailAccountConfig> others;
  bool has_password{false};
  MailCheckState imap_check{MailCheckState::kNOT_RUN};
  MailCheckState smtp_check{MailCheckState::kNOT_RUN};
  /// The user chose to add the account without a working connection.
  bool accept_unverified{false};
};

/// Whether every enabled transport connected.
auto MailSetupChecksPassed(const MailSetupState& state) -> bool;

/// Whether @p step may be left forwards.
auto MailSetupCanAdvance(MailSetupStep step, const MailSetupState& state)
    -> bool;
