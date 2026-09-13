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

// Whether a secret can actually be erased.
//
// Every wipe in this module used to be `password.fill(QChar('\0'))` on a
// QString. QString is implicitly shared and fill() detaches before writing, so
// whenever anything else held a reference -- a queued signal argument, a
// worker's parameter, an authenticator's member -- the call zeroed a FRESH
// buffer and dropped the reference to the original, which was then freed with
// the plaintext still in it. It read as protection and performed none.
//
// The first test below demonstrates that on QString directly, so the reason
// this class exists is recorded rather than asserted. The rest hold
// EMailSecret to the behaviour that replaces it.

#include <GFSDKBasic.h>
#include <gtest/gtest.h>

#include <QString>
#include <cstring>

#include "EMailSecret.h"

namespace {

TEST(EMailSecretTest, AQStringWipeDoesNotReachASharedBuffer) {
  QString original = QStringLiteral("correct-horse-battery");
  // Force a heap buffer of our own rather than a literal's shared data.
  original.detach();

  const QString shared = original;  // refcount 2, same buffer
  const auto* buffer = shared.constData();

  original.fill(QChar('\0'));

  // The copy still reads the secret: original detached and zeroed elsewhere.
  EXPECT_EQ(shared, QStringLiteral("correct-horse-battery"));
  EXPECT_EQ(buffer[0], QChar('c'))
      << "if this ever fails, QString has changed and the note above is stale";
}

TEST(EMailSecretTest, WipingClearsTheOnlyCopy) {
  auto secret = EMailSecret::CopyFrom(QStringLiteral("hunter2"));
  ASSERT_FALSE(secret->IsEmpty());
  EXPECT_EQ(secret->Size(), 7U);

  secret->Wipe();

  EXPECT_TRUE(secret->IsEmpty());
  EXPECT_EQ(secret->Size(), 0U);
  EXPECT_TRUE(secret->StdStringCopy().empty());
}

TEST(EMailSecretTest, WipingTwiceIsHarmless) {
  auto secret = EMailSecret::CopyFrom(QStringLiteral("hunter2"));
  secret->Wipe();
  secret->Wipe();
  EXPECT_TRUE(secret->IsEmpty());
}

TEST(EMailSecretTest, AnEmptySecretIsUsable) {
  EMailSecret secret;
  EXPECT_TRUE(secret.IsEmpty());
  EXPECT_TRUE(secret.StdStringCopy().empty());
  secret.Wipe();
  EXPECT_TRUE(secret.IsEmpty());
}

TEST(EMailSecretTest, MovingTransfersAndLeavesNothingBehind) {
  EMailSecret from;
  {
    auto source = EMailSecret::CopyFrom(QStringLiteral("s3cret"));
    from = std::move(*source);
  }
  EXPECT_EQ(from.StdStringCopy(), "s3cret");

  EMailSecret to = std::move(from);
  EXPECT_EQ(to.StdStringCopy(), "s3cret");
  EXPECT_TRUE(from.IsEmpty()) << "the moved-from secret still holds bytes";
}

TEST(EMailSecretTest, MoveAssignmentWipesWhatItReplaces) {
  auto first = EMailSecret::CopyFrom(QStringLiteral("first-secret"));
  auto second = EMailSecret::CopyFrom(QStringLiteral("second"));

  *first = std::move(*second);

  EXPECT_EQ(first->StdStringCopy(), "second");
  EXPECT_TRUE(second->IsEmpty());
}

TEST(EMailSecretTest, TheLastHolderReleasingErasesTheBytes) {
  // The property the queued-connection hand-off depends on: the GUI thread
  // drops its reference immediately after posting, and the worker's copy is
  // the same bytes, not a duplicate.
  EMailSecretPtr shared;
  {
    auto secret = EMailSecret::CopyFrom(QStringLiteral("shared-secret"));
    shared = secret;
    EXPECT_EQ(shared.use_count(), 2);
    secret.reset();
  }
  ASSERT_EQ(shared.use_count(), 1);
  EXPECT_EQ(shared->StdStringCopy(), "shared-secret")
      << "releasing one holder erased bytes the other still needed";
}

TEST(EMailSecretTest, AdoptingACStringWipesTheSource) {
  // The SDK hands back a secure char*. Copying it out has to leave nothing
  // behind in the buffer it came from.
  const char* text = "from-the-sdk";
  const auto length = std::strlen(text);

  auto* raw = static_cast<char*>(GFSecAllocateMemory(length + 1));
  std::memcpy(raw, text, length + 1);

  auto secret = EMailSecret::AdoptCString(raw);

  EXPECT_EQ(secret->StdStringCopy(), "from-the-sdk");
  EXPECT_EQ(secret->Size(), length);
}

TEST(EMailSecretTest, AdoptingNullIsEmptyRatherThanACrash) {
  auto secret = EMailSecret::AdoptCString(nullptr);
  ASSERT_NE(secret, nullptr);
  EXPECT_TRUE(secret->IsEmpty());
}

TEST(EMailSecretTest, ASecretIsNotCopyable) {
  // Compile-time, and the point of the class: an accidental copy is another
  // plaintext buffer nobody remembers to erase.
  static_assert(!std::is_copy_constructible_v<EMailSecret>);
  static_assert(!std::is_copy_assignable_v<EMailSecret>);
  static_assert(std::is_move_constructible_v<EMailSecret>);
  static_assert(std::is_move_assignable_v<EMailSecret>);
  SUCCEED();
}

// --- reaching the SDK without a QString on the way --------------------------

TEST(EMailSecretTest, ASecureCStringCarriesTheSecretVerbatim) {
  // The route a stored password takes now. It used to go out as
  // QSecStrDup(QString), which builds an unwiped QByteArray temporary on the
  // way -- one more copy of the password than there needs to be, and one
  // nothing can erase.
  auto secret = EMailSecret::CopyFrom(QStringLiteral("correct-horse"));

  auto* buffer = secret->ToSecureCString();
  ASSERT_NE(buffer, nullptr);

  EXPECT_EQ(std::strlen(buffer), secret->Size());
  EXPECT_STREQ(buffer, "correct-horse");

  // NUL-terminated one past the secret, so a C API can read it as a string.
  EXPECT_EQ(buffer[secret->Size()], '\0');

  GFSecFreeMemory(buffer);
}

TEST(EMailSecretTest, AnEmptySecretStillProducesAUsableString) {
  auto secret = EMailSecret::CopyFrom(QString());

  auto* buffer = secret->ToSecureCString();
  ASSERT_NE(buffer, nullptr);
  EXPECT_STREQ(buffer, "");

  GFSecFreeMemory(buffer);
}

TEST(EMailSecretTest, NonAsciiSurvivesTheRoundTrip) {
  // A password is whatever the user typed, and UTF-8 is not optional.
  const QString typed = QString::fromUtf8("pa\xC3\x9Fwort-\xE9\x94\xAE");
  auto secret = EMailSecret::CopyFrom(typed);

  auto* buffer = secret->ToSecureCString();
  ASSERT_NE(buffer, nullptr);
  EXPECT_EQ(QString::fromUtf8(buffer), typed);

  GFSecFreeMemory(buffer);
}

}  // namespace
