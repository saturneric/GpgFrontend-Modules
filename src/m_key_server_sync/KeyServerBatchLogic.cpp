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

#include "KeyServerBatchLogic.h"

#include <QCoreApplication>
#include <QSet>

namespace KeyServerBatchLogic {

namespace {

/// The failures, one per line, under the headline.
auto WithFailures(QString headline, const QStringList& failures) -> QString {
  if (failures.isEmpty()) return headline;
  headline += QStringLiteral("\n\n");
  headline += QCoreApplication::translate("GTrC", "Failed:");
  for (const auto& f : failures) headline += QStringLiteral("\n") + f;
  return headline;
}

}  // namespace

auto Normalise(const std::optional<BatchKey>& key,
               const std::optional<QList<BatchKey>>& keys) -> QList<BatchKey> {
  QList<BatchKey> all;
  if (key.has_value()) all.append(*key);
  if (keys.has_value()) all.append(*keys);

  QList<BatchKey> out;
  QSet<QString> seen;
  for (const auto& k : all) {
    const auto fpr = k.fingerprint.trimmed().toUpper();
    if (fpr.isEmpty() || seen.contains(fpr)) continue;
    seen.insert(fpr);
    out.append(k);
  }
  return out;
}

auto JoinForImport(const QList<QByteArray>& blocks) -> QByteArray {
  QByteArray out;
  for (const auto& b : blocks) {
    if (b.trimmed().isEmpty()) continue;
    out.append(b);
    if (!b.endsWith('\n')) out.append('\n');
  }
  return out;
}

auto RefreshSummary(int fetched, int total, const QStringList& failures)
    -> QString {
  return WithFailures(
      QCoreApplication::translate("GTrC",
                                  "%1 of %2 keys were fetched from the key "
                                  "server.")
          .arg(fetched)
          .arg(total),
      failures);
}

auto PublishSummary(int published, int total, const QString& host,
                    const QStringList& failures) -> QString {
  return WithFailures(
      QCoreApplication::translate("GTrC",
                                  "%1 of %2 keys were published to the key "
                                  "server %3.")
          .arg(published)
          .arg(total)
          .arg(host),
      failures);
}

}  // namespace KeyServerBatchLogic
