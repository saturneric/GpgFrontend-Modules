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
class QPushButton;
class QToolButton;
class QTreeWidget;
class QFormLayout;
class QLabel;

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
class EMailPageView : public QWidget {
  Q_OBJECT

 public:
  explicit EMailPageView(QWidget* parent = nullptr);

 public slots:
  /**
   * @brief Takes the document's bytes as the message to show.
   *
   * Parsing only; nothing is written back, and the widget comes out clean.
   */
  void LoadFromSource(const QByteArray& source);  // NOLINT

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

 private slots:
  void slot_add_attachment();
  void slot_remove_attachment();
  void slot_save_attachment();
  void slot_save_all_attachments();
  void slot_selection_changed();

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
  /// Shows or hides the Cc and Bcc rows. Never clears them: collapsing a row
  /// is a view choice, not a decision to discard what is in it.
  void set_cc_bcc_visible(bool visible);

  EMailMetaData message_;
  bool dirty_{false};
  bool loading_{false};

  QLineEdit* from_edit_{};
  QLineEdit* to_edit_{};
  QLineEdit* cc_edit_{};
  QLineEdit* bcc_edit_{};
  QLineEdit* subject_edit_{};
  QToolButton* cc_bcc_toggle_{};
  QFormLayout* header_form_{};
  QPlainTextEdit* body_edit_{};
  QTreeWidget* attachment_list_{};
  QPushButton* add_button_{};
  QPushButton* remove_button_{};
  QPushButton* save_button_{};
  QPushButton* save_all_button_{};
  QLabel* attachment_heading_{};
  QLabel* unsigned_notice_{};
};
