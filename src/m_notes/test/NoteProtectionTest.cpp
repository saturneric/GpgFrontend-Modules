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

#include <algorithm>

#include "NoteProtection.h"

using namespace Qt::StringLiterals;
using namespace gf::notes;

namespace {

const QString kAlice = u"AAAA000000000000000000000000000000000001"_s;
const QString kBob = u"BBBB000000000000000000000000000000000002"_s;

auto Facts(const QString& fpr, bool usable = true) -> KeyFacts {
  KeyFacts k;
  k.fingerprint = fpr;
  k.found = true;
  k.can_encrypt = usable;
  k.label = fpr.left(4);
  return k;
}

auto Pkesk(const QString& primary, bool found = true) -> DetectedPacket {
  DetectedPacket p;
  p.pkesk = true;
  p.key_id = primary.right(16);
  p.key_found = found;
  p.primary_fingerprint = found ? primary : QString();
  return p;
}

auto Skesk() -> DetectedPacket {
  DetectedPacket p;
  p.pkesk = false;
  return p;
}

auto Config(QStringList r, bool pass = false) -> ProtectionConfig {
  ProtectionConfig c;
  c.recipients = std::move(r);
  c.passphrase = pass;
  c.Normalise();
  return c;
}

auto Has(const Assessment& a, IssueKind k) -> bool {
  return std::any_of(a.issues.cbegin(), a.issues.cend(),
                     [k](const auto& i) { return i.kind == k; });
}

}  // namespace

TEST(NoteProtectionTest, ConfirmedHereAndConsistentIsResolved) {
  const auto c = Config({kAlice, kBob});
  const auto a =
      Assess(c, {Pkesk(kAlice), Pkesk(kBob)},
             {{kAlice, Facts(kAlice)}, {kBob, Facts(kBob)}}, {c.Hash()});
  EXPECT_TRUE(a.Resolved());
  EXPECT_EQ(a.proposal, c);
}

TEST(NoteProtectionTest, ImportedMetadataIsAProposalNotConsent) {
  const auto c = Config({kAlice});
  const auto a = Assess(c, {Pkesk(kAlice)}, {{kAlice, Facts(kAlice)}}, {});
  EXPECT_FALSE(a.confirmed);
  EXPECT_TRUE(Has(a, IssueKind::kNOT_CONFIRMED));
  EXPECT_EQ(a.proposal, c);
}

TEST(NoteProtectionTest, AMetadataPacketMismatchIsNeverConfirmed) {
  const auto c = Config({kAlice});  // but the file is encrypted to Bob too
  const auto a =
      Assess(c, {Pkesk(kAlice), Pkesk(kBob)},
             {{kAlice, Facts(kAlice)}, {kBob, Facts(kBob)}}, {c.Hash()});
  EXPECT_FALSE(a.confirmed);
  EXPECT_TRUE(Has(a, IssueKind::kMISMATCH));
}

TEST(NoteProtectionTest, KeysThatCannotBeUsedAreIssuesNotSilentDrops) {
  const auto c = Config({kAlice, kBob});
  const auto a = Assess(c, {Pkesk(kAlice), Pkesk(kBob, false)},
                        {{kAlice, Facts(kAlice, false)}}, {c.Hash()});
  EXPECT_FALSE(a.Resolved());
  EXPECT_TRUE(Has(a, IssueKind::kUNUSABLE_KEY));
  EXPECT_TRUE(Has(a, IssueKind::kMISSING_KEY));
  EXPECT_EQ(a.proposal.recipients.size(), 2) << "nobody removed";
}

TEST(NoteProtectionTest, ExpiredOrRevokedKeysAreUnusable) {
  const auto c = Config({kAlice, kBob});
  auto expired = Facts(kAlice);
  expired.expired = true;
  auto revoked = Facts(kBob);
  revoked.revoked = true;
  const auto a = Assess(c, {Pkesk(kAlice), Pkesk(kBob)},
                        {{kAlice, expired}, {kBob, revoked}}, {c.Hash()});
  EXPECT_FALSE(a.Resolved());
  EXPECT_EQ(std::count_if(a.issues.cbegin(), a.issues.cend(),
                          [](const auto& i) {
                            return i.kind == IssueKind::kUNUSABLE_KEY;
                          }),
            2);
}

TEST(NoteProtectionTest, HiddenRecipientsNeedAnExplicitDecision) {
  DetectedPacket hidden = Pkesk(kAlice, false);
  hidden.hidden = true;
  hidden.key_id.clear();
  const auto a =
      Assess(std::nullopt, {hidden, Pkesk(kBob)}, {{kBob, Facts(kBob)}}, {});
  EXPECT_TRUE(Has(a, IssueKind::kHIDDEN_RECIPIENT));
}

TEST(NoteProtectionTest, APassphraseNoteIsConfirmedWithoutHoldingIt) {
  // The Host asks for the passphrase when it encrypts; nothing is held here.
  const auto c = Config({}, true);
  const auto a = Assess(c, {Skesk()}, {}, {c.Hash()});
  EXPECT_TRUE(a.Resolved());
}

TEST(NoteProtectionTest, ANewNoteHasNothingToProposeYet) {
  const auto a = Assess(std::nullopt, {}, {}, {});
  EXPECT_TRUE(a.proposal.IsEmpty());
  EXPECT_FALSE(a.confirmed);
}

TEST(NoteProtectionTest, HashesAreStableOverOrderAndCase) {
  EXPECT_EQ(Config({kAlice, kBob}).Hash(),
            Config({kBob.toLower(), kAlice}).Hash());
  EXPECT_NE(Config({kAlice}).Hash(), Config({kAlice}, true).Hash());
  const auto back = ProtectionConfig::FromJson(Config({kAlice}, true).ToJson());
  ASSERT_TRUE(back.has_value());
  EXPECT_EQ(*back, Config({kAlice}, true));
}

TEST(NoteProtectionTest, KeysAndAPassphraseAreNeverUsedTogether) {
  ProtectionConfig keys;
  keys.recipients = {u"AA"_s};
  EXPECT_TRUE(keys.IsExclusive());
  ProtectionConfig pass;
  pass.passphrase = true;
  EXPECT_TRUE(pass.IsExclusive());
  ProtectionConfig both = keys;
  both.passphrase = true;
  EXPECT_FALSE(both.IsExclusive());
  EXPECT_FALSE(ProtectionConfig{}.IsExclusive()) << "nobody at all";
}

TEST(NoteProtectionTest, EveryLostReaderIsNamed) {
  ProtectionConfig from;
  from.recipients = {u"AA"_s, u"BB"_s};
  ProtectionConfig to;
  to.recipients = {u"bb"_s, u"CC"_s};
  auto lost = LostBetween(from, to);
  EXPECT_EQ(lost.recipients, QStringList{u"AA"_s});
  EXPECT_FALSE(lost.passphrase);

  ProtectionConfig pass;
  pass.passphrase = true;
  lost = LostBetween(pass, to);
  EXPECT_TRUE(lost.passphrase) << "switching to keys drops passphrase readers";
  lost = LostBetween(from, pass);
  EXPECT_EQ(lost.recipients.size(), 2) << "and the other way round, every key";
  EXPECT_TRUE(LostBetween(to, to).IsEmpty());
}
