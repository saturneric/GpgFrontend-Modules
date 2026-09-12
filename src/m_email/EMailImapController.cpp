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

#include "EMailImapController.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QThread>
#include <QVBoxLayout>

#include "EMailAccountStore.h"
#include "EMailCredentialStore.h"
#include "GFModuleCommonUtils.hpp"

namespace {

auto Tr(const char* text) -> QString {
  return QCoreApplication::translate("EMailImapController", text);
}

enum Column { kDate = 0, kFrom, kSubject, kSize, kMessageId, kColumnCount };

}  // namespace

EMailImapController::EMailImapController(QWidget* parent) : QDialog(parent) {
  setWindowTitle(Tr("Import from IMAP"));
  resize(900, 560);
  build_ui();

  accounts_ = EMailAccountStore::ImapAccounts();
  for (const auto& account : accounts_) {
    account_combo_->addItem(account.Label().isEmpty() ? account.imap.host
                                                      : account.Label());
  }

  const auto preferred = EMailAccountStore::DefaultAccount();
  for (int i = 0; i < accounts_.size(); ++i) {
    if (accounts_.at(i).id != preferred.id) continue;
    account_combo_->setCurrentIndex(i);
    break;
  }

  start_worker();

  // Deliberately NOT connecting here. Opening this window is a request to
  // choose an account, not to reach out over the network -- connecting on open
  // is what made the dialog demand a password before the user had even seen
  // it.
  if (accounts_.isEmpty()) {
    status_label_->setText(
        Tr("No mail account with IMAP enabled is configured yet."));
  } else {
    status_label_->setText(
        Tr("Choose an account and select Connect to browse its messages."));
  }
  refresh_idle_state();
}

EMailImapController::~EMailImapController() { stop_worker(); }

auto EMailImapController::HasUsableAccount() -> bool {
  return !EMailAccountStore::ImapAccounts().isEmpty();
}

void EMailImapController::build_ui() {
  auto* outer = new QVBoxLayout(this);

  auto* top = new QHBoxLayout;
  account_combo_ = new QComboBox(this);
  folder_combo_ = new QComboBox(this);
  search_edit_ = new QLineEdit(this);
  search_edit_->setPlaceholderText(Tr("Search subject or sender"));
  search_button_ = new QPushButton(Tr("Search"), this);

  top->addWidget(new QLabel(Tr("Account"), this));
  top->addWidget(account_combo_, 1);
  top->addWidget(new QLabel(Tr("Folder"), this));
  top->addWidget(folder_combo_, 1);
  top->addWidget(search_edit_, 2);
  top->addWidget(search_button_);
  outer->addLayout(top);

  table_ = new QTableWidget(this);
  table_->setColumnCount(kColumnCount);
  table_->setHorizontalHeaderLabels(
      {Tr("Date"), Tr("From"), Tr("Subject"), Tr("Size"), Tr("Message-ID")});
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->setSelectionMode(QAbstractItemView::SingleSelection);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->horizontalHeader()->setStretchLastSection(false);
  table_->horizontalHeader()->setSectionResizeMode(kSubject,
                                                   QHeaderView::Stretch);
  outer->addWidget(table_, 1);

  status_label_ = new QLabel(this);
  status_label_->setWordWrap(true);
  outer->addWidget(status_label_);

  auto* buttons = new QHBoxLayout;
  connect_button_ = new QPushButton(Tr("Connect"), this);
  connect_button_->setDefault(true);
  more_button_ = new QPushButton(Tr("Load more"), this);
  open_button_ = new QPushButton(Tr("Open"), this);
  open_button_->setDefault(true);
  cancel_button_ = new QPushButton(Tr("Cancel"), this);

  buttons->addWidget(more_button_);
  buttons->addStretch();
  buttons->addWidget(connect_button_);
  buttons->addWidget(open_button_);
  buttons->addWidget(cancel_button_);
  outer->addLayout(buttons);

  connect(account_combo_, &QComboBox::currentIndexChanged, this,
          &EMailImapController::slot_account_changed);
  connect(folder_combo_, &QComboBox::currentIndexChanged, this,
          &EMailImapController::slot_folder_changed);
  connect(search_button_, &QPushButton::clicked, this,
          &EMailImapController::slot_search);
  connect(search_edit_, &QLineEdit::returnPressed, this,
          &EMailImapController::slot_search);
  connect(more_button_, &QPushButton::clicked, this,
          &EMailImapController::slot_load_more);
  connect(open_button_, &QPushButton::clicked, this,
          &EMailImapController::slot_open_selected);
  connect(table_, &QTableWidget::itemDoubleClicked, this,
          &EMailImapController::slot_open_selected);
  connect(cancel_button_, &QPushButton::clicked, this, &QDialog::reject);
  connect(connect_button_, &QPushButton::clicked, this,
          &EMailImapController::slot_connect);
}

void EMailImapController::start_worker() {
  thread_ = new QThread(this);
  worker_ = new EMailImapWorker;

  // Moved before anything is asked of it, so every vmime object it creates is
  // born with the worker thread's affinity. None of that graph is thread-safe
  // and an IMAP connection is strictly sequential, which is exactly what one
  // worker on one thread with a queued event loop gives us for free.
  worker_->moveToThread(thread_);
  connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);

  // The controller is the context object on every one of these, so a result
  // that arrives after this window is gone is discarded by Qt rather than
  // delivered into a destroyed widget.
  connect(worker_, &EMailImapWorker::SignalConnected, this,
          &EMailImapController::handle_connected);
  connect(worker_, &EMailImapWorker::SignalFolders, this,
          &EMailImapController::handle_folders);
  connect(worker_, &EMailImapWorker::SignalMessages, this,
          &EMailImapController::handle_messages);
  connect(worker_, &EMailImapWorker::SignalMessageFetched, this,
          &EMailImapController::handle_fetched);
  connect(worker_, &EMailImapWorker::SignalFailed, this,
          &EMailImapController::handle_failed);

  thread_->start();
}

void EMailImapController::stop_worker() {
  if (thread_ == nullptr) return;

  if (worker_ != nullptr) {
    // Unblocks whatever socket call the worker is sitting inside; without it
    // quit() would wait on an event loop that is not currently running.
    worker_->Token()->Cancel();
    QMetaObject::invokeMethod(worker_, "Disconnect", Qt::QueuedConnection);
  }

  thread_->quit();

  // Bounded, and deliberately not followed by terminate(): killing a thread
  // inside OpenSSL or getaddrinfo corrupts process state, so a thread that
  // will not come back is leaked on purpose rather than made to stop.
  if (!thread_->wait(5000)) {
    LOG_ERROR("IMAP worker thread did not stop; leaving it to finish");
    thread_->deleteLater();
  }

  thread_ = nullptr;
  worker_ = nullptr;
}

void EMailImapController::closeEvent(QCloseEvent* event) {
  closing_ = true;
  if (worker_ != nullptr) worker_->Token()->Cancel();
  QDialog::closeEvent(event);
}

void EMailImapController::connect_to_selected_account() {
  const auto index = account_combo_->currentIndex();
  if (index < 0 || index >= accounts_.size()) return;

  const auto account = accounts_.at(index);

  auto password = EMailCredentialStore::Load(account.id);
  if (password.isEmpty()) {
    // No prompt. A password is configuration, and it is set in exactly one
    // place -- Settings -- so there is a single answer to "where does this
    // credential live", rather than a dialog that holds one for a session and
    // then forgets it.
    status_label_->setText(
        Tr("No password is stored for this account. Set one in Settings, "
           "under Mail Accounts."));
    QMessageBox::information(
        this, Tr("No password stored"),
        Tr("This account has no stored password, so it cannot be opened. "
           "Set the password in Settings, under Mail Accounts, and turn on "
           "the option to remember it."));
    return;
  }

  folders_.clear();
  folder_combo_->clear();
  rows_.clear();
  refresh_table();

  set_busy(true, Tr("Connecting..."));
  const auto seq = next_seq();
  QMetaObject::invokeMethod(
      worker_, "Connect", Qt::QueuedConnection, Q_ARG(quint64, seq),
      Q_ARG(MailAccountConfig, account), Q_ARG(QString, password));
  password.fill(QChar('\0'));
}

void EMailImapController::slot_account_changed() {
  if (closing_) return;

  // Switching accounts invalidates what is on screen, but is not itself a
  // request to connect -- that stays an explicit act.
  folders_.clear();
  folder_combo_->clear();
  rows_.clear();
  current_folder_.clear();
  cursor_ = 0;
  refresh_table();

  if (worker_ != nullptr) {
    QMetaObject::invokeMethod(worker_, "Disconnect", Qt::QueuedConnection);
  }

  status_label_->setText(
      Tr("Choose an account and select Connect to browse its messages."));
  refresh_idle_state();
}

void EMailImapController::slot_connect() { connect_to_selected_account(); }

void EMailImapController::refresh_idle_state() {
  const auto has_account = account_combo_->currentIndex() >= 0;
  const auto connected = !folders_.isEmpty();

  connect_button_->setVisible(!connected);
  connect_button_->setEnabled(has_account);

  folder_combo_->setEnabled(connected);
  search_edit_->setEnabled(connected);
  search_button_->setEnabled(connected);
  open_button_->setEnabled(connected);
  more_button_->setEnabled(false);
}

void EMailImapController::slot_folder_changed() {
  const auto index = folder_combo_->currentIndex();
  if (index < 0 || index >= folders_.size()) return;

  current_folder_ = folders_.at(index).path;
  searching_ = false;
  search_edit_->clear();
  request_page(true);
}

void EMailImapController::slot_search() {
  const auto query = search_edit_->text().trimmed();
  if (query.isEmpty()) {
    searching_ = false;
    request_page(true);
    return;
  }
  if (current_folder_.isEmpty()) return;

  searching_ = true;
  rows_.clear();
  refresh_table();

  const auto index = account_combo_->currentIndex();
  const auto page_size = index >= 0 && index < accounts_.size()
                             ? accounts_.at(index).page_size
                             : kMailDefaultPageSize;

  set_busy(true, Tr("Searching..."));
  QMetaObject::invokeMethod(worker_, "SearchMessages", Qt::QueuedConnection,
                            Q_ARG(quint64, next_seq()),
                            Q_ARG(QString, current_folder_),
                            Q_ARG(QString, query), Q_ARG(int, page_size));
}

void EMailImapController::slot_load_more() { request_page(false); }

void EMailImapController::request_page(bool reset) {
  if (current_folder_.isEmpty() || worker_ == nullptr) return;

  if (reset) {
    rows_.clear();
    cursor_ = 0;
    refresh_table();
  }

  const auto index = account_combo_->currentIndex();
  const auto page_size = index >= 0 && index < accounts_.size()
                             ? accounts_.at(index).page_size
                             : kMailDefaultPageSize;

  set_busy(true, Tr("Loading messages..."));
  QMetaObject::invokeMethod(
      worker_, "ListMessages", Qt::QueuedConnection, Q_ARG(quint64, next_seq()),
      Q_ARG(QString, current_folder_), Q_ARG(quint64, cursor_),
      Q_ARG(int, page_size), Q_ARG(int, static_cast<int>(rows_.size())));
}

void EMailImapController::handle_connected(quint64 seq) {
  if (!is_current(seq)) return;
  set_busy(true, Tr("Loading folders..."));
  QMetaObject::invokeMethod(worker_, "ListFolders", Qt::QueuedConnection,
                            Q_ARG(quint64, next_seq()));
}

void EMailImapController::handle_folders(
    quint64 seq, const QList<EMailFolderInfo>& folders) {
  if (!is_current(seq)) return;
  set_busy(false, {});

  folders_.clear();
  folder_combo_->clear();

  for (const auto& folder : folders) {
    if (!folder.selectable) continue;
    folders_.append(folder);
    folder_combo_->addItem(folder.path);
  }

  if (folders_.isEmpty()) {
    status_label_->setText(
        Tr("This account has no folders that can be opened."));
    refresh_idle_state();
    return;
  }

  refresh_idle_state();

  // Start in the inbox when the server says which one it is.
  int start = 0;
  for (int i = 0; i < folders_.size(); ++i) {
    if (!folders_.at(i).is_inbox) continue;
    start = i;
    break;
  }
  folder_combo_->setCurrentIndex(start);
  slot_folder_changed();
}

void EMailImapController::handle_messages(quint64 seq,
                                          const EMailMessagePage& page) {
  if (!is_current(seq)) return;
  set_busy(false, {});

  rows_ += page.rows;
  if (!page.rows.isEmpty()) cursor_ = page.rows.last().uid;

  refresh_table();

  // The cap is reported rather than hidden: a silently truncated list would
  // leave the user believing they had seen everything there was.
  if (page.capped) {
    status_label_->setText(
        Tr("Showing the first %1 messages. Use the search box to narrow this "
           "down rather than loading more.")
            .arg(rows_.size()));
  } else if (page.remaining > 0) {
    status_label_->setText(Tr("Showing %1 messages; %2 older ones not loaded.")
                               .arg(rows_.size())
                               .arg(page.remaining));
  } else {
    status_label_->setText(Tr("Showing %1 messages.").arg(rows_.size()));
  }

  more_button_->setEnabled(!searching_ && page.remaining > 0 && !page.capped);
}

void EMailImapController::handle_fetched(quint64 seq,
                                         const QByteArray& raw_eml) {
  if (!is_current(seq)) return;
  set_busy(false, {});

  if (raw_eml.isEmpty()) {
    status_label_->setText(Tr("That message came back empty."));
    return;
  }

  emit SignalMessageChosen(raw_eml);
  accept();
}

void EMailImapController::handle_failed(quint64 seq, const MailError& error) {
  if (!is_current(seq)) return;
  set_busy(false, {});
  if (error.category == MailErrorCategory::kCANCELLED) return;
  show_error(error);
}

void EMailImapController::slot_open_selected() {
  const auto row = table_->currentRow();
  if (row < 0 || row >= rows_.size()) return;

  const auto& summary = rows_.at(row);

  // Refused on the size the listing already reported, so an oversized message
  // is never downloaded to find out it was too big.
  if (summary.TooLarge()) {
    QMessageBox::warning(
        this, Tr("Message too large"),
        Tr("That message is %1, which is larger than this application will "
           "open.")
            .arg(QLocale().formattedDataSize(summary.size)));
    return;
  }

  set_busy(true, Tr("Fetching message..."));
  QMetaObject::invokeMethod(
      worker_, "FetchMessage", Qt::QueuedConnection, Q_ARG(quint64, next_seq()),
      Q_ARG(QString, current_folder_), Q_ARG(quint64, summary.uid));
}

void EMailImapController::slot_cancel_busy() {
  if (worker_ != nullptr) worker_->Token()->Cancel();
}

void EMailImapController::refresh_table() {
  table_->setRowCount(static_cast<int>(rows_.size()));

  for (int i = 0; i < rows_.size(); ++i) {
    const auto& row = rows_.at(i);

    const auto date = row.date.isValid()
                          ? QLocale().toString(row.date, QLocale::ShortFormat)
                          : QString();

    // Everything here came from the server and is shown as text only. A
    // subject or folder name is never treated as markup or as a path.
    auto* date_item = new QTableWidgetItem(date);
    auto* from_item = new QTableWidgetItem(row.from);
    auto* subject_item = new QTableWidgetItem(row.subject);
    auto* size_item =
        new QTableWidgetItem(QLocale().formattedDataSize(row.size));
    auto* id_item = new QTableWidgetItem(row.message_id);

    if (row.TooLarge()) {
      size_item->setToolTip(Tr("Too large to open"));
    }

    table_->setItem(i, kDate, date_item);
    table_->setItem(i, kFrom, from_item);
    table_->setItem(i, kSubject, subject_item);
    table_->setItem(i, kSize, size_item);
    table_->setItem(i, kMessageId, id_item);
  }
}

void EMailImapController::set_busy(bool busy, const QString& what) {
  account_combo_->setEnabled(!busy);
  connect_button_->setEnabled(!busy && account_combo_->currentIndex() >= 0);

  // Re-enabled only for a session that actually has folders: leaving a busy
  // state must not leave the browse controls live on a window that never
  // connected to anything.
  const auto connected = !folders_.isEmpty();
  folder_combo_->setEnabled(!busy && connected);
  search_edit_->setEnabled(!busy && connected);
  search_button_->setEnabled(!busy && connected);
  open_button_->setEnabled(!busy && connected);
  more_button_->setEnabled(!busy && connected && more_button_->isEnabled());

  if (busy) {
    status_label_->setText(what);
    cancel_button_->setText(Tr("Stop"));
    disconnect(cancel_button_, &QPushButton::clicked, nullptr, nullptr);
    connect(cancel_button_, &QPushButton::clicked, this,
            &EMailImapController::slot_cancel_busy);
  } else {
    cancel_button_->setText(Tr("Cancel"));
    disconnect(cancel_button_, &QPushButton::clicked, nullptr, nullptr);
    connect(cancel_button_, &QPushButton::clicked, this, &QDialog::reject);
  }
}

void EMailImapController::show_error(const MailError& error) {
  auto text = error.detail;
  if (!error.protocol_detail.isEmpty()) {
    text += (text.isEmpty() ? QString() : "\n\n") + error.protocol_detail;
  }

  status_label_->setText(error.title);
  QMessageBox::warning(this, error.title, text.isEmpty() ? error.title : text);
}
