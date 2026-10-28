# Copyright (C) 2021-2024 Saturneric <eric@bktus.com>
#
# This file is part of GpgFrontend.
#
# GpgFrontend is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# GpgFrontend is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GpgFrontend. If not, see <https://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: GPL-3.0-or-later

# The editor page is web/dist/, built from web/src/ by
# scripts/build_note_editor.sh and committed: building needs no Node.
gf_add_module(
  NAME    notes
  QT      Core Gui Widgets Concurrent
  SOURCES NotesModule.cpp
          NotePageView.cpp
          NoteCrypto.cpp
          NoteModel.cpp
          NoteProtection.cpp
          NotePersistence.cpp
          NoteImage.cpp
          NoteBridge.cpp
          NoteFiles.cpp
          NoteStyle.cpp
  LUA_SCRIPTS ui/main.lua
  WEB_ASSETS  web/dist)

if(GPGFRONTEND_MODULES_BUILD_TESTS)
  add_subdirectory(test)
endif()
