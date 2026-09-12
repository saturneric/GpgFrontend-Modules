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

#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QShortcut>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "EMailHelper.h"
#include "EMailViewStyle.h"

namespace {

// Deliberately untyped: lupdate stops attributing tr() calls to the enclosing
// class once it meets an enum with an explicit underlying type in this file,
// so every string below one would silently drop out of the catalogues.
enum HeaderColumn { kCOL_NAME = 0, kCOL_VALUE = 1 };

// The exact bytes of a field, for the tooltip. Latin-1 rather than UTF-8 on
// purpose: every byte maps to exactly one character, so nothing is replaced or
// merged and what is on screen is what is in the file.
auto AsExactText(const QByteArray& bytes) -> QString {
  return QString::fromLatin1(bytes);
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
  row->setSpacing(6);

  filter_ = new QLineEdit(this);
  filter_->setClearButtonEnabled(true);
  filter_->setPlaceholderText(tr("Filter headers"));
  filter_->setToolTip(
      tr("Show only the headers whose name or value contains this text."));
  row->addWidget(filter_);
  layout->addLayout(row);

  tree_ = new QTreeWidget(this);
  tree_->setColumnCount(2);
  tree_->setHeaderLabels({tr("Name"), tr("Value")});
  tree_->setRootIsDecorated(false);
  tree_->setAlternatingRowColors(true);
  tree_->setUniformRowHeights(true);
  tree_->setSelectionBehavior(QAbstractItemView::SelectRows);
  tree_->setTextElideMode(Qt::ElideRight);
  tree_->header()->setSectionResizeMode(kCOL_NAME,
                                        QHeaderView::ResizeToContents);
  tree_->header()->setSectionResizeMode(kCOL_VALUE, QHeaderView::Stretch);
  tree_->setContextMenuPolicy(Qt::CustomContextMenu);
  EMailPolishTree(tree_);
  layout->addWidget(tree_, 1);

  // Takes the tree's place rather than a row under it, so an empty tab is one
  // sentence instead of a sentence beneath a blank ruled box.
  empty_notice_ = EMailEmptyNotice(this);
  empty_notice_->setVisible(false);
  layout->addWidget(empty_notice_, 1);

  connect(filter_, &QLineEdit::textChanged, this,
          [this](const QString&) { apply_filter(); });

  // Where a reader's hand goes for "find" on a long list of headers. Scoped to
  // this widget and its children: several message tabs can be open, and the
  // key must reach the filter of the one being looked at.
  auto* find = new QShortcut(QKeySequence::Find, this);
  find->setContext(Qt::WidgetWithChildrenShortcut);
  connect(find, &QShortcut::activated, this, [this]() {
    filter_->setFocus(Qt::ShortcutFocusReason);
    filter_->selectAll();
  });

  // Escape clears the filter rather than closing anything, and only while it
  // has something in it -- otherwise the key is swallowed here instead of
  // reaching whatever would normally answer it.
  auto* clear = new QShortcut(QKeySequence(Qt::Key_Escape), filter_);
  clear->setContext(Qt::WidgetShortcut);
  connect(clear, &QShortcut::activated, this, [this]() {
    if (!filter_->text().isEmpty()) filter_->clear();
  });

  connect(tree_, &QTreeWidget::customContextMenuRequested, this,
          [this](const QPoint& pos) {
            if (tree_->itemAt(pos) == nullptr) return;

            // The row under the cursor, not the current one. Qt makes them
            // agree for a right-click, but a context menu raised from the
            // keyboard has no cursor over a row at all, and then the two
            // differ.
            auto* item = tree_->itemAt(pos);

            QMenu menu(this);
            auto* copy_value = menu.addAction(tr("Copy Value"));
            auto* copy_field = menu.addAction(tr("Copy Whole Field"));
            auto* chosen = menu.exec(tree_->viewport()->mapToGlobal(pos));
            if (chosen == copy_value) copy_item(item, false);
            if (chosen == copy_field) copy_item(item, true);
          });

  apply_colors();
}

void EMailHeaderView::apply_colors() {
  // The single place this view's colours are decided; see EMailIsRestyle.
  // Fonts are set at build time and must not be touched here.
  EMailSetLabelColor(empty_notice_, EMailMutedColor(this));
  EMailPaintTreeHeader(tree_);
  EMailRepaintTree(tree_);
}

void EMailHeaderView::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);
  if (EMailIsRestyle(event)) apply_colors();
}

void EMailHeaderView::Clear() {
  loaded_ = false;
  root_ = EMailPart{};
  raw_.clear();
  tree_->clear();
  refresh();
}

void EMailHeaderView::SetMessage(const EMailPart& root, const QByteArray& raw) {
  root_ = root;
  raw_ = raw;
  loaded_ = true;
  refresh();
}

void EMailHeaderView::refresh() {
  tree_->clear();

  if (!loaded_) {
    empty_notice_->setText(tr("No message is open."));
    empty_notice_->setVisible(true);
    tree_->setVisible(false);
    filter_->setEnabled(false);
    return;
  }

  const auto block = RawHeaderBlock(root_, raw_);
  const auto fields = SplitRawHeaderFields(block);

  // The decoded reading, which only lines up field for field when the parser
  // and the raw split agree on how many there are. When they do not, the bytes
  // are the answer that can still be trusted.
  const bool decoded_aligns = fields.size() == root_.header_fields.size();

  for (int i = 0; i < fields.size(); ++i) {
    const auto& field = fields.at(i);

    // A line that is not `name: value` at all. Shown as it stands rather than
    // dropped -- it is exactly what someone reading raw headers is looking for.
    const bool malformed = field.name.isEmpty();

    const auto decoded = decoded_aligns && !malformed
                             ? root_.header_fields.at(i).second
                             : AsExactText(field.value);

    auto* item = new QTreeWidgetItem(tree_);
    item->setText(kCOL_NAME, malformed ? tr("(malformed)") : field.name);
    item->setText(kCOL_VALUE, decoded);

    // The exact field, folds and encoding included, is always one hover away.
    item->setToolTip(kCOL_NAME, AsExactText(field.raw_line));
    item->setToolTip(kCOL_VALUE, AsExactText(field.raw_line));

    if (malformed) {
      EMailSetCellTone(item, kCOL_NAME, EMailTone::kWARN, this);
    } else {
      item->setFont(kCOL_NAME,
                    QFontDatabase::systemFont(QFontDatabase::FixedFont));
    }

    // Kept on the row so filtering and copying read what the message says,
    // never what the column happens to be showing after elision.
    item->setData(kCOL_NAME, Qt::UserRole, AsExactText(field.raw_line));
    item->setData(kCOL_VALUE, Qt::UserRole, AsExactText(field.value));
  }

  filter_->setEnabled(tree_->topLevelItemCount() > 0);
  apply_filter();
}

void EMailHeaderView::apply_filter() {
  const auto needle = filter_->text().trimmed();

  int shown = 0;
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    auto* item = tree_->topLevelItem(i);
    const bool match =
        needle.isEmpty() ||
        item->text(kCOL_NAME).contains(needle, Qt::CaseInsensitive) ||
        item->text(kCOL_VALUE).contains(needle, Qt::CaseInsensitive) ||
        item->data(kCOL_NAME, Qt::UserRole)
            .toString()
            .contains(needle, Qt::CaseInsensitive);
    item->setHidden(!match);
    if (match) shown++;
  }

  // A message with no headers is a fact about the message, and the table is
  // replaced by the sentence saying so. A filter that matches nothing is a
  // fact about what was typed: the table stays where it is, empty, because
  // swapping it for a label on every keystroke flickers, and because the
  // filter box directly above is self-evidently the reason it is empty.
  const bool empty = tree_->topLevelItemCount() == 0;
  tree_->setVisible(!empty);
  empty_notice_->setVisible(empty);
  if (empty) {
    empty_notice_->setText(tr("This message has no headers to show."));
  }
}

void EMailHeaderView::ShowNotice(const QString& text) {
  loaded_ = false;
  tree_->clear();
  tree_->setVisible(false);
  filter_->setEnabled(false);
  empty_notice_->setText(text);
  empty_notice_->setVisible(true);
}

void EMailHeaderView::copy_selected(bool whole_field) {
  copy_item(tree_->currentItem(), whole_field);
}

void EMailHeaderView::copy_item(QTreeWidgetItem* item, bool whole_field) {
  if (item == nullptr) return;

  const auto role = whole_field ? kCOL_NAME : kCOL_VALUE;
  // The message's own bytes, not the elided text the column is showing.
  QApplication::clipboard()->setText(item->data(role, Qt::UserRole).toString());
}
