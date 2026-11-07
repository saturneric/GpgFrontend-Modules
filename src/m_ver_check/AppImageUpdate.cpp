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
#include "AppImageUpdate.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QPointer>

auto ReplaceableAppImage(const QString& appimage_env) -> QString {
  if (appimage_env.isEmpty()) return {};
  const QFileInfo image(appimage_env);
  if (!image.isAbsolute() || !image.isFile() || image.isSymLink() ||
      !image.isWritable()) {
    return {};
  }
  // The new file is written beside the image and renamed over it: the
  // directory, not only the file, has to take it.
  const QFileInfo dir(image.absolutePath());
  if (!dir.isDir() || !dir.isWritable()) return {};
  return image.absoluteFilePath();
}

/// Where the download goes: into the save file, hashed and counted on the
/// way, and refused past the size the descriptor states.
class AppImageInstaller::Sink : public QIODevice {
 public:
  Sink(QSaveFile* out, qint64 limit, QObject* parent)
      : QIODevice(parent), out_(out), limit_(limit) {
    open(QIODevice::WriteOnly);
  }

  [[nodiscard]] auto Received() const -> qint64 { return received_; }
  [[nodiscard]] auto Overflowed() const -> bool { return overflowed_; }
  [[nodiscard]] auto Sha256() const -> QString {
    return QString::fromLatin1(hash_.result().toHex());
  }

 protected:
  auto readData(char* /*data*/, qint64 /*max*/) -> qint64 override {
    return -1;
  }

  auto writeData(const char* data, qint64 len) -> qint64 override {
    if (received_ + len > limit_) {
      overflowed_ = true;
      return -1;
    }
    const auto written = out_->write(data, len);
    if (written != len) return -1;
    hash_.addData(QByteArrayView(data, len));
    received_ += len;
    return len;
  }

 private:
  QSaveFile* out_;
  qint64 limit_;
  qint64 received_ = 0;
  bool overflowed_ = false;
  QCryptographicHash hash_{QCryptographicHash::Sha256};
};

AppImageInstaller::AppImageInstaller(QString image, UpdateFile file,
                                     Download download, QObject* parent)
    : QObject(parent),
      image_(std::move(image)),
      file_(std::move(file)),
      download_(std::move(download)) {}

// An unfinished save file is discarded when it goes: closing the dialog in
// the middle of a download leaves the image as it was, and no stray file.
AppImageInstaller::~AppImageInstaller() = default;

void AppImageInstaller::Start() {
  if (started_) return;
  started_ = true;

  if (image_.isEmpty() || file_.size <= 0 || file_.sha256.size() != 64 ||
      file_.url.scheme() != QLatin1String("https")) {
    done(false, QCoreApplication::translate(
                    "UpdateTab", "The update does not describe its file."));
    return;
  }

  save_ = new QSaveFile(image_, this);
  // Never write over the image directly: a failure halfway would leave it
  // broken. Without a temporary file beside it there is no update.
  save_->setDirectWriteFallback(false);
  if (!save_->open(QIODevice::WriteOnly)) {
    done(false, QCoreApplication::translate(
                    "UpdateTab",
                    "The update could not be written next to the "
                    "application: %1")
                    .arg(save_->errorString()));
    return;
  }
  sink_ = new Sink(save_, file_.size, this);

  QPointer<AppImageInstaller> self(this);
  download_(
      file_.url, sink_,
      [self](qint64 received, qint64 total) {
        if (!self.isNull()) emit self->Progress(received, total);
      },
      [self](bool ok, const QString& error) {
        if (!self.isNull()) self->done(ok, error);
      });
}

void AppImageInstaller::done(bool ok, const QString& error) {
  if (save_ == nullptr) {
    emit Finished(false, error);
    return;
  }
  QSaveFile* save = save_;
  save_ = nullptr;  // once: a late callback finds nothing to finish

  auto refuse = [&](const QString& why) {
    save->cancelWriting();
    save->commit();  // with writing cancelled: removes the temporary file
    emit Finished(false, why);
  };

  // More than the descriptor states ends the transfer, which then reports
  // itself cancelled: the reason is the file, not the network.
  if (sink_->Overflowed()) {
    refuse(QCoreApplication::translate(
        "UpdateTab",
        "The download is not the file the signed update describes."));
    return;
  }
  if (!ok) {
    refuse(QCoreApplication::translate("UpdateTab", "The download failed: %1")
               .arg(error));
    return;
  }
  // Exactly the file the signed index vouches for, or nothing.
  if (sink_->Received() != file_.size || sink_->Sha256() != file_.sha256) {
    refuse(QCoreApplication::translate(
        "UpdateTab",
        "The download is not the file the signed update describes."));
    return;
  }

  // The image's own permissions, executable included, before it is renamed
  // into place: the new file is run as the old one was.
  const auto perms = QFile::permissions(image_) | QFileDevice::ExeOwner |
                     QFileDevice::ReadOwner | QFileDevice::WriteOwner;
  if (!save->setPermissions(perms) || !save->commit()) {
    // A failed commit has already removed the temporary file.
    const auto why = save->errorString();
    emit Finished(false, QCoreApplication::translate(
                             "UpdateTab",
                             "The update could not be written next to the "
                             "application: %1")
                             .arg(why));
    return;
  }
  emit Finished(true, {});
}
