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

#include "PGPInspectModel.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "GFModuleTr.h"
#include "PGPPacketNames.h"

using namespace Qt::StringLiterals;

namespace {

auto ParseFields(const QJsonValue& value) -> QList<PGPInspectField> {
  QList<PGPInspectField> fields;
  for (const auto& entry : value.toArray()) {
    const auto object = entry.toObject();
    // A row with neither half says nothing; dropping it keeps the detail
    // table from growing blank lines out of a malformed document.
    if (!object.contains("label") && !object.contains("value")) continue;
    fields.push_back({object.value("label").toString(),
                      object.value("value").toString(),
                      {}});
  }
  return fields;
}

auto ParsePacket(const QJsonObject& object) -> PGPInspectPacket {
  PGPInspectPacket packet;
  packet.tag = object.value("tag").toInt(-1);
  packet.offset = static_cast<qint64>(object.value("offset").toDouble());
  packet.header_version = object.value("headerVersion").toString();
  packet.header_length =
      static_cast<qint64>(object.value("headerLength").toDouble());
  packet.body_length =
      static_cast<qint64>(object.value("bodyLength").toDouble());
  packet.length_type = object.value("lengthType").toString();
  packet.raw = QByteArray::fromHex(object.value("raw").toString().toLatin1());
  packet.raw_truncated = object.value("rawTruncated").toBool();
  packet.error = object.value("error").toString();

  for (const auto& child : object.value("children").toArray()) {
    packet.children.push_back(ParsePacket(child.toObject()));
  }
  return packet;
}

auto ParseBlock(const QJsonObject& object) -> PGPInspectBlock {
  PGPInspectBlock block;
  block.kind = object.value("kind").toString();
  block.offset = static_cast<qint64>(object.value("offset").toDouble());
  block.error = object.value("error").toString();

  if (object.value("armor").isObject()) {
    const auto armor = object.value("armor").toObject();
    block.has_armor = true;
    block.armor.block_type = armor.value("blockType").toString();
    block.armor.headers = ParseFields(armor.value("headers"));
    block.armor.crc24 = armor.value("crc24").toString();
  }

  if (object.value("cleartext").isObject()) {
    const auto cleartext = object.value("cleartext").toObject();
    block.has_cleartext = true;
    block.cleartext_text_size =
        static_cast<qint64>(cleartext.value("textSize").toDouble());
    block.cleartext_headers = ParseFields(cleartext.value("headers"));
  }

  for (const auto& packet : object.value("packets").toArray()) {
    block.packets.push_back(ParsePacket(packet.toObject()));
  }
  return block;
}

void DecodePackets(QList<PGPInspectPacket>& packets) {
  for (auto& packet : packets) {
    DecodePackets(packet.children);
    // Past the walk's document budget a packet carries framing only; that
    // is a cap the summary line reports, not a defect in this packet.
    if (!packet.raw.isEmpty()) {
      packet.decoded = PGPDecodePacket(packet.raw, packet.WireLength());
    }

    // The decompressed stream ends where its last packet does.
    if (packet.tag == 8 && !packet.children.isEmpty()) {
      const auto& last = packet.children.last();
      packet.decoded.overview.push_back(
          {QStringLiteral("Decompressed Size"),
           QStringLiteral("%1 bytes").arg(last.offset + last.WireLength()),
           {}});
    }
  }
}

auto CountPackets(const QList<PGPInspectPacket>& packets) -> int {
  int count = 0;
  for (const auto& packet : packets) {
    count += 1 + CountPackets(packet.children);
  }
  return count;
}

}  // namespace

auto PGPInspectDocument::PacketCount() const -> int {
  int count = 0;
  for (const auto& block : blocks) count += CountPackets(block.packets);
  return count;
}

auto ParsePGPInspectDocument(const QByteArray& json) -> PGPInspectDocument {
  PGPInspectDocument document;

  QJsonParseError error{};
  const auto parsed = QJsonDocument::fromJson(json, &error);
  if (error.error != QJsonParseError::NoError) {
    document.parse_error = error.errorString();
    return document;
  }
  if (!parsed.isObject()) {
    document.parse_error = GTrC::tr("The inspector returned no data.");
    return document;
  }

  const auto object = parsed.object();
  document.valid = true;
  document.format = object.value("format").toString();
  document.size = static_cast<qint64>(object.value("size").toDouble());

  for (const auto& note : object.value("errors").toArray()) {
    document.errors.append(note.toString());
  }
  for (const auto& block : object.value("blocks").toArray()) {
    document.blocks.push_back(ParseBlock(block.toObject()));
  }
  return document;
}

void PGPInspectDecode(PGPInspectDocument& document) {
  for (auto& block : document.blocks) DecodePackets(block.packets);
}

auto PGPInspectHasStructure(const PGPInspectDocument& document) -> bool {
  return document.valid && document.PacketCount() > 0;
}

auto PGPInspectPacketSummary(const PGPInspectPacket& packet,
                             const QString& index) -> QString {
  // The index to find and name it by ("#2.3") and what it is. Version, tag
  // number, offset and framing stay in the Overview tab.
  return QString("#%1  %2").arg(index, PGPTagName(packet.tag));
}

auto PGPInspectPacketIndex(const QString& parent, int position) -> QString {
  return parent.isEmpty() ? QString::number(position)
                          : QString("%1.%2").arg(parent).arg(position);
}

auto PGPInspectBlockSummary(const PGPInspectBlock& block, int index)
    -> QString {
  QString kind;
  if (block.kind == "cleartext"_L1) {
    kind = GTrC::tr("Cleartext Signed Message");
  } else if (block.has_armor) {
    kind = block.armor.block_type;
  } else {
    kind = GTrC::tr("Binary Data");
  }

  // Counted through locals: a function call inside a translate() %n
  // argument makes lupdate silently skip every string in the file.
  const int total = CountPackets(block.packets);
  const auto nested = total != block.packets.size();
  const auto count =
      nested ? GTrC::tr("%n packet(s) including nested", nullptr, total)
             : GTrC::tr("%n packet(s)", nullptr, total);
  return GTrC::tr("Block %1: %2 (%3)").arg(index).arg(kind, count);
}
