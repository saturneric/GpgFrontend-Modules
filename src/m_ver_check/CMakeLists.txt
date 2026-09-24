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
# The initial version of the source code is inherited from
# the gpg4usb project, which is under GPL-3.0-or-later.
#
# All the source code of GpgFrontend was modified and released by
# Saturneric <eric@bktus.com> starting on May 12, 2021.
#
# SPDX-License-Identifier: GPL-3.0-or-later

# Everything about this module -- id, version, name, description, author,
# capabilities and translation context -- lives in module.json beside this file.
#
# SOURCES is spelled out rather than left to aux_source_directory: the test
# directory below compiles the Host-free half (ReleaseCheck, UpdateChecker,
# UpdatePresentation, SoftwareVersion) on its own.
gf_add_module(
  NAME    ver_check
  QT      Core Network Widgets
  SOURCES VersionCheckingModule.cpp
          ReleaseCheck.cpp
          SoftwareVersion.cpp
          UpdateChecker.cpp
          UpdatePresentation.cpp
          UpdateTab.cpp
          Utils.cpp
  UI_DIR  ui
  LUA_SCRIPTS ui/main.lua)

if(GPGFRONTEND_MODULES_BUILD_TESTS)
  add_subdirectory(test)
endif()
