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

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "AppImageUpdate.h"

namespace {

auto Sha256(const QByteArray& data) -> QString {
  return QString::fromLatin1(
      QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

auto Read(const QString& path) -> QByteArray {
  QFile f(path);
  return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

/// A transfer that writes @p body in small pieces, as a network would, and
/// stops at the first write the sink refuses.
auto Serve(const QByteArray& body, bool ok = true, const QString& error = {})
    -> AppImageInstaller::Download {
  return [=](const QUrl& /*url*/, QIODevice* sink,
             const std::function<void(qint64, qint64)>& progress,
             const std::function<void(bool, const QString&)>& done) {
    for (qsizetype at = 0; at < body.size(); at += 7) {
      const auto piece = body.mid(at, 7);
      if (sink->write(piece) != piece.size()) {
        done(false, QStringLiteral("Operation canceled"));
        return;
      }
      progress(at + piece.size(), body.size());
    }
    done(ok, error);
  };
}

class AppImageInstallerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(dir_.isValid());
    image_ = dir_.path() + "/GpgFrontend.AppImage";
    QFile f(image_);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(kOld);
    f.close();
    ASSERT_TRUE(QFile::setPermissions(
        image_, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                    QFileDevice::ExeOwner | QFileDevice::ReadGroup |
                    QFileDevice::ExeGroup | QFileDevice::ReadOther |
                    QFileDevice::ExeOther));
  }

  /// Runs an install to its end: whether it succeeded, and why not.
  auto Run(const UpdateFile& file, const AppImageInstaller::Download& serve)
      -> std::pair<bool, QString> {
    AppImageInstaller installer(image_, file, serve);
    std::pair<bool, QString> result{false, QStringLiteral("no answer")};
    int answers = 0;
    QObject::connect(&installer, &AppImageInstaller::Finished,
                     [&](bool ok, const QString& error) {
                       result = {ok, error};
                       ++answers;
                     });
    installer.Start();
    EXPECT_EQ(answers, 1);
    return result;
  }

  auto File(const QByteArray& body) -> UpdateFile {
    return {QUrl("https://builds.example/gpgfrontend/x.AppImage"), Sha256(body),
            body.size()};
  }

  /// Nothing beside the image: no temporary file left behind.
  void ExpectOnlyTheImage() {
    EXPECT_EQ(QDir(dir_.path()).entryList(QDir::Files | QDir::Hidden),
              QStringList{"GpgFrontend.AppImage"});
  }

  static inline const QByteArray kOld = QByteArray("old image ").repeated(20);
  static inline const QByteArray kNew =
      QByteArray("the new image ").repeated(50);

  QTemporaryDir dir_;
  QString image_;
};

}  // namespace

TEST_F(AppImageInstallerTest, TheVerifiedFileReplacesTheImage) {
  const auto [ok, error] = Run(File(kNew), Serve(kNew));
  EXPECT_TRUE(ok) << error.toStdString();
  EXPECT_EQ(Read(image_), kNew);
  // Run as the old one was: still executable for everyone it was before.
  const auto perms = QFile::permissions(image_);
  EXPECT_TRUE(perms & QFileDevice::ExeOwner);
  EXPECT_TRUE(perms & QFileDevice::ExeOther);
  ExpectOnlyTheImage();
}

TEST_F(AppImageInstallerTest, AnyOtherFileLeavesTheImageAlone) {
  auto tampered = kNew;
  tampered[10] = 'X';
  auto [ok, error] = Run(File(kNew), Serve(tampered));
  EXPECT_FALSE(ok);
  EXPECT_EQ(error, "The download is not the file the signed update describes.");
  EXPECT_EQ(Read(image_), kOld);
  ExpectOnlyTheImage();

  // Short of the stated size.
  std::tie(ok, error) = Run(File(kNew), Serve(kNew.left(100)));
  EXPECT_FALSE(ok);
  EXPECT_EQ(Read(image_), kOld);

  // Past it: the transfer is stopped there, and the reason is the file.
  std::tie(ok, error) = Run(File(kNew), Serve(kNew + "trailing"));
  EXPECT_FALSE(ok);
  EXPECT_EQ(error, "The download is not the file the signed update describes.");
  EXPECT_EQ(Read(image_), kOld);
  ExpectOnlyTheImage();
}

TEST_F(AppImageInstallerTest, AFailedTransferLeavesTheImageAlone) {
  const auto [ok, error] =
      Run(File(kNew), Serve(kNew.left(50), false, "HTTP 404"));
  EXPECT_FALSE(ok);
  EXPECT_EQ(error, "The download failed: HTTP 404");
  EXPECT_EQ(Read(image_), kOld);
  ExpectOnlyTheImage();
}

TEST_F(AppImageInstallerTest, AnUndescribedFileIsNotEvenFetched) {
  bool fetched = false;
  const AppImageInstaller::Download spy =
      [&](const QUrl&, QIODevice*, const std::function<void(qint64, qint64)>&,
          const std::function<void(bool, const QString&)>& done) {
        fetched = true;
        done(true, {});
      };
  auto file = File(kNew);
  file.url = QUrl("http://builds.example/x.AppImage");
  EXPECT_FALSE(Run(file, spy).first);
  file = File(kNew);
  file.sha256.clear();
  EXPECT_FALSE(Run(file, spy).first);
  file = File(kNew);
  file.size = 0;
  EXPECT_FALSE(Run(file, spy).first);
  EXPECT_FALSE(fetched);
  EXPECT_EQ(Read(image_), kOld);
}

// Closing the dialog halfway: the download is dropped with the installer,
// and so is the half-written file.
TEST_F(AppImageInstallerTest, StoppingHalfwayLeavesNothingBehind) {
  QIODevice* held = nullptr;
  {
    AppImageInstaller installer(
        image_, File(kNew),
        [&](const QUrl&, QIODevice* sink,
            const std::function<void(qint64, qint64)>&,
            const std::function<void(bool, const QString&)>&) {
          sink->write(kNew.left(30));
          held = sink;  // and never answers
        });
    installer.Start();
    ASSERT_NE(held, nullptr);
  }
  EXPECT_EQ(Read(image_), kOld);
  ExpectOnlyTheImage();
}

TEST(ReplaceableAppImageTest, OnlyAWritableImageInAWritableFolder) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const auto image = dir.path() + "/GpgFrontend.AppImage";
  QFile f(image);
  ASSERT_TRUE(f.open(QIODevice::WriteOnly));
  f.close();

  EXPECT_EQ(ReplaceableAppImage(image), image);
  EXPECT_TRUE(ReplaceableAppImage({}).isEmpty());
  EXPECT_TRUE(ReplaceableAppImage(dir.path() + "/missing.AppImage").isEmpty());
  EXPECT_TRUE(ReplaceableAppImage(dir.path()).isEmpty());  // a folder
  EXPECT_TRUE(ReplaceableAppImage("GpgFrontend.AppImage").isEmpty());

  const auto link = dir.path() + "/link.AppImage";
  ASSERT_TRUE(QFile::link(image, link));
  EXPECT_TRUE(ReplaceableAppImage(link).isEmpty());

  // A folder the user cannot write: the new file has nowhere to go.
  const auto locked = QFileDevice::ReadOwner | QFileDevice::ExeOwner;
  ASSERT_TRUE(QFile::setPermissions(dir.path(), locked));
  EXPECT_TRUE(ReplaceableAppImage(image).isEmpty());
  QFile::setPermissions(dir.path(), locked | QFileDevice::WriteOwner);
}
