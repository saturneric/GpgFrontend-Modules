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

#include <GFModule.h>

#include <QAction>
#include <QApplication>
#include <QMainWindow>
#include <QMenu>
#include <QPointer>

#include "GFModuleIdentity.h"
#include "PGPInspectDialog.h"

namespace {

/// The ceiling on what is read out of a tab. Past this, the menu entry stays
/// greyed out rather than making every menu opening pay for a packet walk.
constexpr qint64 kMaxInspectSize = 16LL * 1024 * 1024;

/// The current tab's document, or an invalid one when there is nothing to
/// show. Cheap enough to run when the menu opens: the walk is a linear pass
/// over at most kMaxInspectSize bytes and decrypts nothing.
auto InspectCurrentTab() -> PGPInspectDocument {
  const auto bytes = PGPInspectCurrentTabBytes();
  if (bytes.isEmpty() || bytes.size() > kMaxInspectSize) return {};
  return PGPInspectBytes(bytes);
}

void RaiseInspectDialog(QWidget *parent) {
  const auto bytes = PGPInspectCurrentTabBytes();
  auto *dialog =
      new PGPInspectDialog(PGPInspectBytes(bytes), bytes.size(), parent);
  dialog->setModal(false);
  dialog->show();
}

}  // namespace

auto OnActivate() -> GFResult {
  // Nothing to register with the host: the module owns no settings page, no
  // tab view and no metatype. Its whole surface is one menu action, added
  // when the menu is mounted.
  return GFResult::Ok();
}

auto OnMainWindowMenuMounted(const GFEvent &event) -> GFEventResult {
  QMainWindow *main_window = nullptr;
  if (auto r = event.RequireGui("main_window", main_window); !r.ok) return r;

  QMenu *advance_menu = nullptr;
  if (auto r = event.RequireGui("advance_menu", advance_menu); !r.ok) return r;

  LOG_DEBUG("adding openpgp structure inspector to the advanced menu");

  QMetaObject::invokeMethod(
      QApplication::instance(),
      [=]() -> void {
        auto *action =
            new QAction(QCoreApplication::translate(
                            "GTrC", "Open OpenPGP Structure Inspector"),
                        nullptr);
        action->setToolTip(QCoreApplication::translate(
            "GTrC", "Show the packet structure of the current tab"));
        QObject::connect(action, &QAction::triggered, main_window,
                         [=]() { RaiseInspectDialog(main_window); });
        advance_menu->addAction(action);

        // Decided when the menu opens rather than once at mount time: what
        // the current tab holds changes constantly, and an entry that is
        // enabled over plain text promises a window that would have nothing
        // in it.
        QPointer<QAction> guarded(action);
        QObject::connect(advance_menu, &QMenu::aboutToShow, action, [guarded] {
          if (guarded.isNull()) return;
          guarded->setEnabled(PGPInspectHasStructure(InspectCurrentTab()));
        });
      },
      Qt::BlockingQueuedConnection);

  return GFEventResult::Ok();
}

// The module's whole framework surface: which events it handles, what runs at
// each lifecycle point, and one forwarder to the runtime that implements all
// of it. Everything above is business logic.
constexpr GFEventBinding kEvents[] = {
    {"MAINWINDOW_MENU_MOUNTED", &OnMainWindowMenuMounted},
};

constexpr GFModuleHooks kHooks = {
    sizeof(GFModuleHooks),
    GF_MODULE_ID,
    GF_MODULE_VERSION,
    GF_MODULE_TRANSLATION_CONTEXT,
    &OnActivate,
    nullptr,  // nothing registered with the host, so nothing to undo
    nullptr,
    kEvents,
    std::size(kEvents),
};

extern "C" GF_MODULE_EXPORT auto GFModuleGetApi(uint32_t abi)
    -> const GFModuleApi * {
  return GFModuleRuntimeGetApi(abi, &kHooks);
}
