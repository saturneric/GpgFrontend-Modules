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

#include "PGPPacketNames.h"

#include <QStringList>
#include <array>

using namespace Qt::StringLiterals;

namespace {

auto Numbered(const char* name, int id) -> QString {
  return u"%1 (%2)"_s.arg(QLatin1StringView(name)).arg(id);
}

auto Unknown(int id) -> QString { return u"Unknown (%1)"_s.arg(id); }

/// GnuPG's ML-KEM + ECC composite, which predates the RFC 9980 code points.
constexpr int kGnupgKyber = 8;

auto PublicKeyAlgorithmNameOrNull(int id) -> const char* {
  switch (id) {
    case 1:
      return "RSA";
    case 2:
      return "RSA (Encrypt-Only)";
    case 3:
      return "RSA (Sign-Only)";
    case kGnupgKyber:
      return "Kyber (GnuPG, ML-KEM+ECC)";
    case 9:
      return "Dilithium (GnuPG)";
    case 16:
      return "Elgamal (Encrypt-Only)";
    case 17:
      return "DSA";
    case 18:
      return "ECDH";
    case 19:
      return "ECDSA";
    case 20:
      return "Elgamal (Encrypt or Sign)";
    case 21:
      return "Diffie-Hellman (X9.42)";
    case 22:
      return "EdDSA (legacy)";
    case 25:
      return "X25519";
    case 26:
      return "X448";
    case 27:
      return "Ed25519";
    case 28:
      return "Ed448";
    case 30:
      return "ML-DSA-65+Ed25519";
    case 31:
      return "ML-DSA-87+Ed448";
    case 32:
      return "SLH-DSA-SHAKE-128s";
    case 33:
      return "SLH-DSA-SHAKE-128f";
    case 34:
      return "SLH-DSA-SHAKE-256s";
    case 35:
      return "ML-KEM-768+X25519";
    case 36:
      return "ML-KEM-1024+X448";
    case 37:
      return "ML-KEM-768+ECDH-NIST-P-384";
    case 38:
      return "ML-KEM-1024+ECDH-NIST-P-521";
    case 39:
      return "ML-KEM-768+ECDH-brainpoolP384r1";
    case 40:
      return "ML-KEM-1024+ECDH-brainpoolP512r1";
    default:
      return (id >= 100 && id <= 110) ? "Private/Experimental" : nullptr;
  }
}

struct Curve {
  const char* oid_hex;
  const char* name;
  int bits;
};

constexpr std::array<Curve, 11> kCurves = {{
    {"2a8648ce3d030107", "NIST P-256", 256},
    {"2b81040022", "NIST P-384", 384},
    {"2b81040023", "NIST P-521", 521},
    {"2b8104000a", "secp256k1", 256},
    {"2b2403030208010107", "brainpoolP256r1", 256},
    {"2b240303020801010b", "brainpoolP384r1", 384},
    {"2b240303020801010d", "brainpoolP512r1", 512},
    {"2b06010401da470f01", "Ed25519 (legacy)", 256},
    {"2b060104019755010501", "Curve25519 (legacy)", 256},
    {"2b6571", "Ed448", 448},
    {"2b656f", "X448", 448},
}};

auto FindCurve(const QByteArray& oid) -> const Curve* {
  const auto hex = oid.toHex();
  for (const auto& curve : kCurves) {
    if (hex == curve.oid_hex) return &curve;
  }
  return nullptr;
}

/// An OID's DER body in dotted form, for a curve no table names.
auto DottedOidOf(const QByteArray& oid) -> QString {
  if (oid.isEmpty()) return u"empty"_s;
  QStringList arcs;
  const auto first = static_cast<quint8>(oid.at(0));
  arcs << QString::number(first / 40) << QString::number(first % 40);
  quint64 value = 0;
  for (qsizetype i = 1; i < oid.size(); ++i) {
    const auto byte = static_cast<quint8>(oid.at(i));
    value = (value << 7) | (byte & 0x7f);
    if ((byte & 0x80) == 0) {
      arcs << QString::number(value);
      value = 0;
    }
  }
  return arcs.join('.');
}

}  // namespace

auto PGPTagName(int tag) -> QString {
  switch (tag) {
    case 1:
      return u"Public-Key Encrypted Session Key"_s;
    case 2:
      return u"Signature"_s;
    case 3:
      return u"Symmetric-Key Encrypted Session Key"_s;
    case 4:
      return u"One-Pass Signature"_s;
    case 5:
      return u"Secret Key"_s;
    case 6:
      return u"Public Key"_s;
    case 7:
      return u"Secret Subkey"_s;
    case 8:
      return u"Compressed Data"_s;
    case 9:
      return u"Symmetrically Encrypted Data"_s;
    case 10:
      return u"Marker"_s;
    case 11:
      return u"Literal Data"_s;
    case 12:
      return u"Trust"_s;
    case 13:
      return u"User ID"_s;
    case 14:
      return u"Public Subkey"_s;
    case 17:
      return u"User Attribute"_s;
    case 18:
      return u"Symmetrically Encrypted and Integrity Protected Data"_s;
    case 19:
      return u"Modification Detection Code"_s;
    case 20:
      return u"OCB Encrypted Data (GnuPG)"_s;
    case 21:
      return u"Padding"_s;
    default:
      return (tag >= 60 && tag <= 63) ? u"Private/Experimental"_s
                                      : u"Unknown"_s;
  }
}

auto PGPPublicKeyAlgorithmName(int id) -> QString {
  const auto* name = PublicKeyAlgorithmNameOrNull(id);
  return name == nullptr ? Unknown(id) : Numbered(name, id);
}

auto PGPPublicKeyAlgorithmShortName(int id) -> QString {
  const auto* name = PublicKeyAlgorithmNameOrNull(id);
  return name == nullptr ? Unknown(id) : QString::fromLatin1(name);
}

auto PGPSymmetricAlgorithmName(int id) -> QString {
  switch (id) {
    case 0:
      return u"Plaintext"_s;
    case 1:
      return u"IDEA"_s;
    case 2:
      return u"TripleDES"_s;
    case 3:
      return u"CAST5"_s;
    case 4:
      return u"Blowfish"_s;
    case 7:
      return u"AES-128"_s;
    case 8:
      return u"AES-192"_s;
    case 9:
      return u"AES-256"_s;
    case 10:
      return u"Twofish"_s;
    case 11:
      return u"Camellia-128"_s;
    case 12:
      return u"Camellia-192"_s;
    case 13:
      return u"Camellia-256"_s;
    default:
      return Unknown(id);
  }
}

auto PGPSymmetricBlockSize(int id) -> int {
  if (id >= 1 && id <= 4) return 8;
  if ((id >= 7 && id <= 13)) return 16;
  return 0;
}

auto PGPHashAlgorithmName(int id) -> QString {
  switch (id) {
    case 1:
      return u"MD5"_s;
    case 2:
      return u"SHA1"_s;
    case 3:
      return u"RIPEMD160"_s;
    case 8:
      return u"SHA256"_s;
    case 9:
      return u"SHA384"_s;
    case 10:
      return u"SHA512"_s;
    case 11:
      return u"SHA224"_s;
    case 12:
      return u"SHA3-256"_s;
    case 14:
      return u"SHA3-512"_s;
    default:
      return Unknown(id);
  }
}

auto PGPAeadAlgorithmName(int id) -> QString {
  switch (id) {
    case 1:
      return u"EAX"_s;
    case 2:
      return u"OCB"_s;
    case 3:
      return u"GCM"_s;
    default:
      return Unknown(id);
  }
}

auto PGPAeadNonceSize(int id) -> int {
  switch (id) {
    case 1:
      return 16;
    case 2:
      return 15;
    case 3:
      return 12;
    default:
      return 0;
  }
}

auto PGPCompressionAlgorithmName(int id) -> QString {
  switch (id) {
    case 0:
      return u"Uncompressed"_s;
    case 1:
      return u"ZIP (RFC 1951 deflate)"_s;
    case 2:
      return u"ZLIB (RFC 1950)"_s;
    case 3:
      return u"BZip2"_s;
    default:
      return Unknown(id);
  }
}

auto PGPSignatureTypeShortName(int type) -> QString {
  switch (type) {
    case 0x00:
      return u"Binary"_s;
    case 0x01:
      return u"Text"_s;
    case 0x02:
      return u"Standalone"_s;
    case 0x10:
      return u"GenericCertification"_s;
    case 0x11:
      return u"PersonaCertification"_s;
    case 0x12:
      return u"CasualCertification"_s;
    case 0x13:
      return u"PositiveCertification"_s;
    case 0x18:
      return u"SubkeyBinding"_s;
    case 0x19:
      return u"PrimaryKeyBinding"_s;
    case 0x1f:
      return u"DirectKey"_s;
    case 0x20:
      return u"KeyRevocation"_s;
    case 0x28:
      return u"SubkeyRevocation"_s;
    case 0x30:
      return u"CertificationRevocation"_s;
    case 0x40:
      return u"Timestamp"_s;
    case 0x50:
      return u"ThirdPartyConfirmation"_s;
    default:
      return u"Unknown"_s;
  }
}

auto PGPSignatureTypeName(int type) -> QString {
  return u"%1 (0x%2)"_s.arg(PGPSignatureTypeShortName(type))
      .arg(type, 2, 16, QChar('0'));
}

auto PGPSignatureSubpacketName(int type) -> QString {
  switch (type) {
    case 2:
      return u"Signature Creation Time"_s;
    case 3:
      return u"Signature Expiration Time"_s;
    case 4:
      return u"Exportable Certification"_s;
    case 5:
      return u"Trust Signature"_s;
    case 6:
      return u"Regular Expression"_s;
    case 7:
      return u"Revocable"_s;
    case 9:
      return u"Key Expiration Time"_s;
    case 11:
      return u"Preferred Symmetric Ciphers"_s;
    case 12:
      return u"Revocation Key"_s;
    case 16:
      return u"Issuer Key ID"_s;
    case 20:
      return u"Notation"_s;
    case 21:
      return u"Preferred Hash Algorithms"_s;
    case 22:
      return u"Preferred Compression Algorithms"_s;
    case 23:
      return u"Key Server Preferences"_s;
    case 24:
      return u"Preferred Key Server"_s;
    case 25:
      return u"Primary User ID"_s;
    case 26:
      return u"Policy URI"_s;
    case 27:
      return u"Key Flags"_s;
    case 28:
      return u"Signer's User ID"_s;
    case 29:
      return u"Reason for Revocation"_s;
    case 30:
      return u"Features"_s;
    case 31:
      return u"Signature Target"_s;
    case 32:
      return u"Embedded Signature"_s;
    case 33:
      return u"Issuer Fingerprint"_s;
    case 34:
      return u"Preferred AEAD Algorithms (LibrePGP)"_s;
    case 35:
      return u"Intended Recipient Fingerprint"_s;
    case 37:
      return u"Attested Certifications"_s;
    case 38:
      return u"Key Block"_s;
    case 39:
      return u"Preferred AEAD Ciphersuites"_s;
    default:
      return (type >= 100 && type <= 110)
                 ? Numbered("Private/Experimental", type)
                 : Numbered("Unknown Subpacket", type);
  }
}

auto PGPRevocationReasonName(int code) -> QString {
  switch (code) {
    case 0:
      return u"No reason specified"_s;
    case 1:
      return u"Key is superseded"_s;
    case 2:
      return u"Key material has been compromised"_s;
    case 3:
      return u"Key is retired and no longer used"_s;
    case 32:
      return u"User ID information is no longer valid"_s;
    default:
      return Unknown(code);
  }
}

auto PGPCurveName(const QByteArray& oid) -> QString {
  const auto* curve = FindCurve(oid);
  return curve == nullptr ? u"Unknown (%1)"_s.arg(DottedOidOf(oid))
                          : QString::fromLatin1(curve->name);
}

auto PGPDottedOid(const QByteArray& oid) -> QString { return DottedOidOf(oid); }

auto PGPCurveBits(const QByteArray& oid) -> int {
  const auto* curve = FindCurve(oid);
  return curve == nullptr ? 0 : curve->bits;
}

auto PGPEccPointCurve(const QByteArray& point) -> QString {
  // Curve25519 points carry a 0x40 prefix; X448 points are 56 bare octets
  // (GnuPG's SOS writer keeps leading zero octets); Weierstrass points are
  // uncompressed 0x04 || x || y. A NIST and a brainpool curve of the same
  // size encode identically, so those are reported as either.
  if (point.isEmpty()) return {};
  const auto prefix = static_cast<quint8>(point.at(0));
  const auto size = point.size();
  if (prefix == 0x40 && size == 33) return u"Curve25519"_s;
  if (prefix == 0x04 && size == 65) return u"NIST P-256 or brainpoolP256r1"_s;
  if (prefix == 0x04 && size == 97) return u"NIST P-384 or brainpoolP384r1"_s;
  if (prefix == 0x04 && size == 129) return u"brainpoolP512r1"_s;
  if (prefix == 0x04 && size == 133) return u"NIST P-521"_s;
  if (size == 56) return u"X448"_s;
  return {};
}

auto PGPKeyFlagsText(const QByteArray& flags) -> QString {
  const auto bit = [&flags](int index, quint8 mask) {
    return index < flags.size() &&
           (static_cast<quint8>(flags.at(index)) & mask) != 0;
  };

  struct Flag {
    int index;
    quint8 mask;
    const char* letter;
    const char* meaning;
  };
  // sq's letters, in sq's order.
  constexpr std::array<Flag, 9> kFlags = {{
      {0, 0x01, "C", "certify"},
      {0, 0x02, "S", "sign"},
      {0, 0x04, "Et", "encrypt communications"},
      {0, 0x08, "Er", "encrypt storage"},
      {0, 0x20, "A", "authenticate"},
      {0, 0x10, "D", "split key"},
      {0, 0x80, "G", "group key"},
      {1, 0x04, "ADSK", "additional decryption subkey"},
      {1, 0x08, "T", "timestamping"},
  }};

  QString letters;
  QStringList meanings;
  for (const auto& flag : kFlags) {
    if (!bit(flag.index, flag.mask)) continue;
    letters += QLatin1StringView(flag.letter);
    meanings << QLatin1StringView(flag.meaning);
  }
  if (letters.isEmpty()) return u"none"_s;
  return u"%1 (%2)"_s.arg(letters, meanings.join(", "_L1));
}

auto PGPFeaturesText(const QByteArray& features) -> QString {
  const auto first =
      features.isEmpty() ? 0 : static_cast<quint8>(features.at(0));
  QStringList names;
  if ((first & 0x01) != 0) names << u"SEIPDv1"_s;
  if ((first & 0x02) != 0) names << u"OCB Encrypted Data (GnuPG)"_s;
  if ((first & 0x04) != 0) names << u"v5 Keys (LibrePGP)"_s;
  if ((first & 0x08) != 0) names << u"SEIPDv2"_s;
  return names.isEmpty() ? u"none"_s : names.join(", "_L1);
}

auto PGPS2KTypeName(int type) -> QString {
  switch (type) {
    case 0:
      return u"Simple"_s;
    case 1:
      return u"Salted"_s;
    case 3:
      return u"Iterated"_s;
    case 4:
      return u"Argon2"_s;
    case 101:
      return u"GNU Extension"_s;
    default:
      return Unknown(type);
  }
}
