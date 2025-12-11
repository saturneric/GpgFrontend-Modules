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

#include "SearchKeyDialog.h"

#include <QRegularExpression>

#include "GFModuleCommonUtils.hpp"
#include "GFSDKGpg.h"
#include "PKSInterface.h"
#include "VKSInterface.h"

//
#include "ui_SearchKeyDialog.h"

SearchKeyDialog::SearchKeyDialog(QWidget* parent)
    : ui_(SecureCreateSharedObject<Ui_SearchKeyDialog>()) {
  ui_->setupUi(this);

  connect(ui_->searchButton, &QPushButton::clicked, this,
          &SearchKeyDialog::slot_search);

  connect(ui_->tableWidget, &QTableWidget::cellActivated, this,
          &SearchKeyDialog::slot_import);

  slot_set_error_message("");
  slot_set_loading(false);

  ui_->tableWidget->setColumnCount(7);
  QStringList headers;
  headers << tr("Key ID") << tr("UID") << tr("Creation Date")
          << tr("Expiration Date") << tr("Algorithm") << tr("Key Size")
          << tr("Status");
  ui_->tableWidget->setHorizontalHeaderLabels(headers);

  ui_->searchTypeComboBox->addItem(tr("By Key ID"), "keyid");
  ui_->searchTypeComboBox->addItem(tr("By Email"), "email");
  ui_->searchTypeComboBox->addItem(tr("By Fingerprint"), "fpr");

  ui_->keyServerComboBox->addItem("https://keyserver.ubuntu.com");
  ui_->keyServerComboBox->addItem("https://keys.openpgp.org");
  ui_->keyServerComboBox->addItem("https://pgp.mit.edu");

  ui_->keyServerComboBox->setCurrentIndex(0);
}

void SearchKeyDialog::slot_search() {
  if (ui_->searchEdit->text().isEmpty()) {
    slot_set_error_message(tr("Search value is empty."));
    return;
  }

  auto* task = new PKSInterface(this);

  connect(task, &PKSInterface::SignalKeyServerSearchResultParsed, this,
          &SearchKeyDialog::slot_search_finished_pks);

  slot_set_error_message("");
  slot_set_loading(true);
  ui_->tableWidget->clearContents();
  ui_->tableWidget->setRowCount(0);

  auto url = ui_->keyServerComboBox->currentText();
  if (url.isEmpty()) {
    slot_set_error_message(tr("Key server URL is empty."));
    slot_set_loading(false);
    return;
  }

  // check url format
  QUrl keyserver_url(url);
  if (!keyserver_url.isValid() || keyserver_url.scheme().isEmpty() ||
      keyserver_url.host().isEmpty()) {
    slot_set_error_message(tr("Invalid key server URL format."));
    slot_set_loading(false);
    return;
  }

  // check search type email and validate email format
  auto search_type = ui_->searchTypeComboBox->currentData().toString();
  auto search_value = ui_->searchEdit->text().trimmed();

  if (search_type == "email") {
    QRegularExpression email_regex{
        R"(^\s*(.*\s*)?<\s*([a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\.[a-zA-Z0-9-.]+)\s*>\s*$|(^[a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\.[a-zA-Z0-9-.]+$))"};
    if (!email_regex.match(search_value).hasMatch()) {
      slot_set_error_message(tr("Invalid email format."));
      slot_set_loading(false);
      return;
    }
  } else if (search_type == "fpr") {
    if (search_value.startsWith("0x") || search_value.startsWith("0X")) {
      search_value = search_value.mid(2);
    }

    // validate fingerprint format (hex string, length 40 or 16)
    QRegularExpression fpr_regex("^(0x)?[A-Fa-f0-9]{16}([A-Fa-f0-9]{24})?$");
    if (!fpr_regex.match(search_value).hasMatch()) {
      slot_set_error_message(
          tr("Invalid fingerprint format. It should be a hex string of length "
             "16 or 40."));
      slot_set_loading(false);
      return;
    }
  } else if (search_type == "keyid") {
    if (search_value.startsWith("0x") || search_value.startsWith("0X")) {
      search_value = search_value.mid(2);
    }

    // validate keyid format (hex string, length 8 or 16)
    QRegularExpression keyid_regex("^(0x)?[A-Fa-f0-9]{8}([A-Fa-f0-9]{8})?$");
    if (!keyid_regex.match(search_value).hasMatch()) {
      slot_set_error_message(
          tr("Invalid Key ID format. It should be a hex string of length 8 or "
             "16."));
      slot_set_loading(false);
      return;
    }
  } else {
    slot_set_error_message(tr("Unknown search type."));
    slot_set_loading(false);
    return;
  }

  task->Search(keyserver_url.toString(), search_type, search_value);
}

void SearchKeyDialog::slot_set_error_message(const QString& message) {
  ui_->errorLabel->setText("<h4 style='color:red;'>" + message + "</h4>");
}

void SearchKeyDialog::slot_set_loading(bool loading) {
  ui_->progressBar->setVisible(loading);
  ui_->searchButton->setDisabled(loading);
  ui_->keyServerComboBox->setDisabled(loading);
  ui_->searchEdit->setReadOnly(loading);
  ui_->searchTypeComboBox->setDisabled(loading);
}

void SearchKeyDialog::slot_search_finished_pks(
    QNetworkReply::NetworkError error, const QString& error_string,
    const QList<KeyServerKeyInfo>& keys) {
  ui_->tableWidget->clearContents();
  slot_set_error_message("");
  slot_set_loading(false);

  if (error != QNetworkReply::NoError) {
    slot_set_error_message(error_string);
    return;
  }

  ui_->tableWidget->setRowCount(static_cast<int>(keys.size()));

  int row = 0;
  for (const auto& key : keys) {
    auto* keyid_item = new QTableWidgetItem(key.keyid.right(16));
    ui_->tableWidget->setItem(row, 0, keyid_item);

    auto uid = key.uids.isEmpty() ? KeyServerUID() : key.uids.first();
    auto* uid_item = new QTableWidgetItem(uid.uid);
    ui_->tableWidget->setItem(row, 1, uid_item);

    auto* creation_date_item = new QTableWidgetItem(QLocale().toString(
        QDateTime::fromSecsSinceEpoch(key.creation_date.toLongLong()),
        "yyyy-MM-dd"));
    ui_->tableWidget->setItem(row, 2, creation_date_item);

    auto* expiration_date_item = new QTableWidgetItem(QLocale().toString(
        QDateTime::fromSecsSinceEpoch(key.expiration_date.toLongLong()),
        "yyyy-MM-dd"));
    ui_->tableWidget->setItem(row, 3, expiration_date_item);

    auto* algo_item = new QTableWidgetItem(key.algorithm_desc);
    ui_->tableWidget->setItem(row, 4, algo_item);

    auto* size_item = new QTableWidgetItem(key.key_size_desc);
    ui_->tableWidget->setItem(row, 5, size_item);

    auto* flags_item = new QTableWidgetItem(key.flags_desc);
    ui_->tableWidget->setItem(row, 6, flags_item);

    // Apply strikeout font for revoked, disabled, or expired keys
    auto flags = key.flags;
    if (flags.contains("r") || flags.contains("d") || flags.contains("e")) {
      QFont strike_font = keyid_item->font();
      strike_font.setStrikeOut(true);

      keyid_item->setFont(strike_font);
      uid_item->setFont(strike_font);
      creation_date_item->setFont(strike_font);
      expiration_date_item->setFont(strike_font);
      algo_item->setFont(strike_font);
      size_item->setFont(strike_font);
      flags_item->setFont(strike_font);
    }

    ++row;
  }

  ui_->tableWidget->resizeColumnsToContents();
}

void SearchKeyDialog::slot_import(int row, int column) {
  Q_UNUSED(column);

  QString keyid = ui_->tableWidget->item(row, 0)->text();
  FLOG_DEBUG("importing key with keyid %1", keyid);

  auto* task = new PKSInterface(this);

  connect(task, &PKSInterface::SignalKeyServerKeyLookupResult, this,
          &SearchKeyDialog::slot_lookup_finished_pks);

  task->LookupKeyById(ui_->keyServerComboBox->currentText(), keyid);
}

void SearchKeyDialog::slot_lookup_finished_pks(
    QNetworkReply::NetworkError error, const QString& error_string,
    const QByteArray& key_data) {
  if (error != QNetworkReply::NoError) {
    slot_set_error_message(error_string);
    return;
  }

  FLOG_DEBUG("importing key data of size %1, data: %2", key_data.size(),
             QString::fromUtf8(key_data));

  auto channel = GFGpgCurrentGpgContextChannel();
  if (channel < 0) {
    slot_set_error_message(tr("No GPG context is available."));
    return;
  }

  GFGpgImportKeys(channel, this, key_data.data(),
                  static_cast<int>(key_data.size()));
}
