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

#include <QByteArray>
#include <GFModule.h>

#include <QDialog>

#include "PGPInspectModel.h"

class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

/// The exact octets of the current editor tab, or empty when there is no tab.
auto PGPInspectCurrentTabBytes() -> QByteArray;

/// Run the inspector over @p data. Never throws; see PGPInspectDocument::valid.
auto PGPInspectBytes(const QByteArray& data) -> PGPInspectDocument;

/**
 * @brief Shows what the OpenPGP data in the current tab is made of.
 *
 * A dumb view, and a deliberately bare one: it is handed a document and
 * renders it. There is nothing to press -- the content comes from the tab the
 * user was already looking at, so a dialog that asked them to fetch it again
 * would be asking a question they have already answered. The only control is
 * the search box, which is there because a certificate's packet tree is long.
 *
 * Every rule about what the bytes mean lives in PGPInspectModel.h and, below
 * that, in the Rust packet walk.
 */
class PGPInspectDialog : public QDialog, public gf::ui::DialogWidget {
  Q_OBJECT

 public:
  PGPInspectDialog(const PGPInspectDocument& document, qint64 size,
                   QWidget* parent = nullptr);

 private slots:
  void slot_filter_changed(const QString& needle);

 private:
  void create_widgets();
  void render_document();
  void add_packets(QTreeWidgetItem* parent,
                   const QVector<PGPInspectPacket>& packets);
  void add_fields(QTreeWidgetItem* parent,
                  const QVector<PGPInspectField>& fields);

  PGPInspectDocument document_;
  qint64 size_ = 0;

  QLineEdit* search_{};
  QTreeWidget* tree_{};
  QLabel* summary_label_{};
};

/// Show a row when it matches, when an ancestor matched, or when a descendant
/// does. Returns whether @p item ended up visible.
auto PGPInspectFilterItem(QTreeWidgetItem* item, const QString& needle,
                          bool ancestor_matched) -> bool;
