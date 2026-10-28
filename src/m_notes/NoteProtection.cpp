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

#include "NoteProtection.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

using namespace Qt::StringLiterals;

namespace gf::notes {

auto LostBetween(const ProtectionConfig& from, const ProtectionConfig& to)
    -> LostReaders {
  LostReaders lost;
  for (const auto& r : from.recipients) {
    if (!to.recipients.contains(r, Qt::CaseInsensitive)) lost.recipients << r;
  }
  lost.passphrase = from.passphrase && !to.passphrase;
  return lost;
}

void ProtectionConfig::Normalise() {
  for (auto& r : recipients) r = r.trimmed().toUpper();
  recipients.removeAll(QString());
  recipients.sort();
  recipients.removeDuplicates();
}

auto ProtectionConfig::ToJson() const -> QJsonObject {
  return {{u"schema"_s, 1},
          {u"recipients"_s, QJsonArray::fromStringList(recipients)},
          {u"passphrase"_s, passphrase}};
}

auto ProtectionConfig::FromJson(const QJsonObject& json)
    -> std::optional<ProtectionConfig> {
  if (json.value(u"schema"_s).toInt() != 1) return std::nullopt;
  ProtectionConfig c;
  for (const auto& v : json.value(u"recipients"_s).toArray()) {
    if (v.isString()) c.recipients.append(v.toString());
  }
  c.passphrase = json.value(u"passphrase"_s).toBool();
  c.Normalise();
  return c;
}

auto ProtectionConfig::Hash() const -> QByteArray {
  auto copy = *this;
  copy.Normalise();
  return QCryptographicHash::hash(
             QJsonDocument(copy.ToJson()).toJson(QJsonDocument::Compact),
             QCryptographicHash::Sha256)
      .toHex();
}

auto Assess(const std::optional<ProtectionConfig>& recorded,
            const QList<DetectedPacket>& packets,
            const QHash<QString, KeyFacts>& keys,
            const QList<QByteArray>& confirmed_hashes) -> Assessment {
  Assessment a;

  // What the packets say, as a configuration.
  ProtectionConfig from_packets;
  bool has_skesk = false;
  for (const auto& p : packets) {
    if (!p.pkesk) {
      has_skesk = true;
      continue;
    }
    if (p.hidden) {
      a.issues.append({IssueKind::kHIDDEN_RECIPIENT, {}});
    } else if (!p.key_found) {
      a.issues.append({IssueKind::kMISSING_KEY, p.key_id});
    } else {
      from_packets.recipients.append(p.primary_fingerprint);
    }
  }
  from_packets.passphrase = has_skesk;
  from_packets.Normalise();

  a.proposal = recorded.value_or(from_packets);
  a.proposal.Normalise();

  // The record must match the packets of a file that has any.
  if (recorded.has_value() && !packets.isEmpty()) {
    auto known = *recorded;
    // A recipient whose key is not here cannot be checked against a packet.
    QStringList checkable;
    for (const auto& r : known.recipients) {
      if (keys.value(r).found) checkable.append(r);
    }
    const auto packet_set = QSet<QString>(from_packets.recipients.cbegin(),
                                          from_packets.recipients.cend());
    const auto record_set = QSet<QString>(checkable.cbegin(), checkable.cend());
    if (packet_set != record_set || known.passphrase != has_skesk) {
      a.issues.append({IssueKind::kMISMATCH, {}});
    }
  }

  for (const auto& r : a.proposal.recipients) {
    const auto k = keys.value(r);
    if (!k.found) {
      a.issues.append({IssueKind::kMISSING_KEY, r});
    } else if (!k.Usable()) {
      a.issues.append({IssueKind::kUNUSABLE_KEY, k.DisplayName()});
    }
  }

  if (a.proposal.IsEmpty()) a.issues.append({IssueKind::kNOT_CONFIRMED, {}});

  a.confirmed =
      !a.proposal.IsEmpty() && confirmed_hashes.contains(a.proposal.Hash()) &&
      !std::any_of(a.issues.cbegin(), a.issues.cend(), [](const auto& i) {
        return i.kind == IssueKind::kMISMATCH;
      });
  if (!a.confirmed) a.issues.prepend({IssueKind::kNOT_CONFIRMED, {}});

  // De-duplicate, keeping the first of each.
  QList<Issue> unique;
  for (const auto& i : a.issues) {
    if (!std::any_of(unique.cbegin(), unique.cend(), [&](const auto& u) {
          return u.kind == i.kind && u.subject == i.subject;
        })) {
      unique.append(i);
    }
  }
  a.issues = unique;
  return a;
}

}  // namespace gf::notes
