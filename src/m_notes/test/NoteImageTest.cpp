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

#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>

#include "NoteImage.h"

using namespace Qt::StringLiterals;
using namespace gf::notes;

namespace {

auto Encode(const QImage& image, const char* format, const QString& text = {})
    -> QByteArray {
  QBuffer b;
  b.open(QIODevice::WriteOnly);
  QImageWriter w(&b, format);
  if (!text.isEmpty()) w.setText(u"Description"_s, text);
  EXPECT_TRUE(w.write(image));
  return b.data();
}

auto Image(bool alpha) -> QImage {
  QImage img(64, 48, alpha ? QImage::Format_ARGB32 : QImage::Format_RGB32);
  for (int y = 0; y < img.height(); ++y)
    for (int x = 0; x < img.width(); ++x)
      img.setPixel(x, y,
                   qRgba(x * 4, y * 5, (x * y) & 0xff, alpha ? 128 : 255));
  return img;
}

}  // namespace

TEST(NoteImageTest, APngStaysAPngAndLosesItsMetadata) {
  const auto in = Encode(Image(true), "png", u"GFNOTE-SECRET-LOCATION"_s);
  ASSERT_TRUE(in.contains("GFNOTE-SECRET-LOCATION"));
  QString why;
  const auto out = PrepareImage(in, &why);
  ASSERT_TRUE(out.has_value()) << why.toStdString();
  EXPECT_EQ(out->mime, u"image/png"_s);
  EXPECT_TRUE(out->path.startsWith(u"assets/"_s));
  EXPECT_TRUE(out->path.endsWith(u".png"_s));
  EXPECT_FALSE(out->data.contains("GFNOTE-SECRET-LOCATION"));
}

TEST(NoteImageTest, APhotographBecomesAJpeg) {
  if (!QImageWriter::supportedImageFormats().contains("jpeg")) GTEST_SKIP();
  const auto out = PrepareImage(Encode(Image(false), "jpeg"), nullptr);
  ASSERT_TRUE(out.has_value());
  EXPECT_EQ(out->mime, u"image/jpeg"_s);
  EXPECT_TRUE(out->path.endsWith(u".jpg"_s));
}

TEST(NoteImageTest, TheSameImageGetsTheSameName) {
  const auto a = PrepareImage(Encode(Image(true), "png"), nullptr);
  const auto b = PrepareImage(Encode(Image(true), "png"), nullptr);
  ASSERT_TRUE(a.has_value() && b.has_value());
  EXPECT_EQ(a->path, b->path);
}

TEST(NoteImageTest, WhatIsNotAnAllowedImageIsRefused) {
  QString why;
  EXPECT_FALSE(PrepareImage("not an image", &why).has_value());
  EXPECT_FALSE(PrepareImage("<svg xmlns='http://www.w3.org/2000/svg'/>", &why)
                   .has_value());
  // A PNG signature with nothing behind it.
  EXPECT_FALSE(PrepareImage(QByteArray("\x89PNG\r\n\x1a\n", 8) + "junk", &why)
                   .has_value());
  EXPECT_FALSE(
      PrepareImage(QByteArray(kMaxImageBytes + 1, 'x'), &why).has_value());
}
