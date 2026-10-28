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

#include "PGPPacketDecoder.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QStringDecoder>
#include <QStringList>
#include <algorithm>
#include <optional>

#include "GFModuleTr.h"
#include "PGPPacketNames.h"

using namespace Qt::StringLiterals;

namespace {

// ---------------------------------------------------------------------------
// Value formatting
// ---------------------------------------------------------------------------

/// Upper-case hex without separators, the way sq prints fingerprints,
/// salts and digest prefixes.
auto Hex(const QByteArray& bytes) -> QString {
  return QString::fromLatin1(bytes.toHex()).toUpper();
}

/// Upper-case hex in groups of four, sixteen octets to a line, so a long
/// value reads as a block rather than one line running off the cell.
auto GroupedHex(const QByteArray& bytes) -> QString {
  constexpr qsizetype kGroup = 4;
  constexpr qsizetype kLine = 32;
  const auto hex = Hex(bytes);
  QString out;
  out.reserve(hex.size() + (hex.size() / kGroup));
  for (qsizetype i = 0; i < hex.size(); i += kGroup) {
    if (i > 0) out += (i % kLine == 0) ? '\n' : ' ';
    out += hex.mid(i, kGroup);
  }
  return out;
}

auto Time(quint32 seconds) -> QString {
  return QDateTime::fromSecsSinceEpoch(seconds).toUTC().toString(
      u"yyyy-MM-dd HH:mm:ss 'UTC'"_s);
}

auto Duration(quint32 seconds) -> QString {
  constexpr quint32 kDay = 24 * 60 * 60;
  if (seconds == 0) return u"never"_s;
  if (seconds < kDay) return u"%1 s"_s.arg(seconds);
  const auto days = seconds / kDay;
  const auto rest = seconds % kDay;
  auto text = days == 1 ? u"1 day"_s : u"%1 days"_s.arg(days);
  if (rest != 0) text += u" %1 s"_s.arg(rest);
  return text;
}

auto Size(qint64 bytes) -> QString {
  return bytes == 1 ? u"1 byte"_s : u"%1 bytes"_s.arg(bytes);
}

/// The octets as text when they are valid UTF-8, else as hex.
auto TextOrHex(const QByteArray& bytes) -> QString {
  auto decoder = QStringDecoder(QStringDecoder::Utf8);
  const QString text = decoder(bytes);
  if (decoder.hasError()) return Hex(bytes);
  return text;
}

auto YesNo(bool value) -> QString { return value ? u"yes"_s : u"no"_s; }

auto Field(const QString& label, const QString& value,
           QList<PGPInspectField> children = {}, QString detail = {})
    -> PGPInspectField {
  return {label, value, std::move(children), std::move(detail)};
}

/// The length annotation of a fixed-size value.
auto BytesDetail(qsizetype bytes) -> QString {
  return GTrC::tr("%1 bytes").arg(bytes);
}

/// The length annotation of an MPI or SOS. Its bit count is how the value
/// was encoded -- for a curve point that is not the key's strength -- so it
/// is spelled as an encoding, after the byte length.
auto MpiDetail(qsizetype bytes, int bits) -> QString {
  return GTrC::tr("%1 bytes · encoded as MPI (%2 bits)").arg(bytes).arg(bits);
}

// ---------------------------------------------------------------------------
// The reader
// ---------------------------------------------------------------------------

/**
 * A cursor over octets that records a labelled span for every read.
 *
 * Once a read runs short the reader stops for good -- every later read
 * returns nothing -- and Finish() covers whatever it did not consume with
 * one "unparsed" span. That is the whole failure model: a decode can stop
 * early, but it cannot leave an octet unaccounted for or read past the end.
 */
class Reader {
 public:
  Reader(QByteArray data, QList<PGPHexSpan>* spans, QString* error,
         qint64 base = 0, qint64 declared = -1, bool input_capped = false)
      : data_(std::move(data)),
        spans_(spans),
        error_(error),
        base_(base),
        declared_(declared),
        input_capped_(input_capped) {}

  [[nodiscard]] auto Failed() const -> bool { return failed_; }
  [[nodiscard]] auto Remaining() const -> qint64 { return data_.size() - pos_; }

  /// What is left of the body on the wire, which is more than Remaining()
  /// when the walk capped the octets it handed over.
  [[nodiscard]] auto RemainingOnWire() const -> qint64 {
    return declared_ >= 0 ? std::max<qint64>(declared_ - pos_, Remaining())
                          : Remaining();
  }

  auto Take(qint64 count, const QString& label, bool redacted = false)
      -> std::optional<QByteArray> {
    if (failed_) return std::nullopt;
    if (count < 0 || count > Remaining()) {
      Fail(input_capped_ && count <= RemainingOnWire()
               ? u"the rest of the packet was not handed over"_s
               : u"%1 needs %2, only %3 remain"_s.arg(label, Size(count),
                                                      Size(Remaining())));
      return std::nullopt;
    }
    auto bytes = data_.mid(pos_, count);
    Mark(count, label, redacted);
    return bytes;
  }

  auto Octet(const QString& label) -> std::optional<int> {
    const auto bytes = Take(1, label);
    if (!bytes) return std::nullopt;
    return static_cast<quint8>(bytes->at(0));
  }

  auto Uint(int width, const QString& label) -> std::optional<quint32> {
    const auto bytes = Take(width, label);
    if (!bytes) return std::nullopt;
    quint32 value = 0;
    for (const auto byte : *bytes)
      value = (value << 8) | static_cast<quint8>(byte);
    return value;
  }

  /// An MPI or SOS: a two-octet bit count, then that many bits in whole
  /// octets. The spans are named the way sq names them: `<name>_len`, then
  /// `<name>`.
  auto Mpi(const QString& name, int* bits = nullptr)
      -> std::optional<QByteArray> {
    const auto count = Uint(2, name + "_len"_L1);
    if (!count) return std::nullopt;
    if (bits != nullptr) *bits = static_cast<int>(*count);
    return Take((*count + 7) / 8, name);
  }

  /// Everything that is left, which is no failure even when the walk
  /// capped the packet.
  auto Rest(const QString& label, bool redacted = false) -> QByteArray {
    if (failed_) return {};
    return *Take(Remaining(), label, redacted);
  }

  /// A reader over the next @p count octets, which this one then skips. A
  /// failure inside it stops only it: the length prefix it came from still
  /// says where the next field starts.
  auto Sub(qint64 count, const QString& label) -> std::optional<Reader> {
    if (failed_) return std::nullopt;
    if (count < 0 || count > Remaining()) {
      Fail(input_capped_ && count <= RemainingOnWire()
               ? u"the rest of the packet was not handed over"_s
               : u"%1 claims %2, only %3 remain"_s.arg(label, Size(count),
                                                       Size(Remaining())));
      return std::nullopt;
    }
    Reader sub(data_.mid(pos_, count), spans_, error_, base_ + pos_, -1,
               input_capped_);
    pos_ += count;
    return sub;
  }

  void Fail(const QString& why) {
    if (failed_) return;
    failed_ = true;
    if (error_->isEmpty()) *error_ = why;
  }

  /// Cover the unconsumed octets. With @p expect_end, leftovers after a
  /// clean decode are themselves the finding.
  void Finish(bool expect_end = true) {
    const auto left = Remaining();
    if (left == 0) return;
    if (!failed_ && expect_end) {
      Fail(u"%1 more than the packet's fields account for"_s.arg(Size(left)));
    }
    Mark(left, u"unparsed"_s, false);
  }

 private:
  void Mark(qint64 count, const QString& label, bool redacted) {
    if (count > 0) spans_->push_back({base_ + pos_, count, label, redacted});
    pos_ += count;
  }

  QByteArray data_;
  qint64 pos_ = 0;
  QList<PGPHexSpan>* spans_;
  QString* error_;
  qint64 base_;
  qint64 declared_;
  bool input_capped_;
  bool failed_ = false;
};

// ---------------------------------------------------------------------------
// Framing
// ---------------------------------------------------------------------------

/// Where a run of body octets sits on the wire.
struct Segment {
  qint64 body_offset;
  qint64 wire_offset;
  qint64 length;
};

struct Framing {
  int tag = -1;
  QByteArray body;
  /// The body's length on the wire when the header says it; -1 otherwise.
  qint64 body_declared = -1;
  QList<Segment> segments;
  QList<PGPHexSpan> spans;
  QString error;
};

/// A new-format length: 1, 2 or 5 octets, or a partial-length chunk.
struct NewLength {
  qint64 length = 0;
  int octets = 0;
  bool partial = false;
};

auto ReadNewLength(const QByteArray& wire, qint64 at)
    -> std::optional<NewLength> {
  const auto octet = [&wire](qint64 i) {
    return static_cast<quint8>(wire.at(i));
  };
  if (at >= wire.size()) return std::nullopt;
  const auto first = octet(at);
  if (first < 192) return NewLength{first, 1, false};
  if (first < 224) {
    if (at + 1 >= wire.size()) return std::nullopt;
    return NewLength{((first - 192) << 8) + octet(at + 1) + 192, 2, false};
  }
  if (first < 255) return NewLength{qint64{1} << (first & 0x1f), 1, true};
  if (at + 4 >= wire.size()) return std::nullopt;
  const qint64 length = (qint64{octet(at + 1)} << 24) |
                        (qint64{octet(at + 2)} << 16) |
                        (qint64{octet(at + 3)} << 8) | octet(at + 4);
  return NewLength{length, 5, false};
}

auto ReadFraming(const QByteArray& wire, qint64 wire_length) -> Framing {
  Framing f;
  if (wire.isEmpty()) {
    f.error = u"no octets were handed over"_s;
    return f;
  }

  const auto ctb = static_cast<quint8>(wire.at(0));
  if ((ctb & 0x80) == 0) {
    f.error = u"the first octet is not a packet header"_s;
    return f;
  }
  f.spans.push_back({0, 1, u"CTB"_s, false});

  const auto add_body = [&f, &wire](qint64 at, qint64 length) {
    const auto available = std::clamp<qint64>(wire.size() - at, 0, length);
    if (available == 0) return;
    f.segments.push_back({f.body.size(), at, available});
    f.body += wire.mid(at, available);
  };

  if ((ctb & 0x40) == 0) {
    // Old format: the tag in bits 5-2, the length type in bits 1-0.
    f.tag = (ctb >> 2) & 0x0f;
    const auto type = ctb & 0x03;
    if (type == 3) {
      f.body_declared = wire_length - 1;
      add_body(1, wire.size() - 1);
      return f;
    }
    const int octets = type == 0 ? 1 : type == 1 ? 2 : 4;
    if (wire.size() < 1 + octets) {
      f.error = u"the length runs past the end"_s;
      return f;
    }
    qint64 length = 0;
    for (int i = 1; i <= octets; ++i) {
      length = (length << 8) | static_cast<quint8>(wire.at(i));
    }
    f.spans.push_back({1, octets, u"length"_s, false});
    f.body_declared = length;
    add_body(1 + octets, length);
    return f;
  }

  // New format: the tag in bits 5-0, then one or more lengths.
  f.tag = ctb & 0x3f;
  qint64 at = 1;
  bool partial = false;
  while (true) {
    const auto length = ReadNewLength(wire, at);
    if (!length) {
      if (at < wire_length && at >= wire.size()) break;  // capped, not broken
      f.error = u"the length runs past the end"_s;
      return f;
    }
    f.spans.push_back({at, length->octets, u"length"_s, false});
    at += length->octets;
    add_body(at, length->length);
    at += length->length;
    partial |= length->partial;
    if (!length->partial) {
      if (!partial) f.body_declared = length->length;
      break;
    }
    if (at >= wire.size()) break;
  }
  return f;
}

/// Body spans placed on the wire, split wherever a chunk boundary cuts them.
auto MapToWire(const QList<PGPHexSpan>& body_spans,
               const QList<Segment>& segments) -> QList<PGPHexSpan> {
  QList<PGPHexSpan> out;
  for (const auto& span : body_spans) {
    auto start = span.offset;
    const auto end = span.offset + span.length;
    auto label = span.label;
    for (const auto& segment : segments) {
      const auto segment_end = segment.body_offset + segment.length;
      if (start >= segment_end || start >= end) continue;
      const auto piece_end = std::min(end, segment_end);
      out.push_back({segment.wire_offset + (start - segment.body_offset),
                     piece_end - start, label, span.redacted});
      // A piece continuing past a chunk boundary is the same field.
      label = span.label.isEmpty() ? QString() : u"(continued)"_s;
      start = piece_end;
    }
  }
  return out;
}

// ---------------------------------------------------------------------------
// Shared parts
// ---------------------------------------------------------------------------

/// A string-to-key specifier, as a group of fields.
auto ReadS2K(Reader& r) -> PGPInspectField {
  auto group = Field(u"S2K"_s, {});
  const auto type = r.Octet(u"s2k_type"_s);
  if (!type) return group;
  group.value = PGPS2KTypeName(*type);

  const auto hash = [&r, &group] {
    if (const auto id = r.Octet(u"s2k_hash_algo"_s)) {
      group.children << Field(u"Hash"_s, PGPHashAlgorithmName(*id));
      group.value += u", "_s + PGPHashAlgorithmName(*id);
    }
  };
  const auto salt = [&r, &group](int size) {
    if (const auto bytes = r.Take(size, u"s2k_salt"_s)) {
      group.children << Field(u"Salt"_s, Hex(*bytes));
    }
  };

  switch (*type) {
    case 0:
      hash();
      break;
    case 1:
      hash();
      salt(8);
      break;
    case 3:
      hash();
      salt(8);
      if (const auto c = r.Octet(u"s2k_count"_s)) {
        // RFC 9580 §3.7.1.3: the octet count of input hashed.
        const auto bytes = qint64{16 + (*c & 15)} << ((*c >> 4) + 6);
        group.children << Field(u"Hash Bytes"_s, QString::number(bytes));
      }
      break;
    case 4: {
      salt(16);
      const auto t = r.Octet(u"argon2_t"_s);
      const auto p = r.Octet(u"argon2_p"_s);
      const auto m = r.Octet(u"argon2_m"_s);
      if (t && p && m) {
        group.children << Field(u"Passes"_s, QString::number(*t))
                       << Field(u"Parallelism"_s, QString::number(*p))
                       << Field(u"Memory"_s,
                                u"2^%1 KiB (%2 MiB)"_s.arg(*m).arg(
                                    *m >= 10 ? qint64{1} << (*m - 10) : 0));
      }
      break;
    }
    case 101: {
      hash();
      const auto magic = r.Take(3, u"gnu_magic"_s);
      const auto mode = r.Octet(u"gnu_mode"_s);
      if (!magic || !mode) break;
      if (*mode == 1) {
        group.children << Field(u"Mode"_s, u"no secret key (gnu-dummy)"_s);
      } else if (*mode == 2) {
        group.children << Field(u"Mode"_s, u"on a smartcard"_s);
        if (const auto len = r.Octet(u"serial_len"_s)) {
          if (const auto serial = r.Take(*len, u"serial"_s)) {
            group.children << Field(u"Card Serial"_s, Hex(*serial));
          }
        }
      } else {
        group.children << Field(u"Mode"_s, QString::number(*mode));
      }
      break;
    }
    default:
      r.Fail(u"unknown S2K type %1"_s.arg(*type));
  }
  return group;
}

/// Raw fixed-size values: each read and listed under its display name.
struct Value {
  const char* span;
  const char* label;
  int size;  // octets; 0 for an MPI
};

void ReadValues(Reader& r, std::initializer_list<Value> values,
                QList<PGPInspectField>& out) {
  for (const auto& v : values) {
    const auto span = QString::fromLatin1(v.span);
    const auto label = QString::fromLatin1(v.label);
    if (v.size == 0) {
      int bits = 0;
      if (const auto mpi = r.Mpi(span, &bits)) {
        out << Field(label, GroupedHex(*mpi), {}, MpiDetail(mpi->size(), bits));
      }
    } else if (const auto bytes = r.Take(v.size, span)) {
      out << Field(label, GroupedHex(*bytes), {}, BytesDetail(bytes->size()));
    }
  }
}

// ---------------------------------------------------------------------------
// Keys
// ---------------------------------------------------------------------------

struct PublicParams {
  int bits = 0;
  QString curve;
  QString variant;
  /// What format the material is in; see PGPDecodedPacket::material_summary.
  QString summary;
  /// Whether the parameters' end could be found, which a secret key needs
  /// to know where its secret part starts.
  bool understood = true;
};

/// "A · B · C", leaving out the empty parts.
auto Joined(const QStringList& parts) -> QString {
  QStringList shown;
  for (const auto& part : parts) {
    if (!part.isEmpty()) shown << part;
  }
  return shown.join(u" · "_s);
}

/// The algorithm-specific public part of a key (RFC 9580 §5.5.5).
///
/// Every field is listed as the format has it: native keys are fixed-size
/// octet strings with no curve OID, MPI or KDF; legacy ECC keys carry a curve
/// OID and an MPI-encoded point, and ECDH also its KDF parameters. The
/// summary is built from the algorithm octet, a parsed OID and the lengths
/// actually read -- never from a length alone.
auto ReadPublicParams(Reader& r, int algorithm,
                      QList<PGPInspectField>& material) -> PublicParams {
  PublicParams p;
  const auto name = PGPPublicKeyAlgorithmShortName(algorithm);
  const auto mpi = [&r, &material](const char* span, const char* label) {
    int bits = 0;
    const auto value = r.Mpi(QString::fromLatin1(span), &bits);
    if (value) {
      material << Field(QString::fromLatin1(label), GroupedHex(*value), {},
                        MpiDetail(value->size(), bits));
    }
    return value ? bits : 0;
  };
  const auto curve = [&r, &p, &material] {
    const auto len = r.Octet(u"curve_len"_s);
    if (!len) return;
    const auto oid = r.Take(*len, u"curve"_s);
    if (!oid) return;
    p.curve = PGPCurveName(*oid);
    p.bits = PGPCurveBits(*oid);
    material << Field(u"Curve"_s, p.curve, {},
                      GTrC::tr("OID %1").arg(PGPDottedOid(*oid)));
  };
  // A native key: fixed-size octet strings, read and summed as they are.
  const auto native = [&r, &material,
                       &name](std::initializer_list<Value> parts) {
    QStringList sizes;
    ReadValues(r, parts, material);
    for (const auto& part : parts) sizes << QString::number(part.size);
    return Joined({name,
                   parts.size() > 1 ? GTrC::tr("Native composite key")
                                    : GTrC::tr("Native public key"),
                   GTrC::tr("%1 bytes").arg(sizes.join(u" + "_s))});
  };

  switch (algorithm) {
    case 1:
    case 2:
    case 3:
      // An RSA modulus's bit length is the key size.
      p.bits = mpi("rsa_n", "Modulus (n)");
      mpi("rsa_e", "Exponent (e)");
      p.summary = Joined({name, GTrC::tr("MPI integers"),
                          GTrC::tr("%1-bit modulus").arg(p.bits)});
      break;
    case 17:
      p.bits = mpi("dsa_p", "Prime (p)");
      mpi("dsa_q", "Group Order (q)");
      mpi("dsa_g", "Generator (g)");
      mpi("dsa_y", "Public Value (y)");
      p.summary = Joined({name, GTrC::tr("MPI integers"),
                          GTrC::tr("%1-bit prime").arg(p.bits)});
      break;
    case 16:
    case 20:
      p.bits = mpi("elgamal_p", "Prime (p)");
      mpi("elgamal_g", "Generator (g)");
      mpi("elgamal_y", "Public Value (y)");
      p.summary = Joined({name, GTrC::tr("MPI integers"),
                          GTrC::tr("%1-bit prime").arg(p.bits)});
      break;
    case 19:
    case 22:
      curve();
      mpi(algorithm == 19 ? "ecdsa_public" : "eddsa_public", "Public Point");
      p.summary = Joined({name, p.curve, GTrC::tr("MPI-encoded point")});
      break;
    case 18: {
      curve();
      mpi("ecdh_public", "Public Point");
      p.summary = Joined(
          {name, p.curve, GTrC::tr("MPI-encoded point with KDF parameters")});
      // The KDF parameters: a length, a reserved octet, then the hash and
      // the key-wrapping cipher.
      const auto len = r.Octet(u"kdf_len"_s);
      if (!len) break;
      auto kdf = r.Sub(*len, u"kdf"_s);
      if (!kdf) break;
      kdf->Octet(u"kdf_reserved"_s);
      if (const auto hash = kdf->Octet(u"kdf_hash"_s)) {
        material << Field(u"KDF Hash"_s, PGPHashAlgorithmName(*hash));
      }
      if (const auto sym = kdf->Octet(u"kek_symm"_s)) {
        material << Field(u"KEK Cipher"_s, PGPSymmetricAlgorithmName(*sym));
      }
      kdf->Finish(false);
      break;
    }
    case 25:
      p.bits = 256;
      p.summary = native({{"x25519_public", "Public Key", 32}});
      break;
    case 26:
      p.bits = 448;
      p.summary = native({{"x448_public", "Public Key", 56}});
      break;
    case 27:
      p.bits = 256;
      p.summary = native({{"ed25519_public", "Public Key", 32}});
      break;
    case 28:
      p.bits = 448;
      p.summary = native({{"ed448_public", "Public Key", 57}});
      break;
    case 30:
      p.summary = native({{"ed25519_public", "Ed25519 Public Key", 32},
                          {"mldsa65_public", "ML-DSA-65 Public Key", 1952}});
      break;
    case 31:
      p.summary = native({{"ed448_public", "Ed448 Public Key", 57},
                          {"mldsa87_public", "ML-DSA-87 Public Key", 2592}});
      break;
    case 32:
    case 33:
      p.summary = native({{"slhdsa_public", "SLH-DSA Public Key", 32}});
      break;
    case 34:
      p.summary = native({{"slhdsa_public", "SLH-DSA Public Key", 64}});
      break;
    case 35:
      p.summary = native({{"x25519_public", "X25519 Public Key", 32},
                          {"mlkem768_public", "ML-KEM-768 Public Key", 1184}});
      break;
    case 36:
      p.summary =
          native({{"x448_public", "X448 Public Key", 56},
                  {"mlkem1024_public", "ML-KEM-1024 Public Key", 1568}});
      break;
    case 8: {
      // GnuPG's Kyber: the ECC curve OID, the ECC point as an SOS, then
      // the ML-KEM key behind a four-octet length.
      curve();
      const auto oid_curve = p.curve;
      p.bits = 0;
      mpi("ecc_public", "ECC Public Point");
      const auto len = r.Uint(4, u"kyber_public_len"_s);
      const auto key = len ? r.Take(*len, u"kyber_public"_s) : std::nullopt;
      p.summary = Joined(
          {name, oid_curve,
           key ? GTrC::tr("ECC point and %1-byte ML-KEM key").arg(key->size())
               : QString()});
      if (!key) break;
      material << Field(u"ML-KEM Public Key"_s, GroupedHex(*key), {},
                        BytesDetail(key->size()));
      // GnuPG's layout names no ML-KEM parameter set; the key's length is
      // what tells them apart.
      const char* kem = key->size() == 1184   ? "ML-KEM-768"
                        : key->size() == 1568 ? "ML-KEM-1024"
                                              : nullptr;
      if (kem != nullptr) {
        p.variant = u"%1 + %2"_s.arg(QLatin1StringView(kem),
                                     oid_curve.startsWith("Curve25519"_L1)
                                         ? u"Curve25519"_s
                                         : oid_curve);
      }
      p.curve.clear();
      break;
    }
    default:
      p.understood = false;
      p.summary = Joined(
          {PGPPublicKeyAlgorithmName(algorithm),
           GTrC::tr("Not decoded: the inspector does not know this "
                    "algorithm's format; its octets are in the Hex tab")});
  }
  if (r.Failed() && p.understood) {
    p.summary = Joined(
        {p.summary, GTrC::tr("Incomplete: parsing stopped inside the key "
                             "material, see the Overview tab")});
  }
  return p;
}

/// A key's fingerprint from its public part, by version (RFC 9580 §5.5.4).
auto Fingerprint(int version, const QByteArray& public_part) -> QByteArray {
  QByteArray prefix;
  const auto length = public_part.size();
  if (version == 4) {
    prefix += static_cast<char>(0x99);
    prefix += static_cast<char>((length >> 8) & 0xff);
    prefix += static_cast<char>(length & 0xff);
  } else {
    prefix += static_cast<char>(version == 6 ? 0x9b : 0x9a);
    for (int shift = 24; shift >= 0; shift -= 8) {
      prefix += static_cast<char>((length >> shift) & 0xff);
    }
  }
  const auto algorithm =
      version == 4 ? QCryptographicHash::Sha1 : QCryptographicHash::Sha256;
  return QCryptographicHash::hash(prefix + public_part, algorithm);
}

/// The protection of a secret key, then its secret part as one redacted
/// range. Nothing past the protection parameters is ever read as a value.
auto ReadSecretPart(Reader& r, int version) -> PGPInspectField {
  auto group = Field(u"Secret Key"_s, {});
  const auto usage = r.Octet(u"s2k_usage"_s);
  if (!usage) return group;

  // v6 (and LibrePGP v5) count the octets of the protection fields.
  if (*usage != 0 && version >= 5) r.Octet(u"s2k_params_len"_s);

  const auto s2k = [&r, &group, version] {
    if (version == 6) r.Octet(u"s2k_len"_s);
    group.children << ReadS2K(r);
  };

  switch (*usage) {
    case 0:
      group.value = u"Unprotected"_s;
      break;
    case 253: {
      group.value = u"Encrypted (AEAD)"_s;
      const auto sym = r.Octet(u"sym_algo"_s);
      const auto aead = r.Octet(u"aead_algo"_s);
      if (!sym || !aead) break;
      group.children << Field(u"Cipher"_s, PGPSymmetricAlgorithmName(*sym))
                     << Field(u"AEAD Mode"_s, PGPAeadAlgorithmName(*aead));
      s2k();
      if (const auto iv = r.Take(PGPAeadNonceSize(*aead), u"aead_iv"_s)) {
        group.children << Field(u"Nonce"_s, Hex(*iv));
      }
      break;
    }
    case 254:
    case 255: {
      group.value = *usage == 254 ? u"Encrypted (SHA-1 checksum)"_s
                                  : u"Encrypted (two-octet checksum)"_s;
      const auto sym = r.Octet(u"sym_algo"_s);
      if (!sym) break;
      group.children << Field(u"Cipher"_s, PGPSymmetricAlgorithmName(*sym));
      s2k();
      if (r.Failed()) break;
      if (const auto iv = r.Take(PGPSymmetricBlockSize(*sym), u"iv"_s)) {
        group.children << Field(u"IV"_s, Hex(*iv));
      }
      break;
    }
    default:
      // A legacy key: the octet is the cipher itself, with an MD5 S2K.
      group.value = u"Encrypted (legacy, MD5 key derivation)"_s;
      group.children << Field(u"Cipher"_s, PGPSymmetricAlgorithmName(*usage));
      if (const auto iv = r.Take(PGPSymmetricBlockSize(*usage), u"iv"_s)) {
        group.children << Field(u"IV"_s, Hex(*iv));
      }
  }

  const auto secret = r.Rest(
      *usage == 0 ? u"secret key material"_s : u"encrypted_mpis"_s, true);
  group.children << Field(u"Secret Material"_s,
                          u"%1, not shown"_s.arg(Size(secret.size())));
  return group;
}

void DecodeKey(Reader& r, const QByteArray& body, bool secret,
               PGPDecodedPacket& out) {
  const auto version = r.Octet(u"version"_s);
  if (!version) return;
  out.version = *version;
  out.overview << Field(u"Version"_s, QString::number(*version));

  if (*version < 2 || *version > 6) {
    r.Fail(u"unknown key version %1"_s.arg(*version));
    return;
  }

  const auto created = r.Uint(4, u"creation_time"_s);
  if (!created) return;
  out.overview << Field(u"Created"_s, Time(*created));

  if (*version <= 3) {
    if (const auto days = r.Uint(2, u"expiration_days"_s)) {
      out.overview << Field(u"Expires After"_s,
                            *days == 0 ? u"never"_s : u"%1 days"_s.arg(*days));
    }
  }

  const auto algorithm = r.Octet(u"pk_algo"_s);
  if (!algorithm) return;
  out.overview << Field(u"Public Key Algorithm"_s,
                        PGPPublicKeyAlgorithmName(*algorithm));

  // v5 and v6 keys say how long their public parameters are.
  std::optional<Reader> scoped;
  Reader* params_reader = &r;
  if (*version >= 5) {
    const auto len = r.Uint(4, u"public_len"_s);
    if (!len) return;
    scoped = r.Sub(*len, u"key material"_s);
    if (!scoped) return;
    params_reader = &*scoped;
  }

  const auto params =
      ReadPublicParams(*params_reader, *algorithm, out.material);
  out.material_summary = params.summary;
  if (scoped) {
    // An algorithm we cannot read is still bounded by its declared length.
    if (!params.understood) scoped->Rest(u"public key material"_s);
    scoped->Finish();
  }

  if (params.bits > 0) {
    out.overview << Field(u"Key Size"_s, u"%1 bits"_s.arg(params.bits));
  }
  if (!params.curve.isEmpty()) out.overview << Field(u"Curve"_s, params.curve);
  if (!params.variant.isEmpty()) {
    out.overview << Field(u"Variant"_s, params.variant);
  }

  QString key_id;
  const auto public_complete = !r.Failed() && !params_reader->Failed() &&
                               (params.understood || *version >= 5) &&
                               *version >= 4;
  if (public_complete) {
    const auto public_end = body.size() - r.Remaining();
    const auto fingerprint = Fingerprint(*version, body.left(public_end));
    key_id = Hex(*version == 4 ? fingerprint.right(8) : fingerprint.left(8));
    out.overview << Field(u"Fingerprint"_s, Hex(fingerprint))
                 << Field(u"Key ID"_s, key_id);
  }

  out.headline = PGPPublicKeyAlgorithmShortName(*algorithm);
  if (!key_id.isEmpty()) out.headline += u", "_s + key_id;

  if (!secret) {
    if (!params.understood && *version < 5) {
      r.Rest(u"public key material"_s);
    }
    return;
  }

  if (!params.understood && *version < 5) {
    // Without the public part's end there is no telling where the secret
    // part starts, so all of it is treated as secret.
    r.Rest(u"key material"_s, true);
    out.material << Field(
        u"Secret Key"_s,
        u"the algorithm is unknown, so its octets are not shown"_s);
    return;
  }
  out.material << ReadSecretPart(r, *version);
}

// ---------------------------------------------------------------------------
// Signatures
// ---------------------------------------------------------------------------

struct SignatureParts {
  int version = -1;
  QString headline;
  QList<PGPInspectField> overview;
  QList<PGPInspectField> subpackets;
  QList<PGPInspectField> material;
  QString material_summary;
};

/// How deep embedded signatures are decoded. A real one holds one level;
/// the cap is what keeps a crafted chain from exhausting the stack.
constexpr int kMaxSignatureDepth = 4;

void ReadSignature(Reader& r, SignatureParts& sig, int depth);

/// What the overview lifts out of the subpacket areas.
struct SubpacketFacts {
  std::optional<quint32> created;
  std::optional<quint32> expires_after;
  QString issuer_fingerprint;
  QString issuer_key_id;
};

auto AlgorithmList(const QByteArray& ids, QString (*name)(int)) -> QString {
  QStringList names;
  for (const auto id : ids) names << name(static_cast<quint8>(id));
  return names.isEmpty() ? u"none"_s : names.join(", "_L1);
}

/// One subpacket's value, read from @p v (exactly the value's octets).
auto ReadSubpacketValue(Reader& v, int type, SubpacketFacts& facts, int depth)
    -> PGPInspectField {
  auto field = Field(PGPSignatureSubpacketName(type), {});
  const auto whole = [&v](const char* label) {
    return v.Rest(QString::fromLatin1(label));
  };

  switch (type) {
    case 2:
      if (const auto t = v.Uint(4, u"sig creation time"_s)) {
        field.value = Time(*t);
        facts.created = *t;
      }
      break;
    case 3:
      if (const auto t = v.Uint(4, u"sig expiration time"_s)) {
        field.value = Duration(*t);
        facts.expires_after = *t;
      }
      break;
    case 9:
      if (const auto t = v.Uint(4, u"key expiration time"_s)) {
        field.value =
            *t == 0 ? u"never"_s : Duration(*t) + u" after key creation"_s;
      }
      break;
    case 4:
    case 7:
    case 25:
      if (const auto b = v.Octet(type == 25  ? u"primary user id"_s
                                 : type == 4 ? u"exportable"_s
                                             : u"revocable"_s)) {
        field.value = YesNo(*b != 0);
      }
      break;
    case 5: {
      const auto level = v.Octet(u"trust level"_s);
      const auto amount = v.Octet(u"trust amount"_s);
      if (level && amount) {
        field.value = u"level %1, amount %2"_s.arg(*level).arg(*amount);
      }
      break;
    }
    case 6:
    case 24:
    case 26:
    case 28: {
      auto text = whole(type == 28 ? "signers user id" : "value");
      if (type == 6 && text.endsWith('\0')) text.chop(1);
      field.value = TextOrHex(text);
      break;
    }
    case 11:
      field.value = AlgorithmList(whole("symmetric algorithms"),
                                  &PGPSymmetricAlgorithmName);
      break;
    case 21:
      field.value =
          AlgorithmList(whole("hash algorithms"), &PGPHashAlgorithmName);
      break;
    case 22:
      field.value = AlgorithmList(whole("compression algorithms"),
                                  &PGPCompressionAlgorithmName);
      break;
    case 34:
      field.value =
          AlgorithmList(whole("aead algorithms"), &PGPAeadAlgorithmName);
      break;
    case 39: {
      const auto pairs = whole("aead ciphersuites");
      QStringList names;
      for (qsizetype i = 0; i + 1 < pairs.size(); i += 2) {
        names << PGPSymmetricAlgorithmName(static_cast<quint8>(pairs.at(i))) +
                     '/' +
                     PGPAeadAlgorithmName(static_cast<quint8>(pairs.at(i + 1)));
      }
      field.value = names.isEmpty() ? u"none"_s : names.join(", "_L1);
      break;
    }
    case 23: {
      const auto flags = whole("keyserver preferences");
      const auto no_modify =
          !flags.isEmpty() && (static_cast<quint8>(flags.at(0)) & 0x80) != 0;
      field.value = no_modify ? u"no-modify"_s : Hex(flags);
      break;
    }
    case 27:
      field.value = PGPKeyFlagsText(whole("key flags"));
      break;
    case 30:
      field.value = PGPFeaturesText(whole("features"));
      break;
    case 16:
      if (const auto id = v.Take(8, u"issuer"_s)) {
        field.value = Hex(*id);
        facts.issuer_key_id = field.value;
      }
      break;
    case 33:
    case 35: {
      const auto is_issuer = type == 33;
      v.Octet(is_issuer ? u"issuer fp version"_s : u"recipient fp version"_s);
      field.value = Hex(whole(is_issuer ? "issuer fp" : "intended recipient"));
      if (is_issuer) facts.issuer_fingerprint = field.value;
      break;
    }
    case 12: {
      const auto klass = v.Octet(u"class"_s);
      const auto algorithm = v.Octet(u"pk_algo"_s);
      const auto fingerprint = whole("fingerprint");
      if (klass && algorithm) {
        field.value = Hex(fingerprint);
        field.children << Field(u"Class"_s,
                                u"0x%1"_s.arg(*klass, 2, 16, QChar('0')))
                       << Field(u"Public Key Algorithm"_s,
                                PGPPublicKeyAlgorithmName(*algorithm));
      }
      break;
    }
    case 29: {
      const auto code = v.Octet(u"reason"_s);
      const auto text = TextOrHex(whole("message"));
      if (code) {
        field.value = PGPRevocationReasonName(*code);
        if (!text.isEmpty()) field.value += u": "_s + text;
      }
      break;
    }
    case 20: {
      const auto flags = v.Take(4, u"flags"_s);
      const auto name_len = v.Uint(2, u"name len"_s);
      const auto value_len = v.Uint(2, u"value len"_s);
      if (!flags || !name_len || !value_len) break;
      const auto name = v.Take(*name_len, u"notation name"_s);
      const auto value = v.Take(*value_len, u"notation value"_s);
      if (!name || !value) break;
      const auto human = (static_cast<quint8>(flags->at(0)) & 0x80) != 0;
      const auto shown = human ? TextOrHex(*value) : Hex(*value);
      field.value = u"%1 = %2"_s.arg(QString::fromUtf8(*name), shown);
      field.children << Field(u"Name"_s, QString::fromUtf8(*name))
                     << Field(u"Value"_s, shown)
                     << Field(u"Human Readable"_s, YesNo(human));
      break;
    }
    case 31: {
      const auto algorithm = v.Octet(u"pk_algo"_s);
      const auto hash = v.Octet(u"hash_algo"_s);
      const auto digest = whole("hash");
      if (algorithm && hash) {
        field.value = Hex(digest);
        field.children << Field(u"Public Key Algorithm"_s,
                                PGPPublicKeyAlgorithmName(*algorithm))
                       << Field(u"Hash Algorithm"_s,
                                PGPHashAlgorithmName(*hash));
      }
      break;
    }
    case 32: {
      // One range in the dump, as in sq; a full signature in the fields.
      const auto bytes = whole("embedded sig");
      if (depth + 1 >= kMaxSignatureDepth) {
        field.value = u"nested too deeply, not decoded"_s;
        break;
      }
      QList<PGPHexSpan> scratch;
      QString error;
      Reader inner(bytes, &scratch, &error);
      SignatureParts embedded;
      ReadSignature(inner, embedded, depth + 1);
      inner.Finish();
      field.value = embedded.headline;
      field.children << embedded.overview << embedded.subpackets;
      if (!error.isEmpty()) field.children << Field(u"Problem"_s, error);
      break;
    }
    default:
      field.value = Hex(whole("value"));
  }
  v.Finish();
  return field;
}

/// A subpacket area: each subpacket's length, tag and value.
auto ReadSubpacketArea(Reader& area, const QString& name, SubpacketFacts& facts,
                       int depth) -> PGPInspectField {
  auto group = Field(name, {});
  while (area.Remaining() > 0 && !area.Failed()) {
    const auto first = area.Take(1, u"subpacket length"_s);
    if (!first) break;
    // The first length octet is already marked; the rest of a long length
    // is marked with it, so the dump shows one "subpacket length" range.
    qint64 length = static_cast<quint8>(first->at(0));
    if (length >= 192 && length < 255) {
      const auto second = area.Take(1, u"subpacket length"_s);
      if (!second) break;
      length = ((length - 192) << 8) + static_cast<quint8>(second->at(0)) + 192;
    } else if (length == 255) {
      const auto rest = area.Uint(4, u"subpacket length"_s);
      if (!rest) break;
      length = *rest;
    }
    if (length == 0) {
      area.Fail(u"a subpacket with no type octet"_s);
      break;
    }
    const auto tag = area.Octet(u"subpacket tag"_s);
    if (!tag) break;
    auto value = area.Sub(length - 1, PGPSignatureSubpacketName(*tag & 0x7f));
    if (!value) break;
    auto field = ReadSubpacketValue(*value, *tag & 0x7f, facts, depth);
    if ((*tag & 0x80) != 0) field.value += u" (critical)"_s;
    group.children << field;
  }
  area.Finish();
  const auto count = group.children.size();
  group.value = count == 1 ? u"1 subpacket"_s : u"%1 subpackets"_s.arg(count);
  return group;
}

/// The signature value itself, by algorithm (RFC 9580 §5.2.3).
auto ReadSignatureValue(Reader& r, int algorithm,
                        QList<PGPInspectField>& material) -> QString {
  const auto name = PGPPublicKeyAlgorithmShortName(algorithm);
  const auto native = [&r, &material,
                       &name](std::initializer_list<Value> parts) {
    QStringList sizes;
    ReadValues(r, parts, material);
    for (const auto& part : parts) sizes << QString::number(part.size);
    return Joined({name,
                   parts.size() > 1 ? GTrC::tr("Native composite signature")
                                    : GTrC::tr("Native signature"),
                   GTrC::tr("%1 bytes").arg(sizes.join(u" + "_s))});
  };
  const auto r_and_s = [&r, &material, &name](const char* r_span,
                                              const char* s_span) {
    ReadValues(r, {{r_span, "r", 0}, {s_span, "s", 0}}, material);
    return Joined({name, GTrC::tr("MPI-encoded r and s")});
  };

  switch (algorithm) {
    case 1:
    case 3:
      ReadValues(r, {{"rsa_sig_s", "RSA Signature (s)", 0}}, material);
      return Joined({name, GTrC::tr("MPI-encoded signature value")});
    case 17:
      return r_and_s("dsa_sig_r", "dsa_sig_s");
    case 19:
      return r_and_s("ecdsa_sig_r", "ecdsa_sig_s");
    case 22:
      return r_and_s("eddsa_sig_r", "eddsa_sig_s");
    case 16:
    case 20:
      return r_and_s("elgamal_sig_r", "elgamal_sig_s");
    case 27:
      return native({{"ed25519_sig", "Ed25519 Signature", 64}});
    case 28:
      return native({{"ed448_sig", "Ed448 Signature", 114}});
    case 30:
      return native({{"ed25519_sig", "Ed25519 Signature", 64},
                     {"mldsa65_sig", "ML-DSA-65 Signature", 3309}});
    case 31:
      return native({{"ed448_sig", "Ed448 Signature", 114},
                     {"mldsa87_sig", "ML-DSA-87 Signature", 4627}});
    case 32:
      return native({{"slhdsa_sig", "SLH-DSA Signature", 7856}});
    case 33:
      return native({{"slhdsa_sig", "SLH-DSA Signature", 17088}});
    case 34:
      return native({{"slhdsa_sig", "SLH-DSA Signature", 29792}});
    default:
      // Unread octets are not a decoded field; they stay in the Hex tab.
      r.Rest(u"signature"_s);
      return Joined(
          {PGPPublicKeyAlgorithmName(algorithm),
           GTrC::tr("Not decoded: the inspector does not know this "
                    "algorithm's signature format; its octets are in the "
                    "Hex tab")});
  }
}

void ReadSignature(Reader& r, SignatureParts& sig, int depth) {
  const auto version = r.Octet(u"version"_s);
  if (!version) return;
  sig.version = *version;
  sig.overview << Field(u"Version"_s, QString::number(*version));

  std::optional<int> type;
  std::optional<int> algorithm;
  std::optional<int> hash;
  SubpacketFacts facts;

  if (*version == 2 || *version == 3) {
    r.Octet(u"hashed_area_len"_s);
    type = r.Octet(u"type"_s);
    facts.created = r.Uint(4, u"creation_time"_s);
    if (const auto id = r.Take(8, u"issuer"_s)) facts.issuer_key_id = Hex(*id);
    algorithm = r.Octet(u"pk_algo"_s);
    hash = r.Octet(u"hash_algo"_s);
  } else if (*version >= 4 && *version <= 6) {
    type = r.Octet(u"type"_s);
    algorithm = r.Octet(u"pk_algo"_s);
    hash = r.Octet(u"hash_algo"_s);
    const auto width = *version == 4 ? 2 : 4;
    for (const auto hashed : {true, false}) {
      const auto len =
          r.Uint(width, hashed ? u"hashed_area_len"_s : u"unhashed_area_len"_s);
      if (!len) break;
      auto area = r.Sub(*len, hashed ? u"hashed area"_s : u"unhashed area"_s);
      if (!area) break;
      sig.subpackets << ReadSubpacketArea(
          *area, hashed ? u"Hashed Area"_s : u"Unhashed Area"_s, facts, depth);
    }
  } else {
    r.Fail(u"unknown signature version %1"_s.arg(*version));
    return;
  }

  if (type) sig.overview << Field(u"Type"_s, PGPSignatureTypeName(*type));
  if (algorithm) {
    sig.overview << Field(u"Public Key Algorithm"_s,
                          PGPPublicKeyAlgorithmName(*algorithm));
  }
  if (hash) {
    sig.overview << Field(u"Hash Algorithm"_s, PGPHashAlgorithmName(*hash));
  }
  if (facts.created) {
    sig.overview << Field(u"Created"_s, Time(*facts.created));
  }
  if (facts.expires_after && *facts.expires_after != 0) {
    auto expires = Duration(*facts.expires_after);
    if (facts.created) {
      expires += u" (%1)"_s.arg(Time(*facts.created + *facts.expires_after));
    }
    sig.overview << Field(u"Expires After"_s, expires);
  }
  if (!facts.issuer_fingerprint.isEmpty()) {
    sig.overview << Field(u"Issuer Fingerprint"_s, facts.issuer_fingerprint);
  } else if (!facts.issuer_key_id.isEmpty()) {
    sig.overview << Field(u"Issuer Key ID"_s, facts.issuer_key_id);
  }

  // The type is the same "Binary" on most rows; the algorithms tell
  // signatures apart.
  if (algorithm && hash) {
    sig.headline = u"%1, %2"_s.arg(PGPPublicKeyAlgorithmShortName(*algorithm),
                                   PGPHashAlgorithmName(*hash));
  }

  const auto prefix1 = r.Take(1, u"digest_prefix1"_s);
  const auto prefix2 = r.Take(1, u"digest_prefix2"_s);
  if (prefix1 && prefix2) {
    sig.overview << Field(u"Digest Prefix"_s, Hex(*prefix1 + *prefix2));
  }
  if (*version == 6) {
    const auto salt_len = r.Octet(u"salt_len"_s);
    if (!salt_len) return;
    if (const auto salt = r.Take(*salt_len, u"salt"_s)) {
      sig.overview << Field(u"Salt"_s, Hex(*salt));
    }
  }
  if (algorithm && !r.Failed()) {
    sig.material_summary = ReadSignatureValue(r, *algorithm, sig.material);
    if (r.Failed()) {
      sig.material_summary = Joined(
          {sig.material_summary,
           GTrC::tr("Incomplete: parsing stopped inside the signature value, "
                    "see the Overview tab")});
    }
  }
}

// ---------------------------------------------------------------------------
// Session keys
// ---------------------------------------------------------------------------

/// The encrypted session key of a PKESK, by algorithm (RFC 9580 §5.1).
auto ReadPkeskValues(Reader& r, int version, int algorithm,
                     QList<PGPInspectField>& overview,
                     QList<PGPInspectField>& material) -> QString {
  const auto name = PGPPublicKeyAlgorithmShortName(algorithm);
  // X25519, X448 and the ML-KEM composites end in a length-prefixed wrapped
  // key, which in a v3 PKESK starts with the cleartext cipher octet.
  const auto wrapped = [&r, &material, version](const char* prefix) {
    const auto span = QString::fromLatin1(prefix);
    const auto len = r.Octet(span + "_esk_len"_L1);
    if (!len) return;
    auto rest = r.Sub(*len, span + "_esk"_L1);
    if (!rest) return;
    if (version == 3) {
      if (const auto sym = rest->Octet(u"sym_algo"_s)) {
        material << Field(u"Session Key Cipher"_s,
                          PGPSymmetricAlgorithmName(*sym));
      }
    }
    const auto key = rest->Rest(span + "_esk"_L1);
    material << Field(u"Wrapped Session Key"_s, GroupedHex(key), {},
                      BytesDetail(key.size()));
  };

  switch (algorithm) {
    case 1:
    case 2: {
      int bits = 0;
      if (const auto mpi = r.Mpi(u"rsa_ciphertext"_s, &bits)) {
        material << Field(u"RSA Ciphertext"_s, GroupedHex(*mpi), {},
                          MpiDetail(mpi->size(), bits));
        // The ciphertext is below the modulus, so its width falls short of
        // the key size only when its top octet happens to be zero.
        overview << Field(
            u"Key Size"_s,
            u"~%1 bits (from ciphertext width)"_s.arg(mpi->size() * 8));
      }
      return Joined({name, GTrC::tr("MPI-encoded ciphertext")});
    }
    case 16:
    case 20:
      ReadValues(
          r,
          {{"elgamal_e", "Elgamal g^k", 0}, {"elgamal_c", "Elgamal m*y^k", 0}},
          material);
      return Joined({name, GTrC::tr("MPI-encoded ciphertext")});
    case 18: {
      const auto summary = Joined(
          {name, GTrC::tr("MPI-encoded ephemeral point and wrapped session "
                          "key")});
      int bits = 0;
      const auto point = r.Mpi(u"ecdh_e"_s, &bits);
      if (!point) return summary;
      material << Field(u"Ephemeral Point"_s, GroupedHex(*point), {},
                        MpiDetail(point->size(), bits));
      const auto curve = PGPEccPointCurve(*point);
      if (!curve.isEmpty()) overview << Field(u"Curve"_s, curve);
      const auto len = r.Octet(u"ecdh_esk_len"_s);
      if (!len) return summary;
      if (const auto key = r.Take(*len, u"ecdh_esk"_s)) {
        material << Field(u"Wrapped Session Key"_s, GroupedHex(*key), {},
                          BytesDetail(key->size()));
      }
      return summary;
    }
    case 25:
    case 26: {
      const auto x25519 = algorithm == 25;
      overview << Field(u"Variant"_s, x25519 ? u"X25519"_s : u"X448"_s);
      const auto* prefix = x25519 ? "x25519" : "x448";
      if (const auto e =
              r.Take(x25519 ? 32 : 56, QString::fromLatin1(prefix) + "_e"_L1)) {
        material << Field(u"Ephemeral Key"_s, GroupedHex(*e), {},
                          BytesDetail(e->size()));
      }
      wrapped(prefix);
      return Joined(
          {name, GTrC::tr("Native ephemeral key and wrapped session key")});
    }
    case 35:
    case 36: {
      const auto small = algorithm == 35;
      overview << Field(u"Variant"_s, small ? u"ML-KEM-768 + X25519"_s
                                            : u"ML-KEM-1024 + X448"_s);
      ReadValues(r,
                 {{"ecdh_e", "Ephemeral ECC Key", small ? 32 : 56},
                  {"mlkem_ct", "ML-KEM Ciphertext", small ? 1088 : 1568}},
                 material);
      wrapped("mlkem");
      return Joined({name, GTrC::tr("Native ephemeral key, ML-KEM ciphertext "
                                    "and wrapped session key")});
    }
    case 8: {
      // GnuPG's Kyber: an SOS ephemeral point, the ML-KEM ciphertext behind
      // a four-octet length, the cipher octet, then the wrapped key.
      const auto summary = Joined(
          {name, GTrC::tr("ECC point, ML-KEM ciphertext and wrapped session "
                          "key")});
      const auto point = r.Mpi(u"ecc_e"_s);
      const auto ct_len = point ? r.Uint(4, u"kyber_ct_len"_s) : std::nullopt;
      const auto ct = ct_len ? r.Take(*ct_len, u"kyber_ct"_s) : std::nullopt;
      const auto sym = ct ? r.Octet(u"sym_algo"_s) : std::nullopt;
      const auto key_len = sym ? r.Octet(u"esk_len"_s) : std::nullopt;
      const auto key = key_len ? r.Take(*key_len, u"esk"_s) : std::nullopt;
      if (!key || key->isEmpty() || r.Remaining() != 0) return summary;
      const char* kem = ct->size() == 1088   ? "ML-KEM-768"
                        : ct->size() == 1568 ? "ML-KEM-1024"
                                             : nullptr;
      const auto curve = PGPEccPointCurve(*point);
      if (kem != nullptr && !curve.isEmpty()) {
        overview << Field(u"Variant"_s,
                          u"%1 + %2"_s.arg(QLatin1StringView(kem), curve));
      }
      material << Field(u"Ephemeral Point"_s, GroupedHex(*point), {},
                        BytesDetail(point->size()))
               << Field(u"ML-KEM Ciphertext"_s, GroupedHex(*ct), {},
                        BytesDetail(ct->size()))
               << Field(u"Session Key Cipher"_s,
                        PGPSymmetricAlgorithmName(*sym))
               << Field(u"Wrapped Session Key"_s, GroupedHex(*key), {},
                        BytesDetail(key->size()));
      return summary;
    }
    default:
      // Unread octets are not a decoded field; they stay in the Hex tab.
      r.Rest(u"esk"_s);
      return Joined(
          {PGPPublicKeyAlgorithmName(algorithm),
           GTrC::tr("Not decoded: the inspector does not know this "
                    "algorithm's session key format; its octets are in the "
                    "Hex tab")});
  }
}

void DecodePkesk(Reader& r, PGPDecodedPacket& out) {
  const auto version = r.Octet(u"version"_s);
  if (!version) return;
  out.version = *version;
  out.overview << Field(u"Version"_s, QString::number(*version));

  QString recipient;
  if (*version == 3) {
    if (const auto id = r.Take(8, u"keyid"_s)) {
      recipient = Hex(*id);
      out.overview << Field(u"Recipient Key ID"_s, *id == QByteArray(8, '\0')
                                                       ? u"anonymous"_s
                                                       : recipient);
    }
  } else if (*version == 6) {
    const auto len = r.Octet(u"recipient_len"_s);
    if (!len) return;
    if (*len == 0) {
      out.overview << Field(u"Recipient"_s, u"anonymous"_s);
    } else {
      r.Octet(u"recipient_version"_s);
      if (const auto fp = r.Take(*len - 1, u"recipient"_s)) {
        recipient = Hex(*fp);
        out.overview << Field(u"Recipient Fingerprint"_s, recipient);
      }
    }
  } else {
    r.Fail(u"unknown PKESK version %1"_s.arg(*version));
    return;
  }

  const auto algorithm = r.Octet(u"pk_algo"_s);
  if (!algorithm) return;
  out.overview << Field(u"Public Key Algorithm"_s,
                        PGPPublicKeyAlgorithmName(*algorithm));
  out.material_summary =
      ReadPkeskValues(r, *version, *algorithm, out.overview, out.material);
  if (r.Failed()) {
    out.material_summary =
        Joined({out.material_summary,
                GTrC::tr("Incomplete: parsing stopped inside the session key "
                         "material, see the Overview tab")});
  }
  out.overview << Field(u"Session Key"_s,
                        u"encrypted, not opened by the inspector"_s);
  out.headline = PGPPublicKeyAlgorithmShortName(*algorithm);
  // A key ID's worth of the recipient is enough to tell rows apart.
  if (!recipient.isEmpty()) {
    out.headline += u", "_s + recipient.left(16) +
                    (recipient.size() > 16 ? u"…"_s : QString());
  }
}

void DecodeSkesk(Reader& r, PGPDecodedPacket& out) {
  const auto version = r.Octet(u"version"_s);
  if (!version) return;
  out.version = *version;
  out.overview << Field(u"Version"_s, QString::number(*version));

  if (*version == 6) r.Octet(u"params_len"_s);
  const auto sym = r.Octet(u"sym_algo"_s);
  if (!sym) return;
  out.overview << Field(u"Symmetric Algorithm"_s,
                        PGPSymmetricAlgorithmName(*sym));

  std::optional<int> aead;
  if (*version == 5 || *version == 6) {
    aead = r.Octet(u"aead_algo"_s);
    if (!aead) return;
    out.overview << Field(u"AEAD Mode"_s, PGPAeadAlgorithmName(*aead));
    if (*version == 6) r.Octet(u"s2k_len"_s);
  } else if (*version != 4) {
    r.Fail(u"unknown SKESK version %1"_s.arg(*version));
    return;
  }

  const auto s2k = ReadS2K(r);
  out.overview << s2k;
  out.headline = PGPSymmetricAlgorithmName(*sym) + u", "_s + s2k.value;

  if (aead) {
    if (const auto iv = r.Take(PGPAeadNonceSize(*aead), u"aead_iv"_s)) {
      out.material << Field(u"Nonce"_s, Hex(*iv), {}, BytesDetail(iv->size()));
    }
  }
  const auto esk = r.Rest(u"esk"_s);
  if (!esk.isEmpty()) {
    out.material << Field(u"Encrypted Session Key"_s, GroupedHex(esk), {},
                          BytesDetail(esk.size()));
  }
}

// ---------------------------------------------------------------------------
// Everything else
// ---------------------------------------------------------------------------

void DecodeOnePass(Reader& r, PGPDecodedPacket& out) {
  const auto version = r.Octet(u"version"_s);
  if (!version) return;
  out.version = *version;
  if (*version != 3 && *version != 6) {
    r.Fail(u"unknown one-pass signature version %1"_s.arg(*version));
    return;
  }
  const auto type = r.Octet(u"type"_s);
  const auto hash = r.Octet(u"hash_algo"_s);
  const auto algorithm = r.Octet(u"pk_algo"_s);
  if (!type || !hash || !algorithm) return;
  out.overview << Field(u"Version"_s, QString::number(*version))
               << Field(u"Type"_s, PGPSignatureTypeName(*type))
               << Field(u"Hash Algorithm"_s, PGPHashAlgorithmName(*hash))
               << Field(u"Public Key Algorithm"_s,
                        PGPPublicKeyAlgorithmName(*algorithm));
  out.headline = u"%1, %2"_s.arg(PGPPublicKeyAlgorithmShortName(*algorithm),
                                 PGPHashAlgorithmName(*hash));

  if (*version == 3) {
    if (const auto id = r.Take(8, u"issuer"_s)) {
      out.overview << Field(u"Issuer Key ID"_s, Hex(*id));
    }
  } else {
    const auto salt_len = r.Octet(u"salt_len"_s);
    if (!salt_len) return;
    if (const auto salt = r.Take(*salt_len, u"salt"_s)) {
      out.overview << Field(u"Salt"_s, Hex(*salt));
    }
    if (const auto fp = r.Take(32, u"issuer"_s)) {
      out.overview << Field(u"Issuer Fingerprint"_s, Hex(*fp));
    }
  }
  if (const auto last = r.Octet(u"last"_s)) {
    // Zero means another one-pass signature follows, nested inside this one.
    out.overview << Field(u"Last"_s, YesNo(*last != 0));
  }
}

void DecodeLiteral(Reader& r, PGPDecodedPacket& out) {
  const auto format = r.Octet(u"format"_s);
  if (!format) return;
  const auto mode = [](int f) {
    switch (f) {
      case 'b':
        return u"binary"_s;
      case 't':
        return u"text"_s;
      case 'u':
        return u"UTF-8 text"_s;
      case 'm':
        return u"MIME"_s;
      case 'l':
        return u"local"_s;
      default:
        return u"unknown"_s;
    }
  };
  out.overview << Field(u"Format"_s,
                        u"%1 (%2)"_s.arg(QChar(*format)).arg(mode(*format)));

  out.headline = mode(*format);

  const auto name_len = r.Octet(u"filename_len"_s);
  if (!name_len) return;
  if (const auto name = r.Take(*name_len, u"filename"_s)) {
    out.overview << Field(u"File Name"_s, TextOrHex(*name));
    // The file name when there is one; the format otherwise.
    if (!name->isEmpty()) out.headline = TextOrHex(*name);
  }
  if (const auto date = r.Uint(4, u"date"_s)) {
    out.overview << Field(u"Date"_s, *date == 0 ? u"not set"_s : Time(*date));
  }
  if (r.Failed()) return;

  const auto size = r.RemainingOnWire();
  const auto data = r.Rest({});
  out.overview << Field(u"Data Size"_s, Size(size));

  // Enough to recognise the content, not to show it all.
  constexpr qsizetype kPreview = 64;
  auto preview = TextOrHex(data.left(kPreview));
  if (size > kPreview) preview += u"…"_s;
  if (!data.isEmpty()) out.overview << Field(u"Preview"_s, preview);
}

void DecodeEncrypted(Reader& r, int tag, PGPDecodedPacket& out) {
  if (tag == 9) {
    out.overview << Field(
        u"Integrity Protection"_s,
        u"none (deprecated, RFC 9580 forbids producing this)"_s);
    out.headline = u"no integrity protection"_s;
  } else {
    const auto version = r.Octet(u"version"_s);
    if (!version) return;
    out.version = *version;
    out.overview << Field(u"Version"_s, QString::number(*version));

    if (tag == 18 && *version == 1) {
      out.overview << Field(u"Integrity Protection"_s,
                            u"SHA-1 modification detection code"_s);
      out.headline = u"SHA-1 MDC"_s;
    } else if ((tag == 18 && *version == 2) || tag == 20) {
      const auto sym = r.Octet(u"sym_algo"_s);
      const auto aead = r.Octet(u"aead_algo"_s);
      const auto chunk = r.Octet(u"chunk_size"_s);
      if (!sym || !aead || !chunk) return;
      out.overview << Field(u"Symmetric Algorithm"_s,
                            PGPSymmetricAlgorithmName(*sym))
                   << Field(u"AEAD Mode"_s, PGPAeadAlgorithmName(*aead))
                   << Field(u"Chunk Size"_s, Size(qint64{1} << (*chunk + 6)));
      if (tag == 18) {
        if (const auto salt = r.Take(32, u"salt"_s)) {
          out.overview << Field(u"Salt"_s, Hex(*salt));
        }
        out.headline = u"%1, %2"_s.arg(PGPSymmetricAlgorithmName(*sym),
                                       PGPAeadAlgorithmName(*aead));
      } else {
        if (const auto iv = r.Take(PGPAeadNonceSize(*aead), u"iv"_s)) {
          out.overview << Field(u"IV"_s, Hex(*iv));
        }
        out.headline = u"%1, %2"_s.arg(PGPSymmetricAlgorithmName(*sym),
                                       PGPAeadAlgorithmName(*aead));
      }
    } else {
      r.Fail(u"unknown container version %1"_s.arg(*version));
      return;
    }
  }
  if (r.Failed()) return;
  const auto size = r.RemainingOnWire();
  r.Rest({});
  out.overview << Field(u"Encrypted Size"_s, Size(size))
               << Field(u"Payload"_s,
                        u"encrypted, not opened by the inspector"_s);
}

void DecodeUserAttribute(Reader& r, PGPDecodedPacket& out) {
  while (r.Remaining() > 0 && !r.Failed()) {
    const auto first = r.Take(1, u"subpacket length"_s);
    if (!first) return;
    qint64 length = static_cast<quint8>(first->at(0));
    if (length >= 192 && length < 255) {
      const auto second = r.Take(1, u"subpacket length"_s);
      if (!second) return;
      length = ((length - 192) << 8) + static_cast<quint8>(second->at(0)) + 192;
    } else if (length == 255) {
      const auto rest = r.Uint(4, u"subpacket length"_s);
      if (!rest) return;
      length = *rest;
    }
    const auto type = length > 0 ? r.Octet(u"subpacket tag"_s) : std::nullopt;
    if (!type) {
      r.Fail(u"an attribute subpacket with no type octet"_s);
      return;
    }
    const auto value = r.Take(length - 1, u"attribute"_s);
    if (!value) return;
    if (*type == 1 && value->size() >= 4) {
      // The image header: a little-endian length, a version, an encoding.
      const auto encoding = static_cast<quint8>(value->at(3));
      out.overview << Field(
          u"Image"_s,
          u"%1, %2"_s.arg(
              encoding == 1 ? u"JPEG"_s : u"encoding %1"_s.arg(encoding),
              Size(value->size())));
    } else {
      out.overview << Field(u"Attribute %1"_s.arg(*type), Size(value->size()));
    }
  }
}

void DecodeBody(int tag, Reader& r, const QByteArray& body,
                PGPDecodedPacket& out) {
  switch (tag) {
    case 1:
      DecodePkesk(r, out);
      break;
    case 2: {
      SignatureParts sig;
      ReadSignature(r, sig, 0);
      out.version = sig.version;
      out.headline = sig.headline;
      out.overview = sig.overview;
      out.subpackets = sig.subpackets;
      out.material = sig.material;
      out.material_summary = sig.material_summary;
      break;
    }
    case 3:
      DecodeSkesk(r, out);
      break;
    case 4:
      DecodeOnePass(r, out);
      break;
    case 5:
    case 6:
    case 7:
    case 14:
      DecodeKey(r, body, tag == 5 || tag == 7, out);
      break;
    case 8: {
      const auto algorithm = r.Octet(u"algo"_s);
      if (!algorithm) break;
      const auto name = PGPCompressionAlgorithmName(*algorithm);
      out.overview << Field(u"Compression Algorithm"_s, name)
                   << Field(u"Compressed Size"_s, Size(r.RemainingOnWire()));
      out.headline = name;
      r.Rest({});
      break;
    }
    case 9:
    case 18:
    case 20:
      DecodeEncrypted(r, tag, out);
      break;
    case 10: {
      const auto marker = r.Rest(u"marker"_s);
      out.overview << Field(
          u"Marker"_s,
          u"%1 (a legacy compatibility marker)"_s.arg(TextOrHex(marker)));
      break;
    }
    case 11:
      DecodeLiteral(r, out);
      break;
    case 12:
      out.overview << Field(u"Trust"_s,
                            u"local trust data, not part of the message"_s)
                   << Field(u"Value"_s, Hex(r.Rest(u"trust"_s)));
      break;
    case 13: {
      const auto user_id = TextOrHex(r.Rest(u"value"_s));
      out.overview << Field(u"User ID"_s, user_id);
      out.headline = user_id;
      break;
    }
    case 17:
      DecodeUserAttribute(r, out);
      break;
    case 19:
      if (const auto digest = r.Take(20, u"digest"_s)) {
        out.overview << Field(u"Digest"_s, Hex(*digest))
                     << Field(u"Algorithm"_s,
                              u"SHA-1 modification detection code"_s);
      }
      break;
    case 21: {
      const auto size = r.RemainingOnWire();
      r.Rest({});
      out.overview << Field(u"Padding Size"_s, Size(size))
                   << Field(u"Contents"_s,
                            u"random padding, carries no meaning"_s);
      break;
    }
    default: {
      const auto size = r.RemainingOnWire();
      const auto bytes = r.Rest({});
      out.overview << Field(u"Body Size"_s, Size(size))
                   << Field(u"First Octets"_s,
                            Hex(bytes.left(16)) +
                                (size > 16 ? u"…"_s : QString()));
    }
  }
}

}  // namespace

auto PGPDecodePacket(const QByteArray& wire, qint64 wire_length)
    -> PGPDecodedPacket {
  PGPDecodedPacket out;
  wire_length = std::max<qint64>(wire_length, wire.size());

  auto framing = ReadFraming(wire, wire_length);
  if (framing.tag < 0) {
    out.error = framing.error;
    if (!wire.isEmpty())
      out.spans.push_back({0, wire.size(), u"unparsed"_s, false});
    return out;
  }

  QList<PGPHexSpan> body_spans;
  QString error;
  const bool capped = wire.size() < wire_length;
  Reader reader(framing.body, &body_spans, &error, 0, framing.body_declared,
                capped);
  DecodeBody(framing.tag, reader, framing.body, out);
  reader.Finish(!capped);

  out.spans = framing.spans + MapToWire(body_spans, framing.segments);
  std::sort(out.spans.begin(), out.spans.end(),
            [](const PGPHexSpan& a, const PGPHexSpan& b) {
              return a.offset < b.offset;
            });

  // Octets the framing could not place in a body, after a broken length.
  const auto covered = out.spans.isEmpty()
                           ? 0
                           : out.spans.last().offset + out.spans.last().length;
  if (covered < wire.size()) {
    out.spans.push_back({covered, wire.size() - covered, u"unparsed"_s, false});
  }

  out.error = framing.error.isEmpty() ? error : framing.error;
  return out;
}

namespace {

// sq's layout: an eight-digit offset, the sixteen hex columns with a gap
// after the eighth, then the label -- or, for payload, its ASCII.
constexpr int kColumns = 16;
constexpr int kOffsetWidth = 8;
constexpr int kHexWidth = (kColumns * 3) - 1 + 1;
/// Everything before the label: the offset, two spaces, the hex, three
/// spaces.
constexpr int kLabelColumn = kOffsetWidth + 2 + kHexWidth + 3;

}  // namespace

auto PGPHexDumpLineWidth(const QList<PGPHexSpan>& spans) -> int {
  qsizetype tail = 0;
  for (const auto& span : spans) {
    tail = std::max(
        tail, span.label.isEmpty() ? qsizetype{kColumns} : span.label.size());
  }
  return kLabelColumn + static_cast<int>(tail);
}

auto PGPFormatHexDump(const QByteArray& wire, const QList<PGPHexSpan>& spans,
                      qint64 wire_length) -> QString {
  const auto column_at = [](int column) {
    return (column * 3) + (column >= 8 ? 1 : 0);
  };

  QStringList lines;
  for (const auto& span : spans) {
    auto pos = span.offset;
    const auto end = std::min<qint64>(span.offset + span.length, wire.size());
    bool first = true;
    while (pos < end) {
      const auto column = static_cast<int>(pos % kColumns);
      const auto count = std::min<qint64>(kColumns - column, end - pos);

      QString hex(kHexWidth, ' ');
      QString ascii(column, ' ');
      for (qint64 i = 0; i < count; ++i) {
        const auto byte = static_cast<quint8>(wire.at(pos + i));
        const auto text =
            span.redacted ? u"--"_s : u"%1"_s.arg(byte, 2, 16, QChar('0'));
        hex.replace(column_at(column + static_cast<int>(i)), 2, text);
        ascii += span.redacted                   ? QChar('-')
                 : (byte >= 0x20 && byte < 0x7f) ? QChar(byte)
                                                 : QChar('.');
      }

      const auto tail =
          span.label.isEmpty() ? ascii : (first ? span.label : QString());
      auto line = u"%1  %2   %3"_s.arg(pos, kOffsetWidth, 16, QChar('0'))
                      .arg(hex, tail);
      while (line.endsWith(' ')) line.chop(1);
      lines << line;
      first = false;
      pos += count;
    }
  }

  if (wire_length > wire.size()) {
    lines << u"… %1 more not shown"_s.arg(Size(wire_length - wire.size()));
  }
  return lines.join('\n');
}
