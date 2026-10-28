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
class QPushButton;
class QStackedWidget;
class QTimer;
class QToolButton;
class QUrl;
class QVBoxLayout;

namespace gf::notes {

class NoteGlyph;

/**
 * @brief One encrypted note, in a tab of its own (persistence = "module").
 *
 * Locked: the file's ciphertext and what its packets say. Open: the
 * decrypted pack in memory and the editor page. Encrypt makes a revision,
 * Save moves it to the note's file, or writes a never-encrypted note as a
 * plain TextPack. Unsaved work is kept the way every other tab keeps it: in
 * the Host's recovery cache (Save()/Load()). Locking and closing wait for
 * anything in flight, then forget.
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
  void FindRequested() override;
  auto EncryptRequested() -> bool override;

 protected:
  void changeEvent(QEvent* event) override;

 private:
  enum class State : uint8_t { kNEW, kLOCKED, kUNLOCKING, kOPEN, kUNAVAILABLE };

  /// How many revisions a note keeps in its temporary folder.
  static constexpr int kKeptRevisions = 3;

  void build_ui();
  void show_locked(const QString& status = {});
  void show_open();
  void show_unavailable(const QString& why);
  void refresh_strip();
  void set_status(const QString& text, const QString& detail = {});
  /// A message above the note; @p transient ones go by themselves.
  void show_banner(const QString& text, bool warning, bool transient = false);
  void restyle();
  /// The application's colours and words, for the editor page.
  void post_ui();

  /// The note file's bytes; on failure the user is told why.
  auto read_file(const QString& path, QByteArray* bytes) -> bool;
  /// Takes @p bytes as the locked note, once they are an encrypted one.
  auto sniff_file(const QByteArray& bytes) -> bool;
  void unlock();
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

  void assess();
  /// The protection for the next revision: the keys checked in the Key
  /// ToolBox, or, with none checked, a passphrase. Asks before anyone loses
  /// access. @p done gets it, or nothing when the user stopped.
  void choose_protection(
      std::function<void(std::optional<ProtectionConfig>)> done);
  void adopt_protection(const ProtectionConfig& config);
  void record_confirmation();
  /// Encrypt into a new revision; then, when @p then_save, save it.
  void encrypt(bool then_save, bool save_as);
  /// Where Save puts the revision: the note's file, or one the user picks.
  auto choose_destination(bool save_as, bool encrypted) -> QString;
  /// Check this note's keys in the Key ToolBox (none, for a passphrase).
  void restore_checked_keys();
  auto revision_dir() -> QString;
  void prune_revisions();

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
  void lock();
  void forget();

  State state_ = State::kNEW;
  int channel_ = 0;
  QString path_;  ///< where explicit saves go; empty until first save

  QByteArray file_;  ///< the ciphertext, while locked
  QList<DetectedPacket> packets_;
  Note note_;
  QString front_matter_;
  QStringList managed_;  ///< assets this application added
  QString note_id_;

  bool plain_ = false;  ///< the file is a TextPack saved without encryption

  ProtectionConfig protection_;
  Assessment assessment_;
  bool protection_confirmed_ = false;
  quint64 rev_ = 0;

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
  bool encrypting_ = false;                   ///< an Encrypt is being prepared

  QStackedWidget* stack_ = nullptr;
  // Locked
  NoteGlyph* locked_glyph_ = nullptr;
  QLabel* locked_title_ = nullptr;
  QLabel* locked_facts_ = nullptr;
  QPushButton* unlock_ = nullptr;  ///< the preferred way

  QLabel* locked_status_ = nullptr;
  QLabel* recipients_heading_ = nullptr;
  QVBoxLayout* recipients_ = nullptr;
  QLabel* recipients_note_ = nullptr;
  // Open
  QWidget* header_ = nullptr;
  QToolButton* protection_button_ = nullptr;
  QLabel* save_state_ = nullptr;
  QToolButton* lock_ = nullptr;
  QWidget* banner_ = nullptr;
  NoteGlyph* banner_glyph_ = nullptr;
  QLabel* banner_text_ = nullptr;
  bool banner_warning_ = false;
  QTimer* banner_timer_ = nullptr;
  QWidget* slot_ = nullptr;
  // Unavailable
  QLabel* unavailable_ = nullptr;
  QTimer* tick_ = nullptr;
  bool restyling_ = false;
};

}  // namespace gf::notes
