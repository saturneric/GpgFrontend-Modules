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

#include <GFModuleNativeWidget.h>
#include <GFModuleWebEditor.h>

#include <QJsonObject>
#include <QWidget>
#include <functional>
#include <memory>

#include "NoteBridge.h"
#include "NoteCrypto.h"
#include "NoteModel.h"
#include "NotePersistence.h"
#include "NoteProtection.h"

class QLabel;
class QLockFile;
class QMenu;
class QStackedWidget;
class QTimer;
class QToolButton;
class QTreeWidget;
class QUrl;
class QVBoxLayout;

namespace gf::notes {

class NoteGlyph;
class NoteStatusButton;

/**
 * @brief One note, in a tab of its own (persistence = "module").
 *
 * A note is its text, open in the editor page, and optionally the last
 * encrypted snapshot of it. Everything goes through the Host's toolbar: an
 * encrypted file waits for Decrypt; Encrypt makes a snapshot for the keys
 * checked in the Key ToolBox, or with a passphrase when none are; Decrypt
 * then takes it off again; Save writes the snapshot when there is one and
 * the text as a plain TextPack when there is not, and never encrypts.
 * Unsaved work is kept the way every other tab keeps it: in the Host's
 * recovery cache (Save()/Load()). Closing waits for anything in flight, then
 * forgets.
 */
class NotePageView : public QWidget, public gf::ui::DocumentWidget {
  Q_OBJECT

 public:
  explicit NotePageView(QWidget* parent = nullptr);
  ~NotePageView() override;

  /// Removes revisions no running note tab owns (left by a crash; they are
  /// encrypted, and the Host's recovery cache holds the same work).
  static void RemoveOrphanedRevisions();

  // DocumentWidget
  void Load(const QByteArray& bytes) override;
  auto Save() -> std::optional<QByteArray> override;
  [[nodiscard]] auto IsDirty() const -> bool override;
  [[nodiscard]] auto CryptoOperations() const
      -> std::optional<uint32_t> override;
  [[nodiscard]] auto SuggestedFileName() const -> QString override;
  void WipeContent() override;
  auto OpenPath(const QString& path) -> bool override;
  auto SaveRequested(bool save_as) -> bool override;
  auto CloseRequested() -> bool override;
  auto CloseDecided(bool save) -> std::optional<bool> override;
  void FindRequested() override;
  auto EncryptRequested() -> bool override;
  auto DecryptRequested() -> bool override;
  [[nodiscard]] auto CryptoOperationTip(uint32_t op) const
      -> std::optional<QString> override;
  [[nodiscard]] auto SaveAllowed() const -> bool override;

 protected:
  void changeEvent(QEvent* event) override;

 private:
  enum class State : uint8_t {
    kNEW,
    kENCRYPTED,   ///< an encrypted file, waiting for Decrypt
    kDECRYPTING,  ///< Decrypt is running
    kOPEN,
    kUNAVAILABLE,
  };

  void build_ui();
  /// The encrypted file, and why the last Decrypt failed, if it did.
  void show_encrypted(const QString& status = {});
  /// The keys the encrypted file names, and what that means for the user.
  void fill_readers();
  void show_open();
  void show_unavailable(const QString& why);
  void refresh_strip();
  void set_status(const QString& text, const QString& detail = {});
  /// Says @p text -- a few words -- on the status button for a while, with
  /// @p detail in its tooltip; a @p sticky one (a failure) stays until the
  /// next Encrypt, Save or Decrypt.
  void notice(const QString& text, bool warning, bool sticky = false,
              const QString& detail = {});
  void clear_notice();
  /// The status button's menu: what the state means, and what to do next.
  void rebuild_status_menu();
  void restyle();
  /// The application's colours and words, for the editor page.
  void post_ui();

  /// The note file's bytes; on failure the user is told why.
  auto read_file(const QString& path, QByteArray* bytes) -> bool;
  /// Takes @p bytes as the encrypted note, once they are an encrypted one.
  auto take_encrypted(const QByteArray& bytes) -> bool;
  /// Decrypt the encrypted file, as the Host's Decrypt would.
  void decrypt_file();
  void opened(const QByteArray& zip);
  /// Show @p note; @p unsaved when it comes from the recovery cache.
  void open_note(const Note& note, bool unsaved);
  void start_editor();
  void on_message(const QByteArray& json);
  void on_ready();
  void on_loaded(const QJsonObject& msg);
  void on_link(const QUrl& url);
  void on_notice(const QString& text);
  void on_image(const QJsonObject& msg);
  void post_json(const QJsonObject& message);
  /// Tell the page the image @p token stands for is not coming, and why.
  void reject_image(const QString& token, const QString& why);

  /// Encrypt the text as it is now into a new snapshot, for the keys
  /// checked in the Key ToolBox, or with a passphrase when none are.
  void encrypt();
  /// Drop the snapshot; the text stays exactly as it is.
  void remove_encryption();
  /// Where Save writes: the note's file, or one the user picks.
  auto choose_destination(bool save_as, bool encrypted) -> QString;
  /// Check the snapshot's keys in the Key ToolBox (none, for a passphrase).
  void restore_checked_keys();
  /// Read which keys the Key ToolBox has checked now; the strip says when
  /// they are not the snapshot's.
  void refresh_checked_keys();
  auto revision_dir() -> QString;
  /// Keep only @p current of this tab's revision files.
  void keep_only_revision(const QString& current);
  /// Names for the snapshot's readers, worked out once per snapshot.
  void describe_readers();

  /// Ask the page for the document; @p done gets a snapshot, or nullopt.
  void request_snapshot(std::function<void(std::optional<Snapshot>)> done);
  void ask_for_snapshot();
  void finish_snapshot(const QJsonObject& reply);
  auto build_snapshot(const QJsonObject& reply) -> std::optional<Snapshot>;
  void run_job(const Job& job);
  void job_done(const Job& job, bool ok, const QString& why);
  auto make_persistence() -> NotePersistence;
  void autosave_tick();

  /// Wait, keeping the application responsive, until @p ready or a minute.
  void wait_until(const std::function<bool()>& ready);
  auto ask_and_close() -> bool;
  /// Save (@p save) or discard, as already decided; true: it may close.
  auto close_as_decided(bool save) -> bool;
  void forget();

  State state_ = State::kNEW;
  int channel_ = 0;
  QString path_;  ///< where explicit saves go; empty until first save

  QByteArray file_;           ///< the ciphertext, until it is decrypted
  FileReaders file_readers_;  ///< who file_ is encrypted for
  Note note_;
  QString front_matter_;
  QStringList managed_;  ///< assets this application added
  QString note_id_;

  bool plain_ = false;  ///< the file is a TextPack saved without encryption

  /// The keys the Key ToolBox had checked when last asked.
  std::optional<ProtectionConfig> checked_;
  bool checked_asked_ = false;  ///< a KeysChecked is on its way
  int checked_tick_ = 0;
  /// Counts Remove Encryption, so an Encrypt begun before one is dropped.
  quint64 removals_ = 0;
  QString described_revision_;  ///< the snapshot the names below are for
  QString readers_text_;
  QStringList readers_lines_;  ///< one per reader, at most ten and a count

  gf::ui::WebEditor editor_;
  NoteBridge bridge_;
  NotePersistence persistence_;
  bool notified_modified_ = false;
  bool snapshot_in_progress_ = false;
  int snapshot_retries_ = 0;
  QList<std::function<void(std::optional<Snapshot>)>> snapshot_waiters_;
  QString last_markdown_;      ///< what the page last handed over
  quint64 page_gen_base_ = 0;  ///< our generation when the page last loaded
  /// The latest snapshot, for the Host's recovery cache (see Save()).
  std::shared_ptr<const void> recovery_;
  quint64 recovery_gen_ = 0;
  quint64 handed_gen_ = 0;  ///< what the Host was last given
  int jobs_in_flight_ = 0;  ///< a decrypt or images being stored

  QString revision_dir_;                      ///< this tab's revisions
  std::unique_ptr<QLockFile> revision_lock_;  ///< says they are in use
  bool encrypting_ = false;                   ///< an Encrypt is under way

  QStackedWidget* stack_ = nullptr;
  // Encrypted
  QLabel* encrypted_title_ = nullptr;
  QLabel* encrypted_facts_ = nullptr;
  QLabel* encrypted_hint_ = nullptr;
  QTreeWidget* readers_table_ = nullptr;
  QLabel* encrypted_status_ = nullptr;
  // Open
  QWidget* header_ = nullptr;
  NoteStatusButton* status_button_ = nullptr;  ///< state; menu: next steps
  QMenu* status_menu_ = nullptr;
  QLabel* save_state_ = nullptr;
  QString notice_;  ///< said on the status button instead of the state
  QString notice_detail_;
  bool notice_warning_ = false;
  QTimer* notice_timer_ = nullptr;
  QString content_note_;  ///< what the editor cannot show of this note
  QWidget* slot_ = nullptr;
  // Unavailable
  QLabel* unavailable_ = nullptr;
  QTimer* tick_ = nullptr;
  bool restyling_ = false;
};

}  // namespace gf::notes
