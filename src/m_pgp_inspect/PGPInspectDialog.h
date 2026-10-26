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
class QPlainTextEdit;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

/// The exact octets of the current editor tab, or empty when there is no tab.
auto PGPInspectCurrentTabBytes() -> QByteArray;

/// Run the inspector over @p data. Never throws; see PGPInspectDocument::valid.
auto PGPInspectBytes(const QByteArray& data) -> PGPInspectDocument;

/**
 * @brief Shows what the OpenPGP data in the current tab is made of.
 *
 * Master and detail. The tree on the left is the structure alone: blocks,
 * packets, and what nests inside them, one line each. Everything a packet
 * says goes in the tabs on the right, one kind of detail per tab, for the
 * selected row:
 *
 *  - Overview: the packet's main facts, and its framing;
 *  - Subpackets: a signature's hashed and unhashed areas;
 *  - Key / Signature / Session Key Material, named for the packet: its
 *    format in one line, then the key parameters, signature values or
 *    encrypted session keys, each with its length and encoding, and a
 *    secret key's protection;
 *  - Hex: the octets, each range labelled, as `sq packet dump --hex` lays
 *    them out.
 *
 * A tab with nothing to show for the selection is hidden, and the tab the
 * reader picked stays picked while they step through packets. The search box
 * filters the tree by everything a packet shows, and marks the matching rows
 * in the tabs.
 *
 * There is nothing to press: the content comes from the tab the user was
 * already looking at. Every rule about what the bytes mean lives in
 * PGPPacketDecoder.h and PGPInspectModel.h.
 */
class PGPInspectDialog : public QDialog, public gf::ui::DialogWidget {
  Q_OBJECT

 public:
  PGPInspectDialog(const PGPInspectDocument& document, qint64 size,
                   QWidget* parent = nullptr);

 protected:
  auto eventFilter(QObject* watched, QEvent* event) -> bool override;

 private slots:
  void slot_filter_changed(const QString& needle);
  void slot_selection_changed(QTreeWidgetItem* current);
  void slot_tab_chosen(int index);

 private:
  void create_widgets();
  void render_document();
  void add_packets(QTreeWidgetItem* parent,
                   const QList<PGPInspectPacket>& packets,
                   const QString& parent_index);
  void measure_columns();
  void layout_columns();
  void show_block(const PGPInspectBlock& block);
  void show_packet(const PGPInspectPacket& packet, const QString& index);
  void fill_fields(QTreeWidget* tree, const QList<PGPInspectField>& fields);
  void show_tabs(bool subpackets, bool material, bool hex);

  PGPInspectDocument document_;
  qint64 size_ = 0;
  /// The tab the reader last picked, kept across selections.
  int preferred_tab_ = 0;
  /// What the longest packet label and size need; 0 before rendering.
  int label_width_ = 0;
  /// The narrowest the name column may get: its "#index" in full.
  int index_width_ = 0;
  int size_width_ = 0;
  /// Set while the dialog itself switches tabs, so that is not mistaken for
  /// the reader's choice.
  bool switching_tabs_ = false;

  QLineEdit* search_{};
  QTreeWidget* tree_{};
  QTabWidget* tabs_{};
  QTreeWidget* overview_{};
  QTreeWidget* subpackets_{};
  QTreeWidget* material_{};
  QLabel* material_summary_{};
  QPlainTextEdit* hex_{};
  QLabel* summary_label_{};
};

/// The item role holding the text a structure row is searched by: its own
/// line, and every field its packet shows in any tab.
constexpr int kPGPInspectSearchRole = Qt::UserRole + 1;

/// Show a row when it matches, when an ancestor matched, or when a descendant
/// does. Returns whether @p item ended up visible.
auto PGPInspectFilterItem(QTreeWidgetItem* item, const QString& needle,
                          bool ancestor_matched) -> bool;
