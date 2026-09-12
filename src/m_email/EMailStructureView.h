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

#include <QWidget>

#include "EMailModel.h"

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;

/**
 * @brief The MIME tree of a message, as it actually is.
 *
 * A dumb view: it renders an already-parsed EMailPart tree and never parses,
 * reserializes or otherwise touches the document. Inspecting a message must
 * leave its bytes exactly as they arrived, and this is one of the surfaces
 * that has to hold that line.
 *
 * Signature coverage is shown per region rather than as a yes/no, because a
 * part can sit inside several nested signatures at once and "signed" would be
 * unable to say which one vouches for it.
 */
class EMailStructureView : public QWidget {
  Q_OBJECT

 public:
  explicit EMailStructureView(QWidget* parent = nullptr);

  /**
   * @brief Shows @p root, with @p regions describing what each signature
   * covers. Passing an empty tree clears the view.
   */
  void SetTree(const EMailPart& root,
               const QList<EMailSignatureRegion>& regions);

  /// Drops everything shown, including any decrypted part sizes.
  void Clear();

  /// Replaces the table with @p text, for when there is nothing to tabulate.
  /// Passing an empty string is not a way to clear the view; use Clear().
  void ShowNotice(const QString& text);

 protected:
  void changeEvent(QEvent* event) override;

 private:
  void build_ui();
  /// The single place this view's colours are decided. Must not touch fonts.
  void apply_colors();
  /// Adds @p part and its children under @p parent_item, or at the top level
  /// when @p parent_item is nullptr.
  void add_part(const EMailPart& part, QTreeWidgetItem* parent_item);
  /// A human summary of which regions cover @p part.
  auto describe_coverage(const EMailPart& part) const -> QString;
  /// Shows the selected part's digest. Computed here rather than while
  /// building the tree: hashing every part of a large message up front would
  /// cost real time for rows nobody looks at.
  void show_selected_digest();
  /// The part behind @p item, or nullptr when the row resolves to nothing.
  [[nodiscard]] auto part_at(QTreeWidgetItem* item) const -> const EMailPart*;
  /// The tree's own menu, for the row at @p pos.
  void show_part_menu(const QPoint& pos);
  /// Asks where to put @p part and writes its bytes there.
  void save_part(const EMailPart& part);

  /// Shown in the tree's place when the tree would be empty.
  QLabel* empty_notice_{};
  QLabel* summary_{};
  QTreeWidget* tree_{};
  QLabel* digest_{};
  QList<EMailSignatureRegion> regions_;
  /// Kept so a selected row can be resolved back to its part. Indexed by the
  /// part's pre-order index, which is what the rows carry.
  EMailPart root_;
};
