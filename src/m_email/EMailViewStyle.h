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

#include <QColor>
#include <QPalette>
#include <QWidget>

#include "GFModuleCommonUtils.hpp"
#include "GFSDKUI.h"

/**
 * @brief The application's own colours, reached through the SDK.
 *
 * Shared by every view in this module so the e-mail tab follows the user's
 * theme and reads as part of the program rather than as a plug-in that picked
 * its own greys. A getter returning 0 means the widget was not usable, so the
 * palette's own colour is the honest fallback.
 *
 * Note the convention the host documents in UIStyle.h: a negative state is
 * de-emphasised rather than red. Red belongs to irreversible acts and to
 * secrets travelling in the clear, not to "this part is unsigned".
 */
inline auto EMailThemeColor(QWidget* w, uint32_t (*getter)(void*)) -> QColor {
  const auto rgba = getter(w);
  return rgba == 0 ? w->palette().color(QPalette::WindowText)
                   : QColor::fromRgba(rgba);
}

inline auto EMailMutedColor(QWidget* w) -> QColor {
  return EMailThemeColor(w, &GFUIMutedTextColor);
}

inline auto EMailWarningColor(QWidget* w) -> QColor {
  return EMailThemeColor(w, &GFUIWarningColor);
}

inline auto EMailAccentColor(QWidget* w, bool positive) -> QColor {
  const auto rgba = GFUIAccentColor(w, positive ? 1 : 0);
  return rgba == 0 ? w->palette().color(QPalette::WindowText)
                   : QColor::fromRgba(rgba);
}

/// A size written the way the rest of the application writes it.
inline auto EMailHumanSize(qint64 bytes) -> QString {
  return UnStrDup(GFUIHumanSize(bytes));
}
