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

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;

/**
 * @brief What is known about a message's signatures, recipients and keys.
 *
 * Three separations are load-bearing here and the layout exists to keep them
 * visible:
 *
 *  - A signature REGION is a range of MIME bytes; a signature RESULT is what
 *    an engine said about it. One region can have several results or none, so
 *    results are grouped under their region rather than listed flat.
 *  - What a message DECLARES (micalg) and what its signature actually used are
 *    shown side by side, and a disagreement is reported rather than resolved.
 *  - A key's USABILITY and its IDENTITY BINDING are different questions. A
 *    usable key belonging to the wrong person is the case worth catching, and
 *    a single combined verdict would hide exactly that.
 */
/**
 * @brief Names the sections of the Security tab, for RevealSection().
 *
 * Plain strings rather than an enum so the two files agree on the identity of
 * a section without either having to see the other's headings -- the headings
 * are translated, and matching on those would break in every language but
 * English.
 */
constexpr auto kSectionSignatures = "signatures";
constexpr auto kSectionRecipients = "recipients";
constexpr auto kSectionKeys = "keys";
constexpr auto kSectionFindings = "findings";

class EMailSecurityView : public QWidget {
  Q_OBJECT

 public:
  explicit EMailSecurityView(QWidget* parent = nullptr);

  /**
   * @brief Shows what parsing and any completed operation established.
   *
   * @param state structural classification
   * @param regions signed byte ranges found while parsing
   * @param results verification results, each already carrying its region_id
   * @param verify_state how far verification has got, so an empty @p results
   *        can be reported as "found nothing" rather than as "not tried"
   * @param recipients the addressed/encrypted cross-check
   * @param addresses every address in the message, for the key lookup
   * @param channel GPG context channel used for that lookup
   * @param findings what inspection flagged about the message
   * @param verify_state how far verification has got, so an empty @p results
   *        can be reported as "found nothing" rather than as "not tried yet"
   * @param message_carries_key whether the message itself has an OpenPGP key
   *        part, which is what makes "import it" an offer this view can make
   */
  void SetMessage(EMailSecurityState state,
                  const QList<EMailSignatureRegion>& regions,
                  const QList<EMailSignatureResult>& results,
                  const QList<EMailRecipientRow>& recipients,
                  const QStringList& addresses, const QString& from,
                  int channel,
                  const QList<EMailFinding>& findings,
                  EMailVerifyState verify_state, bool message_carries_key);

  void Clear();

 signals:
  /// The user asked for the signatures to be verified again. Emitted only
  /// when a verification has already been attempted: before that there is
  /// nothing to repeat, and the Security tab runs one on its own.
  void SignalVerifyAgainRequested();

  /// The user asked for the OpenPGP key parts carried by this message to be
  /// imported. The view knows one exists but not where it is, so the page
  /// that holds the parsed message does the importing.
  void SignalImportMessageKeysRequested();

 public:
  /// Replaces the table with @p text, for when there is nothing to tabulate.
  /// Passing an empty string is not a way to clear the view; use Clear().
  void ShowNotice(const QString& text);

  /// Scrolls to and selects the section named by one of the kSection*
  /// constants. Does nothing when this message has no such section, which is
  /// the honest outcome: there is no row to point at.
  void RevealSection(const QString& section);

 protected:
  void changeEvent(QEvent* event) override;

 private:
  void build_ui();
  /// The single place this view's colours are decided. Must not touch fonts.
  void apply_colors();
  void add_signature_section(const QList<EMailSignatureRegion>& regions,
                             const QList<EMailSignatureResult>& results,
                             EMailVerifyState verify_state);
  void add_recipient_section(const QList<EMailRecipientRow>& recipients);
  void add_key_section(const QStringList& addresses, int channel);
  void add_findings_section(const QList<EMailFinding>& findings);
  /// A section header row; returns it so children can be hung off it.
  /// @p id is one of the kSection* constants, stored on the row so
  /// RevealSection() can find it again without matching translated text.
  auto add_group(const QString& title, const QString& id) -> QTreeWidgetItem*;
  /// The tree's own menu, for the row at @p pos.
  void show_row_menu(const QPoint& pos);

  /// The From address, for the signer consistency check. A signature that
  /// names a different address than the message claims to come from is what a
  /// transplanted signature looks like.
  QString from_;

  /// Kept because the menu's offers depend on them and SetMessage's arguments
  /// are not otherwise retained.
  EMailSecurityState state_{EMailSecurityState::kPLAIN};
  EMailVerifyState verify_state_{EMailVerifyState::kNOT_ATTEMPTED};
  bool message_carries_key_{false};
  bool has_regions_{false};
  QLabel* headline_{};
  QTreeWidget* tree_{};
  /// Shown in the tree's place when the tree would be empty.
  QLabel* empty_notice_{};
};
