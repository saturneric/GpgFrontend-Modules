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

#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <array>

namespace {

/// Which column holds what. Packet rows use the first only; a field row puts
/// its label there and its value beside it.
constexpr int kLabelColumn = 0;
constexpr int kValueColumn = 1;

auto HumanSize(qint64 bytes) -> QString {
  // A pure helper: it writes into the caller's buffer rather than allocating,
  // so there is no context and nothing to free. 64 bytes is far more than any
  // formatted size needs.
  std::array<char, 64> text{};
  const auto written = GFUIHumanSize(bytes, text.data(), text.size());
  return written < 0 ? QString() : QString::fromUtf8(text.data(), written);
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
  setWindowTitle(QCoreApplication::translate("GTrC", "OpenPGP Structure"));
  setAttribute(Qt::WA_DeleteOnClose);
  resize(820, 620);
  create_widgets();
  render_document();
}

void PGPInspectDialog::create_widgets() {
  search_ = new QLineEdit(this);
  search_->setClearButtonEnabled(true);
  search_->setPlaceholderText(
      QCoreApplication::translate("GTrC", "Search packets, fields and values"));
  connect(search_, &QLineEdit::textChanged, this,
          &PGPInspectDialog::slot_filter_changed);

  tree_ = new QTreeWidget(this);
  tree_->setColumnCount(2);
  tree_->setHeaderLabels({QCoreApplication::translate("GTrC", "Structure"),
                          QCoreApplication::translate("GTrC", "Value")});
  tree_->header()->setSectionResizeMode(kLabelColumn,
                                        QHeaderView::ResizeToContents);
  tree_->header()->setSectionResizeMode(kValueColumn, QHeaderView::Stretch);
  tree_->setUniformRowHeights(true);

  summary_label_ = new QLabel(this);
  summary_label_->setWordWrap(true);

  auto* layout = new QVBoxLayout();
  layout->addWidget(search_);
  layout->addWidget(tree_);
  layout->addWidget(summary_label_);
  setLayout(layout);
}

void PGPInspectDialog::render_document() {
  tree_->clear();

  if (!document_.valid) {
    summary_label_->setText(
        document_.parse_error.isEmpty()
            ? QCoreApplication::translate(
                  "GTrC", "The current tab holds no OpenPGP data.")
            : document_.parse_error);
    return;
  }

  int index = 1;
  for (const auto& block : document_.blocks) {
    auto* item = new QTreeWidgetItem(tree_);
    item->setText(kLabelColumn, PGPInspectBlockSummary(block, index++));

    // The block's own facts are rows like any other, so the search box reaches
    // the armor headers and the CRC verdict too.
    QVector<PGPInspectField> facts;
    if (block.has_armor) {
      facts.push_back({QCoreApplication::translate("GTrC", "Armor Type"),
                       block.armor.block_type});
      facts.push_back(
          {QCoreApplication::translate("GTrC", "CRC24"), block.armor.crc24});
      facts += block.armor.headers;
    }
    if (block.has_cleartext) {
      facts.push_back({QCoreApplication::translate("GTrC", "Signed Text"),
                       HumanSize(block.cleartext_text_size)});
      facts += block.cleartext_headers;
    }
    if (!block.error.isEmpty()) {
      facts.push_back(
          {QCoreApplication::translate("GTrC", "Problem"), block.error});
    }
    add_fields(item, facts);

    if (block.has_armor && block.armor.crc24 == QLatin1String("mismatch")) {
      item->setForeground(
          kLabelColumn, QBrush(QColor(GFUIThemeColorForRole(GFModuleSdkContext(), GF_UI_COLOR_DANGER))));
    } else if (!block.error.isEmpty()) {
      item->setForeground(
          kLabelColumn, QBrush(QColor(GFUIThemeColorForRole(GFModuleSdkContext(), GF_UI_COLOR_WARNING))));
    }

    add_packets(item, block.packets);
  }

  tree_->expandAll();

  // The count goes through a local: a function call inside a translate()
  // %n argument makes lupdate silently skip EVERY string in the file.
  const int packet_count = document_.PacketCount();
  auto summary = QCoreApplication::translate("GTrC", "%1, %2, %n packet(s)",
                                             nullptr, packet_count)
                     .arg(document_.format, HumanSize(size_));
  if (!document_.errors.isEmpty()) {
    summary +=
        QLatin1String("  -  ") + document_.errors.join(QLatin1String("; "));
  }
  summary_label_->setText(summary);
}

void PGPInspectDialog::add_packets(QTreeWidgetItem* parent,
                                   const QVector<PGPInspectPacket>& packets) {
  int index = 1;
  for (const auto& packet : packets) {
    auto* item = new QTreeWidgetItem(parent);
    item->setText(kLabelColumn, PGPInspectPacketSummary(packet, index++));

    if (packet.Malformed()) {
      item->setForeground(
          kLabelColumn, QBrush(QColor(GFUIThemeColorForRole(GFModuleSdkContext(), GF_UI_COLOR_WARNING))));
    }

    auto fields = packet.fields;
    if (packet.Malformed()) {
      fields.push_back(
          {QCoreApplication::translate("GTrC", "Problem"), packet.error});
    }
    add_fields(item, fields);

    add_packets(item, packet.children);
  }
}

void PGPInspectDialog::add_fields(QTreeWidgetItem* parent,
                                  const QVector<PGPInspectField>& fields) {
  for (const auto& field : fields) {
    auto* row = new QTreeWidgetItem(parent);
    row->setText(kLabelColumn, field.label);
    row->setText(kValueColumn, field.value);
    row->setForeground(
        kLabelColumn,
        QBrush(QColor(GFUIThemeColorForRole(GFModuleSdkContext(), GF_UI_COLOR_MUTED_TEXT))));
  }
}

void PGPInspectDialog::slot_filter_changed(const QString& needle) {
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    PGPInspectFilterItem(tree_->topLevelItem(i), needle, false);
  }
  if (!needle.isEmpty()) tree_->expandAll();
}

auto PGPInspectFilterItem(QTreeWidgetItem* item, const QString& needle,
                          bool ancestor_matched) -> bool {
  if (item == nullptr) return false;

  const auto matched = needle.isEmpty() ||
                       item->text(0).contains(needle, Qt::CaseInsensitive) ||
                       item->text(1).contains(needle, Qt::CaseInsensitive);

  // A matching packet keeps its whole subtree: the reader searched for the
  // packet, so hiding the fields that describe it would answer the wrong
  // question.
  auto descendant_matched = false;
  for (int i = 0; i < item->childCount(); ++i) {
    descendant_matched |= PGPInspectFilterItem(item->child(i), needle,
                                               ancestor_matched || matched);
  }

  const auto visible = matched || descendant_matched || ancestor_matched;
  item->setHidden(!visible);
  return visible;
}
