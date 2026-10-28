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

#include "NoteFiles.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

using namespace Qt::StringLiterals;

namespace gf::notes {

namespace {

auto WriteWhole(const QString& path, const QByteArray& bytes, QString* why)
    -> bool {
  auto fail = [why](const QString& w) {
    if (why != nullptr) *why = w;
    return false;
  };
  const QFileInfo info(path);
  if (info.isSymLink()) return fail(u"the destination is a symbolic link"_s);
  if (info.exists() && !info.isFile())
    return fail(u"the destination is not a file"_s);

  QSaveFile file(path);
  // Never written in place: a failure must leave the old file whole.
  file.setDirectWriteFallback(false);
  if (!file.open(QIODevice::WriteOnly)) return fail(file.errorString());
  file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  if (file.write(bytes) != bytes.size()) {
    const auto error = file.errorString();
    file.cancelWriting();
    return fail(error);
  }
  if (!file.commit()) return fail(file.errorString());
  return true;
}

}  // namespace

auto WriteNoteFile(const QString& path, const EncryptedPackage& package,
                   QString* why) -> bool {
  return WriteWhole(path, package.Bytes(), why);
}

auto WriteNoteFile(const QString& path, const PlainTextPack& pack, QString* why)
    -> bool {
  return WriteWhole(path, pack.Bytes(), why);
}

auto ReadNoteFile(const QString& path, qint64 max_bytes, QByteArray* out,
                  QString* why) -> bool {
  auto fail = [why](const QString& w) {
    if (why != nullptr) *why = w;
    return false;
  };
  const QFileInfo info(path);
  if (info.isSymLink()) return fail(u"the file is a symbolic link"_s);
  if (!info.isFile()) return fail(u"not a file"_s);
  if (info.size() > max_bytes) return fail(u"the file is too large"_s);
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return fail(file.errorString());
  *out = file.readAll();
  return true;
}

}  // namespace gf::notes
