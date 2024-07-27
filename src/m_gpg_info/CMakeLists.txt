# Copyright (C) 2021 Saturneric <eric@bktus.com>
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

# com.bktus.gpgfrontend.module.integrated.gnupg_info_gathering

aux_source_directory(. INTEGRATED_MODULE_SOURCE)

# define libgpgfrontend_module
add_library(mod_gpg_info SHARED ${INTEGRATED_MODULE_SOURCE})

# install dir
install(TARGETS mod_gpg_info 
  LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/modules")

# link sdk
target_link_libraries(mod_gpg_info PRIVATE
  gpgfrontend_module_sdk)

if(GPGFRONTEND_QT5_BUILD)
  # link Qt core
  target_link_libraries(mod_gpg_info PRIVATE Qt5::Core Qt5::Widgets)
else()
  # link Qt core
  target_link_libraries(mod_gpg_info PRIVATE Qt6::Core Qt6::Widgets)
endif()

# using std c++ 17
target_compile_features(mod_gpg_info PRIVATE cxx_std_17)

# ui
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_AUTOUIC_SEARCH_PATHS} ${CMAKE_CURRENT_SOURCE_DIR}/ui)

# i18n
set(LOCALE_TS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/ts)
set(TS_FILES "${LOCALE_TS_PATH}/ModuleGnuPGInfoGathering.en_US.ts" 
            "${LOCALE_TS_PATH}/ModuleGnuPGInfoGathering.de_DE.ts"
            "${LOCALE_TS_PATH}/ModuleGnuPGInfoGathering.fr_FR.ts"
            "${LOCALE_TS_PATH}/ModuleGnuPGInfoGathering.zh_CN.ts")
qt_add_translations(mod_gpg_info
    RESOURCE_PREFIX "/i18n"
    TS_FILES ${TS_FILES}
    SOURCES ${MODULE_SOURCE_FILES}
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR})