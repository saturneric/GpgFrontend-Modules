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

/**
 * @file PGPPacketNames.h
 * @brief The OpenPGP registries, by number.
 *
 * Every table answers for any number, known or not: an unknown entry is
 * spelled with its number, so a reader is never shown a blank. Names follow
 * RFC 9580, with the GnuPG / LibrePGP code points GnuPG writes, and with
 * sq's spelling where it has one so a dump lines up with `sq packet dump`.
 *
 * The names are technical identifiers rather than prose, so they are not
 * translated -- the same as every other packet dumper.
 */

/// A packet type, e.g. "Signature". "Unknown" for an unassigned tag.
auto PGPTagName(int tag) -> QString;

/// "Ed25519 (27)": the name, then the number it was read from.
auto PGPPublicKeyAlgorithmName(int id) -> QString;

/// The name alone, e.g. "Ed25519", for the short tree-row hint.
auto PGPPublicKeyAlgorithmShortName(int id) -> QString;

auto PGPSymmetricAlgorithmName(int id) -> QString;

/// The cipher's block size in octets, which is the CFB IV length; 0 when
/// the cipher is unknown.
auto PGPSymmetricBlockSize(int id) -> int;

auto PGPHashAlgorithmName(int id) -> QString;

auto PGPAeadAlgorithmName(int id) -> QString;

/// The nonce length the AEAD mode takes; 0 when the mode is unknown.
auto PGPAeadNonceSize(int id) -> int;

auto PGPCompressionAlgorithmName(int id) -> QString;

/// "PositiveCertification (0x13)".
auto PGPSignatureTypeName(int type) -> QString;

/// The type name alone, for the short tree-row hint.
auto PGPSignatureTypeShortName(int type) -> QString;

auto PGPSignatureSubpacketName(int type) -> QString;

auto PGPRevocationReasonName(int code) -> QString;

/// A curve by the DER body of its OID, or "Unknown (<dotted OID>)".
auto PGPCurveName(const QByteArray& oid) -> QString;

/// An OID's DER body in dotted form, e.g. "1.3.132.0.34".
auto PGPDottedOid(const QByteArray& oid) -> QString;

/// The curve's size in bits, the figure sq reports as "Pk size"; 0 when
/// the curve is unknown.
auto PGPCurveBits(const QByteArray& oid) -> int;

/// The curve of an ephemeral ECC point, read off its encoding; empty when
/// the encoding fits no curve.
auto PGPEccPointCurve(const QByteArray& point) -> QString;

/// Key flags as sq abbreviates them ("CSA"), then spelled out.
auto PGPKeyFlagsText(const QByteArray& flags) -> QString;

/// Feature flags by name ("SEIPDv1, SEIPDv2").
auto PGPFeaturesText(const QByteArray& features) -> QString;

/// The S2K specifier type, e.g. "Iterated".
auto PGPS2KTypeName(int type) -> QString;
