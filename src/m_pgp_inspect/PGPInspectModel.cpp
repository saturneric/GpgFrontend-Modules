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

namespace {

auto ParseFields(const QJsonValue& value) -> QVector<PGPInspectField> {
  QVector<PGPInspectField> fields;
  for (const auto& entry : value.toArray()) {
    const auto object = entry.toObject();
    // A row with neither half says nothing; dropping it keeps the detail
    // table from growing blank lines out of a malformed document.
    if (!object.contains("label") && !object.contains("value")) continue;
    fields.push_back(
        {object.value("label").toString(), object.value("value").toString()});
  }
  return fields;
}

auto ParsePacket(const QJsonObject& object) -> PGPInspectPacket {
  PGPInspectPacket packet;
  packet.tag = object.value("tag").toInt(-1);
  packet.tag_name = object.value("tagName").toString();
  packet.offset = static_cast<qint64>(object.value("offset").toDouble());
  packet.header_version = object.value("headerVersion").toString();
  packet.header_length =
      static_cast<qint64>(object.value("headerLength").toDouble());
  packet.body_length =
      static_cast<qint64>(object.value("bodyLength").toDouble());
  packet.length_type = object.value("lengthType").toString();
  // Absent for the packet types that carry no version octet, which is a fact
  // about the type rather than a defect in the document.
  packet.version = object.value("version").toInt(-1);
  packet.fields = ParseFields(object.value("fields"));
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

auto CountPackets(const QVector<PGPInspectPacket>& packets) -> int {
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
    document.parse_error =
        QCoreApplication::translate("GTrC", "The inspector returned no data.");
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

auto PGPInspectHasStructure(const PGPInspectDocument& document) -> bool {
  return document.valid && document.PacketCount() > 0;
}

auto PGPInspectPacketSummary(const PGPInspectPacket& packet, int index)
    -> QString {
  // "#1  Signature (tag 2)  v4  @0x0000  189 B". The offset is hexadecimal
  // because that is what every other packet dumper prints, and what a reader
  // comparing two tools needs in order to line them up.
  auto summary = QCoreApplication::translate("GTrC", "#%1  %2 (tag %3)")
                     .arg(index)
                     .arg(packet.tag_name.isEmpty()
                              ? QCoreApplication::translate("GTrC", "Unknown")
                              : packet.tag_name)
                     .arg(packet.tag);

  if (packet.version >= 0) summary += QString("  v%1").arg(packet.version);

  summary += QString("  @0x%1").arg(packet.offset, 4, 16, QChar('0'));
  summary += QCoreApplication::translate("GTrC", "  %1 B")
                 .arg(packet.header_length + packet.body_length);

  if (packet.length_type != QLatin1String("fixed") &&
      !packet.length_type.isEmpty()) {
    summary += QString("  [%1]").arg(packet.length_type);
  }
  return summary;
}

auto PGPInspectBlockSummary(const PGPInspectBlock& block, int index)
    -> QString {
  if (block.kind == QLatin1String("cleartext")) {
    return QCoreApplication::translate("GTrC",
                                       "Block %1: cleartext-signed message")
        .arg(index);
  }
  if (block.has_armor) {
    return QCoreApplication::translate("GTrC", "Block %1: armored, %2")
        .arg(index)
        .arg(block.armor.block_type);
  }
  return QCoreApplication::translate("GTrC", "Block %1: binary packet stream")
      .arg(index);
}
