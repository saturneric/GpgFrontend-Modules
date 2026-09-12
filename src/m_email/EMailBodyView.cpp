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

#include <QDesktopServices>
#include <QMessageBox>
#include <QRegularExpression>
#include <QUrl>

#include "EMailHelper.h"

namespace {

// Schemes a link is allowed to open at all. Anything else -- file:, qrc:,
// javascript:, or something invented -- is refused rather than handed to the
// desktop, which would happily act on it.
auto IsOpenableScheme(const QString& scheme) -> bool {
  const auto s = scheme.toLower();
  return s == "http" || s == "https" || s == "mailto";
}

// Does this markup reference anything that would have to come off the network?
auto ReferencesRemoteContent(const QString& html) -> bool {
  static const QRegularExpression kRemote(
      R"((?:src|background|href)\s*=\s*["']?\s*(?:https?:|//))",
      QRegularExpression::CaseInsensitiveOption);
  return kRemote.match(html).hasMatch();
}

}  // namespace

EMailBodyView::EMailBodyView(QWidget* parent) : QTextBrowser(parent) {
  setReadOnly(true);
  setUndoRedoEnabled(false);

  // Both are required. setOpenLinks(false) stops the widget navigating on its
  // own, and setOpenExternalLinks(false) stops it handing the URL to the
  // desktop behind our back; the anchorClicked handler is then the only way
  // out, and it discloses the target first.
  setOpenLinks(false);
  setOpenExternalLinks(false);

  connect(this, &QTextBrowser::anchorClicked, this,
          &EMailBodyView::handle_anchor);
}

void EMailBodyView::Clear() {
  inline_parts_.clear();
  inline_types_.clear();
  blocked_remote_content_ = false;
  clear();
}

void EMailBodyView::index_inline_parts(const EMailPart& part) {
  if (!part.content_id.isEmpty() && !part.data.isEmpty()) {
    inline_parts_.insert(part.content_id, part.data);
    inline_types_.insert(part.content_id, part.content_type);
  }
  for (const auto& child : part.children) index_inline_parts(child);
}

auto EMailBodyView::loadResource(int type, const QUrl& name) -> QVariant {
  Q_UNUSED(type)

  // The only resource that may resolve is one that travelled inside this
  // message. Note this is not a filter over "remote" URLs: it is a positive
  // list of one scheme, so a scheme nobody thought of cannot slip past.
  if (name.scheme().compare("cid", Qt::CaseInsensitive) == 0) {
    const auto id =
        name.path().isEmpty() ? name.toString().mid(4) : name.path();
    const auto it = inline_parts_.constFind(id);
    if (it != inline_parts_.constEnd()) return QVariant(it.value());
    return {};
  }

  // Everything else, deliberately: returning an empty QVariant is what keeps
  // the default loader -- and the network -- out of it.
  return {};
}

void EMailBodyView::handle_anchor(const QUrl& url) {
  if (!IsOpenableScheme(url.scheme())) {
    QMessageBox::warning(
        this, tr("Link not opened"),
        tr("This link uses the \"%1\" scheme, which is not opened from a "
           "message.")
            .arg(url.scheme()));
    return;
  }

  const auto host = url.host();
  const auto ace = QString::fromLatin1(QUrl::toAce(host));

  QString detail = tr("This link goes to:\n\n%1").arg(url.toString());

  // A host whose Unicode form differs from its punycode form is the homograph
  // case: "аpple.com" with a Cyrillic 'а' reads identically to the real thing.
  // Both forms are shown, because only one of them is the truth.
  if (!host.isEmpty() && !ace.isEmpty() &&
      ace.compare(host, Qt::CaseInsensitive) != 0) {
    detail += tr("\n\nThe address of this site uses non-Latin characters. "
                 "Its real form is:\n\n%1\n\nA name like this can be made to "
                 "look like a different, well-known one.")
                  .arg(ace);
  }

  QMessageBox box(this);
  box.setIcon(QMessageBox::Question);
  box.setWindowTitle(tr("Open this link?"));
  box.setText(detail);
  box.setStandardButtons(QMessageBox::Open | QMessageBox::Cancel);
  box.setDefaultButton(QMessageBox::Cancel);

  if (box.exec() != QMessageBox::Open) return;
  QDesktopServices::openUrl(url);
}

void EMailBodyView::SetBody(const EMailPart* body, const EMailPart& root) {
  Clear();

  if (body == nullptr) return;

  index_inline_parts(root);

  const auto text = QString::fromUtf8(body->data);

  if (body->content_type == "text/html") {
    blocked_remote_content_ = ReferencesRemoteContent(text);
    setHtml(text);
    if (blocked_remote_content_) emit SignalRemoteContentBlocked();
    return;
  }

  // A non-HTML body is shown as what it is. setPlainText, never setHtml:
  // handing text/plain to an HTML parser would let a message that merely
  // contains angle brackets be rendered as markup.
  setPlainText(text);
}
