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

#include <QString>

#include "EMailAccountModel.h"

class QComboBox;

/**
 * @file
 * @brief The connection-type control, shared by the wizard and the editor.
 *
 * Its strings keep the settings page's translation context, where they have
 * always been translated.
 */

/**
 * @brief Fills @p combo with the security choices offered for @p host.
 *
 * "None" appears only for a loopback host, the only place the model permits it
 * (MailHostAllowsCleartext). A stored cleartext account must be SHOWN as what
 * it is, or the first keystroke anywhere would write something else back.
 */
void EMailFillSecurityCombo(QComboBox* combo, const QString& host);

/// The choice currently selected in @p combo.
auto EMailSecurityOf(const QComboBox* combo) -> MailTlsMode;

/// Selects @p mode, or the safest choice when @p mode is not on offer.
void EMailSelectSecurity(QComboBox* combo, MailTlsMode mode);

/// Rebuilds the choices for a new @p host, keeping the selection if it is
/// still offered and falling back to the safest one if not.
void EMailRefreshSecurityCombo(QComboBox* combo, const QString& host);

/// "imap.example.org · TLS · port 993", or a placeholder when there is no
/// host.
auto EMailServerSummary(const QString& host, MailTlsMode mode, bool imap)
    -> QString;
