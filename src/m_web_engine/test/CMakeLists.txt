# SPDX-License-Identifier: GPL-3.0-or-later

# The policy probe: the renderer's own WebEngineView, driven exactly as the
# module drives it, against a hostile mail and a request logger. Not a gtest
# and not registered with the modules phase -- it needs a display (or
# QT_QPA_PLATFORM=offscreen) and a listening logger -- so it is run by hand,
# through scripts/prototypes/html_view/run_qtwebengine.sh.
#
# The document is built by the Host's own HtmlViewDocument.cpp, compiled in
# here as a source file: a probe of the engine against a different document
# than the one the Host hands it would prove nothing.
add_executable(gf_web_engine_probe
  WebEngineProbe.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WebEngineView.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../WebEngineScheme.cpp
  ${CMAKE_SOURCE_DIR}/src/ui/html/HtmlViewDocument.cpp)

target_include_directories(gf_web_engine_probe PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
  "${CMAKE_SOURCE_DIR}/src/sdk"
  "${CMAKE_SOURCE_DIR}/src/ui/html")

# What the Host's precompiled header gives HtmlViewDocument.cpp.
target_precompile_headers(gf_web_engine_probe PRIVATE ProbePrelude.h)

target_link_libraries(gf_web_engine_probe PRIVATE
  Qt::Core Qt::Widgets Qt::WebEngineCore Qt::WebEngineWidgets)
set_target_properties(gf_web_engine_probe PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test-bin")
