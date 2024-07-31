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

set(INTEGRATED_MODULE_SOURCE "")
aux_source_directory(. INTEGRATED_MODULE_SOURCE)

# capslock
list(APPEND INTEGRATED_MODULE_SOURCE "capslock/capslock.cpp")

if(MINGW)
  list(APPEND INTEGRATED_MODULE_SOURCE "capslock/capslock_win.cpp")
else()
  list(APPEND INTEGRATED_MODULE_SOURCE "capslock/capslock_unix.cpp")
endif()

list(APPEND INTEGRATED_MODULE_SOURCE "pinentry.qrc")

# define module
add_library(mod_pinentry SHARED ${INTEGRATED_MODULE_SOURCE})

# install dir
install(TARGETS mod_pinentry
  LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/modules")

# link options

# link sdk
target_link_libraries(mod_pinentry PRIVATE
  gpgfrontend_module_sdk)

# link qt
target_link_libraries(mod_pinentry PUBLIC Qt::Widgets)

# using std c++ 17
target_compile_features(mod_pinentry PUBLIC cxx_std_17)
