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

#include <QDateTime>
#include <QDialog>
#include <QHash>
#include <QList>
#include <QSet>

#include "EMailAccountModel.h"
#include "EMailImapWorker.h"

class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QThread;
class QTimer;
class QToolButton;

/**
 * @brief A remote file picker for messages. Not an inbox.
 *
 * One window, short-lived, owning one IMAP session for as long as it is open
 * and nothing afterwards. It browses, it returns one message's bytes, and it
 * closes; no mailbox state outlives it, and none of what it learns reaches the
 * document model.
 *
 * It deliberately knows nothing about tabs. The chosen message leaves through
 * SignalMessageChosen() and the module decides what to do with it, which is
 * what keeps the picker from growing into a mail client.
 */
class EMailImapController : public QDialog {
  Q_OBJECT

 public:
  explicit EMailImapController(QWidget* parent = nullptr);
  ~EMailImapController() override;

  /// Whether there is any account this window could browse.
  static auto HasUsableAccount() -> bool;

 signals:
  /**
   * @brief The user picked a message.
   *
   * The whole boundary: raw RFC bytes, exactly as the server holds them, with
   * no folder, no UID and no account attached.
   */
  void SignalMessageChosen(const QByteArray& raw_eml);

 protected:
  void closeEvent(QCloseEvent* event) override;

  /// Every dismissal -- Close, Escape, accept -- passes through here, which
  /// closeEvent() does not: reject() never delivers one.
  void done(int result) override;
  void changeEvent(QEvent* event) override;
  /// Watches the Message-ID label so its elision follows the pane's width.
  auto eventFilter(QObject* watched, QEvent* event) -> bool override;

 private slots:
  void slot_account_changed();
  void slot_folder_changed();
  void slot_search();
  void slot_next_page();
  void slot_previous_page();
  void slot_open_selected();
  void slot_selection_changed();
  /// The message list's own menu, for the row at @p pos.
  void slot_message_menu(const QPoint& pos);
  void slot_refresh();
  void slot_cancel_busy();

  void handle_connected(quint64 seq);
  void handle_folders(quint64 seq, const QList<EMailFolderInfo>& folders);
  void handle_messages(quint64 seq, const EMailMessagePage& page);
  void handle_fetched(quint64 seq, const QByteArray& raw_eml);
  void handle_fetch_progress(quint64 seq, qint64 current, qint64 total);
  void handle_failed(quint64 seq, const MailError& error);

 private:
  void build_ui();
  /// Builds the right-hand metadata pane and its empty state.
  auto build_detail_pane() -> QWidget*;
  void start_worker();
  /// Tear the session and its thread down, on the worker's own thread.
  void stop_worker();

  void connect_to_selected_account();
  void request_page(bool reset);
  void refresh_table();
  /// Enables only what the current connection state actually supports.
  void refresh_idle_state();
  void set_busy(bool busy, const QString& what);
  void show_error(const MailError& error);

  /// Whether a result belongs to the request we are still waiting for. Stale
  /// answers are dropped rather than applied: without this, picking message A
  /// and then B would show A's bytes if A's fetch finished last.
  auto is_current(quint64 seq) const -> bool { return seq == request_seq_; }
  auto next_seq() -> quint64 { return ++request_seq_; }

  /**
   * @brief What one account looked like the last time it was browsed.
   *
   * Metadata only, in memory only, and only for as long as this window is
   * open. It exists so that switching back to an account shows something
   * immediately instead of a blank list, and for no other reason: the message
   * BYTES are deliberately never kept, because a fetched message can hold
   * plaintext, and nothing here is ever written to disk. Restored rows are
   * marked provisional until a refresh confirms them, so a stale row is never
   * mistaken for current server state.
   */
  /// One folder's page, and the folder state it was read under.
  struct FolderViewState {
    QList<EMailMessageSummary> rows;
    QList<quint64> page_starts;
    int page_index{0};
    int remaining{0};

    /// Compared against a fresh STATUS to decide whether `rows` may be shown
    /// as current.
    EMailFolderValidators validators;

    /// Orders eviction when the cap below is reached: the folders a user
    /// actually moves between are the ones worth keeping.
    QDateTime used_at;
  };

  struct AccountViewState {
    QList<EMailFolderInfo> folders;
    QString folder;  ///< the one that was on screen last
    QString search;

    /// Per folder, so moving between folders and back does not re-fetch each
    /// time. Bounded by kMaxCachedFolders; these are envelopes, never bodies.
    QHash<QString, FolderViewState> views;
  };

  /// How many folders' listings are worth keeping per account. Each is a page
  /// of envelopes -- subject, sender, date -- so this is kilobytes, not
  /// megabytes; the cap exists so a user who walks a large folder tree does
  /// not accumulate without bound.
  static constexpr int kMaxCachedFolders = 12;

  /// Marks accounts the controller cannot use, with the reason.
  void refresh_account_availability();
  /// Disables one account after it has failed, so it is not retried blindly.
  void disable_account(const QString& account_id, const QString& reason);
  void remember_current_account();
  /// Files the folder currently on screen into @p state, evicting the least
  /// recently used once the cap is reached.
  void remember_current_folder(AccountViewState& state);
  /// Shows the cached page for @p folder and asks the server to vouch for it.
  /// Returns false when there is nothing usable cached.
  auto restore_cached_folder(const QString& folder) -> bool;
  /// Asks the server to vouch for the cached page now on screen. Returns false
  /// when there is nothing cached to vouch for, and the caller must fetch.
  auto probe_cached_folder() -> bool;
  /**
   * @brief The cached views, one per account, for the life of the process.
   *
   * Deliberately NOT a member: this dialog is created fresh on every open and
   * destroyed with WA_DeleteOnClose, so a cache living in the instance died
   * with it and the second open paid for the same folder listing and the same
   * page of envelopes as the first.
   *
   * In memory only. Nothing here reaches disk, so a mailbox's subjects and
   * senders do not outlive the program that was asked to show them.
   */
  static auto view_cache() -> QHash<QString, AccountViewState>&;

  /// The account that was open when the picker was last dismissed, so the next
  /// open returns to it rather than to the configured default.
  static auto last_account() -> QString&;

  /// Restores a cached view for @p account_id, if there is one.
  auto restore_cached_account(const QString& account_id) -> bool;
  /// Handles a STATUS reply: either adopts the cached rows or fetches a page.
  void handle_folder_status(quint64 seq, const QString& folder,
                            const EMailFolderValidators& validators);

  void refresh_detail();
  /// Shows the list or the sentence standing in for it.
  void refresh_empty_notice();
  /// The single place this dialog's colours are decided. Must not touch fonts.
  void apply_colors();
  /// Redraws the empty-pane glyph in the current theme's muted colour.
  void paint_placeholder_icon();
  /// Re-elides the Message-ID to whatever width its label currently has.
  void refresh_message_id();
  /// Updates the page position, its label, and the two page buttons.
  void refresh_page_controls(bool capped);
  /// The selected message, or nullptr when none is selected or it is too big.
  auto current_summary() const -> const EMailMessageSummary*;
  void set_progress_visible(bool visible);
  /// Switches the bar to a real fraction, or back to indeterminate.
  void set_progress_fraction(qint64 current, qint64 total);

  QList<MailAccountConfig> accounts_;
  QList<EMailFolderInfo> folders_;
  QList<EMailMessageSummary> rows_;
  /// Whether the rows on screen came from the cache and have since been
  /// confirmed current by a STATUS that matched. They are then as good as
  /// freshly fetched, so they are drawn normally rather than dimmed.
  bool cache_confirmed_{false};

  /// The validators the cached rows were fetched under, and the ones the
  /// server reports now. Equal means the cached page is still the page.
  EMailFolderValidators cached_validators_;
  EMailFolderValidators folder_validators_;

  /// The folder the cached rows belong to. The picker may open on a different
  /// folder than the one that was cached, and rows from one folder must never
  /// be vouched for by another folder's STATUS.
  QString cached_folder_;

  /// The folder a pending STATUS is asking about, so a late reply for a folder
  /// the user has already left is ignored rather than acted on.
  QString status_probe_folder_;
  QString current_account_id_;
  /// Accounts that cannot be browsed, and why. Keyed by account id.
  /// The empty-state widgets, kept so a theme change can recolour them.
  QLabel* placeholder_icon_{};
  QLabel* placeholder_label_{};
  QLabel* empty_notice_{};
  QFrame* detail_rule_{};

  QHash<QString, QString> unusable_;
  /// Accounts whose reason came from an attempt that failed rather than from
  /// a configuration that is incomplete. Those are worth offering to retry;
  /// a missing server address is not.
  QSet<QString> failed_;

  EMailImapWorker* worker_{};
  QThread* thread_{};

  quint64 request_seq_{0};
  QString current_folder_;
  /// Where each page begins, as the UID to page back from; index 0 is the
  /// newest page and always starts at 0, meaning "from the newest". Held as a
  /// stack because IMAP can only page BACKWARDS from a UID: returning to an
  /// earlier page means remembering where it started, not computing it.
  QList<quint64> page_starts_;
  int page_index_{0};
  /// Older messages the server says are still beyond the current page.
  int remaining_{0};
  bool searching_{false};
  bool closing_{false};
  /// Set while showing cached rows that have not yet been confirmed.
  bool showing_cached_{false};

  QComboBox* account_combo_{};
  QComboBox* folder_combo_{};

  /// The left pane's "still fetching" state: shown in place of the empty
  /// notice so a folder being loaded is never reported as a folder that is
  /// empty. Its label carries whatever set_busy() was told the work is.
  QWidget* loading_pane_{};
  QLabel* loading_label_{};
  bool busy_{};
  QString busy_what_;
  QLineEdit* search_edit_{};
  QToolButton* refresh_button_{};
  QListWidget* list_{};
  QSplitter* splitter_{};
  QStackedWidget* detail_stack_{};
  QWidget* detail_placeholder_{};
  QWidget* detail_page_{};
  QLabel* detail_subject_{};
  QLabel* detail_from_{};
  /// Date and size on one muted line: two facts of the same weight, neither
  /// worth a row of its own beside the subject.
  QLabel* detail_stamp_{};
  QLabel* detail_id_{};
  QLabel* detail_folder_{};
  QFrame* detail_note_frame_{};
  QLabel* detail_note_{};
  QToolButton* detail_copy_id_{};
  /// The full Message-ID, kept because the label shows an elided form of it.
  QString detail_id_full_;
  QProgressBar* progress_{};
  QTimer* progress_timer_{};
  QPushButton* previous_button_{};
  QPushButton* next_button_{};
  QLabel* page_label_{};
  QPushButton* open_button_{};
  QPushButton* cancel_button_{};
  QLabel* status_label_{};
};
