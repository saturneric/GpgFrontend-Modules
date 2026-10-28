# SPDX-License-Identifier: GPL-3.0-or-later
# The SDK-free half of the notes module: TextPack, protection, the save and
# draft coordinator, images, the page bridge, file writes.
if(NOT TARGET GTest::gtest)
  find_package(GTest QUIET)
endif()

if(NOT TARGET GTest::gtest)
  message(STATUS "GTest not found; skipping notes module tests")
  return()
endif()

add_executable(gf_mod_notes_test
  NotesTestMain.cpp
  NoteModelTest.cpp
  NoteProtectionTest.cpp
  NotePersistenceTest.cpp
  NoteImageTest.cpp
  NoteBridgeTest.cpp
  NoteFilesTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../NoteModel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../NoteProtection.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../NotePersistence.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../NoteImage.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../NoteBridge.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../NoteFiles.cpp)

target_include_directories(gf_mod_notes_test PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
  "${CMAKE_CURRENT_BINARY_DIR}/..")

target_link_libraries(gf_mod_notes_test PRIVATE GTest::gtest Qt::Core Qt::Gui)

set_target_properties(gf_mod_notes_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${GPGFRONTEND_MODULE_TEST_OUTPUT_DIR}")
gf_register_module_test(gf_mod_notes_test)
