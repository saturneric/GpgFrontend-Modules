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

#include "EMailStructureView.h"

#include <QCryptographicHash>
#include <QHeaderView>
#include <QLabel>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "EMailHelper.h"
#include "EMailViewStyle.h"

namespace {

constexpr int kColPart = 0;
constexpr int kColName = 1;
constexpr int kColSize = 2;
constexpr int kColEncoding = 3;
constexpr int kColCoverage = 4;

}  // namespace

EMailStructureView::EMailStructureView(QWidget* parent) : QWidget(parent) {
  build_ui();
}

void EMailStructureView::build_ui() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 6, 8, 6);
  layout->setSpacing(6);

  summary_ = new QLabel(this);
  summary_->setWordWrap(true);
  {
    auto palette = summary_->palette();
    palette.setColor(QPalette::WindowText, EMailMutedColor(this));
    summary_->setPalette(palette);
    auto font = summary_->font();
    font.setPointSizeF(font.pointSizeF() * 0.92);
    summary_->setFont(font);
  }
  layout->addWidget(summary_);

  tree_ = new QTreeWidget(this);
  tree_->setUniformRowHeights(true);
  tree_->setAlternatingRowColors(true);
  tree_->setHeaderLabels(
      {tr("Part"), tr("Name"), tr("Size"), tr("Encoding"), tr("Covered by")});
  tree_->header()->setStretchLastSection(false);
  tree_->header()->setSectionResizeMode(kColPart, QHeaderView::Stretch);
  tree_->header()->setSectionResizeMode(kColName, QHeaderView::Stretch);
  layout->addWidget(tree_, 1);

  digest_ = new QLabel(this);
  digest_->setWordWrap(true);
  digest_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  digest_->setVisible(false);
  {
    auto palette = digest_->palette();
    palette.setColor(QPalette::WindowText, EMailMutedColor(this));
    digest_->setPalette(palette);
    auto font = digest_->font();
    font.setPointSizeF(font.pointSizeF() * 0.92);
    digest_->setFont(font);
  }
  layout->addWidget(digest_);

  connect(tree_, &QTreeWidget::itemSelectionChanged, this,
          &EMailStructureView::show_selected_digest);
}

void EMailStructureView::show_selected_digest() {
  const auto selected = tree_->selectedItems();
  if (selected.isEmpty()) {
    digest_->setVisible(false);
    return;
  }

  const auto index = selected.first()->data(kColPart, Qt::UserRole).toInt();
  const auto flat = FlattenMimeTree(root_);
  if (index < 0 || index >= flat.size()) {
    digest_->setVisible(false);
    return;
  }

  const auto* part = flat[index];
  if (part->is_multipart || part->data.isEmpty()) {
    digest_->setVisible(false);
    return;
  }

  const auto digest =
      QCryptographicHash::hash(part->data, QCryptographicHash::Sha256).toHex();
  digest_->setText(
      tr("SHA-256 of this part: %1").arg(QString::fromLatin1(digest)));
  digest_->setVisible(true);
}

void EMailStructureView::Clear() {
  regions_.clear();
  root_ = EMailPart{};
  tree_->clear();
  summary_->clear();
  digest_->clear();
  digest_->setVisible(false);
}

auto EMailStructureView::describe_coverage(const EMailPart& part) const
    -> QString {
  if (part.covered_by_regions.isEmpty()) return {};

  // Named per region, outermost first. With nesting this is the difference
  // between "something signed it" and knowing which signature did.
  QStringList names;
  for (int id : part.covered_by_regions) {
    names.append(tr("signature %1").arg(id + 1));
  }
  return names.join(", ");
}

void EMailStructureView::add_part(const EMailPart& part,
                                  QTreeWidgetItem* parent_item) {
  auto* item = parent_item == nullptr ? new QTreeWidgetItem(tree_)
                                      : new QTreeWidgetItem(parent_item);

  item->setText(kColPart, part.content_type);
  item->setText(kColName, part.filename);
  // The pre-order index is how a row is resolved back to its part; it is
  // stable for the life of the tree.
  item->setData(kColPart, Qt::UserRole, part.index);

  const auto muted = EMailMutedColor(this);

  // A multipart has no content of its own; showing a size for it would invite
  // the reader to add numbers that do not add up.
  if (!part.is_multipart && !part.is_protocol_part) {
    item->setText(kColSize, EMailHumanSize(part.decoded_size));
    item->setTextAlignment(kColSize, Qt::AlignRight | Qt::AlignVCenter);
    item->setForeground(kColSize, muted);
  }

  item->setText(kColEncoding, part.transfer_encoding);
  item->setForeground(kColEncoding, muted);

  const auto coverage = describe_coverage(part);
  if (!coverage.isEmpty()) {
    item->setText(kColCoverage, coverage);
    item->setForeground(kColCoverage, EMailAccentColor(this, true));
  } else if (!regions_.isEmpty() && !part.is_protocol_part) {
    // Only worth saying once the message actually has a signature: in an
    // unsigned message every row would read "not covered", which is noise.
    item->setText(kColCoverage, tr("not covered"));
    item->setForeground(kColCoverage, EMailWarningColor(this));
  }

  if (part.is_protocol_part) {
    // Structure rather than content. Saying so stops it reading as a missing
    // attachment.
    item->setText(kColName, tr("OpenPGP control part"));
    item->setForeground(kColName, muted);
  } else if (part.is_openpgp_key) {
    item->setToolTip(kColPart, tr("An OpenPGP key, which can be imported."));
  }

  if (!part.content_id.isEmpty()) {
    item->setToolTip(kColName, tr("Content-ID: %1").arg(part.content_id));
  }

  for (const auto& child : part.children) add_part(child, item);
}

void EMailStructureView::SetTree(const EMailPart& root,
                                 const QList<EMailSignatureRegion>& regions) {
  regions_ = regions;
  root_ = root;
  tree_->clear();
  digest_->setVisible(false);

  add_part(root, nullptr);
  tree_->expandAll();

  if (regions_.isEmpty()) {
    summary_->setText(tr("This message carries no signed section."));
    return;
  }

  QStringList lines;
  for (const auto& region : regions_) {
    // The declared algorithm is what the message claims, and it is labelled
    // that way: only a verify can say what the signature really used.
    lines.append(tr("Signature %1 covers %2 (declared %3)")
                     .arg(region.region_id + 1)
                     .arg(EMailHumanSize(region.raw_length))
                     .arg(region.declared_micalg.isEmpty()
                              ? tr("no algorithm")
                              : region.declared_micalg));
  }
  summary_->setText(lines.join("\n"));
}
