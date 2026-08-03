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

#include "KeyServerSettingsPage.h"

#include <QMessageBox>
#include <QPointer>
#include <QTableWidget>
#include <QUrl>

#include "GFModuleCommonUtils.hpp"

//
#include "ui_KeyServerSettingsPage.h"

namespace {

constexpr int kColumnDefault = 0;
constexpr int kColumnAddress = 1;
constexpr int kColumnHkp = 2;
constexpr int kColumnVks = 3;
constexpr int kColumnStatus = 4;
constexpr int kColumnLastTested = 5;
constexpr int kColumnCount = 6;

}  // namespace

KeyServerSettingsPage::KeyServerSettingsPage(QWidget* parent)
    : QWidget(parent),
      ui_(SecureCreateSharedObject<Ui_KeyServerSettingsPage>()) {
  ui_->setupUi(this);

  ui_->keyServerListGroupBox->setTitle(tr("Key Server List"));
  ui_->addGroupBox->setTitle(tr("Add a Key Server"));
  ui_->operationsGroupBox->setTitle(tr("Operations"));

  ui_->addKeyServerButton->setText(tr("Add"));
  ui_->setDefaultButton->setText(tr("Set As Default"));
  ui_->testButton->setText(tr("Test Selected"));
  ui_->removeButton->setText(tr("Delete Selected"));

  ui_->addKeyServerEdit->setPlaceholderText(tr("https://keys.example.org"));
  ui_->tipsLabel->setText(tr(
      "A new key server is tested against the HKP and VKS interfaces before "
      "it is added. Searching uses HKP. Publishing and refreshing always use "
      "the default server: over VKS where it offers it, over HKP otherwise."));

  ui_->keyServerTable->setColumnCount(kColumnCount);
  ui_->keyServerTable->setHorizontalHeaderLabels(
      {tr("Default"), tr("Address"), tr("HKP"), tr("VKS"), tr("Status"),
       tr("Last Tested")});

  connect(ui_->addKeyServerButton, &QPushButton::clicked, this,
          &KeyServerSettingsPage::slot_add);
  // Typing an address and pressing Enter is the obvious way to add one.
  connect(ui_->addKeyServerEdit, &QLineEdit::returnPressed, this,
          &KeyServerSettingsPage::slot_add);
  connect(ui_->removeButton, &QPushButton::clicked, this,
          &KeyServerSettingsPage::slot_remove);
  connect(ui_->setDefaultButton, &QPushButton::clicked, this,
          &KeyServerSettingsPage::slot_set_default);
  connect(ui_->testButton, &QPushButton::clicked, this,
          &KeyServerSettingsPage::slot_test_selected);

  switch_ui_busy(false);
  SetSettings();
}

void KeyServerSettingsPage::SetSettings() {
  entries_ = KeyServerList::Load();
  default_url_ = KeyServerList::DefaultUrl();
  refresh_table();
}

void KeyServerSettingsPage::ApplySettings() {
  KeyServerList::Store(entries_, default_url_);
}

void KeyServerSettingsPage::refresh_table() {
  ui_->keyServerTable->clearContents();
  ui_->keyServerTable->setRowCount(static_cast<int>(entries_.size()));

  const auto yes = tr("yes");
  const auto no = tr("no");

  int row = 0;
  for (const auto& entry : entries_) {
    const auto set = [this, row](int column, const QString& text) {
      auto* item = new QTableWidgetItem(text);
      item->setTextAlignment(Qt::AlignCenter);
      ui_->keyServerTable->setItem(row, column, item);
      return item;
    };

    set(kColumnDefault, entry.url == default_url_ ? "*" : QString{});
    auto* address = set(kColumnAddress, entry.url);
    address->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    set(kColumnHkp, entry.hkp ? yes : no);
    set(kColumnVks, entry.vks ? yes : no);

    auto* status =
        set(kColumnStatus, entry.verified ? tr("Verified") : tr("Unverified"));
    // The reason a server did not verify is too long for a cell but too useful
    // to drop, so it lives where the user can reach it on demand.
    if (!entry.detail.isEmpty()) status->setToolTip(entry.detail);

    set(kColumnLastTested,
        entry.last_tested.isValid()
            ? QLocale().toString(entry.last_tested.toLocalTime(),
                                 QLocale::ShortFormat)
            : tr("never"));

    ++row;
  }

  for (int column = 0; column < kColumnCount; ++column) {
    ui_->keyServerTable->resizeColumnToContents(column);
  }
}

auto KeyServerSettingsPage::selected_row() const -> int {
  const auto selection = ui_->keyServerTable->selectionModel()->selectedRows();
  return selection.isEmpty() ? -1 : selection.first().row();
}

void KeyServerSettingsPage::slot_add() {
  auto url = ui_->addKeyServerEdit->text().trimmed();
  if (url.isEmpty()) return;

  // Someone typing a bare hostname means https, and silently guessing http
  // would downgrade them without asking.
  if (!url.contains("://")) url.prepend("https://");
  while (url.endsWith('/')) url.chop(1);

  const QUrl parsed(url);
  if (!parsed.isValid() || parsed.host().isEmpty()) {
    QMessageBox::warning(this, tr("Invalid Address"),
                         tr("\"%1\" is not a valid key server address.")
                             .arg(ui_->addKeyServerEdit->text().trimmed()));
    return;
  }

  const auto already_listed =
      std::any_of(entries_.cbegin(), entries_.cend(),
                  [&url](const KeyServerEntry& e) { return e.url == url; });
  if (already_listed) {
    QMessageBox::information(
        this, tr("Already Listed"),
        tr("%1 is already in the key server list.").arg(url));
    return;
  }

  if (parsed.scheme() == "http") {
    const auto answer = QMessageBox::warning(
        this, tr("Insecure Key Server Address"),
        tr("%1 uses plain HTTP, so anyone on the network can see and change "
           "what you look up or publish. Add it anyway?")
            .arg(url),
        QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer == QMessageBox::Cancel) return;
  }

  KeyServerEntry entry;
  entry.url = url;
  entries_.append(entry);

  // First entry added to an emptied list becomes the default, so the list is
  // never in a state where nothing is selected.
  if (default_url_.isEmpty()) default_url_ = url;

  ui_->addKeyServerEdit->clear();
  refresh_table();
  start_probe(url);
}

void KeyServerSettingsPage::slot_remove() {
  const auto row = selected_row();
  if (row < 0 || row >= entries_.size()) return;

  const auto removed = entries_.at(row).url;
  entries_.removeAt(row);

  // Removing the default has to hand the role to somebody, or every operation
  // would fall back to whatever happened to be first anyway — better to make
  // that explicit and visible in the table.
  if (default_url_ == removed) {
    default_url_ = entries_.isEmpty() ? QString() : entries_.first().url;
  }

  refresh_table();
}

void KeyServerSettingsPage::slot_set_default() {
  const auto row = selected_row();
  if (row < 0 || row >= entries_.size()) return;

  const auto& entry = entries_.at(row);

  // Everything still goes to this server, but publishing over HKP is a weaker
  // deal than over VKS, and here is where the user can pick a different one
  // without hunting for the setting later.
  if (entry.verified && !entry.vks) {
    const auto answer = QMessageBox::warning(
        this, tr("No Verified Publishing"),
        tr("%1 does not support the VKS interface, so publishing and "
           "refreshing will use HKP instead.\n\n"
           "Over HKP the server does not confirm your email address, and an "
           "uploaded key cannot be removed again.\n\n"
           "Use %1 as the default anyway?")
            .arg(QUrl(entry.url).host()),
        QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Ok) return;
  }

  default_url_ = entry.url;
  refresh_table();
}

void KeyServerSettingsPage::slot_test_selected() {
  const auto row = selected_row();
  if (row < 0 || row >= entries_.size()) return;

  start_probe(entries_.at(row).url);
}

void KeyServerSettingsPage::start_probe(const QString& url) {
  ++pending_probes_;
  switch_ui_busy(true);

  auto* prober = new KeyServerProber(this);
  QPointer<KeyServerSettingsPage> self(this);
  connect(prober, &KeyServerProber::SignalProbeFinished, this,
          [self, prober](const KeyServerProber::Result& result) {
            // The dialog can be closed while a probe is still in the air.
            if (self != nullptr) self->apply_probe_result(result);
            prober->deleteLater();
          });

  prober->Probe(url);
}

void KeyServerSettingsPage::apply_probe_result(
    const KeyServerProber::Result& result) {
  --pending_probes_;
  switch_ui_busy(pending_probes_ > 0);

  const auto index = std::find_if(
      entries_.begin(), entries_.end(),
      [&result](const KeyServerEntry& e) { return e.url == result.url; });
  // The row may have been deleted while its probe was running.
  if (index == entries_.end()) return;

  index->hkp = result.hkp;
  index->vks = result.vks;
  index->verified = result.Conforms();
  index->detail = result.detail;
  index->last_tested = QDateTime::currentDateTimeUtc();

  refresh_table();

  if (!result.Conforms()) {
    // The entry stays. A server can be down, or behind a network that is
    // blocking it right now, and dropping it would make the user retype the
    // address to find out — Test Selected re-runs this whenever they want.
    QMessageBox::warning(
        this, tr("Key Server Not Verified"),
        tr("%1 did not answer as a key server.\n\n%2\n\nIt has been added and "
           "marked unverified; use Test Selected to try again.")
            .arg(result.url, result.detail));
  }
}

void KeyServerSettingsPage::switch_ui_busy(bool busy) {
  ui_->progressBar->setVisible(busy);
  ui_->addKeyServerButton->setDisabled(busy);
  ui_->testButton->setDisabled(busy);
}

auto KeyServerSettingsPageFactory(void* data) -> void* {
  Q_UNUSED(data);
  return new KeyServerSettingsPage();
}
