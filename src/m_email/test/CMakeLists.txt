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
# EMailHelper.cpp deliberately carries no GF SDK symbol -- it logs through an
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
  # The fuzz harness itself, replayed over the corpus. See
  # EMailFuzzReplayTest.cpp: it is the only thing that runs gf_mod_email_fuzz
  # code on an ordinary build, and keeps the harness from quietly rotting.
  EMailFuzzReplayTest.cpp
  EMailMimeFuzz.cpp
  EMailCorpusTest.cpp
  EMailPublicCorpusTest.cpp
  EMailTransportTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../EMailHelper.cpp
  # The transport's decision-making, none of which needs a socket or a widget:
  # account policy, envelope construction, Message-ID, and the error taxonomy.
  # Kept SDK-free for exactly this reason.
  ${CMAKE_CURRENT_SOURCE_DIR}/../EMailAccountModel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../EMailOutgoing.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../EMailNetError.cpp
  # The message view's layout rules. Kept out of the view itself so they can be
  # checked at all: EMailPageView needs Qt Widgets and the SDK, and a widget
  # built in one of these bodies would be built off the GUI thread.
  EMailViewLayoutTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../EMailViewLayout.cpp)

target_include_directories(gf_mod_email_test PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
  "${CMAKE_CURRENT_SOURCE_DIR}/../vmime/src"
  "${CMAKE_BINARY_DIR}/src/vmime"
  "${CMAKE_BINARY_DIR}/src")

target_link_libraries(gf_mod_email_test PRIVATE
  GTest::gtest GTest::gtest_main Qt::Core Qt::Network vmime-static)

# The corpus is read from the source tree at run time rather than compiled in
# through a .qrc: this target links Qt::Core only, not gf_res, so ":/test/..."
# paths are not available to it.
target_compile_definitions(gf_mod_email_test PRIVATE
  "GF_EMAIL_TEST_CORPUS_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/corpus\""
  # vmime is linked statically here, as it is in the module. Without this its
  # headers expect the shared-build export header, which is never generated.
  VMIME_STATIC)

set_target_properties(gf_mod_email_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${GPGFRONTEND_MODULE_TEST_OUTPUT_DIR}")

# --- transport tests that need a server -------------------------------------
#
# Separate from gf_mod_email_test on purpose. That target is strictly
# SDK-free; the workers are not, because they log. So this one supplies small
# stand-in SDK symbols, and in return it can drive the REAL IMAP and SMTP
# workers against fake servers on loopback.
#
# What it buys is the class of guarantee nothing else can check: that folders
# are opened read-only, that bodies are peeked, and that a wrong SMTP password
# is refused. Those are properties of what goes on the wire, and a return
# value cannot show them -- which is exactly how SMTP came to skip
# authentication entirely without any test noticing.

add_executable(gf_mod_email_net_test
  EMailNetTest.cpp
  EMailSecretTest.cpp
  EMailSdkStubs.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../EMailAccountModel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../EMailNetError.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../EMailCancelToken.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../EMailSecret.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../EMailTlsSetup.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../EMailImapWorker.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../EMailSmtpWorker.cpp)

target_include_directories(gf_mod_email_net_test PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
  "${CMAKE_CURRENT_SOURCE_DIR}/../vmime/src"
  "${CMAKE_SOURCE_DIR}/src/module_runtime/include"
  "${CMAKE_BINARY_DIR}/src/vmime"
  "${CMAKE_BINARY_DIR}/src"
  "${CMAKE_BINARY_DIR}/src/sdk")

# The REAL module-side SDK, against a fake host table the harness supplies.
# That is the point of the split: the SDK is stateless and takes a context, so
# a test can hand it one without a host anywhere in the process.
target_link_libraries(gf_mod_email_net_test PRIVATE
  GTest::gtest GTest::gtest_main Qt::Core Qt::Network vmime-static
  OpenSSL::SSL OpenSSL::Crypto gf_sdk)

# Only the module export macro needs neutering now: nothing here defines an
# SDK symbol any more.
target_compile_definitions(gf_mod_email_net_test PRIVATE
  VMIME_STATIC "GF_MODULE_EXPORT=")

set_target_properties(gf_mod_email_net_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${GPGFRONTEND_MODULE_TEST_OUTPUT_DIR}")

# --- crypto boundary tests --------------------------------------------------
#
# What octets actually reach GPG.
#
# EMailBasicGpgOpera.cpp is the only file in the module that calls the crypto
# SDK, and everything it passes is a claim about a byte range: the signature
# covers THESE bytes, the digest shown to the user describes THESE bytes. A
# conversion anywhere in that path that drops or rewrites an octet turns a
# correct-looking GOOD verdict into a statement about data the user never saw.
#
# So this target links the real opera against a recording fake SDK and asserts
# byte equality between the slice the module says it verified and what the fake
# was handed. The stubs mirror the shipping GFModuleStrDup, NUL truncation
# included -- weakening them would hide exactly the bug they exist to catch.

add_executable(gf_mod_email_crypto_test
  EMailCryptoBoundaryTest.cpp
  # One verification, one result, every surface quoting it: the aggregation
  # rules, the staleness anchor, the wire form, and the whole-message walk.
  # Here rather than in gf_mod_email_test because the walk needs the recording
  # fake engine that only this target supplies.
  EMailVerifyUnificationTest.cpp
  EMailCryptoSdkStubs.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../EMailBasicGpgOpera.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../EMailVerificationPayload.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../EMailHelper.cpp)

target_include_directories(gf_mod_email_crypto_test PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
  "${CMAKE_CURRENT_SOURCE_DIR}/../vmime/src"
  "${CMAKE_SOURCE_DIR}/src/module_runtime/include"
  "${CMAKE_BINARY_DIR}/src/vmime"
  "${CMAKE_BINARY_DIR}/src"
  "${CMAKE_BINARY_DIR}/src/sdk")

target_link_libraries(gf_mod_email_crypto_test PRIVATE
  GTest::gtest GTest::gtest_main Qt::Core Qt::Network vmime-static gf_sdk)

target_compile_definitions(gf_mod_email_crypto_test PRIVATE
  VMIME_STATIC "GF_MODULE_EXPORT=")

set_target_properties(gf_mod_email_crypto_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${GPGFRONTEND_MODULE_TEST_OUTPUT_DIR}")

# --- fuzzing (opt-in, clang only) -------------------------------------------
#
# The MIME parsers run on unauthenticated input -- a message has to be parsed
# before anything can say whether it is signed -- so they get a libFuzzer
# harness as well as the corpus tests. Off by default: it needs clang and a
# sanitizer build, and it never runs as part of an ordinary test pass.
#
#   cmake -B build-fuzz -DGPGFRONTEND_MODULES_BUILD_TESTS=ON \
#         -DGPGFRONTEND_MODULES_BUILD_FUZZERS=ON \
#         -DCMAKE_CXX_COMPILER=clang++ \
#         -DCMAKE_CXX_FLAGS="-fsanitize=fuzzer-no-link,address,undefined"
#   build-fuzz/artifacts/modules/gf_mod_email_fuzz \
#         modules/src/m_email/test/corpus/public -max_len=1048576
#
# corpus/public is the seed set: those messages are malformed on purpose, so
# they are the interesting starting points. corpus/golden works too, but it is
# well-formed by construction and explores less.
option(GPGFRONTEND_MODULES_BUILD_FUZZERS
  "Build libFuzzer harnesses for the modules (requires clang)" OFF)

if(GPGFRONTEND_MODULES_BUILD_FUZZERS)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS "fuzzers requested but the compiler is not clang; skipping")
  else()
    add_executable(gf_mod_email_fuzz
      EMailMimeFuzz.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../EMailHelper.cpp)

    target_include_directories(gf_mod_email_fuzz PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/.."
      "${CMAKE_CURRENT_SOURCE_DIR}/../vmime/src"
      "${CMAKE_BINARY_DIR}/src/vmime"
      "${CMAKE_BINARY_DIR}/src")

    target_link_libraries(gf_mod_email_fuzz PRIVATE Qt::Core vmime-static)
    target_compile_options(gf_mod_email_fuzz PRIVATE -fsanitize=fuzzer)
    target_link_options(gf_mod_email_fuzz PRIVATE -fsanitize=fuzzer)

    # Deliberately NOT named *_test: run_tests.sh picks module test binaries up
    # by that suffix, and a fuzzer given no corpus argument would run forever.
    set_target_properties(gf_mod_email_fuzz PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${GPGFRONTEND_MODULE_TEST_OUTPUT_DIR}")
  endif()
endif()
