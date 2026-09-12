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

#include <QPlainTextEdit>

#include "EMailModel.h"

/**
 * @brief A message body, shown as the text it literally is.
 *
 * This workspace supports plain-text mail. A body that arrived as HTML is not
 * rendered and is not converted into an approximation of itself: its source is
 * shown verbatim, which is the only form of it that is certainly true.
 *
 * That is a deliberate narrowing, and it removes rather than manages three
 * separate hazards. Qt's rich-text engine is not built for real-world mail: it
 * lays a nested table out again for every pass its parent makes, so cost grows
 * exponentially with nesting depth and an ordinary marketing newsletter -- 45
 * tables, seven deep -- never finishes, synchronously, on the GUI thread,
 * inside setHtml(), with no event loop left to cancel from. Rendering also
 * resolves resources, and an <img src="http://..."> turns opening a message
 * into a read receipt and a confirmation that the address is live. And it lets
 * a link hide its destination behind friendly words.
 *
 * Showing the source ends all three: no layout engine runs on attacker
 * controlled markup, nothing is ever fetched, and every URL is visible as
 * written. The widget is a read-only QPlainTextEdit for the same reason -- it
 * has no rich-text engine to reach, so none of this can be reintroduced by
 * calling the wrong setter.
 */
class EMailBodyView : public QPlainTextEdit {
  Q_OBJECT

 public:
  explicit EMailBodyView(QWidget* parent = nullptr);

  /**
   * @brief Shows @p body as text.
   *
   * @param body the part chosen as the message body; nullptr clears the view
   */
  void SetBody(const EMailPart* body);

  void Clear();

  /// Whether what is shown is HTML source rather than a plain-text body.
  [[nodiscard]] auto ShownAsSource() const -> bool { return shown_as_source_; }

 signals:
  /**
   * @brief An HTML body is being shown as its source.
   *
   * Said out loud rather than done quietly: this is not how the message was
   * meant to look, and the reader is entitled to know which of the two they
   * are looking at.
   */
  void SignalHtmlShownAsSource();

 private:
  bool shown_as_source_{false};
};
