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

/**
 * @file
 * @brief libFuzzer entry point for the MIME, header and filename parsers.
 *
 * These parsers run synchronously on bytes that arrived from outside, before
 * anything has been authenticated -- a message is parsed in order to find out
 * whether it is signed at all. That makes them the widest attack surface in
 * the module, and the one place where "it did not crash on the corpus" is not
 * good enough.
 *
 * The corpus under test/corpus/public is the seed set -- those messages are
 * malformed on purpose, so they are the interesting starting points. Build
 * with:
 *
 *   -DGPGFRONTEND_MODULES_BUILD_FUZZERS=ON
 *   -DCMAKE_CXX_COMPILER=clang++
 *   -DCMAKE_CXX_FLAGS="-fsanitize=fuzzer-no-link,address,undefined"
 *
 * and run: gf_mod_email_fuzz test/corpus/public -max_len=1048576
 *
 * Nothing here asserts on content. The properties being fuzzed are that the
 * parsers terminate, stay inside their buffers, and respect EMailParseLimits
 * however the input is shaped.
 */

#include <QByteArray>
#include <QCoreApplication>
#include <QStringList>
#include <cstddef>
#include <cstdint>

#include "EMailHelper.h"
#include "EMailModel.h"

namespace {

// Qt wants an application object to exist before some of its machinery is
// used. Built once, on the first input, and deliberately never destroyed:
// tearing it down between inputs would dominate the run.
void EnsureQtInitialized() {
  static int argc = 1;
  static char arg0[] = "gf_mod_email_fuzz";
  static char* argv[] = {arg0, nullptr};
  static QCoreApplication* app = nullptr;
  if (app == nullptr && QCoreApplication::instance() == nullptr) {
    app = new QCoreApplication(argc, argv);
  }
}

}  // namespace

extern "C" auto LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
    -> int {
  EnsureQtInitialized();

  // Bound the input the way the application does. Beyond this the module
  // refuses the file outright, so fuzzing past it would explore code that
  // cannot be reached with a real message.
  if (size > 4u * 1024u * 1024u) return 0;

  const QByteArray raw(reinterpret_cast<const char*>(data),
                       static_cast<int>(size));

  vmime::shared_ptr<vmime::message> message;
  if (!CheckIfEMLMessage(raw, message)) return 0;

  // Tighter than the defaults on purpose: a fuzzer finds the pathological
  // shapes quickly, and the limits themselves are part of what is under test.
  EMailParseLimits limits;
  limits.max_depth = 6;
  limits.max_parts = 32;
  limits.max_total_bytes = 8LL * 1024 * 1024;

  EMailPart root;
  QList<EMailSignatureRegion> regions;
  if (ParseMimeTree(message, raw, root, regions, limits) != 0) return 0;

  // Every consumer of the tree, so a malformed message cannot reach one of
  // them through a path the corpus never exercises.
  (void)ClassifyOpenPGPStructure(root, regions);
  (void)SelectBodyPart(root, false);
  (void)SelectBodyPart(root, true);

  for (const auto* part : FlattenMimeTree(root)) {
    // Offsets come from the parser and index the original buffer; this is
    // where an off-by-one becomes an out-of-bounds read.
    (void)RawHeaderBlock(*part, raw);
  }

  EMailMetaData meta;
  (void)GetEMLMetaData(message, meta);
  (void)ExtractParts(message, meta, limits);

  // Attachment names are attacker-chosen strings that end up as filesystem
  // paths, so the sanitizer is fuzzed with whatever the message supplied.
  (void)UniqueAttachmentFileNames(meta.attachments);
  for (const auto& att : meta.attachments) {
    (void)SanitizeAttachmentFileName(att.filename, att.mime_type);
  }

  (void)InspectMessage(meta, root, regions);
  (void)PreflightMessage(meta, root, regions, {});

  for (const auto& address : meta.to + meta.cc + QStringList{meta.from}) {
    (void)LooksLikeSpoofedAddress(address);
  }

  // The derivation path parses nothing new, but it reads every field the
  // parser produced and is reached from a single click on a hostile message.
  EMailMetaData derived;
  BuildDerivedMetaData(meta, EMailReplyMode::kREPLY_ALL, "me@example.com",
                       derived);
  (void)BuildQuotedBody(meta, EMailReplyMode::kFORWARD);

  return 0;
}
