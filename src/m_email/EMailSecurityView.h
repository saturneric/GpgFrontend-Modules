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
   * @param recipients the addressed/encrypted cross-check
   * @param addresses every address in the message, for the key lookup
   * @param channel GPG context channel used for that lookup
   */
  void SetMessage(EMailSecurityState state,
                  const QList<EMailSignatureRegion>& regions,
                  const QList<EMailSignatureResult>& results,
                  const QList<EMailRecipientRow>& recipients,
                  const QStringList& addresses, int channel,
                  const QList<EMailFinding>& findings);

  void Clear();

 private:
  void build_ui();
  void add_signature_section(const QList<EMailSignatureRegion>& regions,
                             const QList<EMailSignatureResult>& results);
  void add_recipient_section(const QList<EMailRecipientRow>& recipients);
  void add_key_section(const QStringList& addresses, int channel);
  void add_findings_section(const QList<EMailFinding>& findings);
  /// A section header row; returns it so children can be hung off it.
  auto add_group(const QString& title) -> QTreeWidgetItem*;

  QLabel* headline_{};
  QTreeWidget* tree_{};
};
