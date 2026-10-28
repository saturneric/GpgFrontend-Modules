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

#include "NoteImage.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>

using namespace Qt::StringLiterals;

namespace gf::notes {

namespace {

/// The format the bytes ARE, not what anything says they are.
auto Sniff(const QByteArray& b) -> QByteArray {
  if (b.startsWith("\x89PNG\r\n\x1a\n")) return "png";
  if (b.startsWith("\xff\xd8\xff")) return "jpeg";
  if (b.startsWith("GIF87a") || b.startsWith("GIF89a")) return "gif";
  if (b.size() >= 12 && b.startsWith("RIFF") && b.mid(8, 4) == "WEBP")
    return "webp";
  return {};
}

auto Decode(const QByteArray& bytes, QImage* image, QByteArray* format,
            QString* why) -> bool {
  auto fail = [why](const QString& w) {
    if (why != nullptr) *why = w;
    return false;
  };
  if (bytes.size() > kMaxImageBytes)
    return fail(u"the image is larger than %1 MiB"_s.arg(kMaxImageBytes /
                                                         (1024 * 1024)));
  const auto fmt = Sniff(bytes);
  if (fmt.isEmpty()) return fail(u"not a PNG, JPEG, GIF or WebP image"_s);

  QBuffer buffer;
  buffer.setData(bytes);
  buffer.open(QIODevice::ReadOnly);
  QImageReader reader(&buffer, fmt);
  reader.setAutoTransform(true);
  // Bounded before decoding: a header can claim any size.
  reader.setAllocationLimit(
      static_cast<int>(kMaxImagePixels * 4 / (1024 * 1024)) + 1);
  const auto size = reader.size();
  if (!size.isValid() ||
      static_cast<qint64>(size.width()) * size.height() > kMaxImagePixels) {
    return fail(u"the image has more than 40 million pixels"_s);
  }
  if (!reader.read(image) || image->isNull()) {
    return fail(u"the image could not be read"_s);
  }
  *format = fmt;
  return true;
}

}  // namespace

auto PrepareImage(const QByteArray& bytes, QString* why)
    -> std::optional<StoredImage> {
  QImage image;
  QByteArray format;
  if (!Decode(bytes, &image, &format, why)) return std::nullopt;

  // Lossless where it matters (transparency, screenshots and other images
  // with few colours), JPEG for photographs.
  const bool png =
      image.hasAlphaChannel() || format == "png" || format == "gif";

  // A new image from the pixels alone: QImage keeps a file's text chunks and
  // writes them back out, so re-encoding the decoded image is not enough.
  const auto pixels = image.convertToFormat(
      image.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32);
  image = QImage(pixels.constBits(), pixels.width(), pixels.height(),
                 pixels.bytesPerLine(), pixels.format())
              .copy();
  QBuffer out;
  out.open(QIODevice::WriteOnly);
  QImageWriter writer(&out, png ? "png" : "jpeg");
  if (!png) writer.setQuality(90);
  if (!writer.write(image)) {
    if (why != nullptr) *why = u"the image could not be stored"_s;
    return std::nullopt;
  }
  StoredImage stored;
  stored.data = out.data();
  stored.mime = png ? u"image/png"_s : u"image/jpeg"_s;
  const auto hash =
      QCryptographicHash::hash(stored.data, QCryptographicHash::Sha256)
          .toHex()
          .left(16);
  stored.path = u"assets/%1.%2"_s.arg(QString::fromLatin1(hash),
                                      png ? u"png"_s : u"jpg"_s);
  return stored;
}

auto IsDisplayableImage(const QByteArray& bytes, QString* mime) -> bool {
  QImage image;
  QByteArray format;
  if (!Decode(bytes, &image, &format, nullptr)) return false;
  if (mime != nullptr) *mime = u"image/"_s + QString::fromLatin1(format);
  return true;
}

}  // namespace gf::notes
