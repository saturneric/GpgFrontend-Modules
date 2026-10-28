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

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "NoteFiles.h"

using namespace Qt::StringLiterals;
using namespace gf::notes;

TEST(NoteFilesTest, AWriteReplacesTheFileWholeAndOwnerOnly) {
  QTemporaryDir dir;
  const auto path = dir.filePath(u"n.textpack.gpg"_s);
  ASSERT_TRUE(
      WriteNoteFile(path, EncryptedPackage::FromCiphertext("one"), nullptr));
  ASSERT_TRUE(
      WriteNoteFile(path, EncryptedPackage::FromCiphertext("two"), nullptr));
  QByteArray back;
  ASSERT_TRUE(ReadNoteFile(path, 1024, &back, nullptr));
  EXPECT_EQ(back, "two");
  const auto perms = QFile::permissions(path);
  EXPECT_FALSE(perms & (QFileDevice::ReadGroup | QFileDevice::ReadOther));
  EXPECT_EQ(QDir(dir.path()).entryList(QDir::Files).size(), 1)
      << "no temporary left";
}

TEST(NoteFilesTest, ASymbolicLinkIsNotFollowed) {
  QTemporaryDir dir;
  const auto target = dir.filePath(u"elsewhere"_s);
  QFile(target).open(QIODevice::WriteOnly);
  const auto link = dir.filePath(u"n.textpack.gpg"_s);
  ASSERT_TRUE(QFile::link(target, link));
  QString why;
  EXPECT_FALSE(
      WriteNoteFile(link, EncryptedPackage::FromCiphertext("x"), &why));
  QByteArray back;
  EXPECT_FALSE(ReadNoteFile(link, 1024, &back, &why));
  QFile t(target);
  t.open(QIODevice::ReadOnly);
  EXPECT_TRUE(t.readAll().isEmpty()) << "the link's target is untouched";
}

TEST(NoteFilesTest, AFailedWriteLeavesTheOldFile) {
  QTemporaryDir dir;
  const auto path = dir.filePath(u"n.textpack.gpg"_s);
  ASSERT_TRUE(
      WriteNoteFile(path, EncryptedPackage::FromCiphertext("old"), nullptr));
  // A directory that refuses new files: the temporary cannot be made.
  QFile::setPermissions(dir.path(),
                        QFileDevice::ReadOwner | QFileDevice::ExeOwner);
  QString why;
  EXPECT_FALSE(
      WriteNoteFile(path, EncryptedPackage::FromCiphertext("new"), &why));
  QFile::setPermissions(dir.path(), QFileDevice::ReadOwner |
                                        QFileDevice::WriteOwner |
                                        QFileDevice::ExeOwner);
  QByteArray back;
  ASSERT_TRUE(ReadNoteFile(path, 1024, &back, nullptr));
  EXPECT_EQ(back, "old");
  EXPECT_EQ(QDir(dir.path()).entryList(QDir::Files).size(), 1);
}

TEST(NoteFilesTest, OversizedFilesAreNotRead) {
  QTemporaryDir dir;
  const auto path = dir.filePath(u"n"_s);
  ASSERT_TRUE(WriteNoteFile(
      path, EncryptedPackage::FromCiphertext(QByteArray(100, 'x')), nullptr));
  QByteArray back;
  EXPECT_FALSE(ReadNoteFile(path, 99, &back, nullptr));
}

TEST(NoteFilesTest, AnUnencryptedNoteIsWrittenTheSameCarefulWay) {
  QTemporaryDir dir;
  const auto path = dir.filePath(u"n.textpack"_s);
  ASSERT_TRUE(
      WriteNoteFile(path, PlainTextPack::FromUnencryptedNote("PK.."), nullptr));
  QByteArray back;
  ASSERT_TRUE(ReadNoteFile(path, 1024, &back, nullptr));
  EXPECT_EQ(back, "PK..");
  EXPECT_FALSE(QFile::permissions(path) &
               (QFileDevice::ReadGroup | QFileDevice::ReadOther));

  const auto link = dir.filePath(u"l.textpack"_s);
  ASSERT_TRUE(QFile::link(path, link));
  EXPECT_FALSE(
      WriteNoteFile(link, PlainTextPack::FromUnencryptedNote("x"), nullptr));
}
