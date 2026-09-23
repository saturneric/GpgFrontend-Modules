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
#include <GFSDKHostCommands.hpp>

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

/**
 * @brief The module's one action: show the current tab's packet structure.
 *
 * Enabled only when the tab holds something to show -- decided when the
 * menu opens, because what the current tab holds changes constantly and an
 * entry enabled over plain text promises a window with nothing in it.
 */
struct OpenInspector {
  static constexpr gf::cmd::Meta kMeta{
      GF_MODULE_ID ".open_inspector",
      GC_TR("Open OpenPGP Structure Inspector"),
      GC_TR("Show the packet structure of the current tab"), "", 0,
      gf::cmd::kNeedsGuiThread};
  using Args = gf::cmd::Unit;
  using Result = gf::cmd::Unit;

  static auto State(const gf::cmd::CommandContext& /*ctx*/) -> uint32_t {
    return PGPInspectHasStructure(InspectCurrentTab())
               ? GF_CMD_STATE_ENABLED | GF_CMD_STATE_VISIBLE
               : GF_CMD_STATE_VISIBLE;
  }
};

auto DoOpenInspector(const gf::cmd::CommandContext& /*ctx*/,
                     const gf::cmd::Unit& /*args*/)
    -> gf::cmd::Outcome<gf::cmd::Unit> {
  // The dialog is the module's mount; the Host owns its frame and opens it.
  Commands().Invoke<gf::cmd::host::ViewOpen>(
      {gf::cmd::ViewRef{QStringLiteral(GF_MODULE_ID ".inspector")}});
  return gf::cmd::Outcome<gf::cmd::Unit>::Success({});
}

}  // namespace

auto OnActivate() -> GFResult {
  // The inspector, built from the current tab each time it is opened. Where
  // it is offered is ui/main.lua's business.
  const bool ok = gf::ui::RegisterNativeWidget<PGPInspectDialog>(
      "inspector",
      {GC_TR("OpenPGP Structure"), "", "", "", ":/icons/help.png", 820, 620},
      [](const QCborMap& /*args*/) {
        const auto bytes = PGPInspectCurrentTabBytes();
        return new PGPInspectDialog(PGPInspectBytes(bytes), bytes.size());
      });
  return ok ? GFResult::Ok()
            : GFResult::Fail("the inspector widget was not registered");
}

// The module's whole framework surface: what it provides, what runs at each
// lifecycle point, and one forwarder to the runtime that implements all of
// it. Everything above is business logic.
const std::array<gf::cmd::Binding, 1> kCommands = {
    gf::cmd::Bind<OpenInspector, &DoOpenInspector>(),
};

const GFModuleHooks kHooks = {
    sizeof(GFModuleHooks),
    GF_MODULE_ID,
    GF_MODULE_VERSION,
    GF_MODULE_TRANSLATION_CONTEXT,
    &OnActivate,
    nullptr,  // the Host withdraws the command, widget and script itself
    nullptr,
    nullptr,  // no events
    0,
    kCommands.data(),
    kCommands.size(),
};

extern "C" GF_MODULE_EXPORT auto GFModuleGetApi(uint32_t abi)
    -> const GFModuleApi * {
  return GFModuleRuntimeGetApi(abi, &kHooks);
}
