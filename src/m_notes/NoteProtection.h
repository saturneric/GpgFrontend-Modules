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

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

/**
 * @file NoteProtection.h
 * @brief Who a note is encrypted to: what the packets say, what was
 * confirmed, and whether a save may go ahead.
 *
 * SDK-free. A packet is routing information -- the key it was made for --
 * never proof of who may read a note or consent to keep encrypting to them.
 * Only the user's confirmation in this profile is that.
 */

namespace gf::notes {

/// The protection a save uses: recipients by primary-key fingerprint, and
/// whether a passphrase opens it too.
struct ProtectionConfig {
  QStringList recipients;  ///< primary-key fingerprints, upper case, sorted
  bool passphrase = false;

  [[nodiscard]] auto IsEmpty() const -> bool {
    return recipients.isEmpty() && !passphrase;
  }
  /// Keys or a passphrase, exactly one of them: all a new save may use.
  [[nodiscard]] auto IsExclusive() const -> bool {
    return recipients.isEmpty() == passphrase;
  }
  [[nodiscard]] auto ToJson() const -> QJsonObject;
  static auto FromJson(const QJsonObject& json)
      -> std::optional<ProtectionConfig>;
  /// Stable over equal configurations; for the local confirmation record.
  [[nodiscard]] auto Hash() const -> QByteArray;
  friend auto operator==(const ProtectionConfig& a, const ProtectionConfig& b)
      -> bool {
    return a.recipients == b.recipients && a.passphrase == b.passphrase;
  }
  void Normalise();
};

/// Who could read saves under one configuration and cannot under another.
struct LostReaders {
  QStringList recipients;   ///< fingerprints
  bool passphrase = false;  ///< those who know the passphrase
  [[nodiscard]] auto IsEmpty() const -> bool {
    return recipients.isEmpty() && !passphrase;
  }
};

/// What moving from @p from to @p to takes away. Nothing is removed silently:
/// the caller shows this and asks before going on.
auto LostBetween(const ProtectionConfig& from, const ProtectionConfig& to)
    -> LostReaders;

/// One session-key packet: a key it names (GFGpgSniffRecipients), or a
/// passphrase packet (from GFPgpInspectData).
struct DetectedPacket {
  bool pkesk = true;
  QString key_id;  ///< subkey id or fingerprint; empty when hidden
  bool hidden = false;
  bool key_found = false;
  QString primary_fingerprint;
  QString label;  ///< "Name <email>", when the key is known
  bool has_secret = false;
};

/// What is known of a key here (GFGpgKeyPrimaryUid).
struct KeyFacts {
  QString fingerprint;
  QString label;
  bool found = false;
  bool can_encrypt = false;
  bool has_secret = false;
  bool expired = false;
  bool revoked = false;
  bool disabled = false;
  [[nodiscard]] auto Usable() const -> bool {
    return found && can_encrypt && !expired && !revoked && !disabled;
  }
  /// The label, or the fingerprint for a key that has none here.
  [[nodiscard]] auto DisplayName() const -> QString {
    return label.isEmpty() ? fingerprint : label;
  }
};

enum class IssueKind {
  kNOT_CONFIRMED,     ///< never confirmed in this profile
  kMISMATCH,          ///< the note's own record disagrees with its packets
  kMISSING_KEY,       ///< a recipient whose key is not here
  kUNUSABLE_KEY,      ///< expired, revoked, disabled, or cannot encrypt
  kHIDDEN_RECIPIENT,  ///< a wildcard packet nobody can name

};

struct Issue {
  IssueKind kind;
  QString subject;  ///< fingerprint, key id or label, for the user
};

struct Assessment {
  /// What a save would use: the confirmed configuration, else a proposal
  /// from the note's record or its packets.
  ProtectionConfig proposal;
  bool confirmed = false;
  /// confirmed, and nothing stands in the way of using it.
  [[nodiscard]] auto Resolved() const -> bool {
    return confirmed && issues.isEmpty();
  }
  QList<Issue> issues;
};

/**
 * @brief Weigh a note's own record against its packets and the keys here.
 *
 * @param recorded the configuration the note carries, if any
 * @param packets what the file's packets say (empty for a new note)
 * @param keys facts for every fingerprint involved

 * @param confirmed_hashes configurations the user confirmed in this profile
 *        for this note
 */
auto Assess(const std::optional<ProtectionConfig>& recorded,
            const QList<DetectedPacket>& packets,
            const QHash<QString, KeyFacts>& keys,
            const QList<QByteArray>& confirmed_hashes) -> Assessment;

}  // namespace gf::notes
