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

#include "EMailHeaderView.h"

#include <QComboBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>

#include "EMailHelper.h"
#include "EMailViewStyle.h"

namespace {

enum HeaderMode : int { kBASIC = 0, kFULL = 1, kRAW = 2 };

// The fields that answer "who sent this, to whom, when and about what".
auto BasicFieldNames() -> QStringList {
  return {"From", "To", "Cc", "Bcc", "Reply-To", "Subject", "Date"};
}

}  // namespace

EMailHeaderView::EMailHeaderView(QWidget* parent) : QWidget(parent) {
  build_ui();
}

void EMailHeaderView::build_ui() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 6, 8, 6);
  layout->setSpacing(6);

  auto* row = new QHBoxLayout();
  row->setContentsMargins(0, 0, 0, 0);

  auto* caption = new QLabel(tr("Show:"), this);
  {
    auto palette = caption->palette();
    palette.setColor(QPalette::WindowText, EMailMutedColor(this));
    caption->setPalette(palette);
  }
  row->addWidget(caption);

  mode_ = new QComboBox(this);
  mode_->addItem(tr("Basic"), kBASIC);
  mode_->addItem(tr("Full"), kFULL);
  mode_->addItem(tr("Raw"), kRAW);
  mode_->setToolTip(
      tr("Basic and Full are decoded for reading. Raw is the original bytes, "
         "exactly as they arrived."));
  row->addWidget(mode_);
  row->addStretch();
  layout->addLayout(row);

  text_ = new QPlainTextEdit(this);
  text_->setReadOnly(true);
  text_->setLineWrapMode(QPlainTextEdit::NoWrap);
  // Headers are column-sensitive: folding and continuation lines only read
  // correctly in a fixed-width face.
  text_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  layout->addWidget(text_, 1);

  connect(mode_, &QComboBox::currentIndexChanged, this,
          [this](int) { refresh(); });
}

void EMailHeaderView::Clear() {
  loaded_ = false;
  root_ = EMailPart{};
  raw_.clear();
  text_->clear();
}

void EMailHeaderView::SetMessage(const EMailPart& root, const QByteArray& raw) {
  root_ = root;
  raw_ = raw;
  loaded_ = true;
  refresh();
}

void EMailHeaderView::refresh() {
  if (!loaded_) {
    text_->clear();
    return;
  }

  const auto mode = mode_->currentData().toInt();

  if (mode == kRAW) {
    // Straight from the original bytes. Decoded as Latin-1 rather than UTF-8
    // on purpose: every byte maps to exactly one character, so nothing is
    // replaced or merged and what is on screen is what is in the file.
    const auto block = RawHeaderBlock(root_, raw_);
    if (block.isEmpty()) {
      text_->setPlainText(tr("The raw header block is not available."));
      return;
    }
    text_->setPlainText(QString::fromLatin1(block));
    return;
  }

  const auto basic = BasicFieldNames();
  QStringList lines;
  for (const auto& field : root_.header_fields) {
    if (mode == kBASIC) {
      bool wanted = false;
      for (const auto& name : basic) {
        if (field.first.compare(name, Qt::CaseInsensitive) == 0) wanted = true;
      }
      if (!wanted) continue;
    }
    // Repeated fields are listed once per occurrence rather than merged: a
    // duplicated header is itself worth seeing.
    lines.append(QString("%1: %2").arg(field.first, field.second));
  }

  if (lines.isEmpty()) {
    text_->setPlainText(tr("This message has no headers to show."));
    return;
  }

  text_->setPlainText(lines.join("\n"));
}
