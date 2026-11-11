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
          RootTrust.cpp
          SemVer.cpp
          SoftwareVersion.cpp
          UpdateChecker.cpp
          UpdatePresentation.cpp
          UpdateTab.cpp
          Utils.cpp
  LINK    sodium
  UI_DIR  ui
  LUA_SCRIPTS ui/main.lua)

# Where every build reads its signed update index: release/product.json of
# the Host tree, the one place the product names it. It reaches the code as
# definitions read by VersionCheckingModule.cpp, "|"-joined. What the index
# is trusted on is the newest root and index-keys list of release/root/ and
# release/index-keys/, built in byte for byte (EmbeddedTrust.cpp, below). A
# build of the modules without them has no channel, and then reports
# "unknown" rather than trust anything.
#
# GF_RELEASE_*_OVERRIDE replace them for a rehearsal against a local mirror
# with throwaway keys (scripts/release/rehearse.sh); a build made so must
# never be distributed, which is why configuring one says so loudly.
set(_gf_product_json "${CMAKE_SOURCE_DIR}/release/product.json")
set(_gf_release_product "")
set(_gf_release_update "")
set(_gf_release_builds "")
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

foreach(_gf_what UPDATE_BASES BUILDS_BASES TRUST CA)
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

list(JOIN _gf_release_update "|" _gf_release_update)
list(JOIN _gf_release_builds "|" _gf_release_builds)
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
    GF_RELEASE_NOTES_HOSTS="${_gf_release_notes_hosts}")

# The trust built in: <dir>/root/<N>.root.json with each of its
# <N>.root.json.<keyid>.minisig, and <dir>/index-keys/<V>.json with its
# .minisig, the highest N and V. <dir> is release/, or for a rehearsal
# GF_RELEASE_TRUST_OVERRIDE (a directory laid out the same).
set(_gf_trust "${CMAKE_SOURCE_DIR}/release")
if(DEFINED GF_RELEASE_TRUST_OVERRIDE)
  set(_gf_trust "${GF_RELEASE_TRUST_OVERRIDE}")
endif()
function(_gf_newest out dir pattern)
  file(GLOB _files RELATIVE "${dir}" "${dir}/*")
  set(_best 0)
  foreach(_f IN LISTS _files)
    if(_f MATCHES "${pattern}" AND CMAKE_MATCH_1 GREATER _best)
      set(_best "${CMAKE_MATCH_1}")
    endif()
  endforeach()
  set(${out} "${_best}" PARENT_SCOPE)
endfunction()
function(_gf_bytes out file)
  file(READ "${file}" _hex HEX)
  string(LENGTH "${_hex}" _n)
  math(EXPR _n "${_n} / 2")
  string(REGEX REPLACE "(..)" "\\\\x\\1" _esc "${_hex}")
  set(${out} "QByteArray(\"${_esc}\", ${_n})" PARENT_SCOPE)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${file}")
endfunction()
_gf_newest(_gf_root "${_gf_trust}/root" "^([0-9]+)\\.root\\.json$")
_gf_newest(_gf_list "${_gf_trust}/index-keys" "^([0-9]+)\\.json$")
set(_gf_trust_code "")
if(_gf_root GREATER 0 AND _gf_list GREATER 0)
  set(_gf_root_file "${_gf_trust}/root/${_gf_root}.root.json")
  _gf_bytes(_gf_data "${_gf_root_file}")
  string(APPEND _gf_trust_code "  t.root.data = ${_gf_data};\n")
  file(GLOB _gf_sigs RELATIVE "${_gf_trust}/root"
       "${_gf_trust}/root/${_gf_root}.root.json.*.minisig")
  foreach(_gf_sig IN LISTS _gf_sigs)
    string(REGEX MATCH "\\.([0-9A-F]+)\\.minisig$" _ "${_gf_sig}")
    _gf_bytes(_gf_data "${_gf_trust}/root/${_gf_sig}")
    string(APPEND _gf_trust_code
           "  t.root.signatures.insert(\"${CMAKE_MATCH_1}\", ${_gf_data});\n")
  endforeach()
  _gf_bytes(_gf_data "${_gf_trust}/index-keys/${_gf_list}.json")
  string(APPEND _gf_trust_code "  t.index_keys.data = ${_gf_data};\n")
  _gf_bytes(_gf_data "${_gf_trust}/index-keys/${_gf_list}.json.minisig")
  string(APPEND _gf_trust_code
         "  t.index_keys.signatures.insert(QString(), ${_gf_data});\n")
else()
  message(STATUS "m_ver_check: no trust root in ${_gf_trust}; this build "
    "has no update channel")
endif()
set(_gf_trust_cpp "${CMAKE_CURRENT_BINARY_DIR}/EmbeddedTrust.cpp")
file(CONFIGURE OUTPUT "${_gf_trust_cpp}" CONTENT "// Generated by CMake from ${_gf_trust}: do not edit.
#include \"ReleaseIndex.h\"

auto BuiltInTrust() -> EmbeddedTrust {
  EmbeddedTrust t;
@_gf_trust_code@  return t;
}
" @ONLY)
target_sources(gf_mod_ver_check PRIVATE "${_gf_trust_cpp}")

if(GPGFRONTEND_MODULES_BUILD_TESTS)
  add_subdirectory(test)
endif()
