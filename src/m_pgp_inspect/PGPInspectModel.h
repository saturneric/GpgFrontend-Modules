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
#include <QString>
#include <QStringList>
#include <QVector>

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

/// One label / value row in a packet's detail table.
struct PGPInspectField {
  QString label;
  QString value;
};

/// A packet, and whatever is nested inside it.
struct PGPInspectPacket {
  /// The numeric packet type ID; -1 when the document did not say.
  int tag = -1;
  QString tag_name;
  qint64 offset = 0;
  /// "old" or "new".
  QString header_version;
  qint64 header_length = 0;
  qint64 body_length = 0;
  /// "fixed", "partial" or "indeterminate".
  QString length_type;
  /// The body's own version octet; -1 for the types that carry none.
  int version = -1;
  QVector<PGPInspectField> fields;
  QVector<PGPInspectPacket> children;
  /// Why this packet could not be decoded, empty when it could.
  QString error;

  [[nodiscard]] auto Malformed() const -> bool { return !error.isEmpty(); }
};

/// The armor envelope of a block.
struct PGPInspectArmor {
  QString block_type;
  QVector<PGPInspectField> headers;
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
  QVector<PGPInspectField> cleartext_headers;
  QVector<PGPInspectPacket> packets;
  QString error;
};

/// What an inspected input turned out to be.
struct PGPInspectDocument {
  /// False when the JSON could not be read at all; see @ref parse_error.
  bool valid = false;
  QString format;
  qint64 size = 0;
  QVector<PGPInspectBlock> blocks;
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
 * @brief The one-line summary a packet gets in the tree.
 *
 * @param packet the packet to describe
 * @param index its 1-based position among its siblings
 */
auto PGPInspectPacketSummary(const PGPInspectPacket& packet, int index)
    -> QString;

/// A block's one-line summary: what kind it is, and what it wraps.
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
