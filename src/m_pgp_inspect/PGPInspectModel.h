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
#include <QList>
#include <QString>
#include <QStringList>

#include "PGPPacketDecoder.h"

/**
 * @file PGPInspectModel.h
 * @brief The inspector's document, and the JSON that carries it.
 *
 * Deliberately free of every SDK and widget symbol. The parsing is the part
 * with rules worth checking -- absent fields, malformed documents, the name
 * tables -- and keeping it here is what lets a plain gtest binary check them
 * without a GUI thread or a loaded host. The dialog stays a dumb view over
 * what comes out.
 */

/// A packet, and whatever is nested inside it.
struct PGPInspectPacket {
  /// The numeric packet type ID; -1 when the document did not say.
  int tag = -1;
  qint64 offset = 0;
  /// "old" or "new".
  QString header_version;
  qint64 header_length = 0;
  qint64 body_length = 0;
  /// "fixed", "partial" or "indeterminate".
  QString length_type;
  /// The packet's wire octets from its CTB on, as far as the walk handed
  /// them over; see @ref raw_truncated.
  QByteArray raw;
  /// Set when @ref raw holds fewer octets than the packet has on the wire.
  bool raw_truncated = false;
  QList<PGPInspectPacket> children;
  /// What the walk found wrong with the framing, empty when nothing.
  QString error;
  /// What @ref raw says. Filled by PGPInspectDecode(), not by the parse:
  /// the menu probe parses a document on every opening and needs none of it.
  PGPDecodedPacket decoded;

  /// The octets on the wire: header plus body.
  [[nodiscard]] auto WireLength() const -> qint64 {
    return header_length + body_length;
  }

  [[nodiscard]] auto Malformed() const -> bool {
    return !error.isEmpty() || !decoded.error.isEmpty();
  }
};

/// The armor envelope of a block.
struct PGPInspectArmor {
  QString block_type;
  QList<PGPInspectField> headers;
  /// "ok", "mismatch" or "absent".
  QString crc24;
};

/// One armor block, or the whole input when it carries none.
struct PGPInspectBlock {
  /// "binary", "armored" or "cleartext".
  QString kind;
  qint64 offset = 0;
  bool has_armor = false;
  PGPInspectArmor armor;
  bool has_cleartext = false;
  qint64 cleartext_text_size = 0;
  QList<PGPInspectField> cleartext_headers;
  QList<PGPInspectPacket> packets;
  QString error;
};

/// What an inspected input turned out to be.
struct PGPInspectDocument {
  /// False when the JSON could not be read at all; see @ref parse_error.
  bool valid = false;
  QString format;
  qint64 size = 0;
  QList<PGPInspectBlock> blocks;
  QStringList errors;
  QString parse_error;

  /// Every packet in the document, containers before their contents.
  [[nodiscard]] auto PacketCount() const -> int;
};

/**
 * @brief Read the inspector's JSON document.
 *
 * Never throws and never returns a half-built document: anything it cannot
 * read leaves @ref PGPInspectDocument::valid false with a reason, and a field
 * that is merely missing takes its default rather than invalidating the rest.
 */
auto ParsePGPInspectDocument(const QByteArray& json) -> PGPInspectDocument;

/**
 * @brief Decode every packet's octets, nested ones included.
 *
 * A compressed packet also gains its decompressed size, which only the
 * walk's children know.
 */
void PGPInspectDecode(PGPInspectDocument& document);

/**
 * @brief The line a packet gets in the structure tree: "#2.1  Signature".
 *
 * Its position and type; everything else about it is in the dialog's tabs.
 *
 * @param index its dotted position, from PGPInspectPacketIndex()
 */
auto PGPInspectPacketSummary(const PGPInspectPacket& packet,
                             const QString& index) -> QString;

/// The dotted position of a packet: "3" at the top, "3.1" for the first
/// packet inside it.
auto PGPInspectPacketIndex(const QString& parent, int position) -> QString;

/// The line a block gets in the structure tree: its number, its armor type
/// (or what it is when it has none), and how many packets it holds. The
/// count includes nested packets, and says so when there are any.
auto PGPInspectBlockSummary(const PGPInspectBlock& block, int index) -> QString;

/**
 * @brief Whether a document holds a structure worth opening a window for.
 *
 * A valid document with no packets in it is the ordinary answer for plain
 * text, a half-written note, or anything else that simply is not OpenPGP --
 * so it is what the menu entry is greyed out on. The distinction matters:
 * "this is not OpenPGP" is a finding, and offering to display it would make
 * the user open a window to be told nothing.
 */
auto PGPInspectHasStructure(const PGPInspectDocument& document) -> bool;
