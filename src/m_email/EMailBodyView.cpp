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

#include "EMailBodyView.h"

EMailBodyView::EMailBodyView(QWidget* parent) : QPlainTextEdit(parent) {
  setReadOnly(true);
  setUndoRedoEnabled(false);

  // Source is read by its structure as much as by its words, so the lines are
  // left exactly as long as they were written.
  setLineWrapMode(QPlainTextEdit::NoWrap);
}

void EMailBodyView::Clear() {
  shown_as_source_ = false;
  clear();
}

void EMailBodyView::SetBody(const EMailPart* body) {
  Clear();

  if (body == nullptr) return;

  // setPlainText, always. It is the whole of the policy: whatever the part
  // claims to be, what is shown is the characters it consists of.
  setPlainText(QString::fromUtf8(body->data));

  if (body->content_type != "text/html") return;

  shown_as_source_ = true;
  emit SignalHtmlShownAsSource();
}
