# SPDX-License-Identifier: GPL-3.0-or-later

# Everything about this module -- id, version, name, description, author,
# capabilities and translation context -- lives in module.json beside this file.
#
# Built only with GPGFRONTEND_MODULES_WEB_ENGINE, and then WebEngine is
# required: asking for the renderer and quietly getting none would leave
# every HTML message as source with nothing to say why.
find_package(Qt6 REQUIRED COMPONENTS WebEngineCore WebEngineWidgets)

gf_add_module(
  NAME    web_engine
  QT      Core Widgets WebEngineCore WebEngineWidgets
  SOURCES WebEngineModule.cpp
          WebEngineScheme.cpp
          WebEngineView.cpp)

if(GPGFRONTEND_MODULES_BUILD_TESTS)
  add_subdirectory(test)
endif()
