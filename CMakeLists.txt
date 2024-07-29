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

message(STATUS "Current Generator: ${CMAKE_GENERATOR}")
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CMAKE_SOURCE_DIR}/cmake/FlagsOverrides.cmake")

# define project
project(GpgFrontendModules
  VERSION 2.1.4
  DESCRIPTION "Modules of GpgFrontend"
  HOMEPAGE_URL "https://gpgfrontend.bktus.com"
  LANGUAGES CXX)

# includes
include(GenerateExportHeader)

# function or mode options
option(GPGFRONTEND_QT5_BUILD "Swith to Qt5 building mode" OFF)

# Using Standard C++-17 (Consider compatibility)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# CMake
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GPGFRONTEND_MODULES_QT5_BUILD "Swith to Qt5 building mode" OFF)

# show cmake version
message(STATUS "GpgFrontend Moudles Build Configuration Started CMAKE Version ${CMAKE_VERSION}")

set(SDK_VERSION "${CMAKE_PROJECT_VERSION}")
set(SDK_PATH "${CMAKE_SOURCE_DIR}/sdk/${SDK_VERSION}")
set(SDK_INCLUDE_PATH "${SDK_PATH}/include")
set(SDK_LIB_PATH "${SDK_PATH}/lib")

message(STATUS "SDK Path: ${SDK_PATH}")

include_directories(
  ${CMAKE_SOURCE_DIR}/include
  ${SDK_INCLUDE_PATH}
)

link_directories(
  ${SDK_LIB_PATH}
)

# Introduce Qt
# Support Qt version: 6.x
if(GPGFRONTEND_QT5_BUILD)
  # Introduce Qt
  # Support Qt version: 5.15.x
  find_package(Qt5 5.15 COMPONENTS Core Widgets Network LinguistTools REQUIRED)
else()
  # Introduce Qt
  # Support Qt version: 6.x
  find_package(Qt6 6 COMPONENTS Core Widgets Network LinguistTools REQUIRED)
endif()

# Qt configuration
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC_OPTIONS "--compress;9")

# rpath config
if(WIN32)
  message(STATUS "Configuring for Windows without rpath")
elseif(APPLE)
  set(CMAKE_MACOSX_RPATH 1)
  set(CMAKE_INSTALL_RPATH "@loader_path/../lib")

  include_directories(
    /usr/local/include
    /opt/homebrew/include
  )

  link_directories(
    /usr/local/lib
    /opt/homebrew/lib
  )
else()
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
  set(CMAKE_INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib")
endif()

# third_party
add_subdirectory(third_party)

# source code
add_subdirectory(src)