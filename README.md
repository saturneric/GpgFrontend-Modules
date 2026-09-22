# GpgFrontend Modules

This directory is a sub-repository of the main GpgFrontend project and must
live at `GpgFrontend/modules`. It cannot be built standalone: toolchains, SDK
headers, Qt setup, and output paths all come from the superproject's CMake. The
modules are built alongside the application, packaged into signed `.gfmodule`
descriptors, and loaded at startup. You can disable individual modules in the
application settings, or remove one from an archive install by deleting its
namespace directory under the modules root (see
[Packaging, Signing & Distribution](#packaging-signing--distribution)).

## Integrated Modules

| Directory           | Module ID                      | Description                                    |
| ------------------- | ------------------------------ | ---------------------------------------------- |
| `m_ver_check`       | `…module.version_checking`     | Checks for new GpgFrontend releases.           |
| `m_gpg_info`        | `…module.gnupg_info_gathering` | Displays local GnuPG installation details.     |
| `m_key_server_sync` | `…module.key_server_sync`      | Searches and syncs keys with HKP/VKS servers.  |
| `m_email`           | `…module.email`                | E-mail helpers built on vmime.                 |
| `m_pgp_inspect`     | `…module.pgp_inspect`          | Inspects the packet structure of OpenPGP data. |

## Repository Layout

```
modules/
├── src/
│   ├── m_ver_check/            # Version checking module
│   ├── m_gpg_info/             # GnuPG info gathering module
│   ├── m_key_server_sync/      # Key server sync module
│   ├── m_email/                # E-mail module
│   ├── m_pgp_inspect/          # OpenPGP structure inspector
│   └── CMakeLists.txt
└── CMakeLists.txt
```

Each module directory holds a `module.json` (its signed identity — see
[Writing a New Module](#writing-a-new-module)) beside its `CMakeLists.txt`.

The module-facing SDK is not in this sub-repository. Two layers live in the
main tree:

- `src/sdk/` — the public SDK (`gf_sdk`): the `GFSDK*` functions and structs a
  module calls directly (`GFGpgEncrypt`, `GFStorageStateGetText`, …), or
  through the `gf::sdk::` C++ conveniences in `GFSDK.hpp`. Stateless — every
  call that needs the host takes an explicit context argument. See
  [`src/sdk/README.md`](../src/sdk/README.md) for the full architecture.
- `src/module_runtime/include/` — `GFModule.h` and friends, statically linked
  into every module. This is what a module author actually includes: it
  supplies the event/result types, memory helpers, logging macros, and the
  one bootstrap function (`GFModuleGetApi`) that negotiates the ABI with the
  host and dispatches events to your hooks.

Both are added to the include path automatically by `gf_add_module()`.

## Build Notes

- Enable modules by setting `-DGPGFRONTEND_BUILD_MODULES=ON` in the
  superproject CMake configuration.
- The default toolchain targets **Qt 6**. If `GPGFRONTEND_QT5_BUILD` is
  enabled, the modules target is skipped entirely.
- `CMAKE_AUTOMOC`, `CMAKE_AUTORCC`, and `CMAKE_AUTOUIC` are enabled
  automatically for all modules.
- Translations, packaging, and signing are all handled by `gf_add_module()` —
  see below. There is nothing left to wire up by hand in a module's own
  `CMakeLists.txt`.
- Translations use the shared locale set: it is declared once as
  `GPGFRONTEND_SUPPORTED_LOCALES` in
  [`cmake/Translations.cmake`](../cmake/Translations.cmake), and `.ts` files
  live under each module's `ts/` subdirectory, named after its
  `translation_context`. Run
  [`scripts/update_translations.sh`](../scripts/update_translations.sh)
  to create/refresh the `.ts` files for the app and every module at once.

## Writing a New Module

### 1. Create the directory

```
modules/src/my_module/
```

Add `add_subdirectory(my_module)` to `modules/src/CMakeLists.txt`.

### 2. Declare the module's identity in `module.json`

```json
{
  "schema_version": 1,
  "id": "com.example.my_module",
  "version": "1.0.0",
  "name": "MyModule",
  "description": "What this module does.",
  "author": "Your Name",
  "capabilities": ["ui"],
  "events": ["APPLICATION_LOADED"],
  "translation_context": "ModuleMyModule"
}
```

An optional `"min_host_version"` field pins the oldest GpgFrontend release the
module requires; it defaults to the version of GpgFrontend it was built
against.

This file is the single source of truth for the module's identity: it feeds
the generated C++ identity header, the translation wiring, and the signed
package manifest, so none of those can drift apart.

`events` is an allowlist, enforced by the **host**: it refuses to subscribe a
packaged module to an event its signed manifest does not name, and refuses
any id this host never fires at all. The runtime additionally reconciles the
list against your handler table and will not activate if they disagree, so
listing an event you don't handle — or handling one you didn't list — is an
error rather than a silent no-op.

`capabilities` decides what your module can reach, and it is enforced. At
activation the host mints a capability table from this list; a group you did
not declare is simply absent, and the matching SDK call returns its failure
value and logs once instead of doing anything. Two kinds of name go in the
list:

| declared  | kind     | effect                                                           |
| --------- | -------- | ---------------------------------------------------------------- |
| `gpg`     | granted  | sign, encrypt, decrypt, verify, keys, key lists, result analysis |
| `pgp`     | granted  | packet-structure inspection, no keyring or engine                |
| `ui`      | granted  | widgets, dialogs, theme colours, settings pages, tab views       |
| `editor`  | granted  | reading the document the user currently has open                 |
| `storage` | granted  | application settings, the caches, the runtime register table     |
| `process` | granted  | running an external program                                      |
| `network` | recorded | **not** enforced — see below                                     |

Buffers, memory, logging, event subscription and translations are always
available and are not declared.

`network` is recorded, signed and shown to the user, but the host does not
mediate it: your module opens a socket through Qt, and there is nothing in
between for the host to withhold. It is kept separate from the granted list
rather than mixed in, so that a permission list never claims to enforce
something it cannot.

Declaring a capability you do not use costs the user trust for nothing.
Failing to declare one you do use is caught at run time, by a log line naming
the capability and the entry point that wanted it — for example:

```
GFGpgDecrypt needs the "gpg" capability, which this module's signed manifest
does not declare. Add it to module.json's "capabilities" and rebuild; the
call did nothing.
```

An unknown capability name fails the build, at configure time, naming the
vocabulary.

### 3. Implement the module

```cpp
// MyModule.cpp
#include <GFModule.h>
#include "GFModuleIdentity.h"  // generated from module.json by gf_add_module()

auto OnActivate() -> GFResult {
  LOG_INFO("MyModule activating");
  return GFResult::Ok();
}

auto OnMainWindowMenuMounted(const GFEvent& event) -> GFEventResult {
  QMenu* help_menu = nullptr;
  if (auto r = event.RequireGui("help_menu", help_menu); !r.ok) return r;
  // ... add actions to help_menu ...
  return GFEventResult::Ok();
}

auto OnDeactivate() -> GFResult { return GFResult::Ok(); }
auto OnUnload() -> void { LOG_INFO("MyModule unloading"); }

constexpr GFEventBinding kEvents[] = {
    {"MAINWINDOW_MENU_MOUNTED", &OnMainWindowMenuMounted},
};

constexpr GFModuleHooks kHooks = {
    sizeof(GFModuleHooks),
    GF_MODULE_ID, GF_MODULE_VERSION, GF_MODULE_TRANSLATION_CONTEXT,
    &OnActivate, &OnDeactivate, &OnUnload,
    kEvents, std::size(kEvents),
};

extern "C" GF_MODULE_EXPORT auto GFModuleGetApi(uint32_t abi)
    -> const GFModuleApi* {
  return GFModuleRuntimeGetApi(abi, &kHooks);
}
```

Every hook in `GFModuleHooks` is optional — pass `nullptr` for one you have
nothing to say about, as the real `m_ver_check` module does for
`on_deactivate` when everything it registered was event-driven rather than
handed to the host directly. `GFModuleGetApi` is the one piece of ABI a
module writes by hand, deliberately not hidden behind a macro: it is what
makes the linker keep `gf_module_runtime`'s entry point, since nothing else
in a module references it — the host resolves the symbol after the module is
linked.

### 4. Add CMakeLists.txt

```cmake
gf_add_module(
  NAME   my_module
  QT     Core Widgets
  # UI_DIR ui              # if you have a Qt Designer forms directory
  # LINK   some_target      # extra libraries, if any
)
```

`gf_add_module()` reads `module.json`, builds the target from every source
file in the directory (or `SOURCES` if given explicitly), generates the
identity header, wires up translations for every locale in
`GPGFRONTEND_SUPPORTED_LOCALES`, and builds + signs the resulting
`.gfmodule` package — all from those handful of values. See
[`modules/src/m_ver_check/CMakeLists.txt`](src/m_ver_check/CMakeLists.txt)
for a real, minimal example.

After adding or changing `tr()`/`GC_TR()` strings, run
[`scripts/update_translations.sh`](../scripts/update_translations.sh) to sync
every locale's `.ts` file — it creates any that are missing, so you never
hand-author them.

> Match the Qt major version of the host build — a version mismatch will
> prevent the module from loading.

## Key Concepts

### What a module may link

Two static archives, both module-side, both linked for you by
`gf_add_module()`:

- **`gf_sdk`** — the public SDK. Every `GFSDK*` function you call is defined
  here, forwarding to the capability table the host handed your module. It is
  **stateless**: no bound table, no current module, no thread-local.
- **`gf_module_runtime`** — the per-module runtime state: your context,
  identity, hooks, event dispatch, translations.

Your module has no link edge to `gf_core`, `gf_ui` or `gf_host_api`, and
cannot acquire one: `gf_add_module()` fails at configure time if a host
library appears anywhere in your link closure, the module is linked with
`-Wl,--no-undefined`, and `scripts/check_module_boundary.sh` inspects the
built native afterwards and fails if it imports any `GF*` symbol or exports
anything but `GFModuleGetApi`.

If you need something the SDK does not offer, the change is a primitive in
`src/sdk/GFSDKHostApi.h`, its implementation in `src/sdk/host/`, and a wrapper
in `src/sdk/api/` — not a link line.

### Every host call takes a context

```cpp
auto OnSomething(const GFEvent& e) -> GFEventResult {
  auto* ctx = e.Context();                       // from the event
  auto keys = gf::sdk::ExportKey(ctx, channel, key_id, true);
  LOG_I() << "exported" << keys.size() << "bytes";
  return GFEventResult::Ok();
}
```

Outside a handler, `GFModuleSdkContext()` returns the same context. It is a
plain load of a pointer that never changes after activation, so it is safe
from any thread — but work you start on a worker should **capture** the
context rather than look it up there. Capturing is what makes a piece of
asynchronous code say which module it belongs to, which is the whole reason
the context is an argument instead of a global.

Functions that need nothing from the host take no context at all:
`GFCompareSoftwareVersion` and `GFUIHumanSize` are the two.

### Logging

Five levels, `LOG_T` (trace) through `LOG_E` (error). The stream form is the
one to reach for:

```cpp
LOG_W() << "open" << path << "failed:" << file.errorString();
LOG_T() << "option line:" << line << "fields:" << fields.size();
```

It takes no format string, so there is no placeholder to get wrong and no
arity to mismatch, and it prints anything QDebug can — enums, `bool`,
`QByteArray`, containers. It is spelled exactly like the host's own
`LOG_W() << ...`, and — like every other module-side call — needs no explicit
context: it reads `GFModuleSdkContext()` internally.

The older forms remain:

```cpp
LOG_INFO("started");                          // one fixed string
FLOG_INFO("started with %1 accounts", count); // Qt placeholders, NOT printf
```

`FLOG_*` uses `QString::arg`, so `%1`/`%2` — a printf `%d` prints itself, and
an argument `arg()` has no overload for must be converted by hand. That is
what the stream form avoids.

Each module logs under its own category, named for its id, so a log line says
which module wrote it and which of its source lines it came from:

```
[module.email] [W] EMailImapController.cpp:412 - imap connect failed
```

Trace is for per-item chatter and stays off at `--log-level debug`; run with
`--log-level trace` to see it.

### Memory management

**SDK arguments are borrowed.** Pass a string straight through —
`id.toUtf8().constData()` for a `QString`, or a string literal — and keep
owning it. Do not pre-allocate a copy to hand over; nothing on the other side
frees it.

**SDK return values are owned.** Reclaim a returned `char*` with `UDUP` (or
`USECDUP` for one from the secure/wiping allocator):

```cpp
QString locale = UDUP(GFAppLocale(GFModuleSdkContext()));
```

The one exception to "arguments are borrowed" is a struct a module builds and
hands over _whole_ — `GFModuleEvent`, `GFModuleEventParam`,
`GFCommandExecuteContext` and the like — whose `char*` members still have to
be allocated with `GFModuleStrDup` (`DUP(...)`), because ownership of the
whole struct is what transfers.

```cpp
DUP("hello")      // char* owned by the callee — only for a transferred struct field
SECDUP("secret")  // same, from the wiping allocator, for a secret
UDUP(ptr)         // consume an owned char* return value, get a QString
USECDUP(ptr)      // same as UDUP, but frees with the wiping allocator
```

### Events

Bind event ids to handlers in a static `GFEventBinding[]` table (see step 3
above) — there is no separate subscribe call, and the set must match
`module.json`'s `events` array exactly. Every handler returns a
`GFEventResult`; the runtime does the transport, so there is nothing to
answer manually on the common path.

Which events exist, and what each one MEANS, is written down once in
[`src/core/module/ModuleEventRegistry.cpp`](../src/core/module/ModuleEventRegistry.cpp).
That catalogue is what the host checks a subscription against, so an id that
is not in it is refused rather than silently never delivered. Each entry
records:

- **observe or extend.** Most events tell you something happened and do not
  read your reply. A small, deliberate set are extension points where what
  you return, or what you do to a borrowed host object, changes the outcome —
  the `EDIT_TAB_TYPE_*_OP_*` operations (your reply replaces the user's
  document), the `FILE_EXT_*` handoffs (the host stops and leaves the work to
  you), the key-server requests, and the menu/tab mounting points.
- **whether the reply is read at all.** `MAINWINDOW_MENU_MOUNTED` hands you a
  menu and ignores what you return; you extend by mutating the menu, not by
  answering.
- **whether you may defer.** Where the flag allows it, return
  `GFEventResult::Deferred()` and answer later through `event.Answer()`,
  from any thread.

Nothing is a veto. No trigger site cancels an operation on a module's word,
and `TriggerEvent` is asynchronous and returns nothing, so a veto is not
expressible today.

```cpp
auto OnMainWindowMenuMounted(const GFEvent& event) -> GFEventResult {
  QMenu* menu = nullptr;
  if (auto r = event.RequireGui("help_menu", menu); !r.ok) return r;
  // ... add actions ...
  return GFEventResult::Ok();
}
```

A handler that starts asynchronous work returns `GFEventResult::Deferred()`
and answers later through `event.Answer()`, from any thread, at most once —
see `CheckUpdate()` in
[`m_ver_check/VersionCheckingModule.cpp`](src/m_ver_check/VersionCheckingModule.cpp)
for a real example.

### Runtime values

A shared, typed register table (`storage` capability) for live
configuration, visible to the host and to other modules. Keys and namespaces
are lower-case; text and bool are kept apart because a lookup of the wrong
type is a miss, not a conversion. Use the `gf::sdk::` wrappers
(`GFSDK.hpp`), which apply a fallback where the raw calls only report
absence:

```cpp
auto* ctx = GFModuleSdkContext();
gf::sdk::SetStateBool(ctx, "my_module", "ready", true);
bool ready = gf::sdk::StateBool(ctx, "my_module", "ready", /*fallback=*/false);

gf::sdk::SetStateText(ctx, "my_module", "last_error", message);
QString v = gf::sdk::StateText(ctx, "my_module", "last_error");
```

### Cache

Three tiers, one set of calls, differing in how long a value lives and
whether it is wiped — `GF_STORE_SESSION` (in memory), `GF_STORE_DURABLE`
(disk, plain), `GF_STORE_SECURE_DURABLE` (disk, wiping allocator):

```cpp
auto* ctx = GFModuleSdkContext();
gf::sdk::SetCacheText(ctx, GF_STORE_SESSION, "key", "value");
gf::sdk::SetCacheText(ctx, GF_STORE_DURABLE, "token", "abc", /*ttl_seconds=*/3600);
QString v = gf::sdk::CacheText(ctx, GF_STORE_DURABLE, "token");
gf::sdk::RemoveCache(ctx, GF_STORE_DURABLE, "token");
```

### GPG operations

Every host call takes the context explicitly, first argument, per
[Every host call takes a context](#every-host-call-takes-a-context). Crypto
entry points return through a move-only `GFGpgResult(ctx)`, which owns
whatever native result it wraps and releases it on every path (including an
early return) through its own destructor. Payloads cross the boundary as
`GFBuf`/`GFBufferView`, not raw `char*`, since a signature or an encrypted
blob is exact octets rather than text.

```cpp
auto* ctx = GFModuleSdkContext();
auto in = GFBuf::Copy(ctx, plain_bytes);
QVector<const char*> key_ids = ...;  // one entry per recipient/signer key id

GFGpgResult r(ctx);
if (GFGpgEncrypt(ctx, channel, key_ids.data(), key_ids.size(), in.View(),
                 /*ascii=*/1, r.Out()) != GF_GPG_OK) {
  // r.ErrorString() explains why the call itself failed
}
if (r.Error() != GF_GPG_ERR_NO_ERROR) {
  // the operation ran but gpg reported an error; r.ErrorString() again
}
auto encrypted = r.DataCopy();
```

`GFGpgSign`, `GFGpgDecrypt` and `GFGpgVerify` follow the same shape. Use
`GFGpgCurrentChannel(ctx)` to obtain the active channel from the main window.
See
[`m_email/EMailBasicGpgOpera.cpp`](src/m_email/EMailBasicGpgOpera.cpp) for
real, complete call sites including error handling.

If you need the same structured result the host's own crypto dialogs show
(recipients, signatures, validity), use `gf::sdk::AnalyseResult(ctx,
operation, channel, err, r.CapsuleId())` — this is engine neutral, working
the same way whether the active engine is GnuPG or rPGP.

### UI

All Qt widget creation and dialog display must happen on the main thread. The
SDK handles the dispatch automatically.

```cpp
// Create a widget on the main thread (dispatches there for you if called
// from a worker)
void* dlg = GUI_OBJECT(MyDialogFactory, QVariant("arg"));

// Show it (non-blocking); "main_window" style handles come from an event's
// own parameters, resolved with event.RequireGui<T>() — see Events below
GFUIShowDialog(GFModuleSdkContext(), dlg, parent_handle);
```

None of the shipped modules currently build a dialog off the main thread —
`m_ver_check`'s update dialog, for example, is built directly with `new
QDialog(parent)` from a menu-click handler, which already runs on the main
thread. Reach for `GUI_OBJECT`/`GFUIShowDialog` only when you need to build
or show UI from code that is not already there.

#### Settings pages

A module can own a page in the application's Settings dialog. Register it
from `OnActivate()` and drop it again in `OnDeactivate()` — the registry
holds a function pointer into your shared object, and leaving it behind
would crash the next time the dialog is built.

```cpp
constexpr auto kSettingsPageId = "com.example.mymodule.settings";

auto MySettingsPageFactory(void* /*data*/) -> void* {
  return new MySettingsPage();  // fresh, unparented, one per dialog
}

auto OnActivate() -> GFResult {
  const auto keywords = QStringList{GC_TR("proxy"), GC_TR("timeout")}.join('\n');
  gf::sdk::RegisterSettingsPage(GFModuleSdkContext(), kSettingsPageId,
                                "features", GC_TR("My Module"),
                                keywords.toUtf8().constData(),
                                MySettingsPageFactory, nullptr);
  return GFResult::Ok();
}

auto OnDeactivate() -> GFResult {
  GFUIUnregisterSettingsPage(GFModuleSdkContext(), kSettingsPageId);
  return GFResult::Ok();
}
```

`gf::sdk::RegisterSettingsPage` (from `GFSDK.hpp`) fills in the append-only
`GFUISettingsPageSpec` struct for you; see
[`m_key_server_sync/KeyServerSyncModule.cpp`](src/m_key_server_sync/KeyServerSyncModule.cpp)
for the real, complete pattern.

The dialog finds your page's `SetSettings()` and `ApplySettings()` **by
name**, so declare both as `public slots` (or `Q_INVOKABLE`). `SetSettings()`
loads the stored values and is called again if the user cancels;
`ApplySettings()` writes them on OK. Stage edits in the widget and only
persist them in `ApplySettings()` — that is what makes Cancel discard them.
Declare a `void SignalRestartNeeded(int)` signal if a change on your page
needs one; the dialog connects to it if it is there.

Register `title` and `keywords` untranslated, with `GC_TR(...)`: modules are
activated before their translator is installed, so anything translated at
registration time would be frozen at its source text for the rest of the
session. `section_id` is one of `application`, `keys_engines`, `features`,
`system`; anything else becomes its own section after those.

### Project settings

Use `GFStorageSettingsRoot(ctx)` (`storage` capability — reading a
preference is not drawing, so it does not need `ui`) to read and write
persistent application settings. Settings are shared across all modules and
the host application, so prefix every key with your module name to avoid
collisions.

```cpp
auto* settings = qobject_cast<QSettings*>(
    static_cast<QObject*>(GFStorageSettingsRoot(GFModuleSdkContext())));

// Write a value
settings->setValue("my_module/check_on_startup", true);

// Read a value with a default
bool check = settings->value("my_module/check_on_startup", false).toBool();
```

Settings are typically read in a `..._LOAD_SETTINGS` event handler and
written in a `..._APPLY_SETTINGS` handler, following the pattern used by the
built-in modules — see `OnNetworkSettingsTabLoadSettings()` /
`OnNetworkSettingsTabApplySettings()` in
[`m_ver_check/VersionCheckingModule.cpp`](src/m_ver_check/VersionCheckingModule.cpp)
for the full, real pattern, including dispatching the actual widget access
onto the main thread with `QMetaObject::invokeMethod`.

### Translations

Wrap strings with `QCoreApplication::translate("GTrC", "...")` (or `GC_TR`
for a string that must stay untranslated until later — see
[Settings pages](#settings-pages)). `GTrC` is a single, fixed translation
context declared by `GFModule.h`; it is unrelated to a module's own
`translation_context` field in `module.json`, which only names the `.qm`
file. There is nothing left to register: `gf_module_runtime` installs the
module's translator automatically during activation, before any event
subscription and before `OnActivate()` runs.

`.ts` files go in `ts/<translation_context>.<locale>.ts` and are embedded as
Qt resources under `:/i18n/`. The locale set is not chosen per module — it
comes from the shared `GPGFRONTEND_SUPPORTED_LOCALES` list (see
[Build Notes](#build-notes)), so the module always offers the same languages
as the application. After adding or changing translatable strings, run
[`scripts/update_translations.sh`](../scripts/update_translations.sh) to sync
every locale's `.ts`.

## Packaging, Signing & Distribution

Every module ships as a signed `.gfmodule` package (a manifest, its Ed25519
signature, and — for an externally-distributed module — the build public key
that signature was made with) plus the native shared library it names. The
package format, verification rules, and trust model live in
[`src/core/module/`](../src/core/module) — this section only summarises the
parts that matter for building and distributing a module.

### Integrated modules (the ones in this directory)

Nothing to do by hand: `gf_add_module()` builds, signs, and installs the
package for you as part of the normal build, using an Ed25519 keypair
generated once per build tree (`gf_module_keygen`, invoked automatically by
the top-level CMake configure). That key is compiled into the host binary as
its trust root and is never exported, published, or reused across builds —
it exists only to prove a package was produced by _this_ build tree, not to
identify a publisher.

The resulting layout, under the modules root (`build/artifacts/modules` in a
dev build):

```
<namespace-root>/<key>/module.gfmodule       # manifest + signature
<namespace-root>/<key>/native/libgf_mod_xxx.so
```

`<key>` is derived from the module's id (`ModuleDirectoryKey()`, e.g.
`com.bktus.gpgfrontend.module.email` → `email-a1b2c3d4e5f60718293a`); it is a
locator only, never trusted on its own — the manager re-derives it from the
signed manifest's `id` and refuses a descriptor sitting in the wrong
directory.

### External modules (a user's own)

An external module lives outside the application entirely, in the user's
profile under `mods/<key>/`, with the same `module.gfmodule` + `native/`
layout as above. For it to load:

1. The user sets **Settings → General → Module Discovery** to "Also Look For
   Modules I've Added". This only controls whether the `mods/` directory is
   scanned at all — finding a module there never implies running it.
2. In the **Module Controller**, the user trusts the build key the module was
   signed with ("signatures by this key are worth considering") and then
   enables that specific module ("run this one"). These are deliberately two
   separate decisions: trusting a key applies to every module that key ever
   signs, so collapsing it with "run this one" into a single click would
   make the first of them too easy to grant by accident.
3. Both take effect on the next restart.

**Current limitation:** there is no first-party tool to sign an external
module with your own keypair. `gf_module_packager` (the CLI `gf_add_module`
calls internally) only signs against the build tree's own compiled-in key,
so it can only ever produce _integrated_-shaped packages. Producing a valid
external `.gfmodule` today means reproducing the format by hand: a
canonically-serialised `manifest.json` (see
[`ModuleManifest.h`](../src/core/module/ModuleManifest.h) for the schema),
an Ed25519 detached signature over its exact bytes, and the signer's public
key, zipped together as `META-INF/manifest.json`, `META-INF/manifest.sig`,
and `META-INF/build-key.pub`. The test suite constructs exactly this by hand
with libsodium — see
[`src/test/core/ModuleDescriptorArchive.h`](../src/test/core/ModuleDescriptorArchive.h)
and
[`src/test/core/GpgCoreTestModuleExternalTrust.cpp`](../src/test/core/GpgCoreTestModuleExternalTrust.cpp)
— but there is no supported command-line workflow for it yet.

### macOS has no external modules

External modules are refused unconditionally on macOS: Library Validation
means the process cannot load a `dylib` that does not carry this
application's own Team ID, and a third-party module by definition does not.
This is a platform policy, not a missing feature — admitting one past
GpgFrontend's own checks would still have it refused by `dyld`.

## Licensing

All modules in this directory are released under **GPL-3.0-or-later**,
consistent with the main GpgFrontend project.
