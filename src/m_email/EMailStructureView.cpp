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

#include <GFSDKGpg.h>

#include <QApplication>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDir>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QSaveFile>
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
  EMailPolishTree(tree_);
  layout->addWidget(tree_, 1);

  // Takes the tree's place rather than a row of its own: exactly one of the
  // two is on screen at any time.
  empty_notice_ = EMailEmptyNotice(this, tr("No message is open."));
  layout->addWidget(empty_notice_, 1);
  tree_->setVisible(false);

  digest_ = new QLabel(this);
  digest_->setWordWrap(true);
  digest_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  digest_->setVisible(false);
  {
    auto font = digest_->font();
    font.setPointSizeF(font.pointSizeF() * 0.92);
    digest_->setFont(font);
  }
  layout->addWidget(digest_);

  connect(tree_, &QTreeWidget::itemSelectionChanged, this,
          &EMailStructureView::show_selected_digest);

  // The tab could describe a part in four columns and a digest and still offer
  // no way to do anything with it -- including importing a key it had already
  // labelled as importable.
  tree_->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(tree_, &QTreeWidget::customContextMenuRequested, this,
          &EMailStructureView::show_part_menu);

  apply_colors();
}

void EMailStructureView::apply_colors() {
  // The one place this view's colours are decided; see EMailIsRestyle for why
  // it has to be able to run again. Fonts are set at build time and must not
  // be touched here.
  EMailSetLabelColor(summary_, EMailMutedColor(this));
  EMailSetLabelColor(digest_, EMailMutedColor(this));
  EMailSetLabelColor(empty_notice_, EMailMutedColor(this));
  EMailPaintTreeHeader(tree_);
  EMailRepaintTree(tree_);
}

void EMailStructureView::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);
  if (EMailIsRestyle(event)) apply_colors();
}

void EMailStructureView::ShowNotice(const QString& text) {
  tree_->clear();
  tree_->setVisible(false);
  digest_->setVisible(false);
  // The summary describes a table that is not there, so it goes with it.
  summary_->setVisible(false);
  empty_notice_->setText(text);
  empty_notice_->setVisible(true);
}

auto EMailStructureView::part_at(QTreeWidgetItem* item) const
    -> const EMailPart* {
  if (item == nullptr) return nullptr;

  const auto index = item->data(kColPart, Qt::UserRole).toInt();
  const auto flat = FlattenMimeTree(root_);
  if (index < 0 || index >= flat.size()) return nullptr;
  return flat[index];
}

void EMailStructureView::show_part_menu(const QPoint& pos) {
  auto* item = tree_->itemAt(pos);
  const auto* part = part_at(item);
  if (part == nullptr) return;

  // Clicking a row selects it, so the digest label below already refers to the
  // part this menu is about.
  tree_->setCurrentItem(item);

  const bool has_bytes = !part->is_multipart && !part->data.isEmpty();

  QMenu menu(this);
  auto* save = menu.addAction(tr("Save Part..."));
  save->setEnabled(has_bytes);

  QAction* import = nullptr;
  if (part->is_openpgp_key) {
    menu.addSeparator();
    // The tooltip on these rows has always said an OpenPGP key "can be
    // imported". Until now nothing in this module could import one, so the
    // sentence described an action that did not exist.
    import = menu.addAction(tr("Import This Key"));
  }

  menu.addSeparator();
  auto* copy_type = menu.addAction(tr("Copy Content-Type"));
  auto* copy_name = menu.addAction(tr("Copy Name"));
  copy_name->setEnabled(!part->filename.isEmpty());
  auto* copy_digest = menu.addAction(tr("Copy SHA-256"));
  copy_digest->setEnabled(has_bytes);

  auto* chosen = menu.exec(tree_->viewport()->mapToGlobal(pos));
  if (chosen == nullptr) return;

  auto* clipboard = QApplication::clipboard();
  if (chosen == save) {
    save_part(*part);
  } else if (import != nullptr && chosen == import) {
    // The host owns the import and whatever it reports about the outcome.
    GFGpgImportKeys(GFGpgCurrentGpgContextChannel(), this,
                    part->data.constData(),
                    static_cast<int>(part->data.size()));
  } else if (chosen == copy_type) {
    clipboard->setText(part->content_type);
  } else if (chosen == copy_name) {
    clipboard->setText(part->filename);
  } else if (chosen == copy_digest) {
    clipboard->setText(QString::fromLatin1(
        QCryptographicHash::hash(part->data, QCryptographicHash::Sha256)
            .toHex()));
  }
}

void EMailStructureView::save_part(const EMailPart& part) {
  // Named from the part's own filename where it has one, and from its type
  // where it does not: a structural part is usually nameless, and "part.txt"
  // beats an empty name box.
  const auto suggested = SanitizeAttachmentFileName(
      part.filename.isEmpty() ? QString("part") : part.filename,
      part.content_type);

  const auto path = QFileDialog::getSaveFileName(
      this, tr("Save Part"), QDir(QDir::homePath()).filePath(suggested));
  if (path.isEmpty()) return;

  // The part's bytes as they are in the message, decoded but not reinterpreted.
  QSaveFile file(path);
  const bool ok = file.open(QIODevice::WriteOnly) &&
                  file.write(part.data) == part.data.size() && file.commit();
  if (!ok) {
    QMessageBox::warning(this, tr("Save Part"),
                         tr("Could not write %1.").arg(path));
  }
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
  ShowNotice(tr("No message is open."));
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

  // A multipart has no content of its own; showing a size for it would invite
  // the reader to add numbers that do not add up.
  if (!part.is_multipart && !part.is_protocol_part) {
    item->setText(kColSize, EMailHumanSize(part.decoded_size));
    item->setTextAlignment(kColSize, Qt::AlignRight | Qt::AlignVCenter);
    EMailSetCellTone(item, kColSize, EMailTone::kMUTED, this);
  }

  item->setText(kColEncoding, part.transfer_encoding);
  EMailSetCellTone(item, kColEncoding, EMailTone::kMUTED, this);

  const auto coverage = describe_coverage(part);
  if (!coverage.isEmpty()) {
    item->setText(kColCoverage, coverage);
    EMailSetCellTone(item, kColCoverage, EMailTone::kGOOD, this);
  } else if (!regions_.isEmpty() && !part.is_protocol_part) {
    // Only worth saying once the message actually has a signature: in an
    // unsigned message every row would read "not covered", which is noise.
    item->setText(kColCoverage, tr("not covered"));
    EMailSetCellTone(item, kColCoverage, EMailTone::kWARN, this);
  }

  if (part.is_protocol_part) {
    // Structure rather than content. Saying so stops it reading as a missing
    // attachment.
    item->setText(kColName, tr("OpenPGP control part"));
    EMailSetCellTone(item, kColName, EMailTone::kMUTED, this);
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

  const bool has_parts = tree_->topLevelItemCount() > 0;
  tree_->setVisible(has_parts);
  empty_notice_->setVisible(!has_parts);
  summary_->setVisible(true);
  if (!has_parts) {
    empty_notice_->setText(tr("This message has no parts to show."));
  }

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
