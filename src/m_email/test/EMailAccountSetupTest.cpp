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

#include <gtest/gtest.h>

#include "EMailAccountSetup.h"

using namespace Qt::StringLiterals;

// The Add Account wizard's decisions and the settings page's validation,
// checked without a widget.

namespace {

auto Account(const QString& id, const QString& address) -> MailAccountConfig {
  MailAccountConfig account;
  account.id = id;
  account.address = address;
  account.imap.enabled = true;
  account.imap.host = u"imap.example.org"_s;
  account.smtp.enabled = true;
  account.smtp.host = u"smtp.example.org"_s;
  return account;
}

TEST(EMailAccountSetupTest, TheDomainIsTakenAfterTheLastAt) {
  EXPECT_EQ(MailDomainOf(u"  Alice@Example.ORG "_s), u"example.org"_s);
  EXPECT_EQ(MailDomainOf(u"\"a@b\"@example.org"_s), u"example.org"_s);
  EXPECT_TRUE(MailDomainOf(u"no-at-sign"_s).isEmpty());
}

TEST(EMailAccountSetupTest, AKnownProviderIsMatchedWhateverTheCase) {
  const auto gmail = MailPresetForAddress(u"someone@GMail.com"_s);
  ASSERT_TRUE(gmail.has_value());
  EXPECT_EQ(gmail->imap.host, u"imap.gmail.com"_s);
  EXPECT_EQ(gmail->imap.tls, MailTlsMode::kIMPLICIT);
  EXPECT_EQ(gmail->smtp.host, u"smtp.gmail.com"_s);
  EXPECT_EQ(gmail->imap.source, MailServerSource::kPRESET);
  EXPECT_EQ(gmail->note, MailProviderNote::kAPP_PASSWORD);
  EXPECT_EQ(gmail->provider, u"Gmail"_s);

  // Every alias of a provider leads to the same servers.
  const auto googlemail = MailPresetForAddress(u"someone@googlemail.com"_s);
  ASSERT_TRUE(googlemail.has_value());
  EXPECT_EQ(googlemail->imap.host, gmail->imap.host);
}

TEST(EMailAccountSetupTest, ASubdomainOfAProviderIsNotThatProvider) {
  EXPECT_FALSE(MailPresetForAddress(u"a@corp.outlook.com"_s).has_value());
  EXPECT_FALSE(MailPresetForAddress(u"a@gmail.com.example"_s).has_value());
  EXPECT_FALSE(MailPresetForAddress(u"not an address"_s).has_value());
}

TEST(EMailAccountSetupTest, EveryPresetUsesTheServersPolicyPorts) {
  // A preset can only name a connection type, never a port, so a provider on
  // other ports would be a suggestion that can never connect.
  for (const auto& address :
       {u"a@gmail.com"_s, u"a@outlook.com"_s, u"a@icloud.com"_s,
        u"a@yahoo.com"_s, u"a@fastmail.com"_s, u"a@mailbox.org"_s,
        u"a@posteo.de"_s, u"a@gmx.de"_s, u"a@web.de"_s, u"a@zoho.com"_s}) {
    const auto preset = MailPresetForAddress(address);
    ASSERT_TRUE(preset.has_value()) << address.toStdString();
    EXPECT_NE(preset->imap.tls, MailTlsMode::kNONE);
    EXPECT_NE(preset->smtp.tls, MailTlsMode::kNONE);
    EXPECT_FALSE(MailHostAllowsCleartext(preset->imap.host));
  }
}

TEST(EMailAccountSetupTest, APatternPresetIsExactlyTheConventionalNames) {
  // Listed only so the address needs no DNS check: a pattern that produced
  // anything other than the guess would be a spelled-out preset in disguise.
  int patterns = 0;
  for (const auto& domain : MailPresetDomains()) {
    if (!MailPresetIsPattern(domain)) continue;
    ++patterns;
    const auto preset = MailPresetForAddress(u"a@"_s + domain);
    const auto guess = MailGuessServers(domain);
    ASSERT_TRUE(preset.has_value()) << domain.toStdString();
    EXPECT_EQ(preset->imap.host, guess.imap.host) << domain.toStdString();
    EXPECT_EQ(preset->smtp.host, guess.smtp.host) << domain.toStdString();
    EXPECT_EQ(preset->imap.tls, guess.imap.tls) << domain.toStdString();
    EXPECT_EQ(preset->smtp.tls, guess.smtp.tls) << domain.toStdString();
  }
  EXPECT_GT(patterns, 0);
}

TEST(EMailAccountSetupTest, ASpelledOutPresetIsNotTheConventionalNames) {
  // One that is should be written as a pattern, so the two cannot drift.
  for (const auto& domain : MailPresetDomains()) {
    if (MailPresetIsPattern(domain)) continue;
    const auto preset = MailPresetForAddress(u"a@"_s + domain);
    const auto guess = MailGuessServers(domain);
    ASSERT_TRUE(preset.has_value()) << domain.toStdString();
    const auto same = preset->imap.host == guess.imap.host &&
                      preset->smtp.host == guess.smtp.host &&
                      preset->imap.tls == guess.imap.tls &&
                      preset->smtp.tls == guess.smtp.tls;
    EXPECT_FALSE(same) << domain.toStdString();
  }
}

TEST(EMailAccountSetupTest, OnlyAnUnlistedDomainNeedsADnsCheck) {
  EXPECT_FALSE(MailNeedsDnsLookup(u"a@gmail.com"_s));
  EXPECT_FALSE(MailNeedsDnsLookup(u"a@outlook.com"_s));
  EXPECT_TRUE(MailNeedsDnsLookup(u"a@example.org"_s));
  // Nothing to ask about yet.
  EXPECT_FALSE(MailNeedsDnsLookup(u"a@localhost"_s));
  EXPECT_FALSE(MailNeedsDnsLookup(u"no address"_s));
}

TEST(EMailAccountSetupTest, AProviderIsKnownByItsPasswordRules) {
  const auto gmail = MailProviderInfoFor(u"a@gmail.com"_s);
  ASSERT_TRUE(gmail.has_value());
  EXPECT_EQ(gmail->name, u"Gmail"_s);
  EXPECT_EQ(gmail->note, MailProviderNote::kAPP_PASSWORD);
  EXPECT_FALSE(MailProviderInfoFor(u"a@example.org"_s).has_value());
}

TEST(EMailAccountSetupTest, TheDnsCheckAsksForTheConventionalNamesInOrder) {
  EXPECT_EQ(MailHostCandidates(u"Example.org"_s, true),
            (QStringList{u"imap.example.org"_s, u"mail.example.org"_s}));
  EXPECT_EQ(MailHostCandidates(u"example.org"_s, false),
            (QStringList{u"smtp.example.org"_s, u"mail.example.org"_s}));
  EXPECT_TRUE(MailHostCandidates({}, true).isEmpty());
}

TEST(EMailAccountSetupTest, TheFirstNameThatExistsIsUsed) {
  // imap.example.org exists, so it wins over mail.; for sending only mail.
  // exists, so that is used.
  const auto found = MailServersFromResolved(
      u"example.org"_s, {u"mail.example.org"_s, u"IMAP.example.org"_s});
  EXPECT_EQ(found.imap.host, u"imap.example.org"_s);
  EXPECT_EQ(found.imap.source, MailServerSource::kRESOLVED);
  EXPECT_EQ(found.imap.tls, MailTlsMode::kIMPLICIT);
  EXPECT_EQ(found.smtp.host, u"mail.example.org"_s);
}

TEST(EMailAccountSetupTest, NoNameThatExistsLeavesTheTransportOpen) {
  const auto found = MailServersFromResolved(u"example.org"_s, {});
  EXPECT_FALSE(found.imap.IsKnown());
  EXPECT_FALSE(found.smtp.IsKnown());

  // And the guess fills it, marked as a guess.
  const auto merged =
      MailMergeSuggestions(found, MailGuessServers(u"example.org"_s));
  EXPECT_EQ(merged.imap.source, MailServerSource::kGUESS);
}

TEST(EMailAccountSetupTest, TheGuessIsImapAndSmtpUnderTheDomain) {
  const auto guess = MailGuessServers(u"Example.org"_s);
  EXPECT_EQ(guess.imap.host, u"imap.example.org"_s);
  EXPECT_EQ(guess.smtp.host, u"smtp.example.org"_s);
  EXPECT_EQ(guess.imap.tls, MailTlsMode::kIMPLICIT);
  EXPECT_EQ(guess.smtp.tls, MailTlsMode::kIMPLICIT);
  EXPECT_EQ(guess.imap.source, MailServerSource::kGUESS);

  EXPECT_FALSE(MailGuessServers({}).imap.IsKnown());
}

TEST(EMailAccountSetupTest, SrvPrefersLowPriorityThenHighWeight) {
  const auto suggestion = MailServersFromSrv({
      {u"_imaps._tcp"_s, u"b.example.org."_s, 993, 20, 0},
      {u"_imaps._tcp"_s, u"a.example.org."_s, 993, 10, 5},
      {u"_imaps._tcp"_s, u"c.example.org."_s, 993, 10, 50},
  });
  EXPECT_EQ(suggestion.imap.host, u"c.example.org"_s);
  EXPECT_EQ(suggestion.imap.tls, MailTlsMode::kIMPLICIT);
  EXPECT_EQ(suggestion.imap.source, MailServerSource::kDNS);
  EXPECT_FALSE(suggestion.smtp.IsKnown());
}

TEST(EMailAccountSetupTest, SrvPrefersTlsFromTheFirstByteOverStarttls) {
  const auto suggestion = MailServersFromSrv({
      {u"_imap._tcp"_s, u"plain.example.org"_s, 143, 0, 0},
      {u"_imaps._tcp"_s, u"tls.example.org"_s, 993, 50, 0},
      {u"_submission._tcp"_s, u"587.example.org"_s, 587, 0, 0},
  });
  EXPECT_EQ(suggestion.imap.host, u"tls.example.org"_s);

  // Only STARTTLS on offer for sending: that is what is suggested.
  EXPECT_EQ(suggestion.smtp.host, u"587.example.org"_s);
  EXPECT_EQ(suggestion.smtp.tls, MailTlsMode::kSTARTTLS);
}

TEST(EMailAccountSetupTest, SrvNeverSuggestsADeclinedService) {
  // "." is the domain saying it does not offer the service at all.
  const auto suggestion = MailServersFromSrv({
      {u"_imaps._tcp"_s, u"."_s, 0, 0, 0},
      {u"_imap._tcp"_s, u"imap.example.org"_s, 143, 0, 0},
      {u"_submissions._tcp"_s, u"."_s, 0, 0, 0},
  });
  EXPECT_EQ(suggestion.imap.host, u"imap.example.org"_s);
  EXPECT_EQ(suggestion.imap.tls, MailTlsMode::kSTARTTLS);
  EXPECT_FALSE(suggestion.smtp.IsKnown());
}

TEST(EMailAccountSetupTest, SrvOnAnotherPortIsNotSuggested) {
  // The port is policy here and cannot be configured, so a server on a
  // different one could never be reached.
  const auto suggestion = MailServersFromSrv({
      {u"_imaps._tcp"_s, u"odd.example.org"_s, 1993, 0, 0},
      {u"_submissions._tcp"_s, u"smtp.example.org"_s, 465, 0, 0},
  });
  EXPECT_FALSE(suggestion.imap.IsKnown());
  EXPECT_EQ(suggestion.smtp.host, u"smtp.example.org"_s);
}

TEST(EMailAccountSetupTest, AMergeFillsOnlyWhatIsMissing) {
  const auto dns =
      MailServersFromSrv({{u"_imaps._tcp"_s, u"mx.example.org"_s, 993, 0, 0}});
  const auto merged =
      MailMergeSuggestions(dns, MailGuessServers(u"example.org"_s));
  EXPECT_EQ(merged.imap.host, u"mx.example.org"_s);
  EXPECT_EQ(merged.imap.source, MailServerSource::kDNS);
  EXPECT_EQ(merged.smtp.host, u"smtp.example.org"_s);
  EXPECT_EQ(merged.smtp.source, MailServerSource::kGUESS);
}

TEST(EMailAccountSetupTest, ANewAccountLogsInWithItsAddress) {
  const auto servers = MailGuessServers(u"example.org"_s);
  const auto account =
      MailNewAccount(u"id1"_s, u" Alice "_s, u" alice@example.org "_s, servers,
                     MailAccountUsage::kBOTH);
  EXPECT_EQ(account.id, u"id1"_s);
  EXPECT_EQ(account.display_name, u"Alice"_s);
  EXPECT_EQ(account.address, u"alice@example.org"_s);
  EXPECT_EQ(account.imap.username, u"alice@example.org"_s);
  EXPECT_EQ(account.smtp.username, u"alice@example.org"_s);
  EXPECT_TRUE(account.imap.enabled);
  EXPECT_TRUE(account.smtp.enabled);
  EXPECT_TRUE(account.IsUsable());
}

TEST(EMailAccountSetupTest, UsageTurnsTheOtherDirectionOff) {
  const auto servers = MailGuessServers(u"example.org"_s);
  const auto receive = MailNewAccount({}, {}, u"a@example.org"_s, servers,
                                      MailAccountUsage::kRECEIVE_ONLY);
  EXPECT_TRUE(receive.imap.enabled);
  EXPECT_FALSE(receive.smtp.enabled);

  const auto send = MailNewAccount({}, {}, u"a@example.org"_s, servers,
                                   MailAccountUsage::kSEND_ONLY);
  EXPECT_FALSE(send.imap.enabled);
  EXPECT_TRUE(send.smtp.enabled);
}

TEST(EMailAccountSetupTest, AUserNameThatWasTheAddressFollowsIt) {
  auto account = Account(u"1"_s, u"new@example.org"_s);
  account.imap.username = u"Old@Example.org"_s;
  account.smtp.username = u"someone-else"_s;

  MailFollowAddress(account, u"old@example.org"_s);
  EXPECT_EQ(account.imap.username, u"new@example.org"_s);
  EXPECT_EQ(account.smtp.username, u"someone-else"_s);
}

TEST(EMailAccountSetupTest, AnEmptyUserNameStaysEmpty) {
  // Empty means "do not authenticate" for SMTP, which is the user's choice.
  auto account = Account(u"1"_s, u"new@example.org"_s);
  MailFollowAddress(account, {});
  EXPECT_TRUE(account.imap.username.isEmpty());
  EXPECT_TRUE(account.smtp.username.isEmpty());
}

TEST(EMailAccountSetupTest, ADisabledAccountIsNotUsableButKeepsItsSettings) {
  auto account = Account(u"1"_s, u"a@example.org"_s);
  ASSERT_TRUE(account.IsUsable());

  account.enabled = false;
  EXPECT_FALSE(account.IsUsable());

  const auto back = MailAccountConfig::FromJson(account.ToJson());
  EXPECT_FALSE(back.enabled);
  EXPECT_EQ(back.imap.host, account.imap.host);
}

TEST(EMailAccountSetupTest, AnAccountStoredBeforeDisablingExistedIsEnabled) {
  auto json = Account(u"1"_s, u"a@example.org"_s).ToJson();
  json.remove(u"enabled"_s);
  EXPECT_TRUE(MailAccountConfig::FromJson(json).enabled);
}

TEST(EMailAccountSetupTest, AProblemFreeAccountHasNoProblems) {
  EXPECT_TRUE(
      MailAccountProblems(Account(u"1"_s, u"a@example.org"_s), {}).isEmpty());
}

TEST(EMailAccountSetupTest, ProblemsAreSaidInOrder) {
  auto account = Account(u"1"_s, {});
  account.imap.host.clear();
  const auto problems = MailAccountProblems(account, {});
  ASSERT_EQ(problems.size(), 2);
  EXPECT_EQ(problems.at(0), MailAccountProblem::kNO_ADDRESS);
  EXPECT_EQ(problems.at(1), MailAccountProblem::kRECEIVE_WITHOUT_SERVER);

  account.address = u"not an address"_s;
  account.imap.enabled = false;
  account.smtp.enabled = false;
  const auto again = MailAccountProblems(account, {});
  EXPECT_TRUE(again.contains(MailAccountProblem::kBAD_ADDRESS));
  EXPECT_TRUE(again.contains(MailAccountProblem::kNOTHING_ENABLED));
}

TEST(EMailAccountSetupTest, ADuplicateAddressIsFoundButNotItself) {
  const auto mine = Account(u"1"_s, u"A@example.org"_s);
  const auto other = Account(u"2"_s, u"a@EXAMPLE.org"_s);

  // The list the settings page holds includes the account being checked.
  EXPECT_TRUE(MailAccountProblems(mine, {mine}).isEmpty());
  EXPECT_EQ(MailAccountProblems(mine, {mine, other}),
            QList<MailAccountProblem>{MailAccountProblem::kDUPLICATE_ADDRESS});
}

TEST(EMailAccountSetupTest, TheAccountStepNeedsAnAddressAndAPassword) {
  MailSetupState state;
  state.account = Account(u"new"_s, u"a@example.org"_s);
  EXPECT_FALSE(MailSetupCanAdvance(MailSetupStep::kACCOUNT, state));

  state.has_password = true;
  EXPECT_TRUE(MailSetupCanAdvance(MailSetupStep::kACCOUNT, state));

  state.account.address = u"a@"_s;
  EXPECT_FALSE(MailSetupCanAdvance(MailSetupStep::kACCOUNT, state));

  // An address another account already has is refused before any lookup.
  state.account.address = u"a@example.org"_s;
  state.others = {Account(u"old"_s, u"A@example.org"_s)};
  EXPECT_FALSE(MailSetupCanAdvance(MailSetupStep::kACCOUNT, state));
}

TEST(EMailAccountSetupTest, TheServersStepNeedsAServerForEachDirectionUsed) {
  MailSetupState state;
  state.account = Account(u"new"_s, u"a@example.org"_s);
  EXPECT_TRUE(MailSetupCanAdvance(MailSetupStep::kSERVERS, state));

  state.account.smtp.host.clear();
  EXPECT_FALSE(MailSetupCanAdvance(MailSetupStep::kSERVERS, state));

  state.account.smtp.enabled = false;
  EXPECT_TRUE(MailSetupCanAdvance(MailSetupStep::kSERVERS, state));

  state.account.imap.enabled = false;
  EXPECT_FALSE(MailSetupCanAdvance(MailSetupStep::kSERVERS, state));
}

TEST(EMailAccountSetupTest, TheCheckStepWaitsThenNeedsSuccessOrConsent) {
  MailSetupState state;
  state.account = Account(u"new"_s, u"a@example.org"_s);
  EXPECT_FALSE(MailSetupCanAdvance(MailSetupStep::kCHECK, state));

  state.imap_check = MailCheckState::kPASSED;
  state.smtp_check = MailCheckState::kRUNNING;
  EXPECT_FALSE(MailSetupCanAdvance(MailSetupStep::kCHECK, state));

  // Consent does not cut a running test short.
  state.accept_unverified = true;
  EXPECT_FALSE(MailSetupCanAdvance(MailSetupStep::kCHECK, state));

  state.smtp_check = MailCheckState::kFAILED;
  EXPECT_TRUE(MailSetupCanAdvance(MailSetupStep::kCHECK, state));

  state.accept_unverified = false;
  EXPECT_FALSE(MailSetupCanAdvance(MailSetupStep::kCHECK, state));

  state.smtp_check = MailCheckState::kPASSED;
  EXPECT_TRUE(MailSetupCanAdvance(MailSetupStep::kCHECK, state));
  EXPECT_TRUE(MailSetupChecksPassed(state));
}

TEST(EMailAccountSetupTest, AStoppedCheckIsNeverAPass) {
  MailSetupState state;
  state.account = Account(u"new"_s, u"a@example.org"_s);
  state.imap_check = MailCheckState::kPASSED;
  state.smtp_check = MailCheckState::kSTOPPED;
  EXPECT_FALSE(MailSetupChecksPassed(state));
  EXPECT_FALSE(MailSetupCanAdvance(MailSetupStep::kCHECK, state));

  // Only the user's explicit choice lets it through, as for a failure.
  state.accept_unverified = true;
  EXPECT_TRUE(MailSetupCanAdvance(MailSetupStep::kCHECK, state));
}

TEST(EMailAccountSetupTest, ADirectionNotUsedNeedsNoCheck) {
  MailSetupState state;
  state.account = Account(u"new"_s, u"a@example.org"_s);
  state.account.smtp.enabled = false;
  state.imap_check = MailCheckState::kPASSED;
  EXPECT_TRUE(MailSetupChecksPassed(state));
  EXPECT_TRUE(MailSetupCanAdvance(MailSetupStep::kCHECK, state));
}

}  // namespace
