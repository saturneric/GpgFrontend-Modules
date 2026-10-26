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

#include "PGPInspectDialog.h"

#include <GFModule.h>
#include <GFSDKPgp.h>
#include <GFSDKUI.h>

#include <QFontDatabase>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStyle>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <algorithm>
#include <array>

#include "GFModuleTr.h"
#include "PGPPacketNames.h"

using namespace Qt::StringLiterals;

namespace {

/// Which column holds what. Packet rows use the first only; a field row puts
/// its label there and its value beside it.
constexpr int kLabelColumn = 0;
constexpr int kValueColumn = 1;
/// The structure tree's third column: a packet's size on the wire.
constexpr int kSizeColumn = 2;

/// Which tab holds what.
constexpr int kOverviewTab = 0;
constexpr int kSubpacketsTab = 1;
constexpr int kMaterialTab = 2;
constexpr int kHexTab = 3;

/// Where a structure row keeps what it stands for. The document is owned by
/// the dialog and never changes after rendering, so the address is stable.
constexpr int kPacketRole = Qt::UserRole + 2;
constexpr int kBlockRole = Qt::UserRole + 3;
/// A packet row's dotted position, e.g. "2.1".
constexpr int kIndexRole = Qt::UserRole + 4;

auto ThemeBrush(int role) -> QBrush {
  return QBrush(QColor(GFUIThemeColorForRole(GFModuleSdkContext(), role)));
}

auto MakeFieldTree(QWidget* parent) -> QTreeWidget* {
  auto* tree = new QTreeWidget(parent);
  tree->setColumnCount(2);
  tree->setHeaderLabels({GTrC::tr("Field"), GTrC::tr("Value")});
  tree->header()->setSectionResizeMode(kLabelColumn,
                                       QHeaderView::ResizeToContents);
  tree->header()->setSectionResizeMode(kValueColumn, QHeaderView::Stretch);
  // Key material comes as lines of hex, so rows take their own height.
  tree->setUniformRowHeights(false);
  tree->setSelectionMode(QAbstractItemView::NoSelection);
  return tree;
}

void FlattenFields(const QList<PGPInspectField>& fields, QStringList& out) {
  for (const auto& field : fields) {
    out << field.label << field.value << field.detail;
    FlattenFields(field.children, out);
  }
}

/// Label / value rows, groups nesting their children. A row that matches
/// @p needle is set in bold.
void AddFieldRows(QTreeWidgetItem* parent, const QList<PGPInspectField>& fields,
                  const QString& needle) {
  for (const auto& field : fields) {
    auto* row = new QTreeWidgetItem(parent);
    // How the value is encoded and how long it is goes under its name, so
    // the value column holds the material alone.
    row->setText(kLabelColumn, field.detail.isEmpty()
                                   ? field.label
                                   : field.label + '\n' + field.detail);
    row->setText(kValueColumn, field.value);
    row->setToolTip(kValueColumn, field.value);
    row->setForeground(kLabelColumn, ThemeBrush(GF_UI_COLOR_MUTED_TEXT));
    // Name and value start on the same line, whichever runs to more lines.
    row->setTextAlignment(kLabelColumn, Qt::AlignLeft | Qt::AlignTop);
    row->setTextAlignment(kValueColumn, Qt::AlignLeft | Qt::AlignTop);
    if (!needle.isEmpty() &&
        (field.label.contains(needle, Qt::CaseInsensitive) ||
         field.value.contains(needle, Qt::CaseInsensitive) ||
         field.detail.contains(needle, Qt::CaseInsensitive))) {
      auto font = row->font(kValueColumn);
      font.setBold(true);
      row->setFont(kLabelColumn, font);
      row->setFont(kValueColumn, font);
    }
    AddFieldRows(row, field.children, needle);
  }
}

/// The material tab is named for what the selected packet holds.
auto MaterialTabTitle(int tag) -> QString {
  switch (tag) {
    case 2:
      return GTrC::tr("Signature Material");
    case 1:
    case 3:
      return GTrC::tr("Session Key Material");
    default:
      return GTrC::tr("Key Material");
  }
}

auto HumanSize(qint64 bytes) -> QString {
  // A pure helper: it writes into the caller's buffer rather than allocating,
  // so there is no context and nothing to free. 64 bytes is far more than any
  // formatted size needs.
  std::array<char, 64> text{};
  const auto written = GFUIHumanSize(bytes, text.data(), text.size());
  return written < 0 ? QString() : QString::fromUtf8(text.data(), written);
}

/// Mark a structure row as having a problem: the colour for the eye, and
/// the parser's own words as its tooltip and accessible description, so the
/// warning does not rest on colour alone.
void FlagRow(QTreeWidgetItem* item, int role, const QStringList& reasons) {
  QStringList text;
  for (const auto& reason : reasons) {
    if (!reason.isEmpty()) text << reason;
  }
  item->setForeground(kLabelColumn, ThemeBrush(role));
  item->setToolTip(kLabelColumn, text.join('\n'));
  item->setData(kLabelColumn, Qt::AccessibleDescriptionRole, text.join(' '));
}

/// A block's own facts: its armor envelope, or its signed text.
auto BlockFacts(const PGPInspectBlock& block) -> QList<PGPInspectField> {
  QList<PGPInspectField> facts;
  if (block.has_armor) {
    facts.push_back({GTrC::tr("Armor Type"), block.armor.block_type, {}});
    facts.push_back({GTrC::tr("CRC24"), block.armor.crc24, {}});
    facts += block.armor.headers;
  }
  if (block.has_cleartext) {
    facts.push_back(
        {GTrC::tr("Signed Text"), HumanSize(block.cleartext_text_size), {}});
    facts += block.cleartext_headers;
  }
  if (!block.error.isEmpty()) {
    facts.push_back({GTrC::tr("Problem"), block.error, {}});
  }
  return facts;
}

}  // namespace

auto PGPInspectCurrentTabBytes() -> QByteArray {
  auto* content = GFEditorTakeCurrentContent(GFModuleSdkContext());
  if (content == nullptr) return {};

  const auto* data =
      static_cast<const char*>(GFBufferData(GFModuleSdkContext(), content));
  const auto size =
      static_cast<qsizetype>(GFBufferSize(GFModuleSdkContext(), content));
  auto bytes = QByteArray(data == nullptr ? "" : data, size);
  GFBufferRelease(GFModuleSdkContext(), content);
  return bytes;
}

auto PGPInspectBytes(const QByteArray& data) -> PGPInspectDocument {
  PGPInspectDocument document;
  if (data.isEmpty()) return document;

  auto* buffer = GFBufferNewFromBytes(GFModuleSdkContext(), data.constData(),
                                      static_cast<size_t>(data.size()));
  if (buffer == nullptr) return document;

  GFBufferRef json = nullptr;
  const auto status = GFPgpInspectData(GFModuleSdkContext(), buffer, &json);
  GFBufferRelease(GFModuleSdkContext(), buffer);

  if (status != 0 || json == nullptr) return document;

  auto* ctx = GFModuleSdkContext();
  const auto* text = static_cast<const char*>(GFBufferData(ctx, json));
  const auto size = static_cast<qsizetype>(GFBufferSize(ctx, json));
  auto parsed =
      ParsePGPInspectDocument(QByteArray(text == nullptr ? "" : text, size));
  GFBufferRelease(ctx, json);
  return parsed;
}

PGPInspectDialog::PGPInspectDialog(const PGPInspectDocument& document,
                                   qint64 size, QWidget* parent)
    : QDialog(parent), document_(document), size_(size) {
  setWindowTitle(GTrC::tr("OpenPGP Structure"));
  setAttribute(Qt::WA_DeleteOnClose);
  resize(1200, 660);
  // The probe that enabled the menu entry only parsed; reading the packets
  // is this window's cost.
  PGPInspectDecode(document_);
  create_widgets();
  render_document();
}

void PGPInspectDialog::create_widgets() {
  search_ = new QLineEdit(this);
  search_->setClearButtonEnabled(true);
  search_->setPlaceholderText(GTrC::tr("Search packets, fields and values"));
  connect(search_, &QLineEdit::textChanged, this,
          &PGPInspectDialog::slot_filter_changed);

  tree_ = new QTreeWidget(this);
  tree_->setColumnCount(3);
  // A packet's name and a few words about it; the tabs hold the rest.
  tree_->setHeaderHidden(true);
  // Widths are set by layout_columns, from the packet labels and the pane's
  // width.
  tree_->header()->setSectionResizeMode(QHeaderView::Fixed);
  tree_->header()->setStretchLastSection(false);
  // Lets the summary collapse entirely before anything else is cut.
  tree_->header()->setMinimumSectionSize(0);
  tree_->setUniformRowHeights(true);
  connect(tree_, &QTreeWidget::currentItemChanged, this,
          &PGPInspectDialog::slot_selection_changed);
  tree_->viewport()->installEventFilter(this);

  overview_ = MakeFieldTree(this);
  subpackets_ = MakeFieldTree(this);
  material_ = MakeFieldTree(this);
  // One line on what format the material is in, above it and quieter than
  // it. Selectable, so it can be copied like the rest of the text here.
  material_summary_ = new QLabel(this);
  material_summary_->setWordWrap(true);
  material_summary_->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                             Qt::TextSelectableByKeyboard);
  material_summary_->setAccessibleName(GTrC::tr("Material format"));
  {
    auto palette = material_summary_->palette();
    palette.setColor(QPalette::WindowText,
                     ThemeBrush(GF_UI_COLOR_MUTED_TEXT).color());
    material_summary_->setPalette(palette);
  }
  auto* material_page = new QWidget(this);
  auto* material_layout = new QVBoxLayout(material_page);
  material_layout->setContentsMargins(0, 0, 0, 0);
  material_layout->addWidget(material_summary_);
  material_layout->addWidget(material_, 1);

  hex_ = new QPlainTextEdit(this);
  hex_->setReadOnly(true);
  hex_->setLineWrapMode(QPlainTextEdit::NoWrap);
  hex_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  tabs_ = new QTabWidget(this);
  tabs_->addTab(overview_, GTrC::tr("Overview"));
  tabs_->addTab(subpackets_, GTrC::tr("Subpackets"));
  tabs_->addTab(material_page, GTrC::tr("Key Material"));
  tabs_->addTab(hex_, GTrC::tr("Hex"));
  connect(tabs_, &QTabWidget::currentChanged, this,
          &PGPInspectDialog::slot_tab_chosen);

  auto* splitter = new QSplitter(Qt::Horizontal, this);
  splitter->addWidget(tree_);
  splitter->addWidget(tabs_);
  // The structure opens on about a quarter of the width, and keeps that
  // share as the window grows; the tabs, the hex dump above all, need the
  // rest. Set once: re-rendering never moves a divider the reader dragged.
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 3);
  splitter->setSizes({width() / 4, width() - (width() / 4)});
  splitter->setChildrenCollapsible(false);

  summary_label_ = new QLabel(this);
  summary_label_->setWordWrap(true);

  auto* layout = new QVBoxLayout();
  layout->addWidget(search_);
  layout->addWidget(splitter, 1);
  layout->addWidget(summary_label_);
  setLayout(layout);
}

void PGPInspectDialog::render_document() {
  tree_->clear();

  if (!document_.valid) {
    summary_label_->setText(
        document_.parse_error.isEmpty()
            ? GTrC::tr("The current tab holds no OpenPGP data.")
            : document_.parse_error);
    show_tabs(false, false, false);
    return;
  }

  int block_index = 1;
  for (const auto& block : document_.blocks) {
    auto* item = new QTreeWidgetItem(tree_);
    item->setText(kLabelColumn, PGPInspectBlockSummary(block, block_index++));
    item->setData(kLabelColumn, kBlockRole,
                  QVariant::fromValue(reinterpret_cast<quintptr>(&block)));

    QStringList search{item->text(kLabelColumn)};
    FlattenFields(BlockFacts(block), search);
    item->setData(kLabelColumn, kPGPInspectSearchRole, search.join('\n'));

    if (block.has_armor && block.armor.crc24 == "mismatch"_L1) {
      FlagRow(item, GF_UI_COLOR_DANGER,
              {GTrC::tr("The armor checksum does not match its contents."),
               block.error});
    } else if (!block.error.isEmpty()) {
      FlagRow(item, GF_UI_COLOR_WARNING, {block.error});
    } else {
      item->setToolTip(kLabelColumn, item->text(kLabelColumn));
    }

    // A block's line runs across all three columns, so its length never
    // pushes the packets' summaries out of view.
    item->setFirstColumnSpanned(true);
    add_packets(item, block.packets, {});
  }

  tree_->expandAll();
  measure_columns();

  // The count goes through a local: a function call inside a translate()
  // %n argument makes lupdate silently skip EVERY string in the file.
  const int packet_count = document_.PacketCount();
  auto summary = GTrC::tr("%1, %2, %n packet(s)", nullptr, packet_count)
                     .arg(document_.format, HumanSize(size_));
  if (!document_.errors.isEmpty()) {
    summary += "  -  "_L1 + document_.errors.join("; "_L1);
  }
  summary_label_->setText(summary);

  // Open on the first packet, so the tabs are never blank.
  auto* first = tree_->topLevelItem(0);
  if (first != nullptr && first->childCount() > 0) first = first->child(0);
  tree_->setCurrentItem(first);
  if (first == nullptr) show_tabs(false, false, false);
}

void PGPInspectDialog::measure_columns() {
  // The name column needs the longest "#index  Type" at its depth; the size
  // column its longest size. Measured once, laid out on every resize.
  const auto metrics = tree_->fontMetrics();
  const auto padding = (2 * tree_->style()->pixelMetric(
                                QStyle::PM_FocusFrameHMargin, nullptr, tree_)) +
                       (metrics.averageCharWidth() * 2);
  label_width_ = 0;
  index_width_ = 0;
  size_width_ = 0;
  for (QTreeWidgetItemIterator it(tree_); *it != nullptr; ++it) {
    if (!(*it)->data(kLabelColumn, kPacketRole).isValid()) continue;
    int depth = 0;
    for (auto* up = (*it)->parent(); up != nullptr; up = up->parent()) ++depth;
    const auto indent = tree_->indentation() * (depth + 1);
    label_width_ =
        std::max(label_width_,
                 indent + metrics.horizontalAdvance((*it)->text(kLabelColumn)) +
                     padding);
    // The index alone is what may never be cut: "#1…" could be #10 or #15.
    const auto index = (*it)->data(kLabelColumn, kIndexRole).toString();
    index_width_ = std::max(
        index_width_,
        indent + metrics.horizontalAdvance("#"_L1 + index + "…"_L1) + padding);
    size_width_ =
        std::max(size_width_,
                 metrics.horizontalAdvance((*it)->text(kSizeColumn)) + padding);
  }
  layout_columns();
}

void PGPInspectDialog::layout_columns() {
  if (label_width_ <= 0) return;
  // Laid out here rather than by the header's stretch logic, which keeps a
  // minimum for the stretched column and would push the size out of view
  // first. In a narrow pane the summary goes first, then the name; the size
  // stays visible.
  const auto viewport = tree_->viewport()->width();
  const auto label =
      std::clamp(viewport - size_width_, std::min(index_width_, label_width_),
                 label_width_);
  tree_->setColumnWidth(kLabelColumn, label);
  tree_->setColumnWidth(kValueColumn,
                        std::max(0, viewport - label - size_width_));
  tree_->setColumnWidth(kSizeColumn, size_width_);
}

auto PGPInspectDialog::eventFilter(QObject* watched, QEvent* event) -> bool {
  if (watched == tree_->viewport() && event->type() == QEvent::Resize) {
    layout_columns();
  }
  return QDialog::eventFilter(watched, event);
}

void PGPInspectDialog::add_packets(QTreeWidgetItem* parent,
                                   const QList<PGPInspectPacket>& packets,
                                   const QString& parent_index) {
  int position = 1;
  for (const auto& packet : packets) {
    const auto index = PGPInspectPacketIndex(parent_index, position++);
    auto* item = new QTreeWidgetItem(parent);
    item->setText(kLabelColumn, PGPInspectPacketSummary(packet, index));
    item->setToolTip(kLabelColumn, item->text(kLabelColumn));
    item->setData(kLabelColumn, kIndexRole, index);
    item->setText(kValueColumn, packet.decoded.headline);
    item->setToolTip(kValueColumn, packet.decoded.headline);
    item->setText(kSizeColumn, HumanSize(packet.WireLength()));
    item->setTextAlignment(kSizeColumn, Qt::AlignRight | Qt::AlignVCenter);
    item->setForeground(kSizeColumn, ThemeBrush(GF_UI_COLOR_MUTED_TEXT));
    item->setForeground(kValueColumn, ThemeBrush(GF_UI_COLOR_MUTED_TEXT));
    item->setData(kLabelColumn, kPacketRole,
                  QVariant::fromValue(reinterpret_cast<quintptr>(&packet)));

    QStringList search{item->text(kLabelColumn), item->text(kValueColumn),
                       packet.error, packet.decoded.error};
    FlattenFields(packet.decoded.overview, search);
    FlattenFields(packet.decoded.subpackets, search);
    FlattenFields(packet.decoded.material, search);
    search << packet.decoded.material_summary;
    item->setData(kLabelColumn, kPGPInspectSearchRole, search.join('\n'));

    // Flagged only for what the walk or the decoder reported.
    if (packet.Malformed()) {
      FlagRow(item, GF_UI_COLOR_WARNING, {packet.error, packet.decoded.error});
    }

    add_packets(item, packet.children, index);
  }
}

void PGPInspectDialog::slot_selection_changed(QTreeWidgetItem* current) {
  if (current == nullptr) return;
  const auto packet = current->data(kLabelColumn, kPacketRole);
  if (packet.isValid()) {
    show_packet(
        *reinterpret_cast<const PGPInspectPacket*>(packet.value<quintptr>()),
        current->data(kLabelColumn, kIndexRole).toString());
    return;
  }
  const auto block = current->data(kLabelColumn, kBlockRole);
  if (block.isValid()) {
    show_block(
        *reinterpret_cast<const PGPInspectBlock*>(block.value<quintptr>()));
  }
}

void PGPInspectDialog::show_block(const PGPInspectBlock& block) {
  fill_fields(overview_, BlockFacts(block));
  show_tabs(false, false, false);
}

void PGPInspectDialog::show_packet(const PGPInspectPacket& packet,
                                   const QString& index) {
  const auto& decoded = packet.decoded;

  auto overview = decoded.overview;
  QList<PGPInspectField> problems;
  if (!packet.error.isEmpty()) {
    problems.push_back({GTrC::tr("Problem"), packet.error, {}});
  }
  if (!decoded.error.isEmpty()) {
    problems.push_back({GTrC::tr("Problem"), decoded.error, {}});
  }
  overview = problems + overview;

  // Where the packet sits and how it is framed, which every packet has
  // whatever its body says.
  overview.push_back(
      {GTrC::tr("Framing"),
       {},
       {{GTrC::tr("Position"), "#"_L1 + index, {}},
        {GTrC::tr("Packet Type"),
         QString("%1 (%2)").arg(PGPTagName(packet.tag)).arg(packet.tag),
         {}},
        {GTrC::tr("Offset"),
         QString("0x%1").arg(packet.offset, 4, 16, QChar('0')),
         {}},
        {GTrC::tr("Header"),
         GTrC::tr("%1 format, %2")
             .arg(packet.header_version, HumanSize(packet.header_length)),
         {}},
        {GTrC::tr("Body"),
         GTrC::tr("%1, %2 length")
             .arg(HumanSize(packet.body_length), packet.length_type),
         {}},
        {GTrC::tr("Size"), HumanSize(packet.WireLength()), {}}}});
  fill_fields(overview_, overview);

  fill_fields(subpackets_, decoded.subpackets);
  fill_fields(material_, decoded.material);
  material_summary_->setText(decoded.material_summary);
  material_summary_->setVisible(!decoded.material_summary.isEmpty());
  tabs_->setTabText(kMaterialTab, MaterialTabTitle(packet.tag));

  const auto has_hex = !packet.raw.isEmpty();
  hex_->setPlainText(
      has_hex ? PGPFormatHexDump(packet.raw, decoded.spans, packet.WireLength())
              : QString());

  // A summary alone is worth the tab: it is where an undecoded format says so.
  show_tabs(!decoded.subpackets.isEmpty(),
            !decoded.material.isEmpty() || !decoded.material_summary.isEmpty(),
            has_hex);
}

void PGPInspectDialog::fill_fields(QTreeWidget* tree,
                                   const QList<PGPInspectField>& fields) {
  tree->clear();
  AddFieldRows(tree->invisibleRootItem(), fields, search_->text());
  tree->expandAll();
}

void PGPInspectDialog::show_tabs(bool subpackets, bool material, bool hex) {
  switching_tabs_ = true;
  tabs_->setTabVisible(kSubpacketsTab, subpackets);
  tabs_->setTabVisible(kMaterialTab, material);
  tabs_->setTabVisible(kHexTab, hex);
  tabs_->setCurrentIndex(tabs_->isTabVisible(preferred_tab_) ? preferred_tab_
                                                             : kOverviewTab);
  switching_tabs_ = false;
}

void PGPInspectDialog::slot_tab_chosen(int index) {
  // Hiding the current tab moves the selection too; only the reader's own
  // clicks count as a choice worth keeping.
  if (!switching_tabs_ && index >= 0) preferred_tab_ = index;
}

void PGPInspectDialog::slot_filter_changed(const QString& needle) {
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    PGPInspectFilterItem(tree_->topLevelItem(i), needle, false);
  }
  if (!needle.isEmpty()) tree_->expandAll();

  // The tabs must never describe a row the filter just hid: move to the
  // first packet still shown.
  auto* current = tree_->currentItem();
  if (current == nullptr || current->isHidden()) {
    for (QTreeWidgetItemIterator it(tree_, QTreeWidgetItemIterator::NotHidden);
         *it != nullptr; ++it) {
      if ((*it)->data(kLabelColumn, kPacketRole).isValid()) {
        tree_->setCurrentItem(*it);  // re-renders the tabs
        return;
      }
    }
  }
  // Re-render the tabs so their matching rows are marked.
  slot_selection_changed(tree_->currentItem());
}

auto PGPInspectFilterItem(QTreeWidgetItem* item, const QString& needle,
                          bool ancestor_matched) -> bool {
  if (item == nullptr) return false;

  const auto searchable = item->data(0, kPGPInspectSearchRole).toString();
  const auto matched = needle.isEmpty() ||
                       item->text(0).contains(needle, Qt::CaseInsensitive) ||
                       item->text(1).contains(needle, Qt::CaseInsensitive) ||
                       searchable.contains(needle, Qt::CaseInsensitive);

  // A matching packet keeps its whole subtree: the reader searched for the
  // packet, so hiding what nests inside it would answer the wrong question.
  auto descendant_matched = false;
  for (int i = 0; i < item->childCount(); ++i) {
    descendant_matched |= PGPInspectFilterItem(item->child(i), needle,
                                               ancestor_matched || matched);
  }

  const auto visible = matched || descendant_matched || ancestor_matched;
  item->setHidden(!visible);
  return visible;
}
