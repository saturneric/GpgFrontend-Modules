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

# Unit tests for the SDK-free MIME code of the e-mail module.
#
# EMAilHelper.cpp deliberately carries no GF SDK symbol -- it logs through an
# injected sink instead -- so it can be compiled straight into a plain gtest
# binary with nothing but Qt Core and vmime behind it. That is the whole reason
# the split exists; keep it that way, or this target stops linking.

if(NOT TARGET GTest::gtest)
  find_package(GTest QUIET)
endif()

if(NOT TARGET GTest::gtest)
  message(STATUS "GTest not found; skipping e-mail module tests")
  return()
endif()

add_executable(gf_mod_email_test
  EMailMimeTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../EMAilHelper.cpp)

target_include_directories(gf_mod_email_test PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
  "${CMAKE_CURRENT_SOURCE_DIR}/../vmime/src"
  "${CMAKE_BINARY_DIR}/src/vmime"
  "${CMAKE_BINARY_DIR}/src")

target_link_libraries(gf_mod_email_test PRIVATE
  GTest::gtest GTest::gtest_main Qt::Core vmime-static)

set_target_properties(gf_mod_email_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
