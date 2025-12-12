# GpgFrontend Modules

This is a sub-repository of the main GpgFrontend project and must live under
`GpgFrontend/modules`. It cannot be built on its own: toolchains, SDK headers,
Qt setup, and output paths all come from the superproject's CMake. The modules
are built alongside the app and loaded at startup. You can disable them in the
app settings, or remove a module in archive installs by deleting its shared
library file.

## Integrated Modules (current)

- `m_ver_check`: checks for new GpgFrontend releases.
- `m_gpg_info`: collects and displays local GnuPG information.
- `m_paper_key`: exports key material in a paper-friendly format.
- `m_key_server_sync`: searches and exchanges keys with key servers (HKP/VKS).
- `m_email`: email helper built on vmime (linked statically in-tree).

## Build Notes

- Build from the GpgFrontend superproject; the top-level `CMakeLists.txt`
  includes this subdir when `GPGFRONTEND_BUILD_MODULES=ON`.
- Default toolchain targets Qt 6; if you enable Qt 5 (`GPGFRONTEND_QT5_BUILD`),
  the modules target is skipped and not built.
- Module sources live under `modules/src`; SDK headers come from the main tree
  (`src/sdk`). Outputs are placed under the `modules` subfolder of the runtime
  output directory configured by the superproject.
- Translations are generated with `qt_add_translations`; `.ts` files live next
  to each module.

## Developing Modules

- Follow the patterns in `modules/src/*/CMakeLists.txt` and use the SDK headers
  provided by the main repo.
- Register your module via `register_module(...)`; superproject CMake enables
  `CMAKE_AUTOMOC`, `CMAKE_AUTORCC`, and `CMAKE_AUTOUIC`.
- Match the Qt major version used by the host build to keep the module loadable.

## Licensing

All modules in this directory are released under GPL-3.0-or-later, consistent
with the main project.
