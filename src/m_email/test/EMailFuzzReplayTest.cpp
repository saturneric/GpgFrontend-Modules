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

// Replays the fuzz harness over the committed corpus.
//
// gf_mod_email_fuzz has existed for a while and has never run anywhere: it
// needs clang and a sanitizer build, it is off by default, and it is
// deliberately not named *_test so the ordinary runner will not pick it up and
// fuzz forever. A harness nobody runs is not coverage, and it can rot --
// stop compiling, or stop reaching the functions it claims to cover -- with
// nothing to say so.
//
// This is not fuzzing. It is a regression test that the harness still builds,
// still runs, and still survives every input we already know about, on an
// ordinary gcc build with no special tooling. scripts/run_fuzz_smoke.sh does
// the actual bounded fuzzing where clang is available.

#include <gtest/gtest.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <cstdint>

// The harness's own entry point, compiled into this binary.
extern "C" auto LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) -> int;

namespace {

auto Feed(const QByteArray& raw) -> int {
  return LLVMFuzzerTestOneInput(
      reinterpret_cast<const uint8_t*>(raw.constData()),
      static_cast<size_t>(raw.size()));
}

auto CorpusFiles(const QString& which) -> QStringList {
  QDir dir(QString(GF_EMAIL_TEST_CORPUS_DIR) + "/" + which);
  QStringList out;
  for (const auto& name : dir.entryList({"*.eml"}, QDir::Files, QDir::Name)) {
    out << dir.filePath(name);
  }
  return out;
}

}  // namespace

TEST(EMailFuzzReplayTest, EveryPublicCorpusMessageSurvivesTheHarness) {
  const auto files = CorpusFiles("public");
  ASSERT_FALSE(files.isEmpty()) << "the hostile corpus is missing";

  for (const auto& path : files) {
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::ReadOnly)) << path.toStdString();
    EXPECT_EQ(Feed(f.readAll()), 0) << path.toStdString();
  }
}

TEST(EMailFuzzReplayTest, EveryGoldenCorpusMessageSurvivesTheHarness) {
  const auto files = CorpusFiles("golden");
  ASSERT_FALSE(files.isEmpty()) << "the golden corpus is missing";

  for (const auto& path : files) {
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::ReadOnly)) << path.toStdString();
    EXPECT_EQ(Feed(f.readAll()), 0) << path.toStdString();
  }
}

TEST(EMailFuzzReplayTest, DegenerateInputsSurviveTheHarness) {
  // The shapes a corpus tends not to contain, and which a parser tends to
  // assume away.
  EXPECT_EQ(Feed(QByteArray()), 0);
  EXPECT_EQ(Feed(QByteArray("\0", 1)), 0);
  EXPECT_EQ(Feed(QByteArray(1024, '\0')), 0);
  EXPECT_EQ(Feed(QByteArray("\r\n\r\n")), 0);
  EXPECT_EQ(Feed(QByteArray("Content-Type: multipart/mixed; boundary=\"\"")),
            0);
  EXPECT_EQ(
      Feed(QByteArray("From: a\r\nContent-Type: multipart/mixed\r\n\r\n--")),
      0);
}

TEST(EMailFuzzReplayTest, TruncationAtEveryOffsetSurvivesTheHarness) {
  // Every prefix of a well-formed signed message. Truncation is the cheapest
  // way to reach a parser that trusted a length it had not yet read.
  const auto files = CorpusFiles("golden");
  ASSERT_FALSE(files.isEmpty());

  QFile f(files.first());
  ASSERT_TRUE(f.open(QIODevice::ReadOnly));
  const auto whole = f.readAll();

  // Every 16th offset: enough to cover the header/body/boundary transitions
  // without making this test the slowest in the suite.
  for (int cut = 0; cut <= whole.size(); cut += 16) {
    EXPECT_EQ(Feed(whole.left(cut)), 0) << "truncated at " << cut;
  }
}
