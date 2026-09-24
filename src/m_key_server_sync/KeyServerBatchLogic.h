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
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

/**
 * @file KeyServerBatchLogic.h
 * @brief The decisions a batch of key-server operations makes, without the
 * network or the SDK, so they can be tested on their own.
 */
namespace KeyServerBatchLogic {

/// One key a batch acts on: the part of the Host's key reference it needs.
struct BatchKey {
  qint64 channel = 0;
  QString key_id;
  QString fingerprint;
};

/**
 * @brief The keys a command was given, as one list.
 *
 * A command is given either one key (the key details dialog) or a selection
 * (a key list). Both together are merged; a key named twice, by fingerprint,
 * is acted on once, in the order it was first named. Keys without a
 * fingerprint are dropped: there is nothing to fetch or publish them by.
 */
auto Normalise(const std::optional<BatchKey>& key,
               const std::optional<QList<BatchKey>>& keys) -> QList<BatchKey>;

/**
 * @brief Several fetched key blocks as one import payload.
 *
 * One import means one dialog for the whole batch. Each block ends in a
 * newline so the next one's armor header starts on a line of its own; empty
 * blocks are skipped.
 */
auto JoinForImport(const QList<QByteArray>& blocks) -> QByteArray;

/// What a lookup failure says when the server simply does not have the key:
/// not an error, and never the transport's own wording.
auto NotFoundText() -> QString;

/// Whether @p error is NotFoundText().
auto IsNotFound(const QString& error) -> bool;

/// What a finished refresh says, e.g. "3 of 4 keys fetched" plus failures.
auto RefreshSummary(int fetched, int total, const QStringList& failures)
    -> QString;

/// What a finished publish says, e.g. "2 of 2 keys published" plus failures.
auto PublishSummary(int published, int total, const QString& host,
                    const QStringList& failures) -> QString;

}  // namespace KeyServerBatchLogic
