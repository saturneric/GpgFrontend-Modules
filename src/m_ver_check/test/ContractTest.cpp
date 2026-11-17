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

// What the release tooling publishes, read as this checker reads it: the
// index, its signature and the descriptors and notes it names, exactly as
// scripts/tests/contract.py writes them with the code that publishes them
// (scripts/tests/fixtures/contract). The Python side holds those files to
// what it publishes now (test_contract.py); this side holds them to what it
// accepts, so neither can change the format without the other noticing.

#include <gtest/gtest.h>

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <optional>

#include "Minisign.h"
#include "ReleaseIndex.h"
#include "RootTrust.h"
#include "UpdateChecker.h"

namespace {

constexpr auto kProduct = "gpgfrontend";
constexpr auto kNightly = "2.4.0-alpha.12+g0123456789ab";
constexpr auto kBeta = "2.2.6-alpha.3+gabcdef012345";

auto Read(const QString& path) -> QByteArray {
  QFile file(QString::fromUtf8(GF_CONTRACT_FIXTURE) + "/" + path);
  if (!file.open(QIODevice::ReadOnly)) {
    ADD_FAILURE() << "cannot read " << path.toStdString();
    return {};
  }
  return file.readAll();
}

auto Object(const QByteArray& json) -> QJsonObject {
  return QJsonDocument::fromJson(json).object();
}

/// The keys of the index-keys list the fixtures are signed under.
auto ContractIndexKeys() -> QStringList {
  QFile file(QString::fromUtf8(GF_TRUST_FIXTURE) + "/index-keys/2.json");
  if (!file.open(QIODevice::ReadOnly)) return {};
  QStringList keys;
  for (const auto& key : Object(file.readAll()).value("keys").toObject()) {
    keys << key.toString();
  }
  return keys;
}

/// A case's index, its signature checked, read a day after it was made.
auto Load(const QString& name) -> std::optional<IndexFacts> {
  const auto data = Read(name + "/update/v1/index.json");
  const auto comment = MinisignVerify(
      ContractIndexKeys(), data, Read(name + "/update/v1/index.json.minisig"));
  if (!comment) return std::nullopt;
  const auto made = QDateTime::fromString(
      Object(data).value("generated_at").toString(), Qt::ISODate);
  return ParseIndex(data, *comment, kProduct, made.addDays(1));
}

auto Builds(const QString& name, const QString& path) -> QByteArray {
  return Read(name + "/builds/" + path);
}

}  // namespace

TEST(ContractTest, TheLimitsAreTheReleaseToolingsOwn) {
  const auto limits = Object(Read("limits.json"));
  EXPECT_EQ(limits.value("max_fetch_bytes").toInteger(),
            UpdateChecker::kMaxFetchBytes);
  EXPECT_EQ(limits.value("max_notes_bytes").toInteger(),
            UpdateChecker::kMaxNotesBytes);
  EXPECT_EQ(limits.value("expiry_skew_seconds").toInteger(),
            kExpirySkewSeconds);
  EXPECT_EQ(limits.value("max_index_lifetime_seconds").toInteger(),
            kMaxIndexLifetimeSeconds);
  EXPECT_EQ(limits.value("max_root_keys").toInteger(), kMaxRootKeys);
  EXPECT_EQ(limits.value("max_root_steps").toInteger(), kMaxRootSteps);
  // What the host signs for, this takes.
  EXPECT_LE(limits.value("index_lifetime_seconds").toInteger(),
            kMaxIndexLifetimeSeconds);
}

TEST(ContractTest, TheSignatureIsTheIndexKeysAndSaysTheSerial) {
  for (const auto* name : {"nightly", "full"}) {
    const auto comment = MinisignVerify(
        ContractIndexKeys(), Read(QString(name) + "/update/v1/index.json"),
        Read(QString(name) + "/update/v1/index.json.minisig"));
    ASSERT_TRUE(comment.has_value()) << name;
    EXPECT_EQ(*comment, QStringLiteral("product=gpgfrontend serial=42"));
  }
}

TEST(ContractTest, AProductWithOnlyItsNightly) {
  const auto facts = Load("nightly");
  ASSERT_TRUE(facts.has_value());
  EXPECT_EQ(facts->serial, 42);
  EXPECT_EQ(facts->nightly_version, kNightly);
  EXPECT_TRUE(facts->releases.isEmpty());

  const auto choice = ChooseUpdate(*facts, "v2.4.0-alpha.3", true);
  ASSERT_EQ(choice.list.outcome, ListOutcome::kFound);
  EXPECT_EQ(choice.list.latest.version, kNightly);
  const auto descriptor = Builds("nightly", choice.descriptor);
  EXPECT_EQ(Sha256Hex(descriptor), choice.descriptor_sha256);

  const auto pick =
      SelectArtifact(descriptor, Platform{"linux", "x86_64", "", "appimage"});
  EXPECT_TRUE(pick.path.endsWith("-linux-x86_64.AppImage"))
      << pick.path.toStdString();
  EXPECT_TRUE(IsSha256(pick.sha256));
  EXPECT_GT(pick.size, 0);
}

TEST(ContractTest, EveryKindOfEntryAProductHas) {
  const auto facts = Load("full");
  ASSERT_TRUE(facts.has_value());

  // Releases: the newest of a series that is not withdrawn, its notes.
  EXPECT_TRUE(facts->withdrawn.contains("2.2.4"));
  const auto stable = ChooseUpdate(*facts, "v2.2.3", false);
  ASSERT_EQ(stable.list.outcome, ListOutcome::kFound);
  EXPECT_EQ(stable.list.latest.version, "2.2.5");
  EXPECT_EQ(Sha256Hex(Builds("full", stable.descriptor)),
            stable.descriptor_sha256);
  ASSERT_FALSE(stable.notes.isEmpty());
  EXPECT_EQ(Sha256Hex(Builds("full", stable.notes)), stable.notes_sha256);
  for (const auto& entry : facts->releases) {
    EXPECT_EQ(Sha256Hex(Builds("full", entry.descriptor)),
              entry.descriptor_sha256)
        << entry.descriptor.toStdString();
  }

  // The series: an eol one's way out, a stable one's beta, a stable one's
  // release candidate before its first release.
  EXPECT_EQ(ResolveSuccessor(*facts, "2.1"), "2.2");
  EXPECT_EQ(facts->series.value("2.2").beta, kBeta);
  EXPECT_EQ(
      Sha256Hex(Builds("full", facts->series.value("2.2").beta_descriptor)),
      facts->series.value("2.2").beta_descriptor_sha256);
  EXPECT_EQ(facts->series.value("2.3").rc, "2.3.0-rc.1");
  EXPECT_TRUE(facts->series.value("2.3").IsPrerelease());
  EXPECT_EQ(facts->series.value("2.4").platforms.value("windows"),
            "10.0.17763");

  // Who is offered what.
  EXPECT_EQ(ChooseUpdate(*facts, "v2.4.0-alpha.3", true).list.latest.version,
            kNightly);
  const auto beta = ChooseUpdate(*facts, "v2.2.5", false, true);
  ASSERT_EQ(beta.list.outcome, ListOutcome::kFound);
  EXPECT_EQ(beta.list.latest.version, kBeta);

  // What a machine downloads of the stable release.
  const auto descriptor = Builds("full", stable.descriptor);
  EXPECT_TRUE(SelectArtifact(descriptor,
                             Platform{"windows", "x86_64", "10.0.19045", "msi"})
                  .path.endsWith("-windows-x86_64.msi"));
  EXPECT_TRUE(
      SelectArtifact(descriptor, Platform{"macos", "arm64", "14.5", "dmg"})
          .path.endsWith("-macos-14.dmg"));
  EXPECT_TRUE(
      SelectArtifact(descriptor, Platform{"macos", "arm64", "15.1", "dmg"})
          .path.endsWith("-macos-15.dmg"));
  EXPECT_TRUE(
      SelectArtifact(descriptor, Platform{"macos", "x86_64", "13.6", "dmg"})
          .path.endsWith("-macos-13-intel.dmg"));
  EXPECT_TRUE(
      SelectArtifact(descriptor, Platform{"linux", "arm64", "", "appimage"})
          .path.endsWith("-linux-aarch64.AppImage"));
}
