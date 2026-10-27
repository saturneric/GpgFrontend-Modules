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

#include "WebEngineScheme.h"

#include <QByteArrayList>
#include <QWebEngineUrlScheme>

auto RegisterPartScheme() -> bool {
  const QByteArray name(GF_HTML_PART_SCHEME);
  // A second activation in the same process finds it from the first.
  if (QWebEngineUrlScheme::schemeByName(name).name() == name) return true;

  QWebEngineUrlScheme scheme(name);
  // gfpart://<session>/<cid>: the session is the host, and nothing else is
  // an authority.
  scheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
  // Secure, so the document is a secure context that never "upgrades" its
  // parts. Not local, not CORS, no fetch API, and never exempt from the
  // document's Content-Security-Policy: the scheme grants nothing.
  scheme.setFlags(QWebEngineUrlScheme::SecureScheme);
  QWebEngineUrlScheme::registerScheme(scheme);
  return QWebEngineUrlScheme::schemeByName(name).name() == name;
}

void ApplyEngineSwitches() {
  static const QByteArrayList kSwitches = {
      "--disable-background-networking",
      "--disable-component-update",
      "--disable-domain-reliability",
      "--dns-prefetch-disable",
      "--no-pings",
  };
  auto flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
  for (const auto& s : kSwitches) {
    if (flags.contains(s)) continue;
    if (!flags.isEmpty()) flags += ' ';
    flags += s;
  }
  qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);
}
