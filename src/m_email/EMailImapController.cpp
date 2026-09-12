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

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "EMailAccountStore.h"
#include "EMailCredentialStore.h"
#include "EMailViewStyle.h"
#include "GFModuleCommonUtils.hpp"

namespace {

auto Tr(const char* text) -> QString {
  return QCoreApplication::translate("EMailImapController", text);
}

/// Roles carrying the two lines a row draws, so the delegate needs no access
/// to the model behind the list.
constexpr int kSubjectRole = Qt::UserRole + 1;
constexpr int kSubtitleRole = Qt::UserRole + 2;
constexpr int kDimmedRole = Qt::UserRole + 3;

/// Only the subject is shown at full strength; how long a message's own
/// waiting delay may be before a progress bar is worth showing.
constexpr int kProgressDelayMs = 200;

/**
 * @brief Draws a message as a subject with a quieter line beneath it.
 *
 * Two lines rather than five columns: a picker is scanned, not studied, and
 * everything else about a message belongs in the detail pane where there is
 * room to label it. The second line is drawn in the theme's muted colour, so
 * it recedes instead of competing with the subject.
 */
class MessageRowDelegate : public QStyledItemDelegate {
 public:
  explicit MessageRowDelegate(QWidget* owner)
      : QStyledItemDelegate(owner), owner_(owner) {}

  auto sizeHint(const QStyleOptionViewItem& option,
                const QModelIndex& index) const -> QSize override {
    auto size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(option.fontMetrics.height() * 2 + 14);
    return size;
  }

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override {
    auto style_option = option;
    initStyleOption(&style_option, index);
    style_option.text.clear();

    auto* style = style_option.widget != nullptr ? style_option.widget->style()
                                                 : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &style_option, painter,
                       style_option.widget);

    const auto selected = (option.state & QStyle::State_Selected) != 0;
    const auto dimmed = index.data(kDimmedRole).toBool();

    auto primary = selected ? option.palette.color(QPalette::HighlightedText)
                            : option.palette.color(QPalette::Text);
    auto secondary = selected ? primary : EMailMutedColor(owner_);
    if (dimmed) {
      // Provisional rows from the cache are drawn faintly rather than
      // labelled, so nothing on screen claims to be fresher than it is.
      primary.setAlpha(150);
      secondary.setAlpha(120);
    }

    auto rect = option.rect.adjusted(8, 4, -8, -4);
    const auto line = option.fontMetrics.height();

    painter->save();
    painter->setPen(primary);
    painter->drawText(
        QRect(rect.left(), rect.top(), rect.width(), line),
        Qt::AlignLeft | Qt::AlignVCenter,
        option.fontMetrics.elidedText(index.data(kSubjectRole).toString(),
                                      Qt::ElideRight, rect.width()));

    auto small = option.font;
    small.setPointSizeF(small.pointSizeF() * 0.9);
    painter->setFont(small);
    painter->setPen(secondary);

    const QFontMetrics small_metrics(small);
    painter->drawText(
        QRect(rect.left(), rect.top() + line + 2, rect.width(), line),
        Qt::AlignLeft | Qt::AlignVCenter,
        small_metrics.elidedText(index.data(kSubtitleRole).toString(),
                                 Qt::ElideRight, rect.width()));
    painter->restore();
  }

 private:
  QWidget* owner_;
};

}  // namespace

EMailImapController::EMailImapController(QWidget* parent) : QDialog(parent) {
  setWindowTitle(Tr("IMAP Controller"));
  resize(960, 600);
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

  refresh_idle_state();

  // Opening this window IS the request to browse, so it connects rather than
  // waiting to be told again. Nothing is prompted for: the password comes from
  // Settings or the account simply cannot be opened. Note that this makes a
  // menu entry reach the network, which the rest of the application avoids --
  // it is justified here only because browsing a remote mailbox is the entire
  // purpose of the window.
  if (accounts_.isEmpty()) {
    status_label_->setText(
        Tr("No mail account with IMAP enabled is configured yet."));
  } else {
    connect_to_selected_account();
  }
}

EMailImapController::~EMailImapController() { stop_worker(); }

auto EMailImapController::HasUsableAccount() -> bool {
  return !EMailAccountStore::ImapAccounts().isEmpty();
}

void EMailImapController::build_ui() {
  auto* outer = new QVBoxLayout(this);
  outer->setSpacing(8);

  // --- header: what is being browsed -------------------------------------
  auto* top = new QHBoxLayout;
  account_combo_ = new QComboBox(this);
  account_combo_->setMinimumWidth(220);
  folder_combo_ = new QComboBox(this);
  folder_combo_->setMinimumWidth(160);

  search_edit_ = new QLineEdit(this);
  search_edit_->setPlaceholderText(Tr("Search subject or sender"));
  search_edit_->setClearButtonEnabled(true);
  search_edit_->addAction(QIcon(":/icons/search.png"),
                          QLineEdit::LeadingPosition);

  refresh_button_ = new QToolButton(this);
  refresh_button_->setIcon(
      QIcon::fromTheme("view-refresh", QIcon(":/icons/refresh.png")));
  refresh_button_->setAutoRaise(true);
  refresh_button_->setToolTip(Tr("Reload this folder from the server"));

  top->addWidget(new QLabel(Tr("Account"), this));
  top->addWidget(account_combo_, 1);
  top->addWidget(new QLabel(Tr("Folder"), this));
  top->addWidget(folder_combo_, 1);
  top->addWidget(search_edit_, 2);
  top->addWidget(refresh_button_);
  outer->addLayout(top);

  // Indeterminate, because none of these operations can report a fraction:
  // IMAP says when it is done, not how far along it is. Shown only after a
  // short delay so a fast folder never flashes a progress bar.
  progress_ = new QProgressBar(this);
  progress_->setRange(0, 0);
  progress_->setTextVisible(false);
  progress_->setFixedHeight(4);
  progress_->setVisible(false);
  outer->addWidget(progress_);

  progress_timer_ = new QTimer(this);
  progress_timer_->setSingleShot(true);
  progress_timer_->setInterval(kProgressDelayMs);
  connect(progress_timer_, &QTimer::timeout, this,
          [this]() { progress_->setVisible(true); });

  // --- the list, and what one row means ----------------------------------
  splitter_ = new QSplitter(Qt::Horizontal, this);
  splitter_->setChildrenCollapsible(false);

  list_ = new QListWidget(splitter_);
  list_->setItemDelegate(new MessageRowDelegate(list_));
  list_->setSelectionMode(QAbstractItemView::SingleSelection);
  list_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  list_->setAlternatingRowColors(true);
  list_->setUniformItemSizes(true);
  list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  list_->setMinimumWidth(260);
  splitter_->addWidget(list_);

  splitter_->addWidget(build_detail_pane());
  splitter_->setStretchFactor(0, 3);
  splitter_->setStretchFactor(1, 2);
  outer->addWidget(splitter_, 1);

  // --- status and actions -------------------------------------------------
  status_label_ = new QLabel(this);
  status_label_->setWordWrap(true);

  auto* buttons = new QHBoxLayout;
  more_button_ = new QPushButton(Tr("Load more"), this);
  open_button_ = new QPushButton(Tr("Open"), this);
  open_button_->setDefault(true);
  cancel_button_ = new QPushButton(Tr("Close"), this);

  buttons->addWidget(more_button_);
  buttons->addWidget(status_label_, 1);
  buttons->addWidget(open_button_);
  buttons->addWidget(cancel_button_);
  outer->addLayout(buttons);

  connect(account_combo_, &QComboBox::currentIndexChanged, this,
          &EMailImapController::slot_account_changed);
  connect(folder_combo_, &QComboBox::currentIndexChanged, this,
          &EMailImapController::slot_folder_changed);
  connect(search_edit_, &QLineEdit::returnPressed, this,
          &EMailImapController::slot_search);
  connect(refresh_button_, &QToolButton::clicked, this,
          &EMailImapController::slot_refresh);
  connect(more_button_, &QPushButton::clicked, this,
          &EMailImapController::slot_load_more);
  connect(list_, &QListWidget::itemSelectionChanged, this,
          &EMailImapController::slot_selection_changed);
  connect(list_, &QListWidget::itemDoubleClicked, this,
          &EMailImapController::slot_open_selected);
  connect(open_button_, &QPushButton::clicked, this,
          &EMailImapController::slot_open_selected);
  connect(cancel_button_, &QPushButton::clicked, this, &QDialog::reject);
}

/**
 * @brief The right-hand pane: everything about one message, and no body.
 *
 * Selecting a message must stay free. None of this costs a round trip -- it
 * is the envelope the listing already carried -- which is what lets a user
 * look through a mailbox without touching a single message's \Seen flag.
 */
auto EMailImapController::build_detail_pane() -> QWidget* {
  detail_stack_ = new QStackedWidget(splitter_);

  detail_placeholder_ = new QWidget(detail_stack_);
  auto* placeholder_layout = new QVBoxLayout(detail_placeholder_);
  placeholder_layout->addStretch();
  auto* placeholder_label = new QLabel(
      Tr("Select a message to see its details."), detail_placeholder_);
  placeholder_label->setAlignment(Qt::AlignCenter);
  placeholder_label->setWordWrap(true);
  {
    auto muted = placeholder_label->palette();
    muted.setColor(QPalette::WindowText, EMailMutedColor(placeholder_label));
    placeholder_label->setPalette(muted);
  }
  placeholder_layout->addWidget(placeholder_label);
  placeholder_layout->addStretch();
  detail_stack_->addWidget(detail_placeholder_);

  detail_page_ = new QWidget(detail_stack_);
  auto* form = new QFormLayout(detail_page_);
  form->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
  form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

  // Everything here is text the SERVER chose. It is rendered as plain,
  // selectable text and never as markup, and a folder name is never used as
  // a path.
  const auto make_field = [this, form](const QString& label) {
    auto* value = new QLabel(detail_page_);
    value->setWordWrap(true);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                   Qt::TextSelectableByKeyboard);
    value->setTextFormat(Qt::PlainText);
    form->addRow(label, value);
    return value;
  };

  detail_subject_ = make_field(Tr("Subject"));
  {
    auto bold = detail_subject_->font();
    bold.setBold(true);
    detail_subject_->setFont(bold);
  }
  detail_from_ = make_field(Tr("From"));
  detail_date_ = make_field(Tr("Date"));
  detail_size_ = make_field(Tr("Size"));
  detail_id_ = make_field(Tr("Message-ID"));
  detail_folder_ = make_field(Tr("Folder"));

  detail_note_ = new QLabel(detail_page_);
  detail_note_->setWordWrap(true);
  detail_note_->setVisible(false);
  form->addRow(QString(), detail_note_);

  auto* scroll = new QScrollArea(detail_stack_);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidget(detail_page_);
  detail_stack_->addWidget(scroll);

  detail_stack_->setCurrentWidget(detail_placeholder_);
  return detail_stack_;
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

  current_account_id_ = account.id;

  // Show whatever this account looked like last time before the network is
  // touched, so switching back is instant. Those rows are provisional and are
  // drawn as such until the refresh below replaces them.
  if (!restore_cached_account(account.id)) {
    folders_.clear();
    folder_combo_->clear();
    rows_.clear();
    refresh_table();
  }

  set_busy(true, Tr("Connecting..."));
  const auto seq = next_seq();
  QMetaObject::invokeMethod(
      worker_, "Connect", Qt::QueuedConnection, Q_ARG(quint64, seq),
      Q_ARG(MailAccountConfig, account), Q_ARG(QString, password));
  password.fill(QChar('\0'));
}

void EMailImapController::slot_account_changed() {
  if (closing_) return;

  // Choosing an account is a request to browse it. What is on screen belongs
  // to the previous one, so it is put away first -- then the new account is
  // connected and refreshed without further ceremony.
  remember_current_account();

  current_folder_.clear();
  cursor_ = 0;
  more_available_ = false;
  searching_ = false;
  search_edit_->clear();

  if (worker_ != nullptr) {
    QMetaObject::invokeMethod(worker_, "Disconnect", Qt::QueuedConnection);
  }

  connect_to_selected_account();
}

void EMailImapController::slot_refresh() {
  if (current_folder_.isEmpty()) {
    connect_to_selected_account();
    return;
  }
  searching_ = false;
  search_edit_->clear();
  request_page(true);
}

void EMailImapController::remember_current_account() {
  if (current_account_id_.isEmpty()) return;

  // Only confirmed rows are worth keeping. Caching a provisional view would
  // let a stale list outlive the one refresh that was going to correct it.
  if (showing_cached_) return;

  AccountViewState state;
  state.folders = folders_;
  state.rows = rows_;
  state.folder = current_folder_;
  state.search = search_edit_->text();
  state.cursor = cursor_;
  state.more_available = more_available_;
  cache_.insert(current_account_id_, state);
}

auto EMailImapController::restore_cached_account(const QString& account_id)
    -> bool {
  const auto it = cache_.constFind(account_id);
  if (it == cache_.constEnd() || it->folders.isEmpty()) return false;

  folders_ = it->folders;
  rows_ = it->rows;
  current_folder_ = it->folder;
  cursor_ = it->cursor;
  more_available_ = it->more_available;
  showing_cached_ = true;

  folder_combo_->blockSignals(true);
  folder_combo_->clear();
  for (const auto& folder : folders_) folder_combo_->addItem(folder.path);
  for (int i = 0; i < folders_.size(); ++i) {
    if (folders_.at(i).path != current_folder_) continue;
    folder_combo_->setCurrentIndex(i);
    break;
  }
  folder_combo_->blockSignals(false);

  refresh_table();
  return true;
}

void EMailImapController::refresh_idle_state() {
  const auto has_account = account_combo_->currentIndex() >= 0;
  const auto connected = !folders_.isEmpty();

  account_combo_->setEnabled(has_account);
  folder_combo_->setEnabled(connected);
  search_edit_->setEnabled(connected);
  refresh_button_->setEnabled(has_account);
  more_button_->setEnabled(more_available_);

  // Open follows the SELECTION, not the connection: an enabled button that
  // does nothing when pressed is worse than a disabled one.
  open_button_->setEnabled(current_summary() != nullptr);
}

/// The message the user has selected, or nullptr when none is or it cannot be
/// opened.
auto EMailImapController::current_summary() const
    -> const EMailMessageSummary* {
  const auto row = list_->currentRow();
  if (row < 0 || row >= rows_.size()) return nullptr;
  if (rows_.at(row).TooLarge()) return nullptr;
  return &rows_.at(row);
}

void EMailImapController::slot_selection_changed() {
  refresh_detail();
  refresh_idle_state();
}

void EMailImapController::refresh_detail() {
  const auto row = list_->currentRow();
  if (row < 0 || row >= rows_.size()) {
    detail_stack_->setCurrentWidget(detail_placeholder_);
    return;
  }

  const auto& summary = rows_.at(row);
  detail_subject_->setText(summary.subject.isEmpty() ? Tr("(no subject)")
                                                     : summary.subject);
  detail_from_->setText(summary.from);
  detail_date_->setText(
      summary.date.isValid()
          ? QLocale().toString(summary.date, QLocale::LongFormat)
          : Tr("Not stated"));
  detail_size_->setText(QLocale().formattedDataSize(summary.size));
  detail_id_->setText(summary.message_id.isEmpty() ? Tr("None")
                                                   : summary.message_id);
  detail_folder_->setText(current_folder_);

  // Said here, where there is room to say why, rather than hidden in a
  // tooltip on a column that no longer exists.
  if (summary.TooLarge()) {
    detail_note_->setText(
        Tr("This message is larger than this application will open, so it "
           "cannot be imported."));
    detail_note_->setStyleSheet(
        QString("color: %1;").arg(EMailWarningColor(detail_note_).name()));
    detail_note_->setVisible(true);
  } else if (showing_cached_) {
    detail_note_->setText(
        Tr("Shown from the previous visit to this account; refreshing."));
    detail_note_->setStyleSheet(
        QString("color: %1;").arg(EMailMutedColor(detail_note_).name()));
    detail_note_->setVisible(true);
  } else {
    detail_note_->setVisible(false);
  }

  detail_stack_->setCurrentIndex(1);
}

void EMailImapController::set_progress_visible(bool visible) {
  if (visible) {
    progress_timer_->start();
    return;
  }
  progress_timer_->stop();
  progress_->setVisible(false);
}

void EMailImapController::slot_folder_changed() {
  const auto index = folder_combo_->currentIndex();
  if (index < 0 || index >= folders_.size()) return;

  current_folder_ = folders_.at(index).path;
  searching_ = false;
  search_edit_->clear();
  showing_cached_ = false;
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

  set_busy(true, Tr("Searching..."));
  QMetaObject::invokeMethod(
      worker_, "SearchMessages", Qt::QueuedConnection,
      Q_ARG(quint64, next_seq()), Q_ARG(QString, current_folder_),
      Q_ARG(QString, query), Q_ARG(int, kMailDefaultPageSize));
}

void EMailImapController::slot_load_more() { request_page(false); }

void EMailImapController::request_page(bool reset) {
  if (current_folder_.isEmpty() || worker_ == nullptr) return;

  if (reset) {
    // The cached rows stay on screen until the fresh page lands, so the list
    // does not blink empty on every switch; handle_messages() clears them.
    if (!showing_cached_) rows_.clear();
    cursor_ = 0;
    more_available_ = false;
    refresh_table();
  }

  set_busy(true, Tr("Loading messages..."));
  QMetaObject::invokeMethod(
      worker_, "ListMessages", Qt::QueuedConnection, Q_ARG(quint64, next_seq()),
      Q_ARG(QString, current_folder_), Q_ARG(quint64, cursor_),
      Q_ARG(int, kMailDefaultPageSize),
      Q_ARG(int, static_cast<int>(rows_.size())));
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

  const auto previous_folder = current_folder_;

  folders_.clear();
  folder_combo_->blockSignals(true);
  folder_combo_->clear();

  for (const auto& folder : folders) {
    if (!folder.selectable) continue;
    folders_.append(folder);
    folder_combo_->addItem(folder.path);
  }
  folder_combo_->blockSignals(false);

  if (folders_.isEmpty()) {
    status_label_->setText(
        Tr("This account has no folders that can be opened."));
    refresh_idle_state();
    return;
  }

  refresh_idle_state();

  // Return to the folder that was being read before, so a refresh or a switch
  // back to this account lands where the user left off; otherwise start in
  // the inbox when the server says which one it is.
  int start = 0;
  for (int i = 0; i < folders_.size(); ++i) {
    if (folders_.at(i).path != previous_folder) continue;
    start = i;
    break;
  }
  if (previous_folder.isEmpty()) {
    for (int i = 0; i < folders_.size(); ++i) {
      if (!folders_.at(i).is_inbox) continue;
      start = i;
      break;
    }
  }

  folder_combo_->blockSignals(true);
  folder_combo_->setCurrentIndex(start);
  folder_combo_->blockSignals(false);

  current_folder_ = folders_.at(start).path;
  request_page(true);
}

void EMailImapController::handle_messages(quint64 seq,
                                          const EMailMessagePage& page) {
  if (!is_current(seq)) return;
  set_busy(false, {});

  // The first confirmed page replaces whatever the cache was showing.
  if (showing_cached_) {
    rows_.clear();
    showing_cached_ = false;
  }

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

  more_available_ = !searching_ && page.remaining > 0 && !page.capped;
  more_button_->setEnabled(more_available_);
  refresh_idle_state();
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
  // Refused on the size the listing already reported, so an oversized message
  // is never downloaded to find out it was too big. current_summary() returns
  // nullptr for one, and the detail pane has already said why.
  const auto* summary = current_summary();
  if (summary == nullptr) return;

  set_busy(true, Tr("Fetching message..."));
  QMetaObject::invokeMethod(
      worker_, "FetchMessage", Qt::QueuedConnection, Q_ARG(quint64, next_seq()),
      Q_ARG(QString, current_folder_), Q_ARG(quint64, summary->uid));
}

void EMailImapController::slot_cancel_busy() {
  if (worker_ != nullptr) worker_->Token()->Cancel();
}

void EMailImapController::refresh_table() {
  const auto previous = list_->currentRow();

  list_->clear();
  for (const auto& row : rows_) {
    // Server-chosen text, carried as data and drawn as plain text by the
    // delegate. It is never interpreted as markup.
    const auto date = row.date.isValid()
                          ? QLocale().toString(row.date, QLocale::ShortFormat)
                          : QString();
    const auto sender = row.from.isEmpty() ? Tr("Unknown sender") : row.from;
    const auto subtitle =
        date.isEmpty() ? sender : QString("%1  ·  %2").arg(sender, date);

    auto* item = new QListWidgetItem(list_);
    item->setData(kSubjectRole,
                  row.subject.isEmpty() ? Tr("(no subject)") : row.subject);
    item->setData(kSubtitleRole, subtitle);
    item->setData(kDimmedRole, showing_cached_);
    list_->addItem(item);
  }

  if (previous >= 0 && previous < list_->count()) {
    list_->setCurrentRow(previous);
  }
  refresh_detail();
}

void EMailImapController::set_busy(bool busy, const QString& what) {
  set_progress_visible(busy);

  account_combo_->setEnabled(!busy);
  refresh_button_->setEnabled(!busy && account_combo_->currentIndex() >= 0);

  // Re-enabled only for a session that actually has folders: leaving a busy
  // state must not leave the browse controls live on a window that never
  // connected to anything.
  const auto connected = !folders_.isEmpty();
  folder_combo_->setEnabled(!busy && connected);
  search_edit_->setEnabled(!busy && connected);
  open_button_->setEnabled(!busy && current_summary() != nullptr);

  // From remembered state, never from the button itself: reading the widget
  // back could only ever clear this flag, so "Load more" once lost during a
  // busy period never returned.
  more_button_->setEnabled(!busy && more_available_);

  if (busy) {
    status_label_->setText(what);
    cancel_button_->setText(Tr("Stop"));
    disconnect(cancel_button_, &QPushButton::clicked, nullptr, nullptr);
    connect(cancel_button_, &QPushButton::clicked, this,
            &EMailImapController::slot_cancel_busy);
  } else {
    // The caller's word for what just finished, so the last busy message does
    // not stay on screen pretending the work is still running.
    status_label_->setText(what);
    cancel_button_->setText(Tr("Close"));
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
  // A failed connection must still leave the window usable: Refresh retries,
  // and another account can be chosen.
  refresh_idle_state();
  QMessageBox::warning(this, error.title, text.isEmpty() ? error.title : text);
}
