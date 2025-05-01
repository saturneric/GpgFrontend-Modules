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

# com.bktus.gpgfrontend.module.integrated.gnupg_info_gathering

set(INTEGRATED_MODULE_SOURCE "")
aux_source_directory(. INTEGRATED_MODULE_SOURCE)

# define libgpgfrontend_module
register_module(gpg_info MODULE_TARGET ${INTEGRATED_MODULE_SOURCE})

# link qt
target_link_libraries(${MODULE_TARGET} PRIVATE Qt::Core Qt::Widgets Qt::Concurrent)

# i18n
set(LOCALE_TS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/ts)
set(TS_FILES "${LOCALE_TS_PATH}/ModuleGnuPGInfoGathering.en_US.ts"
  "${LOCALE_TS_PATH}/ModuleGnuPGInfoGathering.de_DE.ts"
  "${LOCALE_TS_PATH}/ModuleGnuPGInfoGathering.fr_FR.ts"
  "${LOCALE_TS_PATH}/ModuleGnuPGInfoGathering.it_iT.ts"
  "${LOCALE_TS_PATH}/ModuleGnuPGInfoGathering.zh_CN.ts"
  "${LOCALE_TS_PATH}/ModuleGnuPGInfoGathering.zh_TW.ts")

if(NOT XCODE_BUILD)
  qt_add_translations(${MODULE_TARGET}
    RESOURCE_PREFIX "/i18n"
    TS_FILES ${TS_FILES}
    SOURCES ${INTEGRATED_MODULE_SOURCE}
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR})
else()
  add_custom_target("${MODULE_TARGET}_i18n" ALL)
  qt_add_lrelease("${MODULE_TARGET}_i18n"
    TS_FILES ${TS_FILES}
    QM_FILES_OUTPUT_VARIABLE TRANSLATIONS_QM)
  qt_add_resources(${MODULE_TARGET} "${MODULE_TARGET}_i18n"
    PREFIX " /i18n "
    BASE ${CMAKE_CURRENT_BINARY_DIR}
    FILES ${TRANSLATIONS_QM})
endif()