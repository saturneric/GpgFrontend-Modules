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
#pragma once

#include <QObject>
#include <QSaveFile>
#include <QString>
#include <QUrl>
#include <functional>

/**
 * @file AppImageUpdate.h
 * @brief Replacing the running AppImage with a newer one, in place.
 *
 * The file comes from a release descriptor the signed update index vouches
 * for, by hash: the download counts only when its size and sha256 are
 * exactly the descriptor's. It is written beside the image and renamed over
 * it in one step (QSaveFile), so the image on disk is always either the old
 * one or the new one, whole. The running process keeps the old file open
 * and goes on unaffected; the next start is the new version.
 *
 * Nothing here talks to the Host or draws anything: the transfer is handed
 * in, so the whole thing runs in a test against a temporary directory.
 */

/// The file that replaces the image, as a verified descriptor states it.
struct UpdateFile {
  QUrl url;
  QString sha256;  ///< lowercase hex
  qint64 size = 0;
};

/**
 * @brief The image this process may replace: $APPIMAGE's file, when it is a
 * regular file in a directory this user can write. Empty otherwise (an image
 * on a read-only stick, in a system directory, or not an AppImage at all):
 * there the update stays a download.
 */
auto ReplaceableAppImage(const QString& appimage_env) -> QString;

class AppImageInstaller : public QObject {
  Q_OBJECT

 public:
  /// Streams @p url into @p sink, reporting progress; done() once, with
  /// false and a reason when the transfer failed. Writes the sink refuses
  /// must end the transfer.
  using Download = std::function<void(
      const QUrl& url, QIODevice* sink,
      std::function<void(qint64 received, qint64 total)> progress,
      std::function<void(bool ok, const QString& error)> done)>;

  AppImageInstaller(QString image, UpdateFile file, Download download,
                    QObject* parent = nullptr);
  ~AppImageInstaller() override;

  /// Once. Finished() follows, with the image replaced or untouched.
  void Start();

 signals:
  void Progress(qint64 received, qint64 total);
  /// @p ok: the image on disk is now the update. Otherwise it is exactly as
  /// it was, and @p error says why.
  void Finished(bool ok, const QString& error);

 private:
  class Sink;

  void done(bool ok, const QString& error);

  QString image_;
  UpdateFile file_;
  Download download_;
  QSaveFile* save_ = nullptr;
  Sink* sink_ = nullptr;
  bool started_ = false;
};
