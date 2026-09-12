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

#include <QWidget>

#include "EMailModel.h"

class QLineEdit;
class QPlainTextEdit;
class QToolButton;
class QTreeWidget;
class QFormLayout;
class QFrame;
class QLabel;
class QTabWidget;
class EMailStructureView;
class EMailHeaderView;
class EMailSecurityView;
class EMailBodyView;
class QStackedWidget;

/**
 * @brief The message view of an e-mail tab.
 *
 * Mounted into the host's editor page as its primary view; the page keeps
 * owning the raw document, which stays the canonical content of the tab. This
 * widget parses that document into headers, a body and a list of parts, and
 * serializes back only once the user has actually edited something.
 *
 * The last point is load-bearing. PGP/MIME signatures cover exact octets, so a
 * received message must reach a verify untouched. While `dirty_` is false the
 * host reads the original bytes and this widget never regenerates them; after
 * an edit the message is being composed rather than inspected, and vmime is
 * free to rewrite boundaries, header order and encodings.
 */
/**
 * @brief Tells every open e-mail view that the local keyring changed.
 *
 * Safe to call from any thread: the views are widgets, so the work is handed
 * to the GUI thread. Views are looked up as they are at that moment rather
 * than kept in a registry, which keeps a closed tab from ever being notified.
 */
void EMailNotifyKeyringChanged();

class EMailPageView : public QWidget {
  Q_OBJECT

 public:
  explicit EMailPageView(QWidget* parent = nullptr);

 public slots:
  /**
   * @brief Tells this view that the local keyring has changed.
   *
   * Verification answers are only true of the keyring that produced them: a
   * signature reported as unverifiable because the key was missing becomes
   * verifiable the moment that key is imported. This discards those answers so
   * they are worked out again, which is what makes an import visible without
   * reopening the message.
   *
   * Re-derives immediately when the Security tab is the one being looked at,
   * and otherwise leaves the work to the next visit.
   */
  void NotifyKeyringChanged();  // NOLINT

  /**
   * @brief Takes the host's raw document editor and presents it as a tab.
   *
   * Optional half of the page/view contract: a page that finds this member
   * hands over its own editor instead of putting a Message / Raw Source
   * switcher above this widget, so every way of looking at one message sits in
   * one row of tabs.
   *
   * The widget passed in is the real editor over the real document, not a
   * copy. Editing the raw source therefore keeps working, and the document
   * stays the one canonical content of the tab.
   */
  void AdoptSourceView(QWidget* source);  // NOLINT

  /**
   * @brief Takes the document's bytes as the message to show.
   *
   * Parsing only; nothing is written back, and the widget comes out clean.
   */
  void LoadFromSource(const QByteArray& source);  // NOLINT

  /**
   * @brief Locks this view so the document can never be modified.
   *
   * Forensic mode. The rule is about THIS document's bytes: it can never be
   * edited, marked dirty or reserialized, so the message stays exactly as it
   * arrived no matter what the user clicks.
   *
   * It is deliberately not a ban on read-derived work. Reply, Reply All and
   * Forward stay available, because each produces a NEW document in a new tab
   * and leaves this one untouched; so does saving an attachment. What is
   * forbidden is mutation in place.
   */
  void SetForensicMode(bool on);  // NOLINT

  /// Whether this view is locked against modifying its document.
  [[nodiscard]] auto IsForensicMode() const -> bool { return forensic_; }

  /**
   * @brief The message as the user has it now, serialized.
   *
   * Only called when IsDirty() says the view has edits, so the untouched case
   * never pays for a regeneration -- nor risks one.
   */
  QByteArray SaveToSource();  // NOLINT

  /**
   * @brief Whether the user has edited the view since it was last loaded.
   */
  bool IsDirty();  // NOLINT

  /**
   * @brief Zeroes the message content this view holds.
   *
   * Covers the attachment buffers as well as the body: a decrypted attachment
   * living on in a QByteArray would defeat the host's wipe-on-close.
   */
  void WipeContent();  // NOLINT

 signals:
  /**
   * @brief Emitted on the first edit after a load, so the host can mark the
   * tab modified without waiting for a serialization.
   */
  void SignalContentModified();

  /**
   * @brief Emitted just before the adopted raw editor is shown.
   *
   * The host answers by writing any pending edits into the document, so what
   * the user reads as the raw source is never behind the structured view.
   */
  void SignalSourceViewRequested();

 protected:
  /// Accepts a drag only when it carries files and the document may change.
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  /// Attaches dropped files.
  void dropEvent(QDropEvent* event) override;

 private slots:
  void slot_add_attachment();
  /// Attaches the files at @p paths, reporting any that cannot be read.
  void attach_paths(const QStringList& paths);
  void slot_remove_attachment();
  void slot_save_attachment();
  void slot_save_all_attachments();
  void slot_selection_changed();
  /// Opens a new tab holding a message derived from this one. The current
  /// document is only read: deriving never modifies what it derives from.
  void slot_derive_message(int mode);

 private:
  /// Rebuilds the attachment table from `message_`.
  void refresh_attachments();
  /// Writes the header and body widgets from `message_`.
  void refresh_fields();
  /// Reads the header and body widgets back into `message_`.
  void collect_fields();
  /// Marks the view edited, emitting SignalContentModified once per load.
  void mark_dirty();
  /// Saves one attachment into `dir` under `name`, atomically.
  auto write_attachment(const EMailAttachment& att, const QString& dir,
                        const QString& name) -> bool;
  void build_ui();
  /// Builds the Message sub-tab, which holds the editable message itself.
  auto build_message_tab() -> QWidget*;
  /// Reparses `last_source_` into the tree the inspection tabs render.
  void refresh_structure();
  /// Brings the inspection tabs back in line with edits made in the Message
  /// tab, re-deriving the document from the editor before reparsing it.
  ///
  /// Deferred until an inspection tab is actually visited rather than run per
  /// keystroke: serializing re-encodes every attachment, which is far too
  /// expensive to repeat while someone is typing.
  ///
  /// Only ever does anything once the view is dirty, so a message that is
  /// merely being inspected is never reserialized.
  void sync_inspection();
  /// Redraws the status text at whatever width the action row leaves it,
  /// shortening it rather than pushing the buttons off the row.
  void fit_status_banner();
  /// Writes the banner that says what the message is and what to do with it.
  void refresh_status_banner();
  /// Refreshes the Security tab from what parsing and any completed operation
  /// have established. Read-only: it never writes the document, so opening the
  /// tab cannot change the bytes.
  void refresh_security();
  /// Verifies each signature region, once, the first time the Security tab is
  /// actually looked at. Deferred rather than done on load because a message
  /// may carry several signatures and most tabs are never opened; verifying
  /// reads the message and writes nothing, so it is safe to run here.
  void ensure_regions_verified();
  /// Chooses between the formatted and plain renderings of the body, and
  /// shows the toggle only when the message actually offers both.
  void refresh_body_view();
  /// Keeps the status text fitted to whatever room the action row leaves it.
  auto eventFilter(QObject* watched, QEvent* event) -> bool override;
  /// Shows or hides the Cc and Bcc rows. Never clears them: collapsing a row
  /// is a view choice, not a decision to discard what is in it.
  void set_cc_bcc_visible(bool visible);

  EMailMetaData message_;
  /// The parsed tree behind the inspection tabs. Rebuilt on every load and
  /// never written back -- inspection is byte-preserving.
  EMailPart tree_root_;
  QList<EMailSignatureRegion> regions_;
  EMailSecurityState security_state_{EMailSecurityState::kPLAIN};
  /// Verification results, each already stamped with the region whose bytes
  /// produced it. Empty until an operation has actually run.
  QList<EMailSignatureResult> signature_results_;
  QList<EMailRecipientRow> recipient_rows_;
  bool regions_verified_{false};
  /// Whether edits have left the inspection tabs describing an older document
  /// than the one the editor now holds.
  bool inspection_stale_{false};
  /// How this tab is currently displaying things. Not message state and not
  /// envelope state, and never persisted.
  EMailViewState view_state_;
  /// Blind recipients and other envelope state. Kept beside the document
  /// rather than in it: BCC never becomes a header, so it cannot live in
  /// EMailMetaData without leaking into the message.
  EMailComposeState compose_;
  /// The document as it was last loaded or written. Kept so a serialization
  /// that fails can hand back what was there rather than a lossy substitute.
  QByteArray last_source_;
  bool dirty_{false};
  bool loading_{false};
  /// When set, this document cannot be modified or reserialized at all.
  bool forensic_{false};

  QLineEdit* from_edit_{};
  QLineEdit* to_edit_{};
  QLineEdit* cc_edit_{};
  QLineEdit* bcc_edit_{};
  QLineEdit* subject_edit_{};
  QToolButton* cc_bcc_toggle_{};
  QFormLayout* header_form_{};
  QPlainTextEdit* body_edit_{};
  /// Formatted rendering of an HTML body, shown instead of the editor while
  /// reading. Never fetches anything from the network.
  EMailBodyView* body_view_{};
  QStackedWidget* body_stack_{};
  QToolButton* body_mode_toggle_{};
  QToolButton* reply_button_{};
  QToolButton* reply_all_button_{};
  QToolButton* forward_button_{};
  QToolButton* forensic_toggle_{};
  QLabel* remote_content_notice_{};
  QTreeWidget* attachment_list_{};
  QToolButton* add_button_{};
  QToolButton* remove_button_{};
  QToolButton* save_button_{};
  QToolButton* save_all_button_{};
  /// Divider between the actions that derive a new message and the toggle
  /// that locks this one. Hidden with them when there is no message.
  QFrame* action_separator_{};
  QLabel* attachment_heading_{};
  QLabel* unsigned_notice_{};
  QLabel* status_banner_{};
  QTabWidget* tabs_{};
  /// The host's raw document editor, once adopted. Owned by the tab widget
  /// it was placed in; null when the host kept its own switcher instead.
  QWidget* source_view_{};
  EMailStructureView* structure_view_{};
  EMailHeaderView* header_view_{};
  EMailSecurityView* security_view_{};
};
