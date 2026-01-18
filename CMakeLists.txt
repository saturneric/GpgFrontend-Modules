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

cmake_minimum_required(VERSION 3.16)

set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CMAKE_SOURCE_DIR}/cmake/FlagsOverrides.cmake")

# define project
project(GpgFrontendModules
  VERSION ${CMAKE_PROJECT_VERSION}
  DESCRIPTION "Modules of GpgFrontend"
  HOMEPAGE_URL "https://gpgfrontend.bktus.com"
  LANGUAGES CXX)

# includes
include(GenerateExportHeader)

# function or mode options
option(GPGFRONTEND_QT5_BUILD "Switch to Qt5 building mode" OFF)

# Using Standard C++-17 (Consider compatibility)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# CMake
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GPGFRONTEND_MODULES_QT5_BUILD "Swith to Qt5 building mode" OFF)

# show build arguments
message(STATUS "GpgFrontend Modules Source Path: ${CMAKE_SOURCE_DIR}")
message(STATUS "GpgFrontend Modules Runtime Path: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

set(SDK_INCLUDE_PATH "${CMAKE_SOURCE_DIR}/src/sdk")
set(SDK_LIB_PATH "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

message(STATUS "GpgFrontend Modules SDK Headers Path: ${SDK_INCLUDE_PATH} ")
message(STATUS "GpgFrontend Modules SDK Library Path: ${SDK_LIB_PATH} ")

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${SDK_INCLUDE_PATH}
)

# add gpgme header path if needed
if (GNUPG_DEPS_BUILT)
  get_target_property(_incs GnuPGDeps INTERFACE_INCLUDE_DIRECTORIES)
  if (NOT _incs)
    message(FATAL_ERROR "Failed to get GnuPGDeps include directories")
  endif()
  include_directories(${_incs})
endif()

link_directories(
  ${SDK_LIB_PATH}
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/modules)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/modules)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/modules)

message(STATUS "GpgFrontend Modules Runtime Output Path: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "GpgFrontend Modules Library Output Path: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
message(STATUS "GpgFrontend Modules ARCHIVE Output Path: ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")

# Introduce Qt
# Support Qt version: 6.x
if(GPGFRONTEND_QT5_BUILD)
  # Introduce Qt
  # Support Qt version: 5.15.x
  find_package(Qt5 5.15 COMPONENTS Core Widgets Network LinguistTools Concurrent Xml REQUIRED)
else()
  # Introduce Qt
  # Support Qt version: 6.x
  find_package(Qt6 6 COMPONENTS Core Widgets Network LinguistTools Concurrent Xml REQUIRED)
endif()

# Qt configuration
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include(GNUInstallDirs)
set(CMAKE_INSTALL_BINDIR "${CMAKE_INSTALL_LIBDIR}/${APP_NAME_LOWER}/modules")
set(CMAKE_INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}/${APP_NAME_LOWER}/modules")
set(CMAKE_INSTALL_DATAROOTDIR "${CMAKE_INSTALL_LIBDIR}/${APP_NAME_LOWER}/modules")

# third_party
add_subdirectory(third_party)

# source code
add_subdirectory(src)
