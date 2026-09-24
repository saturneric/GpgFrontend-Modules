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
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "EMailAccountStore.h"
#include "EMailCredentialStore.h"
#include "EMailViewStyle.h"
#include "GFModule.h"

namespace {

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
  setWindowTitle(tr("IMAP Controller"));
  resize(960, 600);
  build_ui();

  accounts_ = EMailAccountStore::ImapAccounts();
  account_combo_->blockSignals(true);
  for (const auto& account : accounts_) {
    account_combo_->addItem(account.Label().isEmpty() ? account.imap.host
                                                      : account.Label());
  }
  account_combo_->blockSignals(false);

  refresh_account_availability();

  // Where the user was last time, in preference to the configured default:
  // reopening the picker is almost always a return to what was being read, and
  // the default account is a statement about SENDING. A remembered account
  // that has since been removed or become unusable simply does not match, and
  // the default is used as before.
  const auto preferred = EMailAccountStore::DefaultAccount();
  const auto& remembered = last_account();
  int start = -1;
  for (int i = 0; i < accounts_.size(); ++i) {
    if (unusable_.contains(accounts_.at(i).id)) continue;
    if (start < 0) start = i;
    if (!remembered.isEmpty() && accounts_.at(i).id == remembered) {
      start = i;
      break;
    }
    if (remembered.isEmpty() && accounts_.at(i).id == preferred.id) {
      start = i;
      break;
    }
  }
  if (start >= 0) {
    account_combo_->blockSignals(true);
    account_combo_->setCurrentIndex(start);
    account_combo_->blockSignals(false);
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
        tr("No mail account with IMAP enabled is configured yet."));
  } else if (account_combo_->currentIndex() < 0 ||
             unusable_.contains(
                 accounts_.at(account_combo_->currentIndex()).id)) {
    // Every account is unusable, so there is nothing to reach out to. Saying
    // why beats connecting in order to fail.
    status_label_->setText(
        tr("No configured account can be opened. Check Settings, under Mail "
           "Accounts."));
  } else {
    connect_to_selected_account();
  }
}

/**
 * @brief Greys out the accounts this window cannot browse, and says why.
 *
 * An account with no stored password cannot be opened at all, and offering it
 * as a choice only leads to a dialog explaining that after the fact. This asks
 * the question up front. Nothing here reaches the network: it reads local
 * configuration and the credential store, so an account is judged unusable
 * only when it is unusable for a reason we already know.
 */
void EMailImapController::refresh_account_availability() {
  auto* model = qobject_cast<QStandardItemModel*>(account_combo_->model());

  for (int i = 0; i < accounts_.size(); ++i) {
    const auto& account = accounts_.at(i);

    QString reason;
    if (!account.imap.enabled) {
      reason = tr("IMAP is turned off for this account");
    } else if (account.imap.host.isEmpty()) {
      reason = tr("no server is configured");
    } else if (account.imap.username.isEmpty()) {
      reason = tr("no username is configured");
    } else if (!EMailCredentialStore::Has(account.id)) {
      // Asked whether one exists, not for its value. Loading it decrypts a
      // secret this function has no use for, once per account, every time
      // anything refreshes the list -- including every failure.
      reason = tr("no password is stored");
    }

    // A reason recorded earlier by a real failure outranks these, since it
    // reflects what actually happened rather than what is merely missing.
    if (unusable_.contains(account.id)) {
      reason = unusable_.value(account.id);
    } else if (!reason.isEmpty()) {
      unusable_.insert(account.id, reason);
    }

    const auto usable = reason.isEmpty();
    const auto label =
        account.Label().isEmpty() ? account.imap.host : account.Label();

    account_combo_->setItemText(
        i, usable ? label : QString("%1: %2").arg(label, reason));

    // A configuration problem cannot be retried from here and the entry is
    // closed off. A recorded FAILURE can: the password may have been set in
    // Settings since, or the network may have come back. Leaving those
    // selectable is what makes Refresh a way back -- previously nothing ever
    // cleared a recorded failure, so one bad attempt disabled the account for
    // the whole session.
    const auto retryable = !usable && failed_.contains(account.id);

    if (model != nullptr && model->item(i) != nullptr) {
      model->item(i)->setEnabled(usable || retryable);
    }
    account_combo_->setItemData(
        i,
        usable      ? QString()
        : retryable ? tr("This account could not be opened: %1.\n\nUse "
                         "Refresh to try again.")
                          .arg(reason)
                    : tr("This account cannot be opened: %1.").arg(reason),
        Qt::ToolTipRole);
  }
}

void EMailImapController::disable_account(const QString& account_id,
                                          const QString& reason) {
  if (account_id.isEmpty()) return;
  unusable_.insert(account_id, reason);

  // Recorded as an attempt that failed, as opposed to a configuration that is
  // incomplete. Only the first kind is worth offering to retry.
  failed_.insert(account_id);

  refresh_account_availability();
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
  search_edit_->setPlaceholderText(tr("Search subject or sender"));
  search_edit_->setClearButtonEnabled(true);
  search_edit_->addAction(QIcon(":/icons/search.png"),
                          QLineEdit::LeadingPosition);

  refresh_button_ = new QToolButton(this);
  refresh_button_->setIcon(
      QIcon::fromTheme("view-refresh", QIcon(":/icons/refresh.png")));
  refresh_button_->setAutoRaise(true);
  refresh_button_->setToolTip(tr("Reload this folder from the server"));

  auto* account_label = new QLabel(tr("&Account"), this);
  top->addWidget(account_label);
  top->addWidget(account_combo_, 1);
  auto* folder_label = new QLabel(tr("F&older"), this);
  top->addWidget(folder_label);
  top->addWidget(folder_combo_, 1);
  top->addWidget(search_edit_, 2);
  top->addWidget(refresh_button_);
  outer->addLayout(top);

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
  // The pane around it is already a bordered surface; a frame here draws the
  // same edge twice a few pixels apart.
  list_->setFrameShape(QFrame::NoFrame);

  // An empty folder used to be a blank box whose only explanation sat in the
  // button row at the bottom of the window. The sentence takes the list's
  // place instead, which is where someone looking at the emptiness is looking.
  auto* list_side = new QWidget(splitter_);
  auto* list_layout = new QVBoxLayout(list_side);
  list_layout->setContentsMargins(0, 0, 0, 0);
  list_layout->setSpacing(0);
  list_layout->addWidget(list_, 1);

  empty_notice_ = EMailEmptyNotice(list_side);
  empty_notice_->setVisible(false);
  list_layout->addWidget(empty_notice_, 1);

  // Work in progress, in the place the work will appear. Without this the
  // empty notice held the pane while a folder was still loading, so every slow
  // folder announced itself as empty -- a statement about the folder, made
  // before anything had been fetched, and usually a false one.
  loading_pane_ = new QWidget(list_side);
  auto* loading_layout = new QVBoxLayout(loading_pane_);
  loading_layout->setContentsMargins(24, 24, 24, 24);
  loading_layout->setSpacing(12);
  loading_layout->addStretch(1);

  loading_label_ = EMailEmptyNotice(loading_pane_);
  loading_layout->addWidget(loading_label_);

  // Sized to the sentence above it rather than stretched across the pane: a
  // full-width bar in an otherwise empty panel reads as a piece of furniture,
  // not as something in progress.
  auto* loading_bar = new QProgressBar(loading_pane_);
  loading_bar->setRange(0, 0);  // indeterminate: no total is known here
  loading_bar->setTextVisible(false);
  loading_bar->setFixedSize(140, 6);
  loading_layout->addWidget(loading_bar, 0, Qt::AlignHCenter);

  loading_layout->addStretch(1);
  loading_pane_->setVisible(false);
  list_layout->addWidget(loading_pane_, 1);

  splitter_->addWidget(list_side);

  auto* detail_pane = build_detail_pane();
  // Wide enough for a sender address and a useful amount of a Message-ID. The
  // listing can give the space up: its two lines elide gracefully, and this
  // pane's content does not.
  detail_pane->setMinimumWidth(300);
  splitter_->addWidget(detail_pane);
  splitter_->setStretchFactor(0, 3);
  splitter_->setStretchFactor(1, 2);
  outer->addWidget(splitter_, 1);

  // --- status and actions -------------------------------------------------
  status_label_ = new QLabel(this);
  status_label_->setWordWrap(true);

  auto* buttons = new QHBoxLayout;

  // Pages rather than an ever-growing list. IMAP can only page backwards from
  // a UID, so this is a real position in the mailbox rather than a window onto
  // an accumulating buffer -- and it keeps the amount held in memory fixed
  // however deep the user goes.
  previous_button_ = new QPushButton(tr("Previous"), this);
  next_button_ = new QPushButton(tr("Next"), this);
  page_label_ = new QLabel(this);

  open_button_ = new QPushButton(tr("Open"), this);
  open_button_->setDefault(true);
  cancel_button_ = new QPushButton(tr("Close"), this);

  // Indeterminate, because none of these operations can report a fraction:
  // IMAP says when it is done, not how far along it is. Shown only after a
  // short delay so a fast folder never flashes a progress bar.
  //
  // It sits in the action row rather than as a hairline under the header: up
  // there it was four pixels at the far end of the window from the buttons
  // being waited on, which is not where someone who just clicked is looking.
  progress_ = new QProgressBar(this);
  progress_->setRange(0, 0);
  progress_->setTextVisible(false);
  progress_->setFixedHeight(18);
  progress_->setFixedWidth(120);
  progress_->setVisible(false);

  buttons->addWidget(previous_button_);
  buttons->addWidget(next_button_);
  buttons->addWidget(page_label_);
  buttons->addWidget(status_label_, 1);
  buttons->addWidget(progress_);
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
  connect(previous_button_, &QPushButton::clicked, this,
          &EMailImapController::slot_previous_page);
  connect(next_button_, &QPushButton::clicked, this,
          &EMailImapController::slot_next_page);
  connect(list_, &QListWidget::itemSelectionChanged, this,
          &EMailImapController::slot_selection_changed);
  connect(list_, &QListWidget::itemDoubleClicked, this,
          &EMailImapController::slot_open_selected);
  connect(open_button_, &QPushButton::clicked, this,
          &EMailImapController::slot_open_selected);
  connect(cancel_button_, &QPushButton::clicked, this, &QDialog::reject);

  // Everything a list of messages is expected to answer to. There was not one
  // shortcut or context menu in this window before.
  list_->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(list_, &QListWidget::customContextMenuRequested, this,
          &EMailImapController::slot_message_menu);

  auto* refresh_key = new QShortcut(QKeySequence::Refresh, this);
  refresh_key->setContext(Qt::WidgetWithChildrenShortcut);
  connect(refresh_key, &QShortcut::activated, this,
          &EMailImapController::slot_refresh);

  auto* find_key = new QShortcut(QKeySequence::Find, this);
  find_key->setContext(Qt::WidgetWithChildrenShortcut);
  connect(find_key, &QShortcut::activated, this, [this]() {
    search_edit_->setFocus(Qt::ShortcutFocusReason);
    search_edit_->selectAll();
  });

  // Named for the control they operate, so the underlined letter matches what
  // the user is looking at.
  account_label->setBuddy(account_combo_);
  folder_label->setBuddy(folder_combo_);

  apply_colors();
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

  auto* placeholder_icon = new QLabel(detail_placeholder_);
  placeholder_icon->setAlignment(Qt::AlignCenter);
  placeholder_icon_ = placeholder_icon;
  paint_placeholder_icon();
  placeholder_layout->addWidget(placeholder_icon);

  placeholder_label_ = new QLabel(tr("Select a message to see its details."),
                                  detail_placeholder_);
  placeholder_label_->setAlignment(Qt::AlignCenter);
  placeholder_label_->setWordWrap(true);
  EMailMakeMuted(placeholder_label_);
  placeholder_layout->addWidget(placeholder_label_);
  placeholder_layout->addStretch();
  detail_stack_->addWidget(detail_placeholder_);

  detail_page_ = new QWidget(detail_stack_);
  auto* page_layout = new QVBoxLayout(detail_page_);
  page_layout->setContentsMargins(6, 6, 6, 6);

  auto* card = EMailCard(detail_page_);
  auto* card_layout = qobject_cast<QVBoxLayout*>(card->layout());

  // Everything here is text the SERVER chose. It is rendered as plain,
  // selectable text and never as markup, and a folder name is never used as
  // a path.
  const auto make_value = [card]() {
    auto* value = new QLabel(card);
    value->setWordWrap(true);
    value->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                   Qt::TextSelectableByKeyboard);
    value->setTextFormat(Qt::PlainText);
    return value;
  };

  // --- what the message IS ------------------------------------------------
  //
  // A subject that reads no louder than the folder name makes the pane a list
  // of six equal facts. It is the one thing a person recognises a message by,
  // so it is the one thing set apart.
  detail_subject_ = make_value();
  EMailMakeTitle(detail_subject_);
  card_layout->addWidget(detail_subject_);

  detail_from_ = make_value();
  card_layout->addWidget(detail_from_);

  detail_stamp_ = make_value();
  EMailMakeSecondary(detail_stamp_);
  card_layout->addWidget(detail_stamp_);

  // One pixel, like every other divider in this module. Hand-rolled, this was
  // a two-pixel bevel.
  detail_rule_ = EMailRule(card);
  card_layout->addWidget(detail_rule_);

  // --- handles, for anyone who needs them ---------------------------------
  //
  // Caption ABOVE the value rather than beside it. A label column costs the
  // same ~90px whatever the pane is doing with it, and this pane is the narrow
  // half of a splitter: side by side, a Message-ID had barely a dozen
  // characters left to show. Stacked, the value gets the whole width.
  auto* props = new QVBoxLayout;
  props->setContentsMargins(0, 0, 0, 0);
  props->setSpacing(2);

  const auto add_row = [card, props](const QString& name, QWidget* value) {
    auto* caption = new QLabel(name, card);
    EMailMakeSecondary(caption);
    props->addSpacing(4);
    props->addWidget(caption);
    props->addWidget(value);
  };

  // Elided rather than wrapped, because a Message-ID has no words to break
  // at: wrapping one produces three ragged lines of noise. The whole value
  // stays in the tooltip and on the clipboard, which is where it is useful.
  auto* id_holder = new QWidget(card);
  auto* id_row = new QHBoxLayout(id_holder);
  id_row->setContentsMargins(0, 0, 0, 0);
  id_row->setSpacing(4);

  detail_id_ = make_value();
  detail_id_->setWordWrap(false);
  detail_id_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  detail_id_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  // Dragging the splitter changes how much of the identifier fits, so the
  // elision has to follow the label rather than being decided once.
  detail_id_->installEventFilter(this);
  id_row->addWidget(detail_id_, 1);

  detail_copy_id_ = new QToolButton(id_holder);
  detail_copy_id_->setIcon(
      QIcon::fromTheme("edit-copy", QIcon(":/icons/copy.png")));
  detail_copy_id_->setAutoRaise(true);
  detail_copy_id_->setToolTip(tr("Copy the whole Message-ID"));
  detail_copy_id_->setVisible(false);
  id_row->addWidget(detail_copy_id_);
  add_row(tr("Message-ID"), id_holder);

  detail_folder_ = make_value();
  add_row(tr("Folder"), detail_folder_);
  card_layout->addLayout(props);

  // Said here, where there is room to say why, rather than hidden in a
  // tooltip on a column that no longer exists. A tinted strip rather than a
  // coloured label: the same convention the message workspace uses, and it
  // costs the module its last two stylesheets.
  detail_note_frame_ = EMailTintedBanner(card, EMailMutedColor(card));
  auto* note_row = new QHBoxLayout(detail_note_frame_);
  note_row->setContentsMargins(8, 6, 8, 6);
  detail_note_ = new QLabel(detail_note_frame_);
  detail_note_->setWordWrap(true);
  detail_note_->setTextFormat(Qt::PlainText);
  EMailMakeSecondary(detail_note_);
  note_row->addWidget(detail_note_, 1);
  detail_note_frame_->setVisible(false);
  card_layout->addWidget(detail_note_frame_);

  page_layout->addWidget(card);
  page_layout->addStretch();

  connect(detail_copy_id_, &QToolButton::clicked, this,
          [this]() { QApplication::clipboard()->setText(detail_id_full_); });

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
  connect(worker_, &EMailImapWorker::SignalFolderStatus, this,
          &EMailImapController::handle_folder_status);
  connect(worker_, &EMailImapWorker::SignalMessages, this,
          &EMailImapController::handle_messages);
  connect(worker_, &EMailImapWorker::SignalMessageFetched, this,
          &EMailImapController::handle_fetched);
  connect(worker_, &EMailImapWorker::SignalFetchProgress, this,
          &EMailImapController::handle_fetch_progress);
  connect(worker_, &EMailImapWorker::SignalFailed, this,
          &EMailImapController::handle_failed);

  thread_->start();
}

void EMailImapController::stop_worker() {
  if (thread_ == nullptr) return;

  if (worker_ != nullptr) {
    // Unblocks whatever socket call the worker is sitting inside; without it
    // quit() would wait on an event loop that is not currently running.
    // Everything: the dialog is closing, so queued requests are unwanted too.
    worker_->Token()->CancelAll();
    QMetaObject::invokeMethod(worker_, "Disconnect", Qt::QueuedConnection);
  }

  StopMailWorkerThread(thread_, "IMAP worker thread");
  thread_ = nullptr;
  worker_ = nullptr;
}

void EMailImapController::closeEvent(QCloseEvent* event) {
  closing_ = true;
  if (worker_ != nullptr) worker_->Token()->CancelAll();
  QDialog::closeEvent(event);
}

void EMailImapController::done(int result) {
  // Here rather than in closeEvent(), which is the whole reason the cache
  // appeared not to work: the Close button and Escape both go through
  // reject(), and reject() does NOT deliver a close event -- only the window's
  // own X button does. done() is the one path every dismissal takes.
  //
  // Leaving the picker is the ordinary way to finish with it, so it is the
  // case the cache exists to survive; remembering only on an account switch
  // meant the cache was written by the one path a user with a single account
  // never takes.
  remember_current_account();

  QDialog::done(result);
}

void EMailImapController::connect_to_selected_account() {
  const auto index = account_combo_->currentIndex();
  if (index < 0 || index >= accounts_.size()) {
    // Said, and the controls put back. A silent return here left the folder
    // list and search enabled over a session that had already been told to
    // disconnect.
    status_label_->setText(tr("Choose an account to open."));
    refresh_idle_state();
    return;
  }

  const auto account = accounts_.at(index);

  if (unusable_.contains(account.id)) {
    status_label_->setText(tr("This account cannot be opened: %1.")
                               .arg(unusable_.value(account.id)));
    refresh_idle_state();
    return;
  }

  // No prompt. A password is configuration, and it is set in exactly one place
  // -- Settings -- so there is a single answer to "where does this credential
  // live", rather than a dialog that holds one for a session and then forgets
  // it. An account without one is already greyed out in the list above.
  auto password = EMailCredentialStore::Load(account.id);
  if (password->IsEmpty()) {
    disable_account(account.id, tr("no password is stored"));
    status_label_->setText(
        tr("No password is stored for this account. Set one in Settings, "
           "under Mail Accounts, then use Refresh."));
    refresh_idle_state();
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

  set_busy(true, tr("Connecting..."));
  const auto seq = next_seq();
  QMetaObject::invokeMethod(
      worker_, "Connect", Qt::QueuedConnection, Q_ARG(quint64, seq),
      Q_ARG(MailAccountConfig, account), Q_ARG(EMailSecretPtr, password));
  password.reset();
}

void EMailImapController::slot_account_changed() {
  if (closing_) return;

  // Choosing an account is a request to browse it. What is on screen belongs
  // to the previous one, so it is put away first -- then the new account is
  // connected and refreshed without further ceremony.
  remember_current_account();

  current_folder_.clear();
  page_starts_ = {0};
  page_index_ = 0;
  remaining_ = 0;
  searching_ = false;
  search_edit_->clear();

  if (worker_ != nullptr) {
    QMetaObject::invokeMethod(worker_, "Disconnect", Qt::QueuedConnection);
  }

  connect_to_selected_account();
}

void EMailImapController::slot_refresh() {
  // Refresh is also how an account that was taken out of the list comes back.
  // The reason it was disabled described one attempt, not the account: a
  // password set in Settings since, or a network that has come back, makes it
  // wrong. Without this the only way back was to close and reopen the window.
  const auto index = account_combo_->currentIndex();
  if (index >= 0 && index < accounts_.size()) {
    const auto id = accounts_.at(index).id;
    if (unusable_.contains(id)) {
      unusable_.remove(id);
      failed_.remove(id);
      refresh_account_availability();
      account_combo_->setCurrentIndex(index);
      connect_to_selected_account();
      return;
    }
  }

  if (current_folder_.isEmpty()) {
    connect_to_selected_account();
    return;
  }
  searching_ = false;
  search_edit_->clear();
  request_page(true);
}

auto EMailImapController::last_account() -> QString& {
  // Alongside view_cache(), and with the same lifetime and the same reason:
  // the dialog cannot remember anything itself, because it is destroyed every
  // time it closes.
  static QString account_id;
  return account_id;
}

auto EMailImapController::view_cache() -> QHash<QString, AccountViewState>& {
  // Function-local static: constructed on first use, never destroyed before
  // the process ends, and only ever touched from the GUI thread (every caller
  // is a slot on this dialog).
  static QHash<QString, AccountViewState> cache;
  return cache;
}

void EMailImapController::remember_current_folder(AccountViewState& state) {
  if (current_folder_.isEmpty() || rows_.isEmpty()) return;

  // Only rows the server has vouched for. Filing a provisional view would let
  // a stale page outlive the one refresh that was going to correct it.
  if (showing_cached_ && !cache_confirmed_) return;
  if (!folder_validators_.known) return;

  FolderViewState view;
  view.rows = rows_;
  view.page_starts = page_starts_;
  view.page_index = page_index_;
  view.remaining = remaining_;
  view.validators = folder_validators_;
  view.used_at = QDateTime::currentDateTimeUtc();
  state.views.insert(current_folder_, view);

  // Bounded: a user walking a large tree would otherwise keep every folder
  // they glanced at for the life of the process.
  while (state.views.size() > kMaxCachedFolders) {
    QString oldest;
    QDateTime oldest_at;
    for (auto it = state.views.constBegin(); it != state.views.constEnd();
         ++it) {
      if (it.key() == current_folder_) continue;
      if (oldest.isEmpty() || it->used_at < oldest_at) {
        oldest = it.key();
        oldest_at = it->used_at;
      }
    }
    if (oldest.isEmpty()) break;
    state.views.remove(oldest);
  }
}

void EMailImapController::remember_current_account() {
  if (current_account_id_.isEmpty()) return;

  last_account() = current_account_id_;

  auto& state = view_cache()[current_account_id_];
  if (!folders_.isEmpty()) state.folders = folders_;
  if (!current_folder_.isEmpty()) state.folder = current_folder_;
  state.search = search_edit_->text();

  remember_current_folder(state);
}

auto EMailImapController::restore_cached_account(const QString& account_id)
    -> bool {
  const auto& cache = view_cache();
  const auto it = cache.constFind(account_id);
  if (it == cache.constEnd() || it->folders.isEmpty()) return false;

  folders_ = it->folders;
  current_folder_ = it->folder;

  folder_combo_->blockSignals(true);
  folder_combo_->clear();
  for (const auto& folder : folders_) folder_combo_->addItem(folder.path);
  for (int i = 0; i < folders_.size(); ++i) {
    if (folders_.at(i).path != current_folder_) continue;
    folder_combo_->setCurrentIndex(i);
    break;
  }
  folder_combo_->blockSignals(false);

  // The folder list alone is worth showing even when that folder's page is
  // not cached: the combo is populated and the user can choose immediately.
  const auto view = it->views.constFind(current_folder_);
  if (view == it->views.constEnd() || view->rows.isEmpty()) {
    rows_.clear();
    cached_validators_ = {};
    cached_folder_.clear();
    showing_cached_ = false;
    cache_confirmed_ = false;
    refresh_table();
    return true;
  }

  rows_ = view->rows;
  page_starts_ = view->page_starts;
  page_index_ = view->page_index;
  remaining_ = view->remaining;
  cached_validators_ = view->validators;
  cached_folder_ = current_folder_;
  folder_validators_ = {};
  showing_cached_ = true;
  cache_confirmed_ = false;

  refresh_table();
  return true;
}

auto EMailImapController::restore_cached_folder(const QString& folder) -> bool {
  if (current_account_id_.isEmpty() || folder.isEmpty()) return false;

  const auto& cache = view_cache();
  const auto account = cache.constFind(current_account_id_);
  if (account == cache.constEnd()) return false;

  const auto view = account->views.constFind(folder);
  if (view == account->views.constEnd() || view->rows.isEmpty() ||
      !view->validators.known) {
    return false;
  }

  rows_ = view->rows;
  page_starts_ = view->page_starts;
  page_index_ = view->page_index;
  remaining_ = view->remaining;
  cached_validators_ = view->validators;
  cached_folder_ = folder;
  folder_validators_ = {};
  showing_cached_ = true;
  cache_confirmed_ = false;

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

void EMailImapController::slot_message_menu(const QPoint& pos) {
  auto* item = list_->itemAt(pos);
  if (item == nullptr) return;

  list_->setCurrentItem(item);

  const auto row = list_->currentRow();
  if (row < 0 || row >= rows_.size()) return;
  const auto& summary = rows_.at(row);

  QMenu menu(this);
  auto* open = menu.addAction(tr("Open"));
  open->setEnabled(open_button_->isEnabled());

  menu.addSeparator();
  auto* copy_subject = menu.addAction(tr("Copy Subject"));
  copy_subject->setEnabled(!summary.subject.isEmpty());
  auto* copy_sender = menu.addAction(tr("Copy Sender"));
  copy_sender->setEnabled(!summary.from.isEmpty());
  auto* copy_id = menu.addAction(tr("Copy Message-ID"));
  copy_id->setEnabled(!summary.message_id.isEmpty());

  auto* chosen = menu.exec(list_->viewport()->mapToGlobal(pos));
  if (chosen == nullptr) return;

  auto* clipboard = QApplication::clipboard();
  if (chosen == open) {
    slot_open_selected();
  } else if (chosen == copy_subject) {
    clipboard->setText(summary.subject);
  } else if (chosen == copy_sender) {
    clipboard->setText(summary.from);
  } else if (chosen == copy_id) {
    clipboard->setText(summary.message_id);
  }
}

void EMailImapController::slot_selection_changed() {
  refresh_detail();
  refresh_idle_state();
}

void EMailImapController::paint_placeholder_icon() {
  if (placeholder_icon_ == nullptr) return;

  // Repainted rather than tinted once. The tint is baked into the pixmap's
  // own pixels, so after a light/dark switch a pixmap coloured for the old
  // theme stays exactly as it was -- a pale glyph on a dark ground.
  auto pixmap = QIcon::fromTheme("mail-unread", QIcon(":/icons/email.png"))
                    .pixmap(48, 48);

  QPainter painter(&pixmap);
  painter.setCompositionMode(QPainter::CompositionMode_SourceIn);

  // Muted rather than at full strength: an empty state should read as waiting
  // for the user, not as something demanding their attention.
  auto tint = EMailMutedColor(placeholder_icon_);
  tint.setAlpha(120);
  painter.fillRect(pixmap.rect(), tint);
  painter.end();

  placeholder_icon_->setPixmap(pixmap);
}

void EMailImapController::apply_colors() {
  // The single place this dialog's colours are decided, so a theme change has
  // one thing to call. Every colour here used to be captured once at
  // construction and kept whatever the theme did afterwards.
  paint_placeholder_icon();

  if (placeholder_label_ != nullptr) {
    EMailSetLabelColor(placeholder_label_, EMailMutedColor(this));
  }
  if (detail_rule_ != nullptr) EMailPaintRule(detail_rule_);
  if (detail_note_frame_ != nullptr) {
    EMailTintBanner(detail_note_frame_, EMailMutedColor(this));
  }
  if (empty_notice_ != nullptr) {
    EMailSetLabelColor(empty_notice_, EMailMutedColor(this));
  }
  if (loading_label_ != nullptr) {
    EMailSetLabelColor(loading_label_, EMailMutedColor(this));
  }

  // These two say what they say in a colour that depends on the message being
  // shown, so they are asked to decide again rather than recoloured here.
  refresh_detail();
}

void EMailImapController::changeEvent(QEvent* event) {
  QDialog::changeEvent(event);
  if (EMailIsRestyle(event)) apply_colors();
}

void EMailImapController::refresh_detail() {
  const auto row = list_->currentRow();
  if (row < 0 || row >= rows_.size()) {
    detail_stack_->setCurrentWidget(detail_placeholder_);
    return;
  }

  const auto& summary = rows_.at(row);
  detail_subject_->setText(summary.subject.isEmpty() ? tr("(no subject)")
                                                     : summary.subject);
  detail_from_->setText(summary.from);

  // Written the way the rest of the application writes a size, rather than
  // with Qt's own formatter, so one mailbox listing and one attachment list
  // never disagree about what a kilobyte is.
  const auto when = summary.date.isValid()
                        ? QLocale().toString(summary.date, QLocale::LongFormat)
                        : tr("Date not stated");
  detail_stamp_->setText(
      QString("%1  ·  %2").arg(when, EMailHumanSize(summary.size)));

  detail_id_full_ = summary.message_id;
  refresh_message_id();

  detail_folder_->setText(current_folder_);

  if (summary.TooLarge()) {
    detail_note_->setText(
        tr("This message is too large for this application to open, so it "
           "cannot be imported."));
    EMailSetLabelColor(detail_note_, EMailWarningColor(detail_note_));
    detail_note_frame_->setVisible(true);
  } else if (showing_cached_) {
    detail_note_->setText(
        tr("Shown from the previous visit to this account; refreshing."));
    EMailSetLabelColor(detail_note_, EMailMutedColor(detail_note_));
    detail_note_frame_->setVisible(true);
  } else {
    detail_note_frame_->setVisible(false);
  }

  detail_stack_->setCurrentIndex(1);
}

/**
 * @brief The Message-ID, shortened to fit and whole where it is useful.
 *
 * Elided rather than wrapped: an identifier has no words to break at, so
 * wrapping one produces three ragged lines of noise. The full value is always
 * in the tooltip and always what the copy button writes, so nothing is lost
 * by what is not drawn.
 */
void EMailImapController::refresh_message_id() {
  detail_copy_id_->setVisible(!detail_id_full_.isEmpty());

  if (detail_id_full_.isEmpty()) {
    detail_id_->setText(tr("None"));
    detail_id_->setToolTip({});
    return;
  }

  // Measured against the width the label has right now, which is why this is
  // redone on every resize rather than stored once in an elided form.
  const QFontMetrics metrics(detail_id_->font());
  detail_id_->setText(metrics.elidedText(detail_id_full_, Qt::ElideMiddle,
                                         qMax(80, detail_id_->width())));
  detail_id_->setToolTip(detail_id_full_);
}

auto EMailImapController::eventFilter(QObject* watched, QEvent* event) -> bool {
  if (watched == detail_id_ && event->type() == QEvent::Resize) {
    refresh_message_id();
  }
  return QDialog::eventFilter(watched, event);
}

void EMailImapController::set_progress_visible(bool visible) {
  if (visible) {
    progress_timer_->start();
    return;
  }
  progress_timer_->stop();
  progress_->setVisible(false);
  // Back to indeterminate, so the next operation does not inherit a stale
  // fraction from the last download.
  progress_->setRange(0, 0);
  progress_->setTextVisible(false);
}

void EMailImapController::set_progress_fraction(qint64 current, qint64 total) {
  // A download is the one operation whose length is known ahead of time, so
  // it is the one that can honestly show a fraction. Everything else stays a
  // marquee rather than pretending to measure something it cannot.
  if (total <= 0) {
    progress_->setRange(0, 0);
    progress_->setTextVisible(false);
    return;
  }

  progress_->setRange(0, 100);
  const auto percent = qBound(qint64{0}, current * 100 / total, qint64{100});
  progress_->setValue(static_cast<int>(percent));
}

void EMailImapController::slot_folder_changed() {
  const auto index = folder_combo_->currentIndex();
  if (index < 0 || index >= folders_.size()) return;

  // File the page being left before it is replaced, so coming back to it is
  // the same cheap STATUS as coming back to the account.
  if (!current_account_id_.isEmpty()) {
    remember_current_folder(view_cache()[current_account_id_]);
  }

  current_folder_ = folders_.at(index).path;
  searching_ = false;
  search_edit_->clear();
  showing_cached_ = false;
  cache_confirmed_ = false;

  // Shows this folder's last page immediately, provisionally, and asks the
  // server whether it still holds. A folder the user moves back and forth
  // between is then one round trip rather than a page of envelopes.
  if (restore_cached_folder(current_folder_) && probe_cached_folder()) return;

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

  set_busy(true, tr("Searching..."));
  QMetaObject::invokeMethod(
      worker_, "SearchMessages", Qt::QueuedConnection,
      Q_ARG(quint64, next_seq()), Q_ARG(QString, current_folder_),
      Q_ARG(QString, query), Q_ARG(int, kMailDefaultPageSize));
}

void EMailImapController::slot_next_page() {
  if (rows_.isEmpty() || remaining_ <= 0) return;

  // The next page starts just past the oldest row on this one. Recorded so
  // that coming back to this position later asks the server the same question
  // rather than a reconstructed guess.
  // The paging CURSOR, not the identity: a position in the folder as it was
  // when this page was listed. See EMailMessageSummary::seq.
  const auto next_start = rows_.last().seq;
  ++page_index_;
  if (page_starts_.size() <= page_index_) {
    page_starts_.append(next_start);
  } else {
    page_starts_[page_index_] = next_start;
  }
  request_page(false);
}

void EMailImapController::slot_previous_page() {
  if (page_index_ <= 0) return;
  --page_index_;
  request_page(false);
}

void EMailImapController::request_page(bool reset) {
  if (current_folder_.isEmpty() || worker_ == nullptr) return;

  if (reset) {
    // Back to the newest page. The cached rows stay on screen until the fresh
    // page lands, so the list does not blink empty on every switch;
    // handle_messages() clears them.
    if (!showing_cached_) rows_.clear();
    page_starts_ = {0};
    page_index_ = 0;
    remaining_ = 0;
    refresh_table();
  }

  const auto start =
      page_index_ < page_starts_.size() ? page_starts_.at(page_index_) : 0;

  set_busy(true, tr("Loading messages..."));
  QMetaObject::invokeMethod(
      worker_, "ListMessages", Qt::QueuedConnection, Q_ARG(quint64, next_seq()),
      Q_ARG(QString, current_folder_), Q_ARG(quint64, start),
      Q_ARG(int, kMailDefaultPageSize),
      // One page is held at a time, so the session cap is never approached and
      // "retained" is simply this page.
      Q_ARG(int, 0));
}

void EMailImapController::handle_connected(quint64 seq) {
  if (!is_current(seq)) return;
  set_busy(true, tr("Loading folders..."));
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
        tr("This account has no folders that can be opened."));
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

  if (probe_cached_folder()) return;

  request_page(true);
}

auto EMailImapController::probe_cached_folder() -> bool {
  // A cached page for exactly this folder is worth one STATUS before it is
  // thrown away: if nothing in the mailbox has moved, the envelopes already in
  // hand ARE the current listing, and fetching them again would spend a page
  // of round trips to arrive at the same rows.
  if (worker_ == nullptr || searching_) return false;
  if (!showing_cached_ || rows_.isEmpty()) return false;
  if (!cached_validators_.known || current_folder_ != cached_folder_) {
    return false;
  }

  status_probe_folder_ = current_folder_;
  const auto probe_seq = next_seq();
  set_busy(true, tr("Checking for new messages..."));
  QMetaObject::invokeMethod(worker_, "FolderStatus", Qt::QueuedConnection,
                            Q_ARG(quint64, probe_seq),
                            Q_ARG(QString, current_folder_));
  return true;
}

void EMailImapController::handle_folder_status(
    quint64 seq, const QString& folder,
    const EMailFolderValidators& validators) {
  if (!is_current(seq)) return;

  // The user moved on while this was in flight, so whatever it says is about
  // a folder that is no longer on screen.
  if (folder != status_probe_folder_ || folder != current_folder_) {
    set_busy(false, {});
    request_page(true);
    return;
  }
  status_probe_folder_.clear();

  folder_validators_ = validators;

  // Anything other than a clean match -- a changed UIDVALIDITY, new mail, an
  // expunge, a flag moved elsewhere, or a server that would not answer -- is
  // handled the same way: fetch the page. The shortcut is only ever taken when
  // the server has positively said nothing changed.
  if (!validators.SameAs(cached_validators_)) {
    request_page(true);
    return;
  }

  // Confirmed current. The rows stop being provisional, so they are drawn
  // normally rather than dimmed, and are worth caching again.
  showing_cached_ = false;
  cache_confirmed_ = true;
  set_busy(false, tr("Up to date."));
  refresh_table();
  refresh_page_controls(false);
  refresh_idle_state();
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

  // One page REPLACES the last, rather than accumulating: that is what keeps
  // memory flat no matter how far the user pages into a large mailbox.
  rows_ = page.rows;
  remaining_ = page.remaining;

  // What these rows were read under, so the next visit has something to
  // compare a STATUS against.
  folder_validators_ = page.validators;
  cached_folder_ = current_folder_;
  cached_validators_ = page.validators;
  cache_confirmed_ = false;

  refresh_table();
  refresh_page_controls(page.capped);
  refresh_idle_state();
}

/// The page position, and how much of the mailbox is behind and ahead of it.
void EMailImapController::refresh_page_controls(bool capped) {
  const auto shown = static_cast<int>(rows_.size());

  if (searching_) {
    // Search results are one bounded set, not a position in the mailbox, so
    // paging through them would be meaningless.
    page_label_->clear();
    previous_button_->setVisible(false);
    next_button_->setVisible(false);
    status_label_->setText(
        capped ? tr("Showing the first %1 matches; narrow the search to see "
                    "fewer.")
                     .arg(shown)
               : tr("%1 matches.").arg(shown));
    return;
  }

  previous_button_->setVisible(true);
  next_button_->setVisible(true);

  const auto before = page_index_ * kMailDefaultPageSize;
  const auto total = before + shown + remaining_;
  const auto pages =
      total > 0 ? (total + kMailDefaultPageSize - 1) / kMailDefaultPageSize : 1;

  page_label_->setText(tr("Page %1 of %2").arg(page_index_ + 1).arg(pages));
  status_label_->setText(
      shown == 0
          ? tr("This folder is empty.")
          : tr("%1-%2 of %3").arg(before + 1).arg(before + shown).arg(total));

  previous_button_->setEnabled(page_index_ > 0);
  next_button_->setEnabled(remaining_ > 0);
}

void EMailImapController::handle_fetched(quint64 seq,
                                         const QByteArray& raw_eml) {
  if (!is_current(seq)) return;
  set_busy(false, {});

  if (raw_eml.isEmpty()) {
    status_label_->setText(tr("That message came back empty."));
    return;
  }

  // Opened straight away. Pressing Open was the decision; asking again once
  // the bytes have arrived makes the user confirm something they have already
  // said, and the download it was guarding has happened either way.
  //
  // The window stays open. Downloading a message is not a decision to stop
  // browsing, and closing the picker on every open made looking through a
  // mailbox mean reopening it each time.
  emit SignalMessageChosen(raw_eml);
  status_label_->setText(tr("Opened in a new tab."));
}

void EMailImapController::handle_fetch_progress(quint64 seq, qint64 current,
                                                qint64 total) {
  if (!is_current(seq)) return;

  set_progress_fraction(current, total);
  if (total > 0) {
    status_label_->setText(tr("Downloading message, %1 of %2...")
                               .arg(QLocale().formattedDataSize(current),
                                    QLocale().formattedDataSize(total)));
  }
}

void EMailImapController::handle_failed(quint64 seq, const MailError& error) {
  if (!is_current(seq)) return;
  set_busy(false, {});

  // Stopping is not a failure, but it is an outcome, and it used to produce
  // nothing at all -- set_busy() had just blanked the status line, so the
  // window simply went quiet and left the user wondering whether the Stop had
  // even registered.
  if (error.category == MailErrorCategory::kCANCELLED) {
    status_label_->setText(tr("Stopped."));
    refresh_idle_state();
    return;
  }

  // A failure that retrying cannot fix takes the account out of the list, so
  // the user is not invited to try it again and again. A transient one --
  // a timeout, a dropped connection -- deliberately does not: the account is
  // fine and the network was not.
  switch (error.category) {
    case MailErrorCategory::kAUTH:
      disable_account(current_account_id_, tr("the password was refused"));
      break;
    case MailErrorCategory::kDNS:
      disable_account(current_account_id_, tr("the server was not found"));
      break;
    case MailErrorCategory::kTLS_UNTRUSTED:
      // Not disabled. This is the one TLS failure with a legitimate answer --
      // a server whose certificate nobody vouches for is the normal case for
      // one you run yourself -- and the answer lives in Settings, where the
      // certificate can be looked at and trusted. Taking the account out of
      // the list here would hide the only route to fixing it.
      break;
    case MailErrorCategory::kTLS_HANDSHAKE:
    case MailErrorCategory::kTLS_EXPIRED:
    case MailErrorCategory::kTLS_HOSTNAME:
    case MailErrorCategory::kTLS_REQUIRED:
      disable_account(current_account_id_,
                      tr("the secure connection was refused"));
      break;
    default:
      break;
  }

  show_error(error);
}

void EMailImapController::slot_open_selected() {
  // Refused on the size the listing already reported, so an oversized message
  // is never downloaded to find out it was too big. current_summary() returns
  // nullptr for one, and the detail pane has already said why.
  const auto* summary = current_summary();
  if (summary == nullptr) return;

  set_busy(true, tr("Downloading message..."));

  // Unlike a folder listing, a download is never instant and its size is
  // already known, so the bar is shown straight away rather than after the
  // usual short delay: there is nothing to avoid flashing.
  progress_timer_->stop();
  set_progress_fraction(0, summary->size);
  progress_->setVisible(true);

  QMetaObject::invokeMethod(
      worker_, "FetchMessage", Qt::QueuedConnection, Q_ARG(quint64, next_seq()),
      Q_ARG(QString, current_folder_), Q_ARG(quint64, summary->uid));
}

void EMailImapController::slot_cancel_busy() {
  // Scoped to the request in flight. A stop pressed now must not reach past it
  // and kill whatever the user asks for next.
  if (worker_ != nullptr) worker_->Token()->Cancel(request_seq_);
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
    const auto sender = row.from.isEmpty() ? tr("Unknown sender") : row.from;
    const auto subtitle =
        date.isEmpty() ? sender : QString("%1  ·  %2").arg(sender, date);

    auto* item = new QListWidgetItem(list_);
    item->setData(kSubjectRole,
                  row.subject.isEmpty() ? tr("(no subject)") : row.subject);
    item->setData(kSubtitleRole, subtitle);
    item->setData(kDimmedRole, showing_cached_);
    list_->addItem(item);
  }

  if (previous >= 0 && previous < list_->count()) {
    list_->setCurrentRow(previous);
  }

  refresh_empty_notice();
  refresh_detail();
}

void EMailImapController::refresh_empty_notice() {
  if (empty_notice_ == nullptr) return;

  const bool empty = rows_.isEmpty();

  // Nothing to show YET is not the same as nothing to show. While a folder is
  // being fetched the pane says so, in the place the messages will appear.
  // Rows already on screen -- the cached ones -- are left alone: covering them
  // to announce a refresh would take away what the user was reading.
  if (busy_ && empty) {
    list_->setVisible(false);
    empty_notice_->setVisible(false);
    if (loading_pane_ != nullptr) {
      loading_label_->setText(busy_what_.isEmpty() ? tr("Loading...")
                                                   : busy_what_);
      loading_pane_->setVisible(true);
    }
    return;
  }

  if (loading_pane_ != nullptr) loading_pane_->setVisible(false);

  list_->setVisible(!empty);
  empty_notice_->setVisible(empty);
  if (!empty) return;

  // Which kind of empty this is. "No results" and "nothing here" are different
  // answers, and a folder that has not been opened yet is a third.
  empty_notice_->setText(
      searching_ ? tr("No message in this folder matches that search.")
      : current_folder_.isEmpty()
          ? tr("Choose a folder to see the messages in it.")
          : tr("This folder is empty."));
}

void EMailImapController::set_busy(bool busy, const QString& what) {
  set_progress_visible(busy);

  // Remembered so the left pane can say which work is in progress, in the
  // caller's own words, rather than inventing a second vocabulary for it.
  busy_ = busy;
  busy_what_ = busy ? what : QString{};

  account_combo_->setEnabled(!busy);
  refresh_button_->setEnabled(!busy && account_combo_->currentIndex() >= 0);

  // Re-enabled only for a session that actually has folders: leaving a busy
  // state must not leave the browse controls live on a window that never
  // connected to anything.
  const auto connected = !folders_.isEmpty();
  folder_combo_->setEnabled(!busy && connected);
  search_edit_->setEnabled(!busy && connected);
  open_button_->setEnabled(!busy && current_summary() != nullptr);

  // From remembered state, never from the buttons themselves: reading a
  // widget back could only ever clear such a flag, so a control lost during a
  // busy period never returned.
  previous_button_->setEnabled(!busy && page_index_ > 0);
  next_button_->setEnabled(!busy && remaining_ > 0);

  if (busy) {
    status_label_->setText(what);
    cancel_button_->setText(tr("Stop"));
    disconnect(cancel_button_, &QPushButton::clicked, nullptr, nullptr);
    connect(cancel_button_, &QPushButton::clicked, this,
            &EMailImapController::slot_cancel_busy);
  } else {
    // The caller's word for what just finished, so the last busy message does
    // not stay on screen pretending the work is still running.
    status_label_->setText(what);
    cancel_button_->setText(tr("Close"));
    disconnect(cancel_button_, &QPushButton::clicked, nullptr, nullptr);
    connect(cancel_button_, &QPushButton::clicked, this, &QDialog::reject);
  }

  refresh_empty_notice();
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
