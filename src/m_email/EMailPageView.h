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

#include <QHash>
#include <QStringList>
#include <QWidget>

#include "EMailModel.h"
#include "EMailOutgoing.h"

class QLineEdit;
class QPlainTextEdit;
class QToolButton;
class QTreeWidget;
class QFormLayout;
class QFrame;
class QFont;
class QLabel;
class QTabWidget;
class EMailStructureView;
class EMailHeaderView;
class EMailSecurityView;
class EMailBodyView;
class QStackedWidget;
class QMenu;
class QPushButton;

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

/**
 * @brief Why an e-mail document cannot be modified, when it cannot.
 */
enum class EMailLockReason : uint8_t {
  kNONE = 0,
  kFORENSIC,   ///< the user locked this tab for inspection
  kPROTECTED,  ///< a signature covers the content, or it is ciphertext
};

/**
 * @brief What came of a host request to add content to the view.
 *
 * kNOT_HANDLED is the answer that lets the host fall back to what it would
 * have done anyway; kREFUSED means the view handled the request and said no,
 * and the host must NOT go on to do it itself.
 */
enum EMailAddContentResult : int {
  kEMAIL_ADD_NOT_HANDLED = 0,
  kEMAIL_ADD_DONE = 1,
  kEMAIL_ADD_REFUSED = 2,
};

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
   * @brief Uses the editor font the application was configured with.
   *
   * Optional half of the page/view contract. Applied only to the parts that
   * are the message itself -- the body editor and the rendered body -- and
   * never to the surrounding labels and tables, which are chrome and follow
   * the application font like every other widget.
   */
  void ApplyEditorFont(const QFont& font);  // NOLINT

  /**
   * @brief Takes the document's bytes as the message to show.
   *
   * Parsing only; nothing is written back, and the widget comes out clean.
   */
  /**
   * @brief Attaches an exported public key as an application/pgp-keys part.
   *
   * Optional half of the page/view contract. The host's "Append Public Key"
   * pastes armor into the document, which for a structured document means
   * pasting it into the middle of raw MIME. A view that declares this member
   * is handed the key instead and decides where it belongs.
   *
   * @return one of EMailAddContentResult
   */
  int AttachPublicKey(const QByteArray& key_data,  // NOLINT
                      const QString& suggested_name);

  /**
   * @brief Appends text to the message the view is presenting.
   *
   * For the host actions that append a fingerprint or a date: they mean "put
   * this in what I am writing", which for a structured document is the body,
   * not the document.
   *
   * @return one of EMailAddContentResult
   */
  int AppendBodyText(const QString& text);  // NOLINT

  /**
   * @brief Which crypto operations apply to this message AS IT IS NOW.
   *
   * Optional half of the page/view contract. The host knows which operations
   * a tab TYPE supports; only the view knows which of them mean anything for
   * the message actually open -- there is nothing to decrypt in a plain one,
   * and nothing to verify in an unsigned one.
   *
   * Names are the ones SignalCryptoOperationRequested() uses. An empty list
   * means nothing applies, not "no opinion".
   */
  QStringList AvailableCryptoOperations();  // NOLINT

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
   * @brief The sendable, frozen form of this message.
   *
   * Orchestration only: it flushes the header widgets, decides whether the
   * document may be reused verbatim, and hands the pieces to FreezeOutgoing(),
   * which owns every actual rule about identity, byte preservation and
   * envelopes. Nothing about what makes a message sendable is decided here --
   * that all lives in EMailOutgoing.cpp, where it can be tested without a
   * widget.
   *
   * It has to be a member because only the view knows whether it has unsaved
   * edits and owns the bytes as they were loaded.
   *
   * @param out receives the frozen message
   * @return false when the message cannot be sent as it stands
   */
  bool BuildOutgoing(EMailOutgoingMessage& out);  // NOLINT

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

  /**
   * @brief Asks the host to run a crypto operation on this tab's document.
   *
   * Optional half of the page/view contract. @p operation is one of
   * "encrypt", "decrypt", "sign", "verify", "encrypt_sign" or
   * "decrypt_verify" -- the same six the host already routes per tab type.
   * The view neither performs nor knows how to perform any of them; this only
   * says which one the user asked for, from inside the message rather than
   * from the menu bar.
   */
  void SignalCryptoOperationRequested(const QString& operation);

  /**
   * @brief Emitted when AvailableCryptoOperations() would answer differently.
   *
   * A decrypt turns a message nobody could read into an ordinary one, which
   * changes what may be done to it. Without this the menu bar would keep
   * describing the message as it was when the tab was last switched to.
   */
  void SignalCryptoOperationsChanged();

 protected:
  /// Accepts a drag only when it carries files and the document may change.
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dragMoveEvent(QDragMoveEvent* event) override;
  /// Attaches dropped files.
  void dropEvent(QDropEvent* event) override;
  /// Refreshes the address hints when an address field is entered, so keys
  /// imported since the tab opened are offered too.
  auto eventFilter(QObject* watched, QEvent* event) -> bool override;

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
  /// Unlocks or re-locks the adopted raw editor.
  void slot_toggle_raw_edit(bool on);
  /// Lifts the outermost signature off the message, leaving an ordinary one.
  void slot_remove_protection_layer();
  /// Freezes the message and opens the send dialog. Reads the document only.
  void slot_send_message();

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
  /// Why the document may not be modified, if it may not.
  [[nodiscard]] auto content_lock() const -> EMailLockReason;
  /// Applies content_lock() to every widget that can modify the document.
  /// The single place that decides what is editable, so the two reasons a
  /// message can be locked cannot drift apart.
  void apply_content_lock();
  /// Makes the lock visible rather than merely effective.
  ///
  /// A read-only QLineEdit is drawn exactly like an editable one, so without
  /// this the user meets a form that silently swallows every keystroke. Locked
  /// fields lose their frame and their input background and become what they
  /// actually are: the message, displayed.
  void style_as_locked(bool locked);
  /// True when the content may be changed. Otherwise explains why not, and
  /// for a signed message offers to remove the signature, then returns false.
  auto refuse_when_locked(const QString& what) -> bool;
  /// Keeps the Raw Source tab's notice and unlock control in step.
  void refresh_raw_lock_ui();
  /// Builds the panel shown in place of a body that is still ciphertext.
  auto build_locked_panel() -> QWidget*;
  /// Writes that panel from what is knowable without decrypting.
  void refresh_locked_panel();
  /// Rebuilds the security button's menu for the current state.
  void rebuild_security_menu();
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
  /// Writes what the message IS onto the security button, and rebuilds the
  /// menu of what can be done about it. The button keeps itself fitted to
  /// whatever room the action row leaves it.
  void refresh_security_button();
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
  /// Enables Send when the message could actually go out, and says why not
  /// when it could not. Never hides the button.
  void refresh_send_state();
  /// Enables or disables one action, and when disabled puts @p why in its
  /// tooltip in place of what it normally says. Never changes visibility.
  void set_action_available(QToolButton* button, bool available,
                            const QString& why);
  /// Offers the keyring's addresses as hints on the address fields.
  void install_address_hints();
  /// Updates the short note about how the body is being shown.
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
  /// Bytes produced by lifting a protected layer off the message, waiting to
  /// be handed to the host verbatim. Kept apart from last_source_ so
  /// SaveToSource() can tell "taken out of the original" from "built from the
  /// fields" -- only the latter may go through BuildMimeEML.
  QByteArray pending_replacement_;
  bool dirty_{false};
  bool loading_{false};
  /// When set, this document cannot be modified or reserialized at all.
  bool forensic_{false};
  /// Whether this tab was opened on a message someone else produced, as
  /// opposed to a draft being written here. Set once, when the document is
  /// loaded, and deliberately NOT cleared by re-serialization: a received
  /// message stays a received message after the user edits it.
  ///
  /// Distinct from @ref source_is_original_, which is about the BYTES being
  /// pristine. The two answer different questions and diverge the moment an
  /// opened message is edited.
  bool document_is_received_{false};
  /// Whether @ref last_source_ holds bytes this program did NOT produce --
  /// a message as it was loaded, or an entity lifted out of one. Only those
  /// may be submitted verbatim; bytes our own serializer wrote carry nothing
  /// worth preserving, and reusing them costs the message its Message-ID.
  bool source_is_original_{false};

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
  QToolButton* reply_button_{};
  QToolButton* reply_all_button_{};
  QToolButton* forward_button_{};
  QToolButton* send_button_{};
  /// What each action says when it is available, captured the first time it
  /// is disabled so the explanation can be swapped in and back out.
  QHash<QToolButton*, QString> action_tooltips_;
  QToolButton* forensic_toggle_{};
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
  QToolButton* security_button_{};
  QMenu* security_menu_{};
  /// Says why the message cannot be edited, when it cannot.
  QLabel* locked_notice_{};
  /// Carries locked_notice_ and its icon as one tinted strip, so the reason
  /// reads as the message's state rather than as a footnote under the body.
  QFrame* locked_banner_{};
  QLabel* locked_banner_icon_{};
  /// Shown in place of the body while the message is still ciphertext.
  QWidget* locked_panel_{};
  QLabel* locked_heading_{};
  QLabel* locked_recipients_{};
  QPushButton* locked_decrypt_button_{};
  QTabWidget* tabs_{};
  /// The host's raw document editor, once adopted. Owned by the tab widget
  /// it was placed in; null when the host kept its own switcher instead.
  QWidget* source_view_{};
  /// The Raw Source tab itself: the adopted editor plus the row that says
  /// whether it may be written to. Never the editor alone, or the tab-change
  /// handler would stop recognising it.
  QWidget* raw_tab_{};
  QLabel* raw_notice_{};
  QToolButton* raw_unlock_button_{};
  /// Whether the user has deliberately asked to edit the raw document.
  /// Read-only is the default: these are the octets a signature covers.
  bool raw_unlocked_{false};
  EMailStructureView* structure_view_{};
  EMailHeaderView* header_view_{};
  EMailSecurityView* security_view_{};
};
