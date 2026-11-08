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
# UpdatePresentation, SoftwareVersion, AppImageUpdate) on its own.
gf_add_module(
  NAME    ver_check
  QT      Core Network Widgets
  SOURCES VersionCheckingModule.cpp
          AppImageUpdate.cpp
          Minisign.cpp
          ReleaseCheck.cpp
          ReleaseIndex.cpp
          SemVer.cpp
          SoftwareVersion.cpp
          UpdateChecker.cpp
          UpdatePresentation.cpp
          UpdateTab.cpp
          Utils.cpp
  LINK    sodium
  UI_DIR  ui
  LUA_SCRIPTS ui/main.lua)

# Where every build reads its signed update index, and which keys may sign it:
# release/product.json of the Host tree, the one place the product names
# them. They reach the code as definitions read by VersionCheckingModule.cpp,
# "|"-joined. A build of the modules without that file has no channel, and
# then reports "unknown" rather than trust anything.
#
# GF_RELEASE_*_OVERRIDE replace them for a rehearsal against a local mirror
# with throwaway keys (scripts/release/rehearse.sh); a build made so must
# never be distributed, which is why configuring one says so loudly.
set(_gf_product_json "${CMAKE_SOURCE_DIR}/release/product.json")
set(_gf_release_product "")
set(_gf_release_update "")
set(_gf_release_builds "")
set(_gf_release_keys "")
set(_gf_release_notes_hosts "")
if(EXISTS "${_gf_product_json}")
  file(READ "${_gf_product_json}" _gf_product)
  string(JSON _gf_release_product GET "${_gf_product}" slug)
  foreach(_gf_role update builds)
    string(JSON _gf_n LENGTH "${_gf_product}" bases ${_gf_role})
    set(_gf_list "")
    if(_gf_n GREATER 0)
      math(EXPR _gf_last "${_gf_n} - 1")
      foreach(_gf_i RANGE ${_gf_last})
        string(JSON _gf_base GET "${_gf_product}" bases ${_gf_role} ${_gf_i})
        list(APPEND _gf_list "${_gf_base}")
      endforeach()
    endif()
    set(_gf_release_${_gf_role} "${_gf_list}")
  endforeach()
  string(JSON _gf_n LENGTH "${_gf_product}" signing index_keys)
  if(_gf_n GREATER 0)
    math(EXPR _gf_last "${_gf_n} - 1")
    foreach(_gf_i RANGE ${_gf_last})
      string(JSON _gf_key GET "${_gf_product}" signing index_keys ${_gf_i})
      list(APPEND _gf_release_keys "${_gf_key}")
    endforeach()
  endif()
  # Where release notes may link to (UpdateTab opens no other link).
  string(JSON _gf_n ERROR_VARIABLE _gf_err LENGTH "${_gf_product}"
         notes_link_hosts)
  if(NOT _gf_err AND _gf_n GREATER 0)
    math(EXPR _gf_last "${_gf_n} - 1")
    foreach(_gf_i RANGE ${_gf_last})
      string(JSON _gf_host GET "${_gf_product}" notes_link_hosts ${_gf_i})
      list(APPEND _gf_release_notes_hosts "${_gf_host}")
    endforeach()
  endif()
endif()

foreach(_gf_what UPDATE_BASES BUILDS_BASES INDEX_KEYS CA)
  if(DEFINED GF_RELEASE_${_gf_what}_OVERRIDE)
    message(WARNING "m_ver_check: GF_RELEASE_${_gf_what}_OVERRIDE is set. "
      "This build trusts a rehearsal update channel; never distribute it.")
  endif()
endforeach()
if(DEFINED GF_RELEASE_UPDATE_BASES_OVERRIDE)
  set(_gf_release_update "${GF_RELEASE_UPDATE_BASES_OVERRIDE}")
endif()
if(DEFINED GF_RELEASE_BUILDS_BASES_OVERRIDE)
  set(_gf_release_builds "${GF_RELEASE_BUILDS_BASES_OVERRIDE}")
endif()
if(DEFINED GF_RELEASE_INDEX_KEYS_OVERRIDE)
  set(_gf_release_keys "${GF_RELEASE_INDEX_KEYS_OVERRIDE}")
endif()

list(JOIN _gf_release_update "|" _gf_release_update)
list(JOIN _gf_release_builds "|" _gf_release_builds)
list(JOIN _gf_release_keys "|" _gf_release_keys)
list(JOIN _gf_release_notes_hosts "|" _gf_release_notes_hosts)
# A rehearsal mirror's own CA (scripts/release/rehearse.sh writes it), trusted
# by the update checker's requests only; never set in a build to distribute.
if(DEFINED GF_RELEASE_CA_OVERRIDE AND NOT GF_RELEASE_CA_OVERRIDE STREQUAL "")
  set_property(SOURCE VersionCheckingModule.cpp APPEND PROPERTY
    COMPILE_DEFINITIONS GF_RELEASE_CA="${GF_RELEASE_CA_OVERRIDE}")
endif()
set_property(SOURCE VersionCheckingModule.cpp APPEND PROPERTY
  COMPILE_DEFINITIONS
    GF_RELEASE_PRODUCT="${_gf_release_product}"
    GF_RELEASE_UPDATE_BASES="${_gf_release_update}"
    GF_RELEASE_BUILDS_BASES="${_gf_release_builds}"
    GF_RELEASE_INDEX_KEYS="${_gf_release_keys}"
    GF_RELEASE_NOTES_HOSTS="${_gf_release_notes_hosts}")

if(GPGFRONTEND_MODULES_BUILD_TESTS)
  add_subdirectory(test)
endif()
