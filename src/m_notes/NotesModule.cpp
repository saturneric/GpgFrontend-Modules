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
#include <GFModuleCommand.h>
#include <GFModuleNativeWidget.h>

#include <GFSDKHostCommands.hpp>
#include <array>

#include "GFModuleIdentity.h"
#include "GFModuleTr.h"
#include "NotePageView.h"

/**
 * @file NotesModule.cpp
 * @brief Encrypted notes: rich text, kept as a TextPack encrypted with
 * OpenPGP, in tabs whose files only this module reads and writes.
 */

using namespace Qt::StringLiterals;

namespace {

/// Spelled unqualified: lupdate misreads `gf::cmd::Meta kMeta{` in a class.
using CommandMeta = gf::cmd::Meta;

struct NewNote {
  static constexpr CommandMeta kMeta{GF_MODULE_ID ".new_note",
                                     GTrC::Noop("Note Editor"),
                                     GTrC::Noop("Write a new encrypted note."),
                                     "",
                                     0,
                                     gf::cmd::kNeedsGuiThread};
  using Args = gf::cmd::Unit;
  using Result = gf::cmd::Unit;
};

auto DoNewNote(const gf::cmd::CommandContext& /*ctx*/, const gf::cmd::Unit&)
    -> gf::cmd::Outcome<gf::cmd::Unit> {
  Commands().Invoke<gf::cmd::host::DocumentNew>(
      {u"note"_s, GTrC::tr("Untitled Note")});
  return gf::cmd::Outcome<gf::cmd::Unit>::Success({});
}

auto OnActivate() -> GFResult {
  const bool ok = gf::ui::RegisterNativeWidgetFactory<gf::notes::NotePageView>(
      "editor",
      {GTrC::Noop("Encrypted Note"), "", "textpack.gpg",
       GTrC::Noop("Encrypted Note (*.textpack.gpg);;TextPack (*.textpack);;All "
                  "Files (*)"),
       ":/icons/lock.png", 0, 0},
      [](const QCborMap&) { return new gf::notes::NotePageView(); });
  return ok ? GFResult::Ok()
            : GFResult::Fail("the note editor was not registered");
}

const std::array<gf::cmd::Binding, 1> kCommands = {
    gf::cmd::Bind<NewNote, &DoNewNote>(),
};

const GFModuleHooks kHooks = {
    sizeof(GFModuleHooks),
    GF_MODULE_ID,
    GF_MODULE_VERSION,
    GF_MODULE_TRANSLATION_CONTEXT,
    &OnActivate,
    nullptr,
    nullptr,
    nullptr,
    0,
    kCommands.data(),
    kCommands.size(),
};

}  // namespace

extern "C" GF_MODULE_EXPORT auto GFModuleGetApi(uint32_t abi)
    -> const GFModuleApi* {
  return GFModuleRuntimeGetApi(abi, &kHooks);
}
