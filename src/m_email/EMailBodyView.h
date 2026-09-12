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

#include <QMap>
#include <QTextBrowser>

#include "EMailModel.h"

/**
 * @brief A message body rendered as safely as a rich-text widget allows.
 *
 * The whole design rests on one rule: nothing in a message may cause a network
 * request. QTextBrowser will happily fetch an <img src="http://..."> through
 * its default loadResource(), which turns simply opening a message into a
 * read receipt for the sender -- and worse, into a way to confirm that an
 * address is live. loadResource() is therefore overridden unconditionally and
 * serves exactly one thing: parts that arrived inside this message, addressed
 * by their Content-ID.
 *
 * There is deliberately no "load remote content" switch. Adding one would mean
 * writing a fetcher that sends no cookies or credentials, keeps no cache,
 * allows only https, refuses loopback, link-local and private-network
 * addresses (including after a redirect), and bounds size, time and redirect
 * count. Until such a fetcher exists, offering the option would really mean
 * handing the job back to QTextBrowser's default loader, which does none of
 * that. Blocking, and saying so, is the honest behaviour.
 *
 * Links never open on a click either: the real target is disclosed first, with
 * the host shown in punycode beside its Unicode form when they differ, because
 * a homograph host is invisible precisely when it matters.
 */
class EMailBodyView : public QTextBrowser {
  Q_OBJECT

 public:
  explicit EMailBodyView(QWidget* parent = nullptr);

  /**
   * @brief Shows @p body, resolving any cid: references against @p root.
   *
   * @param body the part chosen as the message body; nullptr clears the view
   * @param root the tree the inline parts live in
   */
  void SetBody(const EMailPart* body, const EMailPart& root);

  void Clear();

  /// Whether the last rendered body asked for content from the network.
  [[nodiscard]] auto BlockedRemoteContent() const -> bool {
    return blocked_remote_content_;
  }

 signals:
  /// Emitted when a body referencing remote content is shown, so the host can
  /// tell the user that something was deliberately not loaded.
  void SignalRemoteContentBlocked();

 protected:
  /**
   * @brief Serves inline parts and refuses everything else.
   *
   * Returning an empty QVariant is what stops the default loader from going
   * to the network, so every path out of here must return one unless the
   * resource came from inside this very message.
   */
  auto loadResource(int type, const QUrl& name) -> QVariant override;

 private:
  void handle_anchor(const QUrl& url);
  /// Collects every part with a Content-ID, so cid: lookups never walk the
  /// tree while the layout engine is asking for resources.
  void index_inline_parts(const EMailPart& part);

  QMap<QString, QByteArray> inline_parts_;
  QMap<QString, QString> inline_types_;
  bool blocked_remote_content_{false};
};
