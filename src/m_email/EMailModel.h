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

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

// vmime
#define VMIME_STATIC
#include <vmime/vmime.hpp>
// vmime extra
#include <vmime/contentDispositionField.hpp>
#include <vmime/contentTypeField.hpp>

/**
 * @brief One non-protocol part of a message.
 *
 * "Attachment" here means every part that is not the message body and not an
 * OpenPGP control part -- so it covers inline images as well as things the
 * user would call an attachment.
 */
struct EMailAttachment {
  QString filename;   ///< as it arrived; NOT safe to use as a path
  QString mime_type;  ///< e.g. "application/pdf"
  QString description;
  QString disposition;  ///< "attachment" or "inline"
  QByteArray data;      ///< decoded content

  /// application/pgp-keys, which the UI can offer to import rather than save.
  bool is_openpgp_key{false};

  /// Whether this part sits inside the multipart/signed subtree. Only those
  /// parts are covered by the signature; anything else arrived unauthenticated
  /// and must be shown as such, however trustworthy the rest of the message
  /// looks.
  bool inside_signed_part{false};
};

struct EMailMetaData {
  // Basic MetaData
  QString from;
  QStringList to;
  QStringList cc;
  QStringList bcc;
  QString subject;
  QDateTime datetime;
  QString micalg;
  QString reply_to;
  QString organization;

  // OpenPGP MetaData
  QString public_keys;
  QByteArray mime;
  QString mime_hash;
  QByteArray signature;
  QByteArray encrypted_data;

  // Content
  QByteArray body;            ///< decoded text body, UTF-8
  QString body_content_type;  ///< e.g. "text/plain"
  QList<EMailAttachment> attachments;
};