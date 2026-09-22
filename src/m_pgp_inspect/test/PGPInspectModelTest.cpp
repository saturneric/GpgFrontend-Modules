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
          "tagName": "Compressed Data",
          "offset": 0,
          "headerVersion": "new",
          "headerLength": 2,
          "bodyLength": 189,
          "lengthType": "fixed",
          "fields": [{"label": "Compression Algorithm", "value": "ZIP"}],
          "children": [
            {
              "tag": 2,
              "tagName": "Signature",
              "offset": 0,
              "headerVersion": "old",
              "headerLength": 3,
              "bodyLength": 100,
              "lengthType": "fixed",
              "version": 4,
              "fields": [{"label": "Hash Algorithm", "value": "Sha256"}],
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
  const auto document = ParsePGPInspectDocument(kDocument);
  const auto& packets = document.blocks.at(0).packets;

  ASSERT_EQ(packets.size(), 1);
  EXPECT_EQ(packets.at(0).tag, 8);
  ASSERT_EQ(packets.at(0).children.size(), 1);
  EXPECT_EQ(packets.at(0).children.at(0).tag, 2);
  EXPECT_EQ(packets.at(0).children.at(0).version, 4);
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
  const auto document = ParsePGPInspectDocument(kDocument);
  const auto& packet = document.blocks.at(0).packets.at(0);
  EXPECT_EQ(packet.version, -1);
  EXPECT_FALSE(packet.Malformed());
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
  EXPECT_TRUE(packet.tag_name.isEmpty());
  EXPECT_EQ(packet.body_length, 0);
}

TEST(PGPInspectModelTest, ADetailRowWithNeitherHalfIsDropped) {
  const auto document = ParsePGPInspectDocument(
      R"({"blocks":[{"packets":[{"fields":[{},{"label":"A","value":"B"}]}]}]})");

  const auto& fields = document.blocks.at(0).packets.at(0).fields;
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

TEST(PGPInspectModelTest, APacketSummaryNamesTagVersionOffsetAndSize) {
  // Held by value: a reference into the temporary document would dangle as
  // soon as this statement ends.
  const auto document = ParsePGPInspectDocument(kDocument);
  const auto& packet = document.blocks.at(0).packets.at(0).children.at(0);
  const auto summary = PGPInspectPacketSummary(packet, 1);

  EXPECT_TRUE(summary.contains("#1"));
  EXPECT_TRUE(summary.contains("Signature"));
  EXPECT_TRUE(summary.contains("tag 2"));
  EXPECT_TRUE(summary.contains("v4"));
  EXPECT_TRUE(summary.contains("@0x0000"));
  // Header plus body: the bytes this packet occupies on the wire.
  EXPECT_TRUE(summary.contains("103"));
}

TEST(PGPInspectModelTest, ASummaryFlagsANonFixedLength) {
  PGPInspectPacket packet;
  packet.tag = 11;
  packet.tag_name = "Literal Data";
  packet.length_type = "partial";

  EXPECT_TRUE(PGPInspectPacketSummary(packet, 2).contains("partial"));
}

TEST(PGPInspectModelTest, ASummaryOfANamelessPacketStillReads) {
  PGPInspectPacket packet;
  packet.tag = 60;

  const auto summary = PGPInspectPacketSummary(packet, 1);
  EXPECT_TRUE(summary.contains("Unknown"));
  EXPECT_TRUE(summary.contains("tag 60"));
}

TEST(PGPInspectModelTest, ABlockSummaryDistinguishesTheThreeKinds) {
  const auto armored = ParsePGPInspectDocument(kDocument).blocks.at(0);
  EXPECT_TRUE(PGPInspectBlockSummary(armored, 1).contains("PGP SIGNATURE"));

  PGPInspectBlock cleartext;
  cleartext.kind = "cleartext";
  EXPECT_TRUE(PGPInspectBlockSummary(cleartext, 1).contains("cleartext"));

  PGPInspectBlock binary;
  binary.kind = "binary";
  EXPECT_TRUE(PGPInspectBlockSummary(binary, 1).contains("binary"));
}
