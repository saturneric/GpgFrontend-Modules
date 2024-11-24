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
add_library(mod_email SHARED ${INTEGRATED_MODULE_SOURCE})

set(VMIME_INSTALL OFF CACHE BOOL "Build vmime static library")
set(VMIME_BUILD_TESTS OFF CACHE BOOL "Build vmime static library")
set(VMIME_HAVE_TLS_SUPPORT OFF CACHE BOOL "Build vmime static library")
set(VMIME_HAVE_SASL_SUPPORT OFF CACHE BOOL "Build vmime static library")
set(VMIME_BUILD_DOCUMENTATION OFF CACHE BOOL "Build vmime static library")
set(VMIME_BUILD_STATIC_LIBRARY ON CACHE BOOL "Build vmime static library")
set(VMIME_BUILD_SHARED_LIBRARY OFF CACHE BOOL "Build vmime static library")
add_subdirectory(vmime "vmime" EXCLUDE_FROM_ALL)

# install dir
install(TARGETS mod_email
  LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/modules")

# vmime headers
target_include_directories(mod_email PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/vmime/src"
  "${CMAKE_BINARY_DIR}/src/vmime"
  "${CMAKE_BINARY_DIR}/src")

# /Users/erich/Documents/Entwicklung/GpgFrontend/modules/build/src/vmime/config.hpp
# /Users/erich/Documents/Entwicklung/GpgFrontend/modules/build
message(STATUS "CMAKE_BINARY_DIR of module email: ${CMAKE_BINARY_DIR}")

# link sdk
target_link_libraries(mod_email PRIVATE
  gpgfrontend_module_sdk)

# link qt
target_link_libraries(mod_email PRIVATE Qt::Core Qt::Widgets Qt::Concurrent)

# link vmime
target_link_libraries(mod_email PRIVATE vmime-static)

# using std c++ 17
target_compile_features(mod_email PRIVATE cxx_std_17)

# i18n
# set(LOCALE_TS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/ts)
# set(TS_FILES "${LOCALE_TS_PATH}/ModuleEMail.en_US.ts"
# "${LOCALE_TS_PATH}/ModuleEMail.de_DE.ts"
# "${LOCALE_TS_PATH}/ModuleEMail.fr_FR.ts"
# "${LOCALE_TS_PATH}/ModuleEMail.it_iT.ts"
# "${LOCALE_TS_PATH}/ModuleEMail.zh_CN.ts"
# "${LOCALE_TS_PATH}/ModuleEMail.zh_TW.ts")
# qt_add_translations(mod_email
# RESOURCE_PREFIX "/i18n"
# TS_FILES ${TS_FILES}
# SOURCES ${INTEGRATED_MODULE_SOURCE}
# INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR})