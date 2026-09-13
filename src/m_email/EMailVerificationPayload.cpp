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

#include "EMailVerificationPayload.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

/// One enum, one table, both directions. Written out as names so that
/// inserting a state in severity order -- which renumbers everything after it
/// -- cannot turn one build's "bad" into another's "could not check".
template <typename EnumT>
struct NameTable {
  struct Entry {
    EnumT value;
    const char* name;
  };
  QList<Entry> entries;

  [[nodiscard]] auto ToName(EnumT value) const -> QString {
    for (const auto& entry : entries) {
      if (entry.value == value) return QString::fromLatin1(entry.name);
    }
    return {};
  }

  [[nodiscard]] auto FromName(const QString& name, EnumT& out) const -> bool {
    for (const auto& entry : entries) {
      if (name == QString::fromLatin1(entry.name)) {
        out = entry.value;
        return true;
      }
    }
    return false;
  }
};

const NameTable<EMailVerifyState> kStateNames{{
    {EMailVerifyState::kNOT_ATTEMPTED, "not_attempted"},
    {EMailVerifyState::kATTEMPTED_EMPTY, "attempted_empty"},
    {EMailVerifyState::kVERIFIED, "verified"},
}};

const NameTable<EMailBadgeState> kBadgeNames{{
    {EMailBadgeState::kNOT_PROTECTED, "not_protected"},
    {EMailBadgeState::kMALFORMED, "malformed"},
    {EMailBadgeState::kENCRYPTED_ONLY, "encrypted_only"},
    {EMailBadgeState::kSIGNED_UNVERIFIED, "signed_unverified"},
    {EMailBadgeState::kSIGNED_GOOD, "signed_good"},
    {EMailBadgeState::kSIGNED_EXPIRED, "signed_expired"},
    {EMailBadgeState::kSIGNED_UNKNOWN_KEY, "signed_unknown_key"},
    {EMailBadgeState::kSIGNED_MISMATCH, "signed_mismatch"},
    {EMailBadgeState::kSIGNED_ERROR, "signed_error"},
    {EMailBadgeState::kSIGNED_BAD, "signed_bad"},
}};

const NameTable<EMailVerifyExec> kExecNames{{
    {EMailVerifyExec::kOK, "ok"},
    {EMailVerifyExec::kENGINE_ERROR, "engine_error"},
    {EMailVerifyExec::kNO_RESULT, "no_result"},
}};

auto EncodeRegion(const EMailSignatureRegion& region) -> QJsonObject {
  return QJsonObject{
      {"region_id", region.region_id},
      {"nesting_depth", region.nesting_depth},
      {"raw_offset", static_cast<double>(region.raw_offset)},
      {"raw_length", static_cast<double>(region.raw_length)},
      {"declared_micalg", region.declared_micalg},
      {"covers_ciphertext_only", region.covers_ciphertext_only},
      {"signed_part_index", region.signed_part_index},
      {"signature_part_index", region.signature_part_index},
  };
}

auto DecodeRegion(const QJsonObject& o, EMailSignatureRegion& region) -> bool {
  if (!o.contains("region_id")) return false;
  region.region_id = o["region_id"].toInt();
  region.nesting_depth = o["nesting_depth"].toInt();
  region.raw_offset = static_cast<qint64>(o["raw_offset"].toDouble(-1));
  region.raw_length = static_cast<qint64>(o["raw_length"].toDouble(0));
  region.declared_micalg = o["declared_micalg"].toString();
  region.covers_ciphertext_only = o["covers_ciphertext_only"].toBool();
  region.signed_part_index = o["signed_part_index"].toInt(-1);
  region.signature_part_index = o["signature_part_index"].toInt(-1);
  return true;
}

auto EncodeVerdict(const EMailRegionVerdict& verdict) -> QJsonObject {
  return QJsonObject{
      {"region_id", verdict.region_id},
      {"nesting_depth", verdict.nesting_depth},
      {"covers_ciphertext_only", verdict.covers_ciphertext_only},
      {"signed_bytes_non_canonical", verdict.signed_bytes_non_canonical},
      {"exec", kExecNames.ToName(verdict.exec)},
      {"verdict", kBadgeNames.ToName(verdict.verdict)},
  };
}

auto DecodeVerdict(const QJsonObject& o, EMailRegionVerdict& verdict) -> bool {
  if (!kExecNames.FromName(o["exec"].toString(), verdict.exec)) return false;
  if (!kBadgeNames.FromName(o["verdict"].toString(), verdict.verdict)) {
    return false;
  }
  verdict.region_id = o["region_id"].toInt(-1);
  verdict.nesting_depth = o["nesting_depth"].toInt();
  verdict.covers_ciphertext_only = o["covers_ciphertext_only"].toBool();
  verdict.signed_bytes_non_canonical =
      o["signed_bytes_non_canonical"].toBool();
  return true;
}

auto EncodeSignature(const EMailSignatureResult& s) -> QJsonObject {
  QJsonArray warnings;
  for (const auto& warning : s.warnings) warnings.append(warning);

  return QJsonObject{
      {"region_id", s.region_id},
      {"fingerprint", s.fingerprint},
      {"pubkey_algo", s.pubkey_algo},
      {"hash_algo", s.hash_algo},
      {"uid", s.uid},
      // ISO-8601 in UTC, so the string does not mean two different instants
      // on two machines. Seconds are all a signature timestamp carries.
      {"sign_time", s.sign_time.isValid()
                        ? s.sign_time.toUTC().toString(Qt::ISODate)
                        : QString()},
      {"validity", s.validity},
      {"warnings", warnings},
      {"micalg_mismatch", s.micalg_mismatch},
  };
}

auto DecodeSignature(const QJsonObject& o, EMailSignatureResult& s) -> bool {
  if (!o.contains("region_id")) return false;
  s.region_id = o["region_id"].toInt(-1);
  s.fingerprint = o["fingerprint"].toString();
  s.pubkey_algo = o["pubkey_algo"].toString();
  s.hash_algo = o["hash_algo"].toString();
  s.uid = o["uid"].toString();

  const auto sign_time = o["sign_time"].toString();
  s.sign_time = sign_time.isEmpty()
                    ? QDateTime()
                    : QDateTime::fromString(sign_time, Qt::ISODate).toLocalTime();

  // Absent reads as -1, not 0: 0 is "fully valid", and a field that did not
  // survive the trip must never arrive as a trusted signature.
  s.validity = o["validity"].toInt(-1);

  s.warnings.clear();
  for (const auto& warning : o["warnings"].toArray()) {
    s.warnings.append(warning.toString());
  }
  s.micalg_mismatch = o["micalg_mismatch"].toBool();
  return true;
}

}  // namespace

auto EncodeVerificationPayload(const EMailVerificationResult& result)
    -> QByteArray {
  QJsonArray regions;
  for (const auto& region : result.regions) regions.append(EncodeRegion(region));

  QJsonArray verdicts;
  for (const auto& verdict : result.verdicts) {
    verdicts.append(EncodeVerdict(verdict));
  }

  QJsonArray signatures;
  for (const auto& signature : result.signatures) {
    signatures.append(EncodeSignature(signature));
  }

  // Only what a consumer renders from. The headers the page already has from
  // parsing the document itself are not echoed back at it; these four are the
  // ones a VERIFICATION produces and nothing else can.
  const QJsonObject meta{
      {"from", result.meta.from},
      {"micalg", result.meta.micalg},
      {"signed_entity_digest", result.meta.signed_entity_digest},
      {"signed_entity_digest_algo", result.meta.signed_entity_digest_algo},
      {"signed_entity_non_canonical", result.meta.signed_entity_non_canonical},
  };

  const QJsonObject root{
      {"schema_version", kEMailVerificationPayloadVersion},
      {"state", kStateNames.ToName(result.state)},
      {"overall", kBadgeNames.ToName(result.overall)},
      {"regions", regions},
      {"verdicts", verdicts},
      {"signatures", signatures},
      {"meta", meta},
      {"source_length", static_cast<double>(result.source_length)},
      {"source_sha256", QString::fromLatin1(result.source_sha256.toHex())},
  };

  return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

auto DecodeVerificationPayload(const QByteArray& payload,
                               EMailVerificationResult& result) -> bool {
  QJsonParseError error{};
  const auto doc = QJsonDocument::fromJson(payload, &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject()) return false;

  const auto root = doc.object();
  if (root["schema_version"].toInt(-1) != kEMailVerificationPayloadVersion) {
    return false;
  }

  // Built aside and only handed over once every field has been understood, so
  // a refusal leaves the caller with whatever it had rather than with half of
  // a verdict.
  EMailVerificationResult decoded;

  if (!kStateNames.FromName(root["state"].toString(), decoded.state)) {
    return false;
  }
  if (!kBadgeNames.FromName(root["overall"].toString(), decoded.overall)) {
    return false;
  }

  for (const auto& entry : root["regions"].toArray()) {
    EMailSignatureRegion region;
    if (!DecodeRegion(entry.toObject(), region)) return false;
    decoded.regions.append(region);
  }

  for (const auto& entry : root["verdicts"].toArray()) {
    EMailRegionVerdict verdict;
    if (!DecodeVerdict(entry.toObject(), verdict)) return false;
    decoded.verdicts.append(verdict);
  }

  for (const auto& entry : root["signatures"].toArray()) {
    EMailSignatureResult signature;
    if (!DecodeSignature(entry.toObject(), signature)) return false;
    decoded.signatures.append(signature);
  }

  const auto meta = root["meta"].toObject();
  decoded.meta.from = meta["from"].toString();
  decoded.meta.micalg = meta["micalg"].toString();
  decoded.meta.signed_entity_digest = meta["signed_entity_digest"].toString();
  decoded.meta.signed_entity_digest_algo =
      meta["signed_entity_digest_algo"].toString();
  decoded.meta.signed_entity_non_canonical =
      meta["signed_entity_non_canonical"].toBool();

  decoded.source_length =
      static_cast<qint64>(root["source_length"].toDouble(0));
  decoded.source_sha256 =
      QByteArray::fromHex(root["source_sha256"].toString().toLatin1());

  // An answer with no anchor cannot be checked against the document it is
  // supposed to describe, and an unanchored verdict is one that can be shown
  // against the wrong message.
  if (decoded.source_sha256.isEmpty() || decoded.source_length < 0) return false;

  result = decoded;
  return true;
}

auto VerificationResultsEquivalent(const EMailVerificationResult& a,
                                   const EMailVerificationResult& b) -> bool {
  if (a.state != b.state || a.overall != b.overall) return false;
  if (a.source_length != b.source_length) return false;
  if (a.source_sha256 != b.source_sha256) return false;

  if (a.meta.from != b.meta.from) return false;
  if (a.meta.micalg != b.meta.micalg) return false;
  if (a.meta.signed_entity_digest != b.meta.signed_entity_digest) return false;
  if (a.meta.signed_entity_digest_algo != b.meta.signed_entity_digest_algo) {
    return false;
  }
  if (a.meta.signed_entity_non_canonical != b.meta.signed_entity_non_canonical) {
    return false;
  }

  if (a.regions.size() != b.regions.size()) return false;
  for (int i = 0; i < a.regions.size(); ++i) {
    const auto& x = a.regions[i];
    const auto& y = b.regions[i];
    if (x.region_id != y.region_id || x.nesting_depth != y.nesting_depth ||
        x.raw_offset != y.raw_offset || x.raw_length != y.raw_length ||
        x.declared_micalg != y.declared_micalg ||
        x.covers_ciphertext_only != y.covers_ciphertext_only ||
        x.signed_part_index != y.signed_part_index ||
        x.signature_part_index != y.signature_part_index) {
      return false;
    }
  }

  if (a.verdicts.size() != b.verdicts.size()) return false;
  for (int i = 0; i < a.verdicts.size(); ++i) {
    const auto& x = a.verdicts[i];
    const auto& y = b.verdicts[i];
    if (x.region_id != y.region_id || x.nesting_depth != y.nesting_depth ||
        x.covers_ciphertext_only != y.covers_ciphertext_only ||
        x.signed_bytes_non_canonical != y.signed_bytes_non_canonical ||
        x.exec != y.exec || x.verdict != y.verdict) {
      return false;
    }
  }

  if (a.signatures.size() != b.signatures.size()) return false;
  for (int i = 0; i < a.signatures.size(); ++i) {
    const auto& x = a.signatures[i];
    const auto& y = b.signatures[i];
    if (x.region_id != y.region_id || x.fingerprint != y.fingerprint ||
        x.pubkey_algo != y.pubkey_algo || x.hash_algo != y.hash_algo ||
        x.uid != y.uid || x.validity != y.validity ||
        x.warnings != y.warnings || x.micalg_mismatch != y.micalg_mismatch) {
      return false;
    }
    // To the second: that is all a signature timestamp carries, and all the
    // ISO-8601 form on the wire preserves.
    const auto x_time = x.sign_time.toUTC().toSecsSinceEpoch();
    const auto y_time = y.sign_time.toUTC().toSecsSinceEpoch();
    if (x.sign_time.isValid() != y.sign_time.isValid()) return false;
    if (x.sign_time.isValid() && x_time != y_time) return false;
  }

  return true;
}
