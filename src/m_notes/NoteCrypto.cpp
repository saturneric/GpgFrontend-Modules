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

#include "NoteCrypto.h"

#include <GFModule.h>
#include <GFSDKArchive.h>
#include <GFSDKBuffer.h>
#include <GFSDKGpg.h>
#include <GFSDKGpgList.h>
#include <GFSDKGpgResult.h>
#include <GFSDKPgp.h>

#include <GFSDK.hpp>
#include <GFSDKBuffer.hpp>
#include <GFSDKGpgResult.hpp>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::StringLiterals;

namespace gf::notes {

auto CurrentChannel() -> int {
  const auto c = GFGpgCurrentChannel(GFModuleSdkContext());
  return c < 0 ? 0 : c;
}

namespace {

/// The tags of the top-level packets of @p file, as the Host frames them.
auto PacketTags(const QByteArray& file) -> std::optional<QList<int>> {
  auto in = GFBuf::Copy(GFModuleSdkContext(), file);
  GFBuf json(GFModuleSdkContext());
  if (GFPgpInspectData(GFModuleSdkContext(), in.View(), json.Out()) != 0)
    return std::nullopt;
  const auto doc = QJsonDocument::fromJson(json.UnwipeableCopy()).object();
  QList<int> tags;
  for (const auto& block : doc.value(u"blocks"_s).toArray()) {
    for (const auto& packet : block.toObject().value(u"packets"_s).toArray()) {
      tags << packet.toObject().value(u"tag"_s).toInt();
    }
  }
  return tags;
}

constexpr int kTagPkesk = 1;
constexpr int kTagSkesk = 3;
constexpr int kTagSed = 9;
constexpr int kTagSeipd = 18;
constexpr int kTagAead = 20;

}  // namespace

auto SniffPackets(int channel, const QByteArray& file)
    -> std::optional<QList<DetectedPacket>> {
  const auto tags = PacketTags(file);
  if (!tags.has_value()) return std::nullopt;
  const bool encrypted = tags->contains(kTagSeipd) ||
                         tags->contains(kTagAead) || tags->contains(kTagSed);
  if (!encrypted) return std::nullopt;

  QList<DetectedPacket> packets;
  if (tags->contains(kTagPkesk)) {
    auto in = GFBuf::Copy(GFModuleSdkContext(), file);
    GFGpgRecipientListRef list = nullptr;
    if (GFGpgSniffRecipients(GFModuleSdkContext(), channel, in.View(), &list) ==
        0) {
      const auto n = GFGpgRecipientCount(GFModuleSdkContext(), list);
      for (size_t i = 0; i < n; ++i) {
        const auto* row = GFGpgRecipientAt(GFModuleSdkContext(), list, i);
        if (row == nullptr) continue;
        DetectedPacket p;
        p.key_id = QString::fromUtf8(row->key_id).toUpper();
        p.hidden = row->hidden != 0;
        p.key_found = row->key_found != 0;
        p.primary_fingerprint = QString::fromUtf8(row->fingerprint).toUpper();
        p.label = QString::fromUtf8(row->uid);
        p.has_secret = row->has_secret != 0;
        packets << p;
      }
      GFGpgRecipientRelease(GFModuleSdkContext(), list);
    }
  }
  for (const auto tag : *tags) {
    if (tag != kTagSkesk) continue;
    DetectedPacket p;
    p.pkesk = false;
    packets << p;
  }
  return packets;
}

auto DescribeKeys(int channel, const QStringList& fingerprints)
    -> QHash<QString, KeyFacts> {
  QHash<QString, KeyFacts> out;
  for (const auto& f : fingerprints) {
    KeyFacts k;
    k.fingerprint = f.toUpper();
    if (const auto uid =
            gf::sdk::PrimaryUid(GFModuleSdkContext(), channel, f)) {
      k.found = true;
      k.label = uid->Address();
      // The usability flags come with the key brief, found by the key's own
      // e-mail. A key with none is left to the Host, which refuses to encrypt
      // to it; Encrypt() then counts the packets and names the loss.
      k.can_encrypt = true;
      GFGpgKeyBriefListRef briefs = nullptr;
      if (!uid->email.isEmpty() &&
          GFGpgFindKeys(GFModuleSdkContext(), channel,
                        uid->email.toUtf8().constData(), &briefs) == 0) {
        const auto n = GFGpgKeyBriefCount(GFModuleSdkContext(), briefs);
        for (size_t i = 0; i < n; ++i) {
          const auto* row = GFGpgKeyBriefAt(GFModuleSdkContext(), briefs, i);
          if (row == nullptr ||
              QString::fromUtf8(row->fingerprint).toUpper() != k.fingerprint) {
            continue;
          }
          k.can_encrypt = row->can_encrypt != 0;
          k.expired = row->usability == 2;
          k.revoked = row->usability == 3;
          k.disabled = row->usability == 4;
          break;
        }
        GFGpgKeyBriefRelease(GFModuleSdkContext(), briefs);
      }
    }
    out.insert(k.fingerprint, k);
  }
  return out;
}

auto Encrypt(int channel, const ProtectionConfig& config,
             const QByteArray& plain) -> CryptoResult {
  CryptoResult r;
  auto in = GFBuf::Copy(GFModuleSdkContext(), plain);
  GFGpgResult result(GFModuleSdkContext());
  if (config.recipients.isEmpty()) {
    GFGpgEncryptSymmetric(GFModuleSdkContext(), channel, in.View(), 0,
                          result.Out());
  } else {
    // The Host leaves out a key it cannot find; a note never loses a reader
    // that way, so each must be found first.
    const auto keys = DescribeKeys(channel, config.recipients);
    for (const auto& f : config.recipients) {
      if (!keys.value(f.toUpper()).found) {
        r.why = u"a reader's key is not in this key database"_s;
        return r;
      }
    }
    QList<QByteArray> utf8;
    std::vector<const char*> ids;
    for (const auto& f : config.recipients) utf8.append(f.toUtf8());
    for (const auto& u : utf8) ids.push_back(u.constData());
    GFGpgEncrypt(GFModuleSdkContext(), channel, ids.data(), ids.size(),
                 in.View(), 0, result.Out());
  }
  r.ok = result.Ok();
  r.error = result.Error();
  r.why = result.ErrorString();
  if (!r.ok) return r;
  r.data = result.DataCopy();

  // And each one got a packet. A packet names the encryption subkey, so this
  // counts them rather than matching primary fingerprints.
  if (!config.recipients.isEmpty()) {
    qsizetype named = 0;
    for (const auto& p :
         SniffPackets(channel, r.data).value_or(QList<DetectedPacket>{})) {
      if (p.pkesk && p.key_found && !p.hidden) ++named;
    }
    if (named < config.recipients.size()) {
      r.ok = false;
      r.data.clear();
      r.why = u"not every reader could be encrypted to"_s;
    }
  }
  return r;
}

auto Decrypt(int channel, const QByteArray& cipher) -> CryptoResult {
  CryptoResult r;
  auto in = GFBuf::Copy(GFModuleSdkContext(), cipher);
  GFGpgResult result(GFModuleSdkContext());
  GFGpgDecrypt(GFModuleSdkContext(), channel, in.View(), result.Out());
  r.ok = result.Ok();
  r.error = result.Error();
  r.why = result.ErrorString();
  if (r.ok) r.data = result.DataCopy();
  return r;
}

auto ZipWrite(const QList<PackEntry>& entries) -> std::optional<QByteArray> {
  std::vector<GFBuf> keep;
  keep.reserve(static_cast<size_t>(entries.size()));
  QList<QByteArray> paths;
  for (const auto& e : entries) paths.append(e.path.toUtf8());
  std::vector<GFArchiveEntry> c;
  for (qsizetype i = 0; i < entries.size(); ++i) {
    keep.push_back(GFBuf::Copy(GFModuleSdkContext(), entries[i].data));
    c.push_back({paths[i].constData(), keep.back().View(),
                 entries[i].directory ? 1 : 0});
  }
  GFBuf out(GFModuleSdkContext());
  if (GFArchiveZipWrite(GFModuleSdkContext(), c.data(), c.size(), out.Out()) !=
      0) {
    return std::nullopt;
  }
  return out.UnwipeableCopy();
}

namespace {

struct ReadState {
  QList<PackEntry> entries;
};

auto OnEntry(void* user, const char* path, GFBufferView data) -> int {
  auto* state = static_cast<ReadState*>(user);
  const auto* bytes =
      static_cast<const char*>(GFBufferData(GFModuleSdkContext(), data));
  state->entries.append(
      {QString::fromUtf8(path),
       bytes == nullptr ? QByteArray()
                        : QByteArray(bytes, static_cast<qsizetype>(GFBufferSize(
                                                GFModuleSdkContext(), data))),
       false});
  return 0;
}

}  // namespace

auto ZipRead(const QByteArray& zip, const Budgets& budgets, QString* why)
    -> std::optional<QList<PackEntry>> {
  auto in = GFBuf::Copy(GFModuleSdkContext(), zip);
  GFArchiveLimits limits{
      sizeof(GFArchiveLimits), budgets.max_total_bytes,
      std::max(budgets.max_body_bytes, budgets.max_asset_bytes),
      static_cast<int32_t>(budgets.max_entries)};
  ReadState state;
  GFBuf reason(GFModuleSdkContext());
  if (GFArchiveZipRead(GFModuleSdkContext(), in.View(), &limits, &OnEntry,
                       &state, reason.Out()) != 0) {
    if (why != nullptr) *why = QString::fromUtf8(reason.UnwipeableCopy());
    return std::nullopt;
  }
  return state.entries;
}

}  // namespace gf::notes
