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

/**
 * @file PGPPacketDecoder.h
 * @brief Reads one packet's wire octets into fields and a byte map.
 *
 * The host's walk hands over each packet's octets uninterpreted; this is
 * where they mean something. One pass produces both halves of the view --
 * the field rows and the labelled byte ranges of the hex dump -- so the two
 * can never disagree about what a byte is.
 *
 * Like the model, it is free of every SDK and widget symbol, so the plain
 * gtest binary checks it against real vectors.
 *
 * It never decrypts, and it never reads secret key material: everything a
 * secret key packet holds past its protection parameters becomes a single
 * redacted range, never a field, and prints as "--" in the dump.
 */

/// One label / value row; a row with children is a group.
struct PGPInspectField {
  QString label;
  QString value;
  QList<PGPInspectField> children;
  /// How the value is encoded and how long it is, e.g. "32 bytes" or
  /// "97 bytes · encoded as MPI (771 bits)"; empty when there is nothing to
  /// say. Kept out of @ref value so the value stays the bare material.
  QString detail;
};

/// One labelled byte range of a packet's wire octets.
struct PGPHexSpan {
  /// Offset from the packet's first octet, the CTB.
  qint64 offset = 0;
  qint64 length = 0;
  /// sq's name for the range; empty for payload, which the dump shows as
  /// ASCII instead.
  QString label;
  /// Secret material: the octets are never printed.
  bool redacted = false;
};

/// What one packet's octets turned out to say.
struct PGPDecodedPacket {
  /// The body's own version octet; -1 for the types that carry none.
  int version = -1;
  /// A few words that tell this packet apart from its neighbours in the
  /// tree, e.g. "Ed25519, SHA512"; empty when nothing parsed says more than
  /// the packet type already does.
  QString headline;
  /// The packet's main facts.
  QList<PGPInspectField> overview;
  /// Signatures only: the Hashed Area and Unhashed Area groups.
  QList<PGPInspectField> subpackets;
  /// Cryptographic values: key parameters, MPIs, encrypted session keys,
  /// and a secret key's protection.
  QList<PGPInspectField> material;
  /// One line saying what format @ref material is in, e.g. "Ed25519 ·
  /// Native public key · 32 bytes", built only from what was parsed: the
  /// algorithm octet, a curve OID, the lengths read. Says so plainly when
  /// the format is unsupported or the parse stopped. Empty for packets
  /// whose material needs no such line.
  QString material_summary;
  /// Contiguous ranges covering every octet that was handed over.
  QList<PGPHexSpan> spans;
  /// The first point the octets stopped making sense; empty when they did.
  QString error;
};

/**
 * @brief Decode one packet.
 *
 * @param wire the packet's octets from its CTB on; may be a prefix of it
 * @param wire_length how many octets the packet has on the wire, which is
 *        more than @p wire holds when the walk capped what it handed over
 *
 * Never throws, and covers every octet of @p wire with a span whatever it
 * holds: what cannot be read is one "unparsed" range plus an error.
 */
auto PGPDecodePacket(const QByteArray& wire, qint64 wire_length)
    -> PGPDecodedPacket;

/**
 * @brief The annotated hex dump, in the layout of `sq packet dump --hex`.
 *
 * Each range starts its own line at its offset's column; the label follows
 * the hex, or for an unlabelled range the octets as ASCII. Redacted octets
 * print as "--". When @p wire_length exceeds what @p wire holds, a closing
 * line says how much was left out.
 */
auto PGPFormatHexDump(const QByteArray& wire, const QList<PGPHexSpan>& spans,
                      qint64 wire_length) -> QString;
