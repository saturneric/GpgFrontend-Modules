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

# These are requirements of this module, not preferences a build tree gets to
# hold an opinion about -- so FORCE. Without it a cache written before an
# option existed wins, and the build silently keeps the old value: a tree
# configured before TLS was turned on went on building vmime with
# VMIME_HAVE_TLS_SUPPORT=OFF, which does not fail to compile, it just removes
# the encryption. The sanitizer build was in exactly that state.
set(VMIME_INSTALL OFF CACHE BOOL "Don't Install" FORCE)
set(VMIME_BUILD_TESTS OFF CACHE BOOL "No Unit Test" FORCE)
set(VMIME_HAVE_TLS_SUPPORT ON CACHE BOOL "TLS for IMAP/SMTP" FORCE)
set(VMIME_TLS_SUPPORT_LIB "openssl" CACHE STRING
    "Use OpenSSL, already a dependency" FORCE)
set(VMIME_HAVE_SASL_SUPPORT OFF CACHE BOOL "No SASL" FORCE)
set(VMIME_BUILD_DOCUMENTATION OFF CACHE BOOL
    "Document IS Not Necessary HERE" FORCE)
set(VMIME_BUILD_STATIC_LIBRARY ON CACHE BOOL "Need Static Library" FORCE)
set(VMIME_BUILD_SHARED_LIBRARY OFF CACHE BOOL "No Shared Library" FORCE)
set(VMIME_HAVE_MESSAGING_PROTO_POP3 OFF CACHE BOOL "No POP3 protocol" FORCE)
set(VMIME_HAVE_MESSAGING_PROTO_SMTP ON CACHE BOOL "SMTP submission" FORCE)
set(VMIME_HAVE_MESSAGING_PROTO_IMAP ON CACHE BOOL "IMAP import" FORCE)
set(VMIME_HAVE_MESSAGING_PROTO_MAILDIR OFF CACHE BOOL
    "No Maildir protocol" FORCE)
set(VMIME_HAVE_MESSAGING_PROTO_SENDMAIL OFF CACHE BOOL
    "No Sendmail protocol" FORCE)
find_package(OpenSSL REQUIRED)

add_subdirectory(vmime "vmime" EXCLUDE_FROM_ALL)

# vmime wraps every TARGET_LINK_LIBRARIES for its TLS backend in
# IF(VMIME_BUILD_SHARED_LIBRARY). We build vmime-static, so OpenSSL never
# reaches consumers on its own and both this module and the gtest target
# would fail to link with undefined symbols. Attach it here instead.
target_link_libraries(vmime-static OpenSSL::SSL OpenSSL::Crypto)

if(MINGW)
  target_link_libraries(vmime-static ws2_32 mlang)
endif()

# Everything about this module -- id, version, name, description, author,
# capabilities and translation context -- lives in module.json beside this file.
gf_add_module(
  NAME mod_email
  QT   Core Widgets Concurrent Network
  LINK vmime-static
  INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/vmime/src"
    "${CMAKE_BINARY_DIR}/src/vmime"
    "${CMAKE_BINARY_DIR}/src")

# Only the module ABI leaves this shared object.
#
# The host resolves one symbol by name, declared GF_MODULE_EXPORT, which is
# __attribute__((visibility("default"))). Everything else -- this module's own
# classes and the whole of vmime -- is private to the object and was being
# exported anyway: 8404 dynamic symbols for an interface of one, of which 711
# were vmime internals nobody outside can call. The export table alone cost
# about 0.84 MB of the stripped binary, and a symbol that might be interposed
# is also a symbol the compiler cannot inline or fold.
#
# vmime needs this as much as the module does, since VMIME_EXPORT is empty in a
# static build and so its symbols carry no visibility of their own. Exceptions
# and dynamic_cast stay correct because nothing vmime defines ever crosses an
# object boundary: the module's interface to the host is plain C.
#
# On Windows this is inert -- a DLL's exports come from dllexport rather than
# from ELF visibility -- so it changes nothing there rather than breaking it.
foreach(target gf_mod_mod_email vmime-static)
  set_target_properties(${target} PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
endforeach()

# tests (opt-in): a standalone gtest binary over the SDK-free MIME code.
# The switch itself lives in modules/CMakeLists.txt -- it is modules-wide.
if(GPGFRONTEND_MODULES_BUILD_TESTS)
  add_subdirectory(test)
endif()
