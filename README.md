# GpgFrontend Modules

This directory is a sub-repository of the main GpgFrontend project and must
live at `GpgFrontend/modules`. It cannot be built standalone: toolchains, SDK
headers, Qt setup, and output paths all come from the superproject's CMake. The
modules are built alongside the application and loaded at startup. You can
disable individual modules in the application settings, or remove a module from
an archive install by deleting its shared-library file.

---

## Integrated Modules

| Directory           | Module ID                      | Description                                   |
| ------------------- | ------------------------------ | --------------------------------------------- |
| `m_ver_check`       | `…module.version_checking`     | Checks for new GpgFrontend releases.          |
| `m_gpg_info`        | `…module.gnupg_info_gathering` | Displays local GnuPG installation details.    |
| `m_key_server_sync` | `…module.key_server_sync`      | Searches and syncs keys with HKP/VKS servers. |
| `m_email`           | `…module.email`                | E-mail helpers built on vmime.                |

---

## Repository Layout

```
modules/
├── include/                    # Shared module-side helpers (header-only)
│   ├── GFModuleDeclare.h       # GF_MODULE_API_DECLARE — forward-declares all entry points
│   ├── GFModuleDefine.h        # GF_MODULE_API_DEFINE_V2 — implements metadata & event dispatch
│   ├── GFModuleCommonUtils.hpp # DUP/UDUP macros, logging, CB_SUCC/CB_ERR, Qt<->C helpers
│   └── GFModuleExport.h        # GF_MODULE_EXPORT visibility attribute
├── src/
│   ├── m_ver_check/            # Version checking module
│   ├── m_gpg_info/             # GnuPG info gathering module
│   ├── m_key_server_sync/      # Key server sync module
│   └── m_email/               # E-mail module
└── CMakeLists.txt
```

SDK headers are at `src/sdk/` in the main tree and added to the include path
automatically. See [`src/sdk/README.md`](../src/sdk/README.md) for the full API
reference.

---

## Build Notes

- Enable modules by setting `-DGPGFRONTEND_BUILD_MODULES=ON` in the
  superproject CMake configuration.
- The default toolchain targets **Qt 6**. If `GPGFRONTEND_QT5_BUILD` is
  enabled, the modules target is skipped entirely.
- Module shared libraries are placed under the `modules/` subdirectory of the
  runtime output directory.
- `CMAKE_AUTOMOC`, `CMAKE_AUTORCC`, and `CMAKE_AUTOUIC` are enabled
  automatically for all modules.
- Translations use `qt_add_translations`; `.ts` files live under each module's
  `ts/` subdirectory.

---

## Writing a New Module

### 1. Create the directory

```
modules/src/my_module/
```

Add `add_subdirectory(my_module)` to `modules/src/CMakeLists.txt`.

### 2. Declare the entry points

```cpp
// MyModule.h
#pragma once
#include "GFModuleDeclare.h"
GF_MODULE_API_DECLARE
```

### 3. Implement the module

```cpp
// MyModule.cpp
#include "MyModule.h"

#include <GFSDKBasic.h>
#include <GFSDKLog.h>

#include "GFModuleDefine.h"
#include "GFModuleCommonUtils.hpp"

GF_MODULE_API_DEFINE_V2(
    "com.example.my_module",   // unique ID — lower-case, dot-separated
    "MyModule",                // display name
    "1.0.0",                   // version
    "What this module does.",  // description
    "Your Name"                // author
);

auto GFRegisterModule() -> int { return 0; }

auto GFActiveModule() -> int {
    LISTEN("APPLICATION_LOADED");
    return 0;
}

REGISTER_EVENT_HANDLER(APPLICATION_LOADED, [](const MEvent& event) -> int {
    LOG_INFO("Hello from MyModule!");
    CB_SUCC(event);
});

auto GFDeactivateModule() -> int { return 0; }
auto GFUnregisterModule() -> int { return 0; }
```

### 4. Add CMakeLists.txt

```cmake
# Collect all source files in this directory
set(MODULE_SOURCE "")
aux_source_directory(. MODULE_SOURCE)

# Register the module; MODULE_TARGET is set to the resulting target name
register_module(my_module MODULE_TARGET ${MODULE_SOURCE})

# Link Qt modules your code needs
target_link_libraries(${MODULE_TARGET} PRIVATE Qt::Core Qt::Widgets)

# --- Translations (optional) ---
set(LOCALE_TS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/ts)
set(TS_FILES
  "${LOCALE_TS_PATH}/ModuleMyModule.en_US.ts"
  "${LOCALE_TS_PATH}/ModuleMyModule.zh_CN.ts")

if(NOT XCODE_BUILD)
  qt_add_translations(${MODULE_TARGET}
    RESOURCE_PREFIX "/i18n"
    TS_FILES ${TS_FILES}
    SOURCES ${MODULE_SOURCE}
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR})
else()
  # Xcode requires a separate target for lrelease
  add_custom_target("${MODULE_TARGET}_i18n" ALL)
  qt_add_lrelease("${MODULE_TARGET}_i18n"
    TS_FILES ${TS_FILES}
    QM_FILES_OUTPUT_VARIABLE TRANSLATIONS_QM)
  qt_add_resources(${MODULE_TARGET} "${MODULE_TARGET}_i18n"
    PREFIX "/i18n"
    BASE ${CMAKE_CURRENT_BINARY_DIR}
    FILES ${TRANSLATIONS_QM})
endif()
```

> Match the Qt major version of the host build — a version mismatch will
> prevent the module from loading.

---

## Key Concepts

### Memory management

All strings and buffers crossing the module boundary must use the SDK
allocator. Never mix `new`/`delete` or `malloc`/`free` with SDK pointers.

```cpp
DUP("hello")      // char* owned by callee — pass to SDK functions
UDUP(ptr)         // consume char*, return QString and free
SECDUP("secret")  // same as DUP but uses the secure (zeroed-on-free) allocator
USECDUP(ptr)      // same as UDUP but frees with the secure allocator
```

A parameter typed `char*` (not `const char*`) takes ownership — always pass
`DUP(...)` at the call site. A returned `char*` is caller-owned — use `UDUP`
to convert and free in one step.

### Events

Subscribe in `GFActiveModule` and handle with `REGISTER_EVENT_HANDLER`.
Every handler must end with `CB_SUCC` or `CB_ERR` to continue the callback
chain.

```cpp
auto GFActiveModule() -> int {
    LISTEN("MAINWINDOW_MENU_MOUNTED");
    return 0;
}

REGISTER_EVENT_HANDLER(MAINWINDOW_MENU_MOUNTED, [](const MEvent& event) -> int {
    auto* menu = GFUIGetGUIObjectAs<QMenu>(event["help_menu"]);
    if (!menu) CB_ERR(event, -1, "no help_menu");
    // ... add actions ...
    CB_SUCC(event);
});
```

### Runtime values

A shared in-process key-value store for live configuration. Keys and
namespaces are normalised to lower-case.

```cpp
GFModuleUpsertRTValue(DUP("my_module"), DUP("ready"), DUP("true"));
QString v = UDUP(GFModuleRetrieveRTValueOrDefault(
    DUP("my_module"), DUP("ready"), DUP("false")));
```

### Cache

```cpp
// Session cache
GFCacheSave(DUP("key"), DUP("value"));
GFCacheSaveWithTTL(DUP("token"), DUP("abc"), 3600);
QString v = UDUP(GFCacheGet(DUP("key")));

// Durable cache (survives restart, JSON values)
GFDurableCacheSave(DUP("state"), DUP(QJsonDocument(obj).toJson()));
QString raw = UDUP(GFDurableCacheGet(DUP("state")));
```

### GPG operations

```cpp
// Sign
GFGpgSignResult* sr = nullptr;
GFGpgSignData(channel, key_ids, count, DUP(plain), 0, 1 /*ascii*/, &sr);
QString sig = UDUP(sr->signature);
GFGpgFreeResult(sr->gpgme_sign_result);
GFFreeMemory(sr);

// Encrypt
GFGpgEncryptionResult* er = nullptr;
GFGpgEncryptData(channel, key_ids, count, DUP(plain), 1, &er);
QString cipher = UDUP(er->encrypted_data);
GFGpgFreeResult(er->gpgme_encrypt_result);
GFFreeMemory(er);

// Decrypt
GFGpgDecryptResult* dr = nullptr;
GFGpgDecryptData(channel, DUP(cipher), &dr);
QString plain = UDUP(dr->decrypted_data);
GFGpgFreeResult(dr->gpgme_decrypt_result);
GFFreeMemory(dr);
```

Use `GFGpgCurrentGpgContextChannel()` to obtain the active channel from the
main window.

### UI

All Qt widget creation and dialog display must happen on the main thread. The
SDK handles the dispatch automatically.

```cpp
// Create a widget on the main thread
void* dlg = GUI_OBJECT(MyDialogFactory, QVariant("arg"));

// Show a dialog (non-blocking)
GFUIShowDialog(dlg, GFUIGetGUIObject(DUP("main_window")));
```

### Project settings

Use `GFUIGlobalSettings()` to read and write persistent application settings.
Settings are shared across all modules and the host application, so prefix
every key with your module name to avoid collisions.

```cpp
auto* settings = qobject_cast<QSettings*>(
    static_cast<QObject*>(GFUIGlobalSettings()));

// Write a value
settings->setValue("my_module/check_on_startup", true);
settings->setValue("my_module/api_endpoint", "https://example.com");

// Read a value with a default
bool check = settings->value("my_module/check_on_startup", false).toBool();
QString endpoint = settings->value("my_module/api_endpoint", "").toString();

// Check whether a key has ever been written
if (!settings->contains("my_module/check_on_startup")) {
    // first run — apply defaults
}
```

Settings are typically read in a `LOAD_SETTINGS` event handler and written in
an `APPLY_SETTINGS` event handler, following the pattern used by the built-in
modules:

```cpp
REGISTER_EVENT_HANDLER(NETWORK_SETTINGS_TAB_LOAD_SETTINGS,
                       [](const MEvent& event) -> int {
    auto* settings = qobject_cast<QSettings*>(
        static_cast<QObject*>(GFUIGlobalSettings()));
    if (!settings) CB_ERR(event, -1, "settings unavailable");

    auto* tab = GFUIGetGUIObjectAs<QWidget>(event["network_settings_tab"]);
    if (!tab) CB_ERR(event, -1, "tab unavailable");

    QMetaObject::invokeMethod(QApplication::instance(), [=]() {
        auto* cb = tab->findChild<QCheckBox*>("my_check_box");
        if (cb)
            cb->setChecked(
                settings->value("my_module/check_on_startup", false).toBool());
    }, Qt::BlockingQueuedConnection);

    CB_SUCC(event);
});

REGISTER_EVENT_HANDLER(NETWORK_SETTINGS_TAB_APPLY_SETTINGS,
                       [](const MEvent& event) -> int {
    auto* settings = qobject_cast<QSettings*>(
        static_cast<QObject*>(GFUIGlobalSettings()));
    if (!settings) CB_ERR(event, -1, "settings unavailable");

    auto* tab = GFUIGetGUIObjectAs<QWidget>(event["network_settings_tab"]);
    if (!tab) CB_ERR(event, -1, "tab unavailable");

    QMetaObject::invokeMethod(QApplication::instance(), [=]() {
        auto* cb = tab->findChild<QCheckBox*>("my_check_box");
        if (cb)
            settings->setValue("my_module/check_on_startup", cb->isChecked());
    }, Qt::QueuedConnection);

    CB_SUCC(event);
});
```

### Translations

```cpp
DEFINE_TRANSLATIONS_STRUCTURE(ModuleMyModule);  // creates GTrC context

auto GFRegisterModule() -> int {
    REGISTER_TRANS_READER();
    return 0;
}
```

Wrap strings with `QCoreApplication::translate("GTrC", "...")`.
`.ts` files go in `ts/ModuleMyModule.<locale>.ts` and are embedded as Qt
resources under `:/i18n/`.

---

## Licensing

All modules in this directory are released under **GPL-3.0-or-later**,
consistent with the main GpgFrontend project.
