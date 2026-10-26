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

#include <QCoreApplication>

#include "PGPInspectModel.h"

namespace {

/// A minimal but complete document, in the shape the Rust walk emits.
constexpr const char* kDocument = R"({
  "format": "armored",
  "size": 325,
  "errors": [],
  "blocks": [
    {
      "kind": "armored",
      "offset": 0,
      "armor": {
        "blockType": "PGP SIGNATURE",
        "headers": [{"label": "Version", "value": "GnuPG v2"}],
        "crc24": "ok"
      },
      "packets": [
        {
          "tag": 8,
          "offset": 0,
          "headerVersion": "new",
          "headerLength": 2,
          "bodyLength": 189,
          "lengthType": "fixed",
          "raw": "",
          "rawTruncated": true,
          "children": [
            {
              "tag": 2,
              "offset": 0,
              "headerVersion": "new",
              "headerLength": 2,
              "bodyLength": 189,
              "lengthType": "fixed",
              "raw": "c2bd0400160a006f05826a635e290910f29a1d9930f596bf471400000000001e002073616c74406e6f746174696f6e732e736571756f69612d7067702e6f7267d9f858c597f2543c34aad671a22714d17afc468aa4da563489d0033186aa531d16210408046811f5ed9f4215ee2bb9f29a1d9930f596bf00007b750100d50684a93169046d737307e8c2034459074f892d98b5ec2babdb948878a1d0ca00fd158d3dc0b90cd5e6d4fbaea19e360ef07436e712b78d6ee88148ba55b7a0b404",
              "rawTruncated": false,
              "children": [],
              "error": null
            }
          ],
          "error": null
        }
      ],
      "error": null
    }
  ]
})";

/// kDocument, parsed and decoded the way the dialog does it.
auto DecodedDocument() -> PGPInspectDocument {
  auto document = ParsePGPInspectDocument(kDocument);
  PGPInspectDecode(document);
  return document;
}

}  // namespace

// -- reading the document ----------------------------------------------------

TEST(PGPInspectModelTest, ReadsTheDocumentShape) {
  const auto document = ParsePGPInspectDocument(kDocument);

  ASSERT_TRUE(document.valid);
  EXPECT_EQ(document.format, QString("armored"));
  EXPECT_EQ(document.size, 325);
  ASSERT_EQ(document.blocks.size(), 1);
  EXPECT_TRUE(document.errors.isEmpty());
}

TEST(PGPInspectModelTest, ReadsTheArmorEnvelope) {
  const auto document = ParsePGPInspectDocument(kDocument);
  const auto& block = document.blocks.at(0);

  ASSERT_TRUE(block.has_armor);
  EXPECT_EQ(block.armor.block_type, QString("PGP SIGNATURE"));
  EXPECT_EQ(block.armor.crc24, QString("ok"));
  ASSERT_EQ(block.armor.headers.size(), 1);
  EXPECT_EQ(block.armor.headers.at(0).label, QString("Version"));
}

TEST(PGPInspectModelTest, NestsChildrenUnderTheirContainer) {
  // The recursion is the reason the tree exists; a flat read would lose it.
  const auto document = DecodedDocument();
  const auto& packets = document.blocks.at(0).packets;

  ASSERT_EQ(packets.size(), 1);
  EXPECT_EQ(packets.at(0).tag, 8);
  ASSERT_EQ(packets.at(0).children.size(), 1);
  EXPECT_EQ(packets.at(0).children.at(0).tag, 2);
  EXPECT_EQ(packets.at(0).children.at(0).decoded.version, 4);
}

TEST(PGPInspectModelTest, CountsNestedPacketsToo) {
  EXPECT_EQ(ParsePGPInspectDocument(kDocument).PacketCount(), 2);
}

TEST(PGPInspectModelTest, KeepsTheFramingFiguresVerbatim) {
  const auto document = ParsePGPInspectDocument(kDocument);
  const auto& packet = document.blocks.at(0).packets.at(0);

  EXPECT_EQ(packet.offset, 0);
  EXPECT_EQ(packet.header_length, 2);
  EXPECT_EQ(packet.body_length, 189);
  EXPECT_EQ(packet.header_version, QString("new"));
  EXPECT_EQ(packet.length_type, QString("fixed"));
}

TEST(PGPInspectModelTest, AbsentVersionReadsAsMinusOne) {
  // Most container packets carry no version octet. That is a fact about the
  // type, so it must not look like a defect in the document.
  const auto document = DecodedDocument();
  const auto& packet = document.blocks.at(0).packets.at(0);
  EXPECT_EQ(packet.decoded.version, -1);
  EXPECT_FALSE(packet.Malformed());
}

TEST(PGPInspectModelTest, ReadsTheRawOctets) {
  const auto document = ParsePGPInspectDocument(kDocument);
  const auto& container = document.blocks.at(0).packets.at(0);
  EXPECT_TRUE(container.raw.isEmpty());
  EXPECT_TRUE(container.raw_truncated);

  const auto& sig = container.children.at(0);
  EXPECT_EQ(sig.raw.size(), sig.WireLength());
  EXPECT_EQ(static_cast<quint8>(sig.raw.at(0)), 0xc2);
  EXPECT_FALSE(sig.raw_truncated);
}

TEST(PGPInspectModelTest, ParsingAloneDecodesNothing) {
  // The menu probe parses a document on every opening; decoding is the
  // dialog's cost, not the probe's.
  const auto document = ParsePGPInspectDocument(kDocument);
  const auto& sig = document.blocks.at(0).packets.at(0).children.at(0);
  EXPECT_TRUE(sig.decoded.spans.isEmpty());
  EXPECT_EQ(sig.decoded.version, -1);
}

TEST(PGPInspectModelTest, DecodingFillsEveryNestedPacket) {
  const auto document = DecodedDocument();
  const auto& sig = document.blocks.at(0).packets.at(0).children.at(0);
  EXPECT_FALSE(sig.decoded.overview.isEmpty());
  ASSERT_EQ(sig.decoded.subpackets.size(), 2);
  EXPECT_EQ(sig.decoded.subpackets.at(0).label, QString("Hashed Area"));
  EXPECT_FALSE(sig.decoded.subpackets.at(0).children.isEmpty())
      << "subpackets nest under their area";
  EXPECT_FALSE(sig.decoded.material.isEmpty());
  EXPECT_FALSE(sig.Malformed());
}

TEST(PGPInspectModelTest, AContainerLearnsItsDecompressedSize) {
  // Only the children say where the decompressed stream ends.
  const auto document = DecodedDocument();
  const auto& overview = document.blocks.at(0).packets.at(0).decoded.overview;
  ASSERT_EQ(overview.size(), 1);
  EXPECT_EQ(overview.at(0).label, QString("Decompressed Size"));
  EXPECT_EQ(overview.at(0).value, QString("191 bytes"));
}

TEST(PGPInspectModelTest, OctetsThatDoNotDecodeMarkThePacketMalformed) {
  auto document = ParsePGPInspectDocument(
      R"({"blocks":[{"packets":[{"tag":2,"headerLength":2,"bodyLength":3,"raw":"c20304ffff"}]}]})");
  PGPInspectDecode(document);
  const auto& packet = document.blocks.at(0).packets.at(0);
  EXPECT_TRUE(packet.error.isEmpty());
  EXPECT_FALSE(packet.decoded.error.isEmpty());
  EXPECT_TRUE(packet.Malformed());
}

// -- the shapes that are not the happy one ------------------------------------

TEST(PGPInspectModelTest, MalformedJsonIsReportedNotThrown) {
  const auto document = ParsePGPInspectDocument("{ this is not json");

  EXPECT_FALSE(document.valid);
  EXPECT_FALSE(document.parse_error.isEmpty());
  EXPECT_TRUE(document.blocks.isEmpty());
}

TEST(PGPInspectModelTest, EmptyInputIsReportedNotThrown) {
  const auto document = ParsePGPInspectDocument({});

  EXPECT_FALSE(document.valid);
  EXPECT_FALSE(document.parse_error.isEmpty());
}

TEST(PGPInspectModelTest, ANonObjectDocumentIsRefused) {
  EXPECT_FALSE(ParsePGPInspectDocument("[1, 2, 3]").valid);
}

TEST(PGPInspectModelTest, AnEmptyDocumentIsValidAndEmpty) {
  // "nothing recognisable here" is a finding the inspector reports, not a
  // failure to read its answer.
  const auto document = ParsePGPInspectDocument(
      R"({"format":"binary","size":4,"blocks":[],"errors":["not OpenPGP data"]})");

  ASSERT_TRUE(document.valid);
  EXPECT_TRUE(document.blocks.isEmpty());
  ASSERT_EQ(document.errors.size(), 1);
  EXPECT_EQ(document.errors.at(0), QString("not OpenPGP data"));
}

TEST(PGPInspectModelTest, APacketErrorSurvivesIntoTheTree) {
  const auto document = ParsePGPInspectDocument(
      R"({"blocks":[{"packets":[{"tag":2,"error":"the packet body is truncated"}]}]})");

  ASSERT_EQ(document.blocks.size(), 1);
  const auto& packet = document.blocks.at(0).packets.at(0);
  EXPECT_TRUE(packet.Malformed());
  EXPECT_EQ(packet.error, QString("the packet body is truncated"));
}

TEST(PGPInspectModelTest, MissingFieldsTakeTheirDefaults) {
  // A document from an older or newer engine must degrade field by field
  // rather than costing the reader the whole tree.
  const auto document =
      ParsePGPInspectDocument(R"({"blocks":[{"packets":[{}]}]})");

  ASSERT_TRUE(document.valid);
  const auto& packet = document.blocks.at(0).packets.at(0);
  EXPECT_EQ(packet.tag, -1);
  EXPECT_TRUE(packet.raw.isEmpty());
  EXPECT_FALSE(packet.raw_truncated);
  EXPECT_EQ(packet.body_length, 0);
}

TEST(PGPInspectModelTest, AHeaderRowWithNeitherHalfIsDropped) {
  const auto document = ParsePGPInspectDocument(
      R"({"blocks":[{"armor":{"headers":[{},{"label":"A","value":"B"}]},"packets":[]}]})");

  const auto& fields = document.blocks.at(0).armor.headers;
  ASSERT_EQ(fields.size(), 1);
  EXPECT_EQ(fields.at(0).label, QString("A"));
}

TEST(PGPInspectModelTest, ReadsTheCleartextHalf) {
  const auto document = ParsePGPInspectDocument(
      R"({"format":"cleartext","blocks":[{"kind":"cleartext","cleartext":{"textSize":42,"headers":[{"label":"Hash","value":"SHA256"}]},"packets":[]}]})");

  const auto& block = document.blocks.at(0);
  ASSERT_TRUE(block.has_cleartext);
  EXPECT_EQ(block.cleartext_text_size, 42);
  ASSERT_EQ(block.cleartext_headers.size(), 1);
  EXPECT_EQ(block.cleartext_headers.at(0).value, QString("SHA256"));
}

// -- whether there is anything to show ----------------------------------------

TEST(PGPInspectModelTest, ADocumentWithPacketsHasStructure) {
  EXPECT_TRUE(PGPInspectHasStructure(ParsePGPInspectDocument(kDocument)));
}

TEST(PGPInspectModelTest, PlainTextHasNoStructure) {
  // The menu entry is greyed out on exactly this: a valid document saying the
  // input is not OpenPGP. Opening a window to be told nothing is worse than
  // an entry that cannot be clicked.
  EXPECT_FALSE(PGPInspectHasStructure(ParsePGPInspectDocument(
      R"({"format":"binary","size":9,"blocks":[],"errors":["not OpenPGP data"]})")));
}

TEST(PGPInspectModelTest, AnUnreadableDocumentHasNoStructure) {
  EXPECT_FALSE(PGPInspectHasStructure(ParsePGPInspectDocument("not json")));
  EXPECT_FALSE(PGPInspectHasStructure({}));
}

TEST(PGPInspectModelTest, ABlockWithNoPacketsHasNoStructure) {
  // An armor envelope whose body decoded to nothing: the header parsed, so
  // the document is valid, but there is still no packet tree to render.
  EXPECT_FALSE(PGPInspectHasStructure(ParsePGPInspectDocument(
      R"({"format":"armored","blocks":[{"kind":"armored","packets":[]}]})")));
}

TEST(PGPInspectModelTest, ANestedPacketAloneIsStructureEnough) {
  EXPECT_TRUE(PGPInspectHasStructure(ParsePGPInspectDocument(
      R"({"blocks":[{"packets":[{"tag":8,"children":[{"tag":2}]}]}]})")));
}

// -- the summaries the tree shows ---------------------------------------------

TEST(PGPInspectModelTest, APacketSummaryIsItsPositionAndName) {
  // Version, tag number, offset and framing are in the Overview tab.
  const auto document = DecodedDocument();
  const auto& packet = document.blocks.at(0).packets.at(0).children.at(0);
  EXPECT_EQ(PGPInspectPacketSummary(packet, "1.1"), QString("#1.1  Signature"));
}

TEST(PGPInspectModelTest, APacketIndexNestsUnderItsParent) {
  EXPECT_EQ(PGPInspectPacketIndex({}, 3), QString("3"));
  EXPECT_EQ(PGPInspectPacketIndex("3", 1), QString("3.1"));
  EXPECT_EQ(PGPInspectPacketIndex("3.1", 12), QString("3.1.12"));
}

TEST(PGPInspectModelTest, ASummaryOfANamelessPacketStillReads) {
  PGPInspectPacket packet;
  packet.tag = 42;
  EXPECT_EQ(PGPInspectPacketSummary(packet, "2"), QString("#2  Unknown"));
}

TEST(PGPInspectModelTest, ABlockSummaryDistinguishesTheThreeKinds) {
  const auto armored = ParsePGPInspectDocument(kDocument).blocks.at(0);
  EXPECT_TRUE(
      PGPInspectBlockSummary(armored, 1).startsWith("Block 1: PGP SIGNATURE"));

  PGPInspectBlock cleartext;
  cleartext.kind = "cleartext";
  EXPECT_TRUE(PGPInspectBlockSummary(cleartext, 2).contains("Cleartext"));

  PGPInspectBlock binary;
  binary.kind = "binary";
  EXPECT_TRUE(PGPInspectBlockSummary(binary, 3).contains("Binary"));
}

TEST(PGPInspectModelTest, ABlockCountsNestedPacketsAndSaysSo) {
  // kDocument holds one container with one packet inside it: two in all.
  const auto nested = ParsePGPInspectDocument(kDocument).blocks.at(0);
  EXPECT_TRUE(PGPInspectBlockSummary(nested, 1).contains("including nested"));

  auto flat = nested;
  flat.packets.first().children.clear();
  EXPECT_FALSE(PGPInspectBlockSummary(flat, 1).contains("nested"));
}
