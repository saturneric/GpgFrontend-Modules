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

#include <gtest/gtest.h>

#include <QFile>
#include <QRegularExpression>
#include <QStringList>

#include "PGPPacketDecoder.h"
#include "PGPPacketNames.h"

using namespace Qt::StringLiterals;

/**
 * The decoder, against the binary vectors in vectors/: the RFC 9580 corpus
 * the Rust engine tests use, and the RFC's own Appendix A samples. Each
 * *.sq-hex.txt beside a vector is `sq packet dump --hex` of it (sq 1.3.1),
 * which the dump layout is pinned against.
 */

namespace {

auto Load(const QString& name) -> QByteArray {
  QFile file(QStringLiteral(GF_PGP_INSPECT_VECTORS "/") + name);
  EXPECT_TRUE(file.open(QIODevice::ReadOnly)) << name.toStdString();
  return file.readAll();
}

/// Split a binary packet stream into each packet's wire octets: the test's
/// own small reading of the framing, so the decoder is not checked against
/// itself.
auto Split(const QByteArray& stream) -> QList<QByteArray> {
  QList<QByteArray> packets;
  qsizetype at = 0;
  const auto octet = [&stream](qsizetype i) {
    return static_cast<quint8>(stream.at(i));
  };
  while (at < stream.size()) {
    const auto start = at;
    const auto ctb = octet(at++);
    if ((ctb & 0x40) == 0) {
      const auto type = ctb & 0x03;
      if (type == 3) {
        at = stream.size();
      } else {
        const int octets = type == 0 ? 1 : type == 1 ? 2 : 4;
        qint64 length = 0;
        for (int i = 0; i < octets; ++i) length = (length << 8) | octet(at++);
        at += length;
      }
    } else {
      while (true) {
        const auto first = octet(at++);
        if (first < 192) {
          at += first;
        } else if (first < 224) {
          at += ((first - 192) << 8) + octet(at++) + 192;
        } else if (first == 255) {
          qint64 length = 0;
          for (int i = 0; i < 4; ++i) length = (length << 8) | octet(at++);
          at += length;
        } else {
          at += qint64{1} << (first & 0x1f);
          continue;
        }
        break;
      }
    }
    packets << stream.mid(start, at - start);
  }
  return packets;
}

struct Decoded {
  QByteArray wire;
  PGPDecodedPacket packet;
};

auto DecodeAll(const QString& name) -> QList<Decoded> {
  QList<Decoded> out;
  for (const auto& wire : Split(Load(name))) {
    out << Decoded{wire, PGPDecodePacket(wire, wire.size())};
  }
  return out;
}

auto Tag(const QByteArray& wire) -> int {
  const auto ctb = static_cast<quint8>(wire.at(0));
  return (ctb & 0x40) != 0 ? ctb & 0x3f : (ctb >> 2) & 0x0f;
}

auto Find(const QList<PGPInspectField>& fields, const QString& label)
    -> const PGPInspectField* {
  for (const auto& field : fields) {
    if (field.label == label) return &field;
    if (const auto* found = Find(field.children, label)) return found;
  }
  return nullptr;
}

auto Value(const QList<PGPInspectField>& fields, const QString& label)
    -> QString {
  const auto* field = Find(fields, label);
  return field == nullptr ? QStringLiteral("<absent>") : field->value;
}

void Flatten(const QList<PGPInspectField>& fields, QStringList& out) {
  for (const auto& field : fields) {
    out << field.label << field.value;
    Flatten(field.children, out);
  }
}

/// Every label and value the packet shows, in every section.
auto AllText(const PGPDecodedPacket& packet) -> QString {
  QStringList out{packet.headline};
  Flatten(packet.overview, out);
  Flatten(packet.subpackets, out);
  Flatten(packet.material, out);
  return out.join('\n');
}

const QStringList kVectors = {
    "sig_good_detached.pgp",
    "sig_v6_detached.pgp",
    "sig_good_inline_v6.pgp",
    "two_signer.pgp",
    "enc_v1seipd_mdc.pgp",
    "enc_v2seipd_ocb.pgp",
    "enc_multi_recipient.pgp",
    "enc_symmetric_v1.pgp",
    "enc_symmetric_v2.pgp",
    "enc_sed_tag9.pgp",
    "sig_inline_compressed.pgp",
    "key1.pgp",
    "key2.pgp",
    "rfc9580_a1_v4_cert.pgp",
    "rfc9580_a2_v4_sig.pgp",
    "rfc9580_a3_v6_cert.pgp",
    "rfc9580_a4_v6_secret_unlocked.pgp",
    "rfc9580_a5_v6_secret_locked.pgp",
    "rfc9580_a8_x25519_ocb.pgp",
    "rfc9580_a9_eax_skesk.pgp",
    "rfc9580_a10_ocb_skesk.pgp",
    "rfc9580_a11_gcm_skesk.pgp",
    "rfc9580_a12_argon2_aes128.pgp",
    "elgamal_subkey_cert.pgp",
    "brainpool_subkey_cert.pgp",
};

void ExpectCovered(const QByteArray& wire, const PGPDecodedPacket& packet,
                   const std::string& what) {
  qint64 at = 0;
  for (const auto& span : packet.spans) {
    EXPECT_EQ(span.offset, at) << what << ": gap or overlap";
    EXPECT_GT(span.length, 0) << what;
    at = span.offset + span.length;
  }
  EXPECT_EQ(at, wire.size()) << what << ": not every octet is covered";
}

/// A new-format packet with a five-octet length.
auto Packet(int tag, const QByteArray& body) -> QByteArray {
  QByteArray out;
  out += static_cast<char>(0xc0 | tag);
  out += static_cast<char>(0xff);
  for (int shift = 24; shift >= 0; shift -= 8) {
    out += static_cast<char>((body.size() >> shift) & 0xff);
  }
  return out + body;
}

auto Be32(quint32 value) -> QByteArray {
  QByteArray out;
  for (int shift = 24; shift >= 0; shift -= 8) {
    out += static_cast<char>((value >> shift) & 0xff);
  }
  return out;
}

/// A Curve25519 point as GnuPG writes it: an SOS of 0x40 || 32 octets.
auto SosX25519Point() -> QByteArray {
  return QByteArray::fromHex("010740") + QByteArray(32, 0x11);
}

/// An X448 point as GnuPG writes it: 56 bare octets, the bit count dropping
/// only the leading zero bits of a non-zero first octet.
auto SosX448Point(quint8 first) -> QByteArray {
  int bits = 448;
  if (first != 0) {
    int leading = 0;
    for (int mask = 0x80; (first & mask) == 0; mask >>= 1) ++leading;
    bits -= leading;
  }
  QByteArray out;
  out += static_cast<char>(bits >> 8);
  out += static_cast<char>(bits & 0xff);
  out += static_cast<char>(first);
  return out + QByteArray(55, 0x55);
}

/// The algorithm-specific part of a GnuPG Kyber (algorithm 8) PKESK.
auto GnupgKyberPkeskValues(const QByteArray& point, int ciphertext_len)
    -> QByteArray {
  return point + Be32(ciphertext_len) + QByteArray(ciphertext_len, 0x22) +
         QByteArray::fromHex("0928") + QByteArray(40, 0x33);
}

auto V3Pkesk(int algorithm, const QByteArray& values) -> QByteArray {
  QByteArray body("\x03", 1);
  body += QByteArray(8, static_cast<char>(0xab));
  body += static_cast<char>(algorithm);
  return Packet(1, body + values);
}

auto GnupgKyberKey(const QByteArray& oid, int public_len) -> QByteArray {
  QByteArray body = QByteArray::fromHex("046000000008");
  body += static_cast<char>(oid.size());
  body +=
      oid + SosX25519Point() + Be32(public_len) + QByteArray(public_len, 0x44);
  return Packet(6, body);
}

auto DecodeWire(const QByteArray& wire) -> PGPDecodedPacket {
  return PGPDecodePacket(wire, wire.size());
}

}  // namespace

// -- the byte map ------------------------------------------------------------

TEST(PGPPacketDecoderTest, EveryVectorDecodesCleanlyAndCoversEveryOctet) {
  for (const auto& name : kVectors) {
    const auto packets = DecodeAll(name);
    ASSERT_FALSE(packets.isEmpty()) << name.toStdString();
    for (const auto& [wire, packet] : packets) {
      const auto what =
          name.toStdString() + " tag " + std::to_string(Tag(wire));
      EXPECT_TRUE(packet.error.isEmpty())
          << what << ": " << packet.error.toStdString();
      ExpectCovered(wire, packet, what);
      EXPECT_FALSE(packet.overview.isEmpty()) << what;
    }
  }
}

TEST(PGPPacketDecoderTest, NoTruncationOrCorruptionCrashesOrLosesAnOctet) {
  for (const auto& name : kVectors) {
    for (const auto& wire : Split(Load(name))) {
      for (qsizetype cut = 0; cut <= wire.size(); ++cut) {
        const auto prefix = wire.left(cut);
        ExpectCovered(prefix, DecodeWire(prefix), name.toStdString());
      }
      for (qsizetype at = 1; at < wire.size(); ++at) {
        auto flipped = wire;
        flipped[at] = static_cast<char>(flipped.at(at) ^ 0xff);
        ExpectCovered(flipped, DecodeWire(flipped), name.toStdString());
      }
    }
  }
}

TEST(PGPPacketDecoderTest, ATruncatedPacketSaysSo) {
  const auto wire = Split(Load("sig_good_detached.pgp")).at(0);
  const auto packet = DecodeWire(wire.left(wire.size() - 10));
  EXPECT_FALSE(packet.error.isEmpty());
}

TEST(PGPPacketDecoderTest, GarbageIsUnparsedNotACrash) {
  const QByteArray garbage("this is definitely not OpenPGP");
  const auto packet = DecodeWire(garbage);
  EXPECT_FALSE(packet.error.isEmpty());
  ExpectCovered(garbage, packet, "garbage");
  EXPECT_TRUE(DecodeWire({}).spans.isEmpty());
}

TEST(PGPPacketDecoderTest, ACappedPayloadIsNoDefectAndTheDumpSaysWhatIsLeft) {
  QByteArray body("b\x04name", 6);
  body += Be32(0) + QByteArray(1000, 'A');
  const auto wire = Packet(11, body);

  const auto packet = PGPDecodePacket(wire.left(100), wire.size());
  EXPECT_TRUE(packet.error.isEmpty()) << packet.error.toStdString();
  EXPECT_EQ(Value(packet.overview, "Data Size"), QString("1000 bytes"));
  ExpectCovered(wire.left(100), packet, "capped literal");

  const auto dump = PGPFormatHexDump(wire.left(100), packet.spans, wire.size());
  EXPECT_TRUE(dump.endsWith("… 916 bytes more not shown") ||
              dump.contains("more not shown"))
      << dump.toStdString();
}

TEST(PGPPacketDecoderTest, APartialLengthBodyIsReassembledAcrossChunks) {
  // A literal packet in two chunks: 512 octets, then the 12 left.
  QByteArray body("b\x04name", 6);
  body += Be32(0) + QByteArray(514, 'Z');
  ASSERT_EQ(body.size(), 524);
  QByteArray wire;
  wire += static_cast<char>(0xcb);
  wire += static_cast<char>(0xe9);  // a partial chunk of 2^9
  wire += body.left(512);
  wire += static_cast<char>(12);
  wire += body.mid(512);

  const auto packet = DecodeWire(wire);
  EXPECT_TRUE(packet.error.isEmpty()) << packet.error.toStdString();
  EXPECT_EQ(Value(packet.overview, "File Name"), QString("name"));
  EXPECT_EQ(Value(packet.overview, "Data Size"), QString("514 bytes"));
  EXPECT_EQ(packet.headline, QString("name")) << "the file name first";
  ExpectCovered(wire, packet, "partial literal");

  int lengths = 0;
  for (const auto& span : packet.spans) lengths += span.label == "length";
  EXPECT_EQ(lengths, 2) << "each chunk's length is its own range";
}

// -- signatures
// ----------------------------------------------------------------

TEST(PGPPacketDecoderTest, AV4SignatureShowsItsAlgorithmsAndEverySubpacket) {
  const auto packets = DecodeAll("sig_good_detached.pgp");
  ASSERT_EQ(packets.size(), 1);
  const auto& sig = packets.at(0).packet;

  EXPECT_EQ(sig.version, 4);
  EXPECT_EQ(Value(sig.overview, "Type"), QString("Binary (0x00)"));
  EXPECT_EQ(Value(sig.overview, "Hash Algorithm"), QString("SHA512"));
  EXPECT_EQ(Value(sig.overview, "Public Key Algorithm"),
            QString("EdDSA (legacy) (22)"));
  EXPECT_EQ(Value(sig.overview, "Digest Prefix"), QString("7B75"));
  EXPECT_EQ(Value(sig.overview, "Issuer Fingerprint"),
            QString("08046811F5ED9F4215EE2BB9F29A1D9930F596BF"));
  EXPECT_EQ(sig.headline, QString("EdDSA (legacy), SHA512"))
      << "the algorithms, not the type every detached signature shares";

  ASSERT_EQ(sig.subpackets.size(), 2);
  EXPECT_EQ(sig.subpackets.at(0).label, QString("Hashed Area"));
  EXPECT_EQ(sig.subpackets.at(1).label, QString("Unhashed Area"));
  EXPECT_EQ(Value(sig.subpackets, "Signature Creation Time"),
            QString("2026-07-24 12:44:25 UTC (critical)"));
  EXPECT_EQ(Value(sig.subpackets, "Issuer Key ID"),
            QString("F29A1D9930F596BF"));
  EXPECT_TRUE(Value(sig.subpackets, "Notation")
                  .startsWith("salt@notations.sequoia-pgp.org = D9F858C5"));

  ASSERT_EQ(sig.material.size(), 2);
  EXPECT_EQ(sig.material.at(0).label, QString("r"));
  EXPECT_EQ(sig.material.at(0).detail,
            QString("32 bytes · encoded as MPI (256 bits)"));
  EXPECT_EQ(sig.material.at(1).label, QString("s"));
  EXPECT_EQ(sig.material_summary,
            QString("EdDSA (legacy) · MPI-encoded r and s"));
}

TEST(PGPPacketDecoderTest, AV6SignatureShowsItsSalt) {
  const auto sig = DecodeAll("sig_v6_detached.pgp").at(0).packet;
  EXPECT_EQ(sig.version, 6);
  EXPECT_EQ(
      Value(sig.overview, "Salt"),
      QString(
          "AF04BB19FF55FF9E2CE9D831D5F524ED17EDDE19768FDFE543EED251B38870D7"));
  EXPECT_EQ(
      Value(sig.overview, "Issuer Fingerprint"),
      QString(
          "A80CAEEA5048ED60F869B3E0D1833E6CBA2037CD8884D0A2A56903C674309DAD"));
  EXPECT_NE(Find(sig.material, "Ed25519 Signature"), nullptr);
}

TEST(PGPPacketDecoderTest, ACertificationDecodesFlagsPreferencesAndBothAreas) {
  const auto packets = DecodeAll("key2.pgp");
  const auto self_sig =
      std::find_if(packets.begin(), packets.end(),
                   [](const Decoded& d) { return Tag(d.wire) == 2; });
  ASSERT_NE(self_sig, packets.end());
  const auto& sig = self_sig->packet;

  EXPECT_EQ(Value(sig.overview, "Type"),
            QString("PositiveCertification (0x13)"));
  EXPECT_TRUE(Value(sig.subpackets, "Key Flags").startsWith("CSA ("));
  EXPECT_EQ(Value(sig.subpackets, "Features"), QString("SEIPDv1"));
  EXPECT_EQ(Value(sig.subpackets, "Primary User ID"), QString("yes"));
  EXPECT_EQ(Value(sig.subpackets, "Preferred Hash Algorithms"),
            QString("none"));
  EXPECT_EQ(Value({sig.subpackets.at(1)}, "Issuer Key ID"),
            QString("4DD9733454846D0C"))
      << "the issuer sits in the unhashed area";
}

TEST(PGPPacketDecoderTest, AV6CertificateDecodesItsPreferences) {
  const auto sig = DecodeAll("rfc9580_a3_v6_cert.pgp").at(1).packet;
  EXPECT_EQ(Value(sig.subpackets, "Preferred Symmetric Ciphers"),
            QString("AES-256, AES-128"));
  EXPECT_EQ(Value(sig.subpackets, "Key Flags"),
            QString("CS (certify, sign) (critical)"));
  EXPECT_EQ(Value(sig.subpackets, "Features"), QString("SEIPDv1, SEIPDv2"));
  EXPECT_EQ(Value(sig.subpackets, "Preferred AEAD Ciphersuites"),
            QString("AES-256/OCB, AES-128/OCB"));
}

TEST(PGPPacketDecoderTest, OnePassSignaturesNameTheirIssuer) {
  int one_pass = 0;
  for (const auto& [wire, packet] : DecodeAll("two_signer.pgp")) {
    if (Tag(wire) != 4) continue;
    ++one_pass;
    EXPECT_NE(Value(packet.overview, "Last"), QString("<absent>"));
    EXPECT_TRUE(Find(packet.overview, "Issuer Key ID") != nullptr ||
                Find(packet.overview, "Issuer Fingerprint") != nullptr);
  }
  EXPECT_EQ(one_pass, 2);
}

// -- keys
// ------------------------------------------------------------------------

TEST(PGPPacketDecoderTest, KeysReportTheFingerprintsTheRfcStates) {
  const auto a1 = DecodeAll("rfc9580_a1_v4_cert.pgp").at(0).packet;
  EXPECT_EQ(Value(a1.overview, "Fingerprint"),
            QString("C959BDBAFA32A2F89A153B678CFDE12197965A9A"));
  EXPECT_EQ(Value(a1.overview, "Key ID"), QString("8CFDE12197965A9A"));
  EXPECT_EQ(Value(a1.overview, "Curve"), QString("Ed25519 (legacy)"));
  EXPECT_EQ(Value(a1.overview, "Key Size"), QString("256 bits"));

  const auto a3 = DecodeAll("rfc9580_a3_v6_cert.pgp");
  EXPECT_EQ(
      Value(a3.at(0).packet.overview, "Fingerprint"),
      QString(
          "CB186C4F0609A697E4D52DFA6C722B0C1F1E27C18A56708F6525EC27BAD9ACC9"));
  EXPECT_EQ(Value(a3.at(0).packet.overview, "Key ID"),
            QString("CB186C4F0609A697"));
  EXPECT_EQ(
      Value(a3.at(2).packet.overview, "Fingerprint"),
      QString(
          "12C83F1E706F6308FE151A417743A1F033790E93E9978488D1DB378DA9930885"));
  EXPECT_EQ(Value(a3.at(2).packet.overview, "Public Key Algorithm"),
            QString("X25519 (25)"));
}

TEST(PGPPacketDecoderTest, AnEcdhSubkeyShowsItsCurveAndKdf) {
  const auto packets = DecodeAll("key2.pgp");
  const auto& primary = packets.at(0).packet;
  EXPECT_EQ(Value(primary.overview, "Fingerprint"),
            QString("3BEDAB48EAAAA195006330414DD9733454846D0C"));
  EXPECT_EQ(Value(primary.overview, "Curve"), QString("NIST P-384"));
  EXPECT_EQ(Value(primary.overview, "Key Size"), QString("384 bits"));

  const auto subkey =
      std::find_if(packets.begin(), packets.end(),
                   [](const Decoded& d) { return Tag(d.wire) == 14; });
  ASSERT_NE(subkey, packets.end());
  EXPECT_EQ(Value(subkey->packet.material, "KDF Hash"), QString("SHA384"));
  EXPECT_EQ(Value(subkey->packet.material, "KEK Cipher"), QString("AES-192"));
  // An uncompressed P-384 point: 0x04 || x || y, 97 octets, 771 bits as
  // encoded, which sq also reports. The key size is the curve's, not that.
  const auto* point = Find(subkey->packet.material, "Public Point");
  ASSERT_NE(point, nullptr);
  EXPECT_EQ(point->detail, QString("97 bytes · encoded as MPI (771 bits)"));
  EXPECT_EQ(Value(subkey->packet.overview, "Key Size"), QString("384 bits"));
}

TEST(PGPPacketDecoderTest, AProtectedSecretKeyShowsItsProtectionOnly) {
  const auto key = DecodeAll("key1.pgp").at(0);
  const auto& material = key.packet.material;
  EXPECT_EQ(Value(material, "Secret Key"),
            QString("Encrypted (SHA-1 checksum)"));
  EXPECT_EQ(Value(material, "Cipher"), QString("AES-256"));
  EXPECT_EQ(Value(material, "S2K"), QString("Iterated, SHA256"));
  EXPECT_EQ(Value(material, "Salt"), QString("39CED85B9E6A4D1E"));
  EXPECT_EQ(Value(material, "Hash Bytes"), QString("16777216"));
  EXPECT_EQ(Value(key.packet.overview, "Key ID"), QString("BDB8BB6BDDFA8497"));
}

TEST(PGPPacketDecoderTest, AnUnprotectedSecretNeverReachesTheFieldsOrTheDump) {
  for (const auto& [wire, packet] :
       DecodeAll("rfc9580_a4_v6_secret_unlocked.pgp")) {
    if (Tag(wire) != 5 && Tag(wire) != 7) continue;
    EXPECT_EQ(Value(packet.material, "Secret Key"), QString("Unprotected"));

    const auto secret =
        std::find_if(packet.spans.begin(), packet.spans.end(),
                     [](const PGPHexSpan& s) { return s.redacted; });
    ASSERT_NE(secret, packet.spans.end()) << "the secret part is one range";
    ASSERT_GE(secret->length, 32);
    const auto octets = wire.mid(secret->offset, secret->length);

    const auto dump = PGPFormatHexDump(wire, packet.spans, wire.size());
    const auto text = AllText(packet);
    // Any run of four secret octets would be a leak, in either spelling.
    for (qsizetype i = 0; i + 4 <= octets.size(); ++i) {
      const auto run = octets.mid(i, 4).toHex();
      const QString spaced =
          QString::fromLatin1(run.mid(0, 2) + ' ' + run.mid(2, 2));
      EXPECT_FALSE(dump.contains(spaced)) << dump.toStdString();
      EXPECT_FALSE(
          text.contains(QString::fromLatin1(run), Qt::CaseInsensitive));
    }
    EXPECT_TRUE(dump.contains("-- -- -- --"));
  }
}

TEST(PGPPacketDecoderTest, AnArgon2ProtectedV6KeyShowsItsAeadAndS2K) {
  const auto material =
      DecodeAll("rfc9580_a5_v6_secret_locked.pgp").at(0).packet.material;
  EXPECT_EQ(Value(material, "Secret Key"), QString("Encrypted (AEAD)"));
  EXPECT_EQ(Value(material, "AEAD Mode"), QString("OCB"));
  EXPECT_EQ(Value(material, "S2K"), QString("Argon2"));
  EXPECT_EQ(Value(material, "Passes"), QString("1"));
  EXPECT_EQ(Value(material, "Parallelism"), QString("4"));
  EXPECT_EQ(Value(material, "Memory"), QString("2^21 KiB (2048 MiB)"));
}

// -- session keys and containers
// -----------------------------------------------

TEST(PGPPacketDecoderTest, AV1MessageIsEcdhPkesksThenAnMdcContainer) {
  const auto packets = DecodeAll("enc_v1seipd_mdc.pgp");
  for (qsizetype i = 0; i + 1 < packets.size(); ++i) {
    const auto& pkesk = packets.at(i).packet;
    EXPECT_EQ(Value(pkesk.overview, "Public Key Algorithm"),
              QString("ECDH (18)"));
    EXPECT_NE(Find(pkesk.overview, "Curve"), nullptr);
  }
  const auto& seipd = packets.last().packet;
  EXPECT_EQ(seipd.version, 1);
  EXPECT_EQ(Value(seipd.overview, "Integrity Protection"),
            QString("SHA-1 modification detection code"));
  EXPECT_EQ(Value(seipd.overview, "Payload"),
            QString("encrypted, not opened by the inspector"));
}

TEST(PGPPacketDecoderTest, AV2MessageShowsItsRecipientAndAeadEnvelope) {
  const auto packets = DecodeAll("enc_v2seipd_ocb.pgp");
  ASSERT_EQ(packets.size(), 2);
  const auto& pkesk = packets.at(0).packet;
  EXPECT_EQ(pkesk.version, 6);
  EXPECT_EQ(
      Value(pkesk.overview, "Recipient Fingerprint"),
      QString(
          "78375611D36C119E961821243CFDCE63521607121372544AA208F80FF9FF3589"));
  EXPECT_EQ(Value(pkesk.overview, "Variant"), QString("X25519"));

  const auto& seipd = packets.at(1).packet;
  EXPECT_EQ(seipd.version, 2);
  EXPECT_EQ(Value(seipd.overview, "Symmetric Algorithm"), QString("AES-256"));
  EXPECT_EQ(Value(seipd.overview, "AEAD Mode"), QString("OCB"));
  EXPECT_EQ(Value(seipd.overview, "Chunk Size"), QString("4096 bytes"));
  EXPECT_EQ(
      Value(seipd.overview, "Salt"),
      QString(
          "68C82325819839616553F083C8B5F3DFE72485B277934E646C7F3D507602FC12"));
}

TEST(PGPPacketDecoderTest, EachRecipientIsDistinguishable) {
  QStringList ids;
  for (const auto& [wire, packet] : DecodeAll("enc_multi_recipient.pgp")) {
    if (Tag(wire) == 1) ids << Value(packet.overview, "Recipient Key ID");
  }
  ASSERT_GE(ids.size(), 3);
  ids.removeDuplicates();
  EXPECT_GE(ids.size(), 3);
}

TEST(PGPPacketDecoderTest, APassphraseMessageShowsItsS2K) {
  const auto skesk = DecodeAll("enc_symmetric_v2.pgp").at(0).packet;
  EXPECT_EQ(skesk.version, 4);
  EXPECT_EQ(Value(skesk.overview, "Symmetric Algorithm"), QString("AES-256"));
  EXPECT_EQ(Value(skesk.overview, "S2K"), QString("Iterated, SHA256"));
  EXPECT_EQ(Value(skesk.overview, "Salt"), QString("C37E9ED61AC976F4"));
  EXPECT_EQ(Value(skesk.overview, "Hash Bytes"), QString("65011712"));
  EXPECT_NE(Find(skesk.material, "Encrypted Session Key"), nullptr);
}

TEST(PGPPacketDecoderTest, V6SkesksShowTheirAeadModeAndArgon2) {
  for (const auto& [name, mode] :
       {std::pair{"rfc9580_a9_eax_skesk.pgp", "EAX"},
        std::pair{"rfc9580_a10_ocb_skesk.pgp", "OCB"},
        std::pair{"rfc9580_a11_gcm_skesk.pgp", "GCM"}}) {
    const auto skesk = DecodeAll(name).at(0).packet;
    EXPECT_EQ(skesk.version, 6) << name;
    EXPECT_EQ(Value(skesk.overview, "AEAD Mode"), QString(mode)) << name;
    EXPECT_NE(Find(skesk.material, "Nonce"), nullptr) << name;
  }
  const auto argon2 = DecodeAll("rfc9580_a12_argon2_aes128.pgp").at(0).packet;
  EXPECT_EQ(Value(argon2.overview, "S2K"), QString("Argon2"));
  EXPECT_NE(Find(argon2.overview, "Passes"), nullptr);
  EXPECT_NE(Find(argon2.overview, "Memory"), nullptr);
}

TEST(PGPPacketDecoderTest, TheLegacyUnprotectedContainerIsFlagged) {
  const auto packets = DecodeAll("enc_sed_tag9.pgp");
  const auto sed =
      std::find_if(packets.begin(), packets.end(),
                   [](const Decoded& d) { return Tag(d.wire) == 9; });
  ASSERT_NE(sed, packets.end());
  EXPECT_TRUE(
      Value(sed->packet.overview, "Integrity Protection").startsWith("none"));
}

TEST(PGPPacketDecoderTest, ACompressedPacketNamesItsAlgorithm) {
  const auto compressed = DecodeAll("sig_inline_compressed.pgp").at(0);
  ASSERT_EQ(Tag(compressed.wire), 8);
  EXPECT_NE(Value(compressed.packet.overview, "Compression Algorithm"),
            QString("<absent>"));
}

// -- algorithm variants, ported from the Rust walk
// ------------------------------

TEST(PGPPacketDecoderTest, AGnupgKyberSessionKeyNamesItsKemAndCurve) {
  for (const auto& [len, variant] :
       {std::pair{1088, "ML-KEM-768 + Curve25519"},
        std::pair{1568, "ML-KEM-1024 + Curve25519"}}) {
    const auto packet =
        DecodeWire(V3Pkesk(8, GnupgKyberPkeskValues(SosX25519Point(), len)));
    EXPECT_TRUE(
        Value(packet.overview, "Public Key Algorithm").contains("Kyber"));
    EXPECT_EQ(Value(packet.overview, "Variant"), QString(variant));
  }
  // Any first octet of an X448 point, including zero.
  for (const quint8 first : {0x9a, 0x01, 0x00}) {
    const auto packet = DecodeWire(
        V3Pkesk(8, GnupgKyberPkeskValues(SosX448Point(first), 1568)));
    EXPECT_EQ(Value(packet.overview, "Variant"), QString("ML-KEM-1024 + X448"))
        << int(first);
  }
}

TEST(PGPPacketDecoderTest, AMalformedGnupgKyberSessionKeyReportsNoVariant) {
  const auto values = GnupgKyberPkeskValues(SosX25519Point(), 1088);
  for (qsizetype cut = 0; cut < values.size(); cut += 7) {
    const auto packet = DecodeWire(V3Pkesk(8, values.left(cut)));
    EXPECT_EQ(Value(packet.overview, "Variant"), QString("<absent>")) << cut;
  }
  // A ciphertext length no ML-KEM parameter set produces.
  EXPECT_EQ(
      Value(DecodeWire(V3Pkesk(8, GnupgKyberPkeskValues(SosX25519Point(), 100)))
                .overview,
            "Variant"),
      QString("<absent>"));
  // Trailing octets past the wrapped key.
  EXPECT_EQ(Value(DecodeWire(V3Pkesk(8, values + QByteArray(1, 0))).overview,
                  "Variant"),
            QString("<absent>"));
}

TEST(PGPPacketDecoderTest, AGnupgKyberKeyNamesItsKemAndCurve) {
  const auto cv25519 = QByteArray::fromHex("2b060104019755010501");
  const auto x448 = QByteArray::fromHex("2b656f");
  for (const auto& [oid, len, variant] :
       {std::tuple{cv25519, 1184, "ML-KEM-768 + Curve25519"},
        std::tuple{x448, 1568, "ML-KEM-1024 + X448"}}) {
    const auto packet = DecodeWire(GnupgKyberKey(oid, len));
    EXPECT_EQ(Value(packet.overview, "Public Key Algorithm"),
              QString("Kyber (GnuPG, ML-KEM+ECC) (8)"));
    EXPECT_EQ(Value(packet.overview, "Variant"), QString(variant));
    EXPECT_EQ(Value(packet.overview, "Key Size"), QString("<absent>"));
  }
  EXPECT_EQ(Value(DecodeWire(GnupgKyberKey(cv25519, 1088)).overview, "Variant"),
            QString("<absent>"))
      << "a PKESK ciphertext length is not a public key length";
}

TEST(PGPPacketDecoderTest, AnRsaSessionKeyEstimatesTheKeySizeFromItsWidth) {
  // A 3072-bit modulus yields a ciphertext of up to 3072 bits; here its top
  // bit is clear, the common case, which must not round down.
  QByteArray mpi;
  mpi += static_cast<char>(3071 >> 8);
  mpi += static_cast<char>(3071 & 0xff);
  mpi += static_cast<char>(0x7f);
  mpi += QByteArray(383, 0x66);
  const auto packet = DecodeWire(V3Pkesk(1, mpi));
  EXPECT_EQ(Value(packet.overview, "Public Key Algorithm"), QString("RSA (1)"));
  EXPECT_EQ(Value(packet.overview, "Key Size"),
            QString("~3072 bits (from ciphertext width)"));
}

// -- names
// -----------------------------------------------------------------------

TEST(PGPPacketDecoderTest, NamesCarryTheirNumberAndNeverComeBackBlank) {
  EXPECT_EQ(PGPPublicKeyAlgorithmName(1), QString("RSA (1)"));
  EXPECT_EQ(PGPPublicKeyAlgorithmName(8),
            QString("Kyber (GnuPG, ML-KEM+ECC) (8)"));
  EXPECT_EQ(PGPPublicKeyAlgorithmName(37),
            QString("ML-KEM-768+ECDH-NIST-P-384 (37)"));
  EXPECT_EQ(PGPPublicKeyAlgorithmName(200), QString("Unknown (200)"));
  EXPECT_EQ(PGPCurveName(QByteArray::fromHex("2b656f")), QString("X448"));
  EXPECT_EQ(PGPCurveName(QByteArray::fromHex("2a8648ce3d030107")),
            QString("NIST P-256"));
  EXPECT_EQ(PGPCurveName(QByteArray::fromHex("2b6570")),
            QString("Unknown (1.3.101.112)"));
  EXPECT_EQ(PGPTagName(60), QString("Private/Experimental"));
  EXPECT_EQ(PGPTagName(42), QString("Unknown"));
}

TEST(PGPPacketDecoderTest, AnEphemeralPointGivesAwayItsCurve) {
  const auto point = [](quint8 prefix, int len) {
    QByteArray p(len, 0x55);
    p[0] = static_cast<char>(prefix);
    return p;
  };
  EXPECT_EQ(PGPEccPointCurve(point(0x40, 33)), QString("Curve25519"));
  EXPECT_EQ(PGPEccPointCurve(point(0x04, 65)),
            QString("NIST P-256 or brainpoolP256r1"));
  EXPECT_EQ(PGPEccPointCurve(point(0x04, 133)), QString("NIST P-521"));
  EXPECT_EQ(PGPEccPointCurve(point(0x00, 56)), QString("X448"));
  EXPECT_TRUE(PGPEccPointCurve(point(0x40, 57)).isEmpty());
  EXPECT_TRUE(PGPEccPointCurve({}).isEmpty());
}

// -- the dump against sq
// ------------------------------------------------------------

namespace {

/// The hex lines of each packet in an sq dump, as "offset  hex" with the
/// label cut off: sq's labels name some ranges differently, its octets and
/// their placement must match exactly.
auto SqColumns(const QString& name) -> QList<QStringList> {
  QList<QStringList> packets;
  const auto text = QString::fromUtf8(Load(name));
  // The offset and the 48 columns of hex; a line with no label is cut short.
  static const QRegularExpression kHexLine(R"(^\s*([0-9a-f]{8}  .{0,48}))");
  // A packet's dump opens with its CTB line. sq also prints small hex dumps
  // inside the field listing (a notation's value), which come before it.
  bool in_packet = false;
  for (const auto& line : text.split('\n')) {
    const auto match = kHexLine.match(line);
    if (!match.hasMatch()) {
      in_packet = false;
      continue;
    }
    if (line.trimmed().endsWith("CTB"_L1)) {
      packets << QStringList();
      in_packet = true;
    }
    // sq gives a field with no octets (an empty preference list) a bare
    // offset line; there is nothing on it to lay out, so the dump omits it.
    const auto columns = match.captured(1).trimmed();
    if (in_packet && columns.size() > 8) packets.last() << columns;
  }
  return packets;
}

auto OurColumns(const QString& dump) -> QStringList {
  QStringList out;
  for (const auto& line : dump.split('\n')) {
    out << line.left(58).trimmed();
  }
  return out;
}

}  // namespace

TEST(PGPPacketDecoderTest, TheDumpLaysOutOctetsExactlyAsSqDoes) {
  for (const auto& base :
       {"sig_good_detached", "enc_symmetric_v1", "key2", "rfc9580_a3_v6_cert",
        "enc_v2seipd_ocb", "rfc9580_a12_argon2_aes128"}) {
    const auto name = QString::fromLatin1(base);
    const auto ours = DecodeAll(name + ".pgp");
    const auto theirs = SqColumns(name + ".sq-hex.txt");
    ASSERT_EQ(ours.size(), theirs.size()) << base;
    for (qsizetype i = 0; i < ours.size(); ++i) {
      const auto& [wire, packet] = ours.at(i);
      const auto dump = PGPFormatHexDump(wire, packet.spans, wire.size());
      EXPECT_EQ(OurColumns(dump).join('\n').toStdString(),
                theirs.at(i).join('\n').toStdString())
          << base << " packet " << i;
    }
  }
}

TEST(PGPPacketDecoderTest, TheDumpUsesSqsLabelsForTheCommonFields) {
  const auto [wire, packet] = DecodeAll("sig_good_detached.pgp").at(0);
  const auto dump = PGPFormatHexDump(wire, packet.spans, wire.size());
  for (const auto& label :
       {"CTB", "length", "version", "type", "pk_algo", "hash_algo",
        "hashed_area_len", "subpacket length", "subpacket tag",
        "sig creation time", "notation name", "issuer fp", "unhashed_area_len",
        "digest_prefix1", "digest_prefix2", "eddsa_sig_r_len", "eddsa_sig_r"}) {
    EXPECT_TRUE(dump.contains(QString("   ") + label)) << label;
  }
  EXPECT_TRUE(dump.startsWith("00000000  c2")) << dump.toStdString();
}

TEST(PGPPacketDecoderTest, TheDumpLineWidthBoundsEveryLine) {
  for (const auto& base :
       {"sig_good_detached", "enc_symmetric_v1", "key2", "rfc9580_a3_v6_cert",
        "enc_v2seipd_ocb", "rfc9580_a12_argon2_aes128"}) {
    for (const auto& [wire, packet] :
         DecodeAll(QString::fromLatin1(base) + ".pgp")) {
      const auto width = PGPHexDumpLineWidth(packet.spans);
      for (const auto& line :
           PGPFormatHexDump(wire, packet.spans, wire.size()).split('\n')) {
        EXPECT_LE(line.size(), width) << base << ": " << line.toStdString();
      }
    }
  }
}

TEST(PGPPacketDecoderTest, TheDumpLineWidthIsTheLongestLabelledLine) {
  // Every range of a signature is labelled, so the longest label sets the
  // widest line exactly.
  const auto [wire, packet] = DecodeAll("sig_good_detached.pgp").at(0);
  qsizetype widest = 0;
  for (const auto& line :
       PGPFormatHexDump(wire, packet.spans, wire.size()).split('\n')) {
    widest = std::max(widest, line.size());
  }
  EXPECT_EQ(PGPHexDumpLineWidth(packet.spans), widest);
}

// -- key material: what format it is in
// ------------------------------------------

namespace {

auto Labels(const QList<PGPInspectField>& fields) -> QStringList {
  QStringList out;
  for (const auto& field : fields) out << field.label;
  return out;
}

auto FirstWithTag(const QList<Decoded>& packets, int tag, int nth = 0)
    -> PGPDecodedPacket {
  for (const auto& [wire, packet] : packets) {
    if (Tag(wire) == tag && nth-- == 0) return packet;
  }
  ADD_FAILURE() << "no packet with tag " << tag;
  return {};
}

/// A v4 public key packet: creation time zero, @p algorithm, @p params.
auto V4Key(int algorithm, const QByteArray& params) -> QByteArray {
  QByteArray body = QByteArray::fromHex("0400000000");
  body += static_cast<char>(algorithm);
  return Packet(6, body + params);
}

}  // namespace

TEST(PGPPacketDecoderTest, ANativeKeyShowsItsOctetsAndNothingItDoesNotHave) {
  // A v6 primary and a v4 primary, both RFC 9580 native Ed25519.
  for (const auto& key : {DecodeAll("rfc9580_a3_v6_cert.pgp").at(0).packet,
                          DecodeAll("key1.pgp").at(0).packet}) {
    EXPECT_EQ(key.material_summary,
              QString("Ed25519 · Native public key · 32 bytes"));
    const auto labels = Labels(key.material);
    EXPECT_TRUE(labels.contains("Public Key"))
        << labels.join(", ").toStdString();
    for (const auto* absent :
         {"Curve", "Public Point", "KDF Hash", "KEK Cipher"}) {
      EXPECT_FALSE(labels.contains(absent)) << absent;
    }
    EXPECT_EQ(Find(key.material, "Public Key")->detail, QString("32 bytes"));
  }
}

TEST(PGPPacketDecoderTest, NativeSubkeysAreDescribedTheSameWay) {
  EXPECT_EQ(DecodeAll("rfc9580_a3_v6_cert.pgp").at(2).packet.material_summary,
            QString("X25519 · Native public key · 32 bytes"));
  // key1's third secret subkey is a native Ed448 key.
  EXPECT_EQ(FirstWithTag(DecodeAll("key1.pgp"), 7, 1).material_summary,
            QString("Ed448 · Native public key · 57 bytes"));
}

TEST(PGPPacketDecoderTest, ALegacyEcdhKeyKeepsItsCurvePointAndKdf) {
  // A Curve25519 subkey (key1) and a NIST P-384 one (key2).
  const auto cv25519 = FirstWithTag(DecodeAll("key1.pgp"), 7);
  EXPECT_EQ(cv25519.material_summary,
            QString("ECDH · Curve25519 (legacy) · MPI-encoded point with KDF "
                    "parameters"));
  EXPECT_EQ(Labels(cv25519.material).mid(0, 4),
            (QStringList{"Curve", "Public Point", "KDF Hash", "KEK Cipher"}));
  EXPECT_EQ(Find(cv25519.material, "Curve")->detail,
            QString("OID 1.3.6.1.4.1.3029.1.5.1"));

  const auto p384 = FirstWithTag(DecodeAll("key2.pgp"), 14);
  EXPECT_EQ(
      p384.material_summary,
      QString("ECDH · NIST P-384 · MPI-encoded point with KDF parameters"));
}

TEST(PGPPacketDecoderTest, OtherLegacyFormatsFollowTheirOwnStructure) {
  EXPECT_EQ(DecodeAll("key2.pgp").at(0).packet.material_summary,
            QString("ECDSA · NIST P-384 · MPI-encoded point"));
  EXPECT_EQ(DecodeAll("rfc9580_a1_v4_cert.pgp").at(0).packet.material_summary,
            QString("EdDSA (legacy) · Ed25519 (legacy) · MPI-encoded point"));
  EXPECT_EQ(
      FirstWithTag(DecodeAll("brainpool_subkey_cert.pgp"), 14).material_summary,
      QString("ECDH · brainpoolP256r1 · MPI-encoded point with KDF "
              "parameters"));

  const auto elgamal = FirstWithTag(DecodeAll("elgamal_subkey_cert.pgp"), 14);
  EXPECT_TRUE(
      elgamal.material_summary.startsWith("Elgamal (Encrypt-Only) · MPI "
                                          "integers · "))
      << elgamal.material_summary.toStdString();
  EXPECT_EQ(Labels(elgamal.material),
            (QStringList{"Prime (p)", "Generator (g)", "Public Value (y)"}));

  // An RSA modulus is the one MPI whose bit length is the key size.
  QByteArray n = QByteArray::fromHex("0800") + QByteArray(256, 0x7f);
  n[2] = static_cast<char>(0xc1);
  const auto rsa = DecodeWire(V4Key(1, n + QByteArray::fromHex("0011010001")));
  EXPECT_TRUE(rsa.error.isEmpty()) << rsa.error.toStdString();
  EXPECT_EQ(rsa.material_summary,
            QString("RSA · MPI integers · 2048-bit modulus"));
}

TEST(PGPPacketDecoderTest, AnUnknownAlgorithmIsSaidToBeUndecoded) {
  const auto params = QByteArray(40, 0x5a);
  // v4: nothing says where the material ends, so none of it becomes a field.
  const auto v4 = DecodeWire(V4Key(99, params));
  EXPECT_TRUE(v4.material.isEmpty());
  EXPECT_TRUE(v4.material_summary.startsWith("Unknown (99) · Not decoded"))
      << v4.material_summary.toStdString();
  ExpectCovered(V4Key(99, params), v4, "unknown v4 key");

  // v6: the declared length bounds it, and the fingerprint still computes.
  QByteArray body = QByteArray::fromHex("0600000000") + static_cast<char>(99);
  body += Be32(params.size()) + params;
  const auto v6 = DecodeWire(Packet(6, body));
  EXPECT_TRUE(v6.material.isEmpty());
  EXPECT_TRUE(v6.material_summary.contains("Not decoded"));
  EXPECT_NE(Find(v6.overview, "Fingerprint"), nullptr);
}

TEST(PGPPacketDecoderTest, MalformedMaterialIsSaidToBeIncomplete) {
  const auto wire = Split(Load("key2.pgp")).at(3);  // the ECDH subkey
  ASSERT_EQ(Tag(wire), 14);
  const auto cut = DecodeWire(wire.left(40));
  EXPECT_FALSE(cut.error.isEmpty());
  EXPECT_TRUE(cut.material_summary.contains("Incomplete"))
      << cut.material_summary.toStdString();
}

TEST(PGPPacketDecoderTest, SignatureMaterialIsDescribedByItsFormat) {
  EXPECT_EQ(DecodeAll("sig_v6_detached.pgp").at(0).packet.material_summary,
            QString("Ed25519 · Native signature · 64 bytes"));

  // An unknown signature algorithm leaves its octets in the Hex tab rather
  // than listing them as a decoded value.
  auto wire = Split(Load("sig_good_detached.pgp")).at(0);
  wire[4] = static_cast<char>(99);  // the public-key algorithm octet
  const auto sig = DecodeWire(wire);
  EXPECT_TRUE(sig.material.isEmpty());
  EXPECT_TRUE(sig.material_summary.startsWith("Unknown (99) · Not decoded"));
}
