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

#include "SemVer.h"

#include <GFSDKApp.h>

#include <QRegularExpression>

auto SemVer::Parse(const QString& text) -> std::optional<SemVer> {
  static const QRegularExpression kRe(
      R"(^[vV]?(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*))"
      R"((?:-((?:0|[1-9]\d*|\d*[A-Za-z-][0-9A-Za-z-]*))"
      R"((?:\.(?:0|[1-9]\d*|\d*[A-Za-z-][0-9A-Za-z-]*))*))?)"
      R"((?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$)");
  const auto m = kRe.match(text.trimmed());
  if (!m.hasMatch()) return std::nullopt;

  SemVer v;
  bool ok = true;
  v.major = m.captured(1).toInt(&ok);
  if (!ok) return std::nullopt;
  v.minor = m.captured(2).toInt(&ok);
  if (!ok) return std::nullopt;
  v.patch = m.captured(3).toInt(&ok);
  if (!ok) return std::nullopt;
  if (!m.captured(4).isEmpty()) v.prerelease = m.captured(4).split(u'.');
  v.build = m.captured(5);
  return v;
}

namespace {

auto IsNumeric(const QString& id) -> bool {
  for (const auto c : id) {
    if (!c.isDigit()) return false;
  }
  return !id.isEmpty();
}

auto CompareIdentifier(const QString& a, const QString& b) -> int {
  const bool na = IsNumeric(a);
  const bool nb = IsNumeric(b);
  if (na && nb) {
    // By length, then digit by digit: no leading zeros (Parse), and no
    // number too long for an integer reads as 0.
    if (a.size() != b.size()) return a.size() < b.size() ? -1 : 1;
    const int c = QString::compare(a, b, Qt::CaseSensitive);
    return c < 0 ? -1 : (c > 0 ? 1 : 0);
  }
  if (na != nb) return na ? -1 : 1;  // a number sorts below a word
  const int c = QString::compare(a, b, Qt::CaseSensitive);  // ASCII order
  return c < 0 ? -1 : (c > 0 ? 1 : 0);
}

}  // namespace

auto SemVerCompare(const QString& a, const QString& b) -> int {
  const auto x = SemVer::Parse(a);
  const auto y = SemVer::Parse(b);
  if (!x || !y) {
    return GFCompareSoftwareVersion(a.toUtf8().constData(),
                                    b.toUtf8().constData());
  }

  for (const auto& [l, r] :
       {std::pair{x->major, y->major}, std::pair{x->minor, y->minor},
        std::pair{x->patch, y->patch}}) {
    if (l != r) return l < r ? -1 : 1;
  }

  // A final release sorts above every prerelease of it.
  if (x->IsPrerelease() != y->IsPrerelease()) {
    return x->IsPrerelease() ? -1 : 1;
  }
  const auto n = std::min(x->prerelease.size(), y->prerelease.size());
  for (qsizetype i = 0; i < n; ++i) {
    if (const int c = CompareIdentifier(x->prerelease[i], y->prerelease[i])) {
      return c;
    }
  }
  if (x->prerelease.size() != y->prerelease.size()) {
    return x->prerelease.size() < y->prerelease.size() ? -1 : 1;
  }
  return 0;
}
