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

# com.bktus.gpgfrontend.module.integrated.email

set(INTEGRATED_MODULE_SOURCE "")
aux_source_directory(. INTEGRATED_MODULE_SOURCE)

# register module
register_module(mod_email MODULE_TARGET ${INTEGRATED_MODULE_SOURCE})

set(VMIME_INSTALL OFF CACHE BOOL "Don't Install")
set(VMIME_BUILD_TESTS OFF CACHE BOOL "No Unit Test")
set(VMIME_HAVE_TLS_SUPPORT OFF CACHE BOOL "No TLS")
set(VMIME_HAVE_SASL_SUPPORT OFF CACHE BOOL "No SASL")
set(VMIME_BUILD_DOCUMENTATION OFF CACHE BOOL "Document IS Not Necessary HERE")
set(VMIME_BUILD_STATIC_LIBRARY ON CACHE BOOL "Need Static Library")
set(VMIME_BUILD_SHARED_LIBRARY OFF CACHE BOOL "No Shared Library")
set(VMIME_HAVE_MESSAGING_PROTO_POP3 OFF CACHE BOOL "No POP3 protocol")
set(VMIME_HAVE_MESSAGING_PROTO_SMTP OFF CACHE BOOL "No SMTP protocol")
set(VMIME_HAVE_MESSAGING_PROTO_IMAP OFF CACHE BOOL "No IMAP protocol")
set(VMIME_HAVE_MESSAGING_PROTO_MAILDIR OFF CACHE BOOL "No Maildir protocol")
set(VMIME_HAVE_MESSAGING_PROTO_SENDMAIL OFF CACHE BOOL "No Sendmail protocol")
add_subdirectory(vmime "vmime" EXCLUDE_FROM_ALL)

if(MINGW)
  target_link_libraries(vmime-static ws2_32 mlang)
endif()

# vmime headers
target_include_directories(${MODULE_TARGET} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/vmime/src"
  "${CMAKE_BINARY_DIR}/src/vmime"
  "${CMAKE_BINARY_DIR}/src")

message(STATUS "CMAKE_BINARY_DIR of module email: ${CMAKE_BINARY_DIR}")

# link qt
target_link_libraries(${MODULE_TARGET} PRIVATE Qt::Core Qt::Widgets Qt::Concurrent)

# link vmime
target_link_libraries(${MODULE_TARGET} PRIVATE vmime-static)

# add gpgme header path if needed
if (GNUPG_DEPS_BUILT)
  get_target_property(_incs GnuPGDeps INTERFACE_INCLUDE_DIRECTORIES)
  message(STATUS "GnuPGDeps include dirs: ${_incs}")
  target_include_directories(${MODULE_TARGET} PRIVATE ${_incs})
endif()

# i18n
set(LOCALE_TS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/ts)
set(TS_FILES "${LOCALE_TS_PATH}/ModuleEMail.en_US.ts"
  "${LOCALE_TS_PATH}/ModuleEMail.de_DE.ts"
  "${LOCALE_TS_PATH}/ModuleEMail.fr_FR.ts"
  "${LOCALE_TS_PATH}/ModuleEMail.it_iT.ts"
  "${LOCALE_TS_PATH}/ModuleEMail.zh_CN.ts"
  "${LOCALE_TS_PATH}/ModuleEMail.zh_TW.ts")

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
    PREFIX "/i18n"
    BASE ${CMAKE_CURRENT_BINARY_DIR}
    FILES ${TRANSLATIONS_QM})
endif()