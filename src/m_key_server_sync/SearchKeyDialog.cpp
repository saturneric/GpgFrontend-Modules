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

#include "GFModule.h"
#include "GFSDKGpg.h"
#include "KeyServerList.h"
#include "PKSInterface.h"
#include "VKSInterface.h"

//
#include "ui_SearchKeyDialog.h"

namespace {

/// Row data slot holding the untruncated handle the keyserver reported, so the
/// import lookup never has to reconstruct it from the shortened display text.
constexpr int kKeyHandleRole = Qt::UserRole + 1;

/// Shorten a keyserver handle to the conventional long key ID for display.
///
/// Where the key ID lives inside the fingerprint depends on the key version: a
/// v4 fingerprint (SHA-1, 40 hex) ends with it, while a v6 fingerprint
/// (SHA-256, 64 hex) starts with it (RFC 9580 §5.5.4.3). Blindly taking the
/// last 16 chars — as this used to — turns a v6 fingerprint into a handle no
/// keyserver can resolve.
auto ShortenKeyHandle(const QString& handle) -> QString {
  if (handle.size() == 64) return handle.left(16);
  if (handle.size() == 40) return handle.right(16);
  return handle;
}

/// Render a keyserver timestamp.
///
/// The machine-readable format leaves a field empty to mean the date does not
/// apply — a key with no expiry sends nothing at all. Feeding that straight to
/// fromSecsSinceEpoch() printed 1970-01-01, which reads as an expired key.
auto FormatKeyServerDate(const QString& seconds, const QString& absent)
    -> QString {
  bool ok = false;
  const auto value = seconds.toLongLong(&ok);
  if (!ok || value <= 0) return absent;

  return QLocale().toString(QDateTime::fromSecsSinceEpoch(value), "yyyy-MM-dd");
}

}  // namespace

SearchKeyDialog::SearchKeyDialog(QWidget* parent)
    : QDialog(parent), ui_(SdkCreateSharedObject<Ui_SearchKeyDialog>()) {
  init_ui();
}

SearchKeyDialog::SearchKeyDialog(const QString& fingerprint, QWidget* parent)
    : QDialog(parent), ui_(SdkCreateSharedObject<Ui_SearchKeyDialog>()) {
  init_ui();
  SetPresetFingerprint(fingerprint);
}

void SearchKeyDialog::init_ui() {
  ui_->setupUi(this);

  connect(ui_->searchButton, &QPushButton::clicked, this,
          &SearchKeyDialog::slot_search);

  // Pressing Enter in the search field (or anywhere in the dialog) must run the
  // search: without this the dialog looks like it "does nothing" until you find
  // the button.
  ui_->searchButton->setDefault(true);
  ui_->searchButton->setAutoDefault(true);
  connect(ui_->searchEdit, &QLineEdit::returnPressed, this,
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

  ui_->searchEdit->setPlaceholderText(
      tr("Enter a value, then press Enter or Search"));
  ui_->searchEdit->setFocus();

  // The list comes from Settings now. The box stays editable so a one-off
  // server can still be typed in, but only what is configured is offered — and
  // an ad-hoc address is not silently added to the list.
  ui_->keyServerComboBox->addItems(KeyServerList::Urls());

  const auto preferred = KeyServerList::UrlFor(KeyServerList::Capability::kHKP);
  const auto index = ui_->keyServerComboBox->findText(preferred);
  ui_->keyServerComboBox->setCurrentIndex(index >= 0 ? index : 0);
}

void SearchKeyDialog::SetPresetFingerprint(const QString& fingerprint) {
  auto fpr = fingerprint.trimmed();

  if (fpr.startsWith("0x", Qt::CaseInsensitive)) {
    fpr = fpr.mid(2);
  }

  fpr.remove(QRegularExpression(R"(\s+)"));

  set_search_type("fpr");
  ui_->searchEdit->setText(fpr);
  ui_->searchEdit->selectAll();
  ui_->searchEdit->setFocus();

  slot_set_error_message("");
}

void SearchKeyDialog::set_search_type(const QString& type) {
  const auto index = ui_->searchTypeComboBox->findData(type);
  if (index >= 0) {
    ui_->searchTypeComboBox->setCurrentIndex(index);
  }
}

void SearchKeyDialog::slot_search() {
  FLOG_DEBUG("key server search triggered, type: %1, value length: %2",
             ui_->searchTypeComboBox->currentData().toString(),
             ui_->searchEdit->text().trimmed().size());

  if (ui_->searchEdit->text().trimmed().isEmpty()) {
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

    // validate fingerprint format: 16 (long key ID), 40 (v4 fingerprint) or
    // 64 (v6 fingerprint, SHA-256 per RFC 9580) hex characters.
    QRegularExpression fpr_regex(
        "^(0x)?([A-Fa-f0-9]{16}|[A-Fa-f0-9]{40}|[A-Fa-f0-9]{64})$");
    if (!fpr_regex.match(search_value).hasMatch()) {
      slot_set_error_message(
          tr("Invalid fingerprint format. It should be a hex string of length "
             "16, 40 or 64."));
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

void SearchKeyDialog::slot_set_info_message(const QString& message) {
  // Neutral (non-red) status line for benign outcomes such as an empty result.
  ui_->errorLabel->setText("<h4>" + message + "</h4>");
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
  ui_->tableWidget->setRowCount(0);
  slot_set_error_message("");
  slot_set_loading(false);

  // HKP keyservers answer /pks/lookup with 404 (ContentNotFoundError) when the
  // search matched no keys. That is an empty result, not a transfer failure, so
  // show a plain notice instead of the alarming raw transport error + URL.
  if (error == QNetworkReply::ContentNotFoundError) {
    slot_set_info_message(tr("No keys found matching your search."));
    return;
  }

  if (error != QNetworkReply::NoError) {
    slot_set_error_message(error_string);
    return;
  }

  if (keys.isEmpty()) {
    slot_set_info_message(tr("No keys found matching your search."));
    return;
  }

  ui_->tableWidget->setRowCount(static_cast<int>(keys.size()));

  int row = 0;
  for (const auto& key : keys) {
    auto* keyid_item = new QTableWidgetItem(ShortenKeyHandle(key.keyid));
    // Import must look the key up by whatever the server reported — for a v6
    // key that is the full 64-hex fingerprint, which the cell no longer shows.
    keyid_item->setData(kKeyHandleRole, key.keyid);
    ui_->tableWidget->setItem(row, 0, keyid_item);

    auto uid = key.uids.isEmpty() ? KeyServerUID() : key.uids.first();
    // A key can legitimately come back with no identity attached: verifying
    // key servers withhold user IDs until the address has been confirmed. An
    // empty cell would read as a parsing failure.
    auto* uid_item = new QTableWidgetItem(
        uid.uid.trimmed().isEmpty() ? tr("(no user ID published)") : uid.uid);
    ui_->tableWidget->setItem(row, 1, uid_item);

    auto* creation_date_item = new QTableWidgetItem(
        FormatKeyServerDate(key.creation_date, tr("Unknown")));
    ui_->tableWidget->setItem(row, 2, creation_date_item);

    auto* expiration_date_item = new QTableWidgetItem(
        FormatKeyServerDate(key.expiration_date, tr("Never")));
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

  auto* keyid_item = row >= 0 ? ui_->tableWidget->item(row, 0) : nullptr;
  if (keyid_item == nullptr) return;

  auto handle = keyid_item->data(kKeyHandleRole).toString();
  if (handle.isEmpty()) handle = keyid_item->text();
  FLOG_DEBUG("importing key with handle %1", handle);

  auto* task = new PKSInterface(this);

  connect(task, &PKSInterface::SignalKeyServerKeyLookupResult, this,
          &SearchKeyDialog::slot_lookup_finished_pks);

  task->LookupKeyById(ui_->keyServerComboBox->currentText(), handle);
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
