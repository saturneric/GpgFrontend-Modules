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
  "commands": ["com.example.my_module.show_about"],
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

`commands` lists every command the module provides. Each id starts with the
module id and a dot, is lower-case and dotted, and appears once. Like
`events`, it is checked in both directions: the runtime will not activate a
module whose bound commands and signed list disagree.

`capabilities` decides what your module can reach, and it is enforced. At
activation the host mints a capability table from this list; a group you did
not declare is simply absent, and the matching SDK call returns its failure
value and logs once instead of doing anything. Two kinds of name go in the
list:

| declared  | kind     | effect                                                           |
| --------- | -------- | ---------------------------------------------------------------- |
| `gpg`     | granted  | sign, encrypt, decrypt, verify, keys, key lists, result analysis |
| `pgp`     | granted  | packet-structure inspection, no keyring or engine                |
| `ui`      | granted  | a UI script, commands, theme colours by role                     |
| `ui.custom` | granted | adds native widgets mounted by the script; requires `ui`       |
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

A module's UI is two parts: typed C++ **commands**, which do the work and
carry every user-visible word, and a Lua **UI script**, which says where those
commands are offered and when they are enabled.

```cpp
// MyModule.cpp
#include <GFModule.h>
#include <GFSDKHostCommands.hpp>  // the Host's own commands: AppMessage, ...
#include "GFModuleIdentity.h"  // generated from module.json by gf_add_module()

struct ShowAbout {
  static constexpr gf::cmd::Meta kMeta{GF_MODULE_ID ".show_about",
                                       GC_TR("About My Module"), "", "", 0,
                                       gf::cmd::kNeedsGuiThread};
  using Args = gf::cmd::Unit;
  using Result = gf::cmd::Unit;
};

auto DoShowAbout(const gf::cmd::CommandContext&, const gf::cmd::Unit&)
    -> gf::cmd::Outcome<gf::cmd::Unit> {
  Commands().Invoke<gf::cmd::host::AppMessage>(
      {gf::cmd::host::AppMessage::Severity::kInfo,
       QCoreApplication::translate("GTrC", "About My Module"),
       QCoreApplication::translate("GTrC", "Hello.")});
  return gf::cmd::Outcome<gf::cmd::Unit>::Success({});
}

auto OnActivate() -> GFResult {
  LOG_INFO("MyModule activating");
  return GFResult::Ok();
}

const std::array<gf::cmd::Binding, 1> kCommands = {
    gf::cmd::Bind<ShowAbout, &DoShowAbout>()};

const GFModuleHooks kHooks = {
    sizeof(GFModuleHooks),
    GF_MODULE_ID, GF_MODULE_VERSION, GF_MODULE_TRANSLATION_CONTEXT,
    &OnActivate, nullptr, nullptr,
    nullptr, 0,                          // events it handles
    kCommands.data(), kCommands.size(),  // commands it provides
};

extern "C" GF_MODULE_EXPORT auto GFModuleGetApi(uint32_t abi)
    -> const GFModuleApi* {
  return GFModuleRuntimeGetApi(abi, &kHooks);
}
```

```lua
-- ui/main.lua: where the command is offered. No user-visible text here.
local about = commands.get("com.example.my_module.show_about")
ui.action { id = "about", anchor = ui.anchor("main.menu.help"), command = about }
```

Every hook in `GFModuleHooks` is optional; pass `nullptr` for one you have
nothing to say about. The runtime registers the commands before
`on_activate` and loads the script after it. At deactivation the host
withdraws everything the module registered -- commands, widgets, script,
subscriptions, translations -- before `on_deactivate` runs, so nothing has to
be unregistered by hand. What `on_deactivate` is for is the module's OWN work:
stop the threads it started, the timers it runs, the network requests it has
in flight. A module that starts none of those passes `nullptr`. `GFModuleGetApi` is
the one piece of ABI a module writes by hand, deliberately not hidden behind
a macro: it is what makes the linker keep `gf_module_runtime`'s entry point,
since nothing else in a module references it. The host resolves the symbol
after the module is linked.

Check the real modules for complete examples:
[`m_pgp_inspect`](src/m_pgp_inspect) (a command and a dialog widget) and
[`m_key_server_sync`](src/m_key_server_sync) (commands with arguments, a
dialog, a settings page and key details buttons).

### 4. Add CMakeLists.txt

```cmake
gf_add_module(
  NAME   my_module
  QT     Core Widgets
  LUA_SCRIPTS ui/main.lua   # embedded, signed with the module, loaded by the host
  # UI_DIR ui              # if you have a Qt Designer forms directory
  # LINK   some_target      # extra libraries, if any
)
```

`LUA_SCRIPTS` compiles the listed scripts into the module under
`:/gf_module/<id>/lua/`. They are data: the host runs them in its own sandbox,
and a module must never link an interpreter of its own
(`scripts/check_module_boundary.sh` refuses one).

`gf_add_module()` reads `module.json`, builds the target from every source
file in the directory (or `SOURCES` if given explicitly), generates the
identity header, wires up translations for every locale in
`GPGFRONTEND_SUPPORTED_LOCALES`, and builds + signs the resulting
`.gfmodule` package — all from those handful of values. See
[`modules/src/m_pgp_inspect/CMakeLists.txt`](src/m_pgp_inspect/CMakeLists.txt)
for a real, small example. `RESOURCES` adds Qt resource files and
`INCLUDE_DIRS` extra include directories.

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
be allocated with `GFMemStrDup` (`DUP(...)`), because ownership of the
whole struct is what transfers. `UDUP` takes a `char*` the SDK handed you;
it does not accept a `QByteArray` or `QString` -- those are already owned,
and freeing their storage through the SDK corrupts the heap, so the call does
not compile.

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
  you return changes the outcome: the six `EDIT_TAB_TYPE_<TYPE>_OP_*` crypto
  operations (your reply replaces the user's document) and the key-server
  requests.
- **whether the reply is read at all.** `TAB_ACTIVATED` tells you a fact and
  ignores what you return.
- **whether you may defer.** Where the flag allows it, return
  `GFEventResult::Deferred()` and answer later through `event.Answer()`,
  from any thread. The host accepts one answer per delivered event. If your
  module is deactivated before it answers, the host answers for it, with a
  failure, and refuses the late answer.

Nothing is a veto. No trigger site cancels an operation on a module's word,
and `TriggerEvent` is asynchronous and returns nothing, so a veto is not
expressible today.

No event carries a Host object. Everything an event hands you is data, and
UI comes from your script and your own widgets (see [UI](#ui)).

A handler that starts asynchronous work returns `GFEventResult::Deferred()`
and answers later through `event.Answer()`, from any thread, at most once.
See the key-server requests in
[`m_key_server_sync/KeyServerSyncModule.cpp`](src/m_key_server_sync/KeyServerSyncModule.cpp)
for a real example.

### Runtime values

A shared, typed register table (`storage` capability) for live
configuration, visible to the host and to other modules. A module may read any
namespace but WRITES only its own, which is its module id (`GFModuleId()`).
Keys and namespaces are lower-case; text and bool are kept apart because a lookup of the wrong
type is a miss, not a conversion. Use the `gf::sdk::` wrappers
(`GFSDK.hpp`), which apply a fallback where the raw calls only report
absence:

```cpp
auto* ctx = GFModuleSdkContext();
gf::sdk::SetStateBool(ctx, GFModuleId(), "ready", true);
bool ready = gf::sdk::StateBool(ctx, GFModuleId(), "ready", /*fallback=*/false);

gf::sdk::SetStateText(ctx, GFModuleId(), "last_error", message);
QString v = gf::sdk::StateText(ctx, GFModuleId(), "last_error");
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
channel, operation, err, r.CapsuleId())` — this is engine neutral, working
the same way whether the active engine is GnuPG or rPGP.

### UI

The full reference (the Lua API, the anchor catalog, the sandbox, the typed
widget protocols) is in [`src/sdk/README.md`](../src/sdk/README.md#ui-integration).
In short:

- **Commands** (`ui`) are the semantic layer. A command has a title, a
  description and a category, all `GC_TR` strings, and typed arguments.
  Invoke one with `Commands().Invoke<C>(args)`, the host's included
  (`gf::cmd::host`, in `GFSDKHostCommands.hpp`).
- **The UI script** (`ui`) places commands on anchors with `ui.action`, and
  reacts to a small, closed set of UI events with `ui.subscribe`.
- **Native widgets** (`ui.custom`) are your own `QWidget`s. Derive from one of
  `gf::ui::DocumentWidget`, `SettingsWidget` or `DialogWidget`, register it in
  `OnActivate()`, and mount it from the script with `ui.mount`. The host owns
  the frame around it: the dialog, the settings page, the document tab.

```cpp
auto OnActivate() -> GFResult {
  gf::ui::RegisterNativeWidget<MySettingsPage>(
      "settings", {GC_TR("My Module"), GC_TR("proxy,timeout")},
      [](const QCborMap&) { return new MySettingsPage(); });
  return GFResult::Ok();
}
```

```lua
ui.mount { id = "settings", anchor = ui.anchor.settings { section = "features" },
           widget = native.widget("settings") }
```

Register titles and keywords untranslated, with `GC_TR(...)`: the host
translates them when it shows them, so they follow a language change.
Settings sections are `application`, `keys_engines`, `features` and `system`.

#### Migrating from the object-based UI API

Every entry point that handed a module a Host object, or took one, now
refuses (and logs the replacement once). One example per pattern:

**A menu entry.** Before: handle `MAINWINDOW_MENU_MOUNTED`, `RequireGui` the
menu, `addAction`. After: a command, placed by the script.

```lua
ui.action { id = "check", anchor = ui.anchor("main.menu.help"),
            command = commands.get("com.example.my_module.check") }
```

**A key details button.** Before: handle `KEY_PAIR_OPERA_MENU_CREATED`. After:
the `key.details.actions` anchor; the host groups buttons by command category
and passes the key.

```lua
ui.action { id = "publish", anchor = ui.anchor("key.details.actions"),
            command = publish,
            update = function(ctx)
              if not ctx.key then return { visible = false } end
              return { args = { key = ctx.key } }
            end }
```

**A dialog with a parent.** Before: `GUI_OBJECT`, `GFUIShowDialog(ctx, dlg,
parent)`. After: a `DialogWidget`, a dialog mount, and the host command
`org.gpgfrontend.view.open`, which accepts only your own mounts.

```lua
local inspector = ui.mount { id = "inspector", anchor = ui.anchor.dialog {},
                             widget = native.widget("inspector") }
```

**A settings page, or widgets injected into the Network tab.** Before:
`RegisterSettingsPage` with `SetSettings`/`ApplySettings` found by name, or the
`NETWORK_SETTINGS_TAB_*` events. After: a `SettingsWidget`
(`LoadSettings`/`ApplySettings`) and a settings mount, as above.

**Reading and writing settings.** Before: `GFStorageSettingsRoot` and a cast
to `QSettings*`. After: the module's own group, no cast.

```cpp
auto* ctx = GFModuleSdkContext();
bool check = gf::sdk::Setting(ctx, GF_SETTING_MODULE, "check_on_startup", true)
                 .toBool();
gf::sdk::SetSetting(ctx, GF_SETTING_MODULE, "check_on_startup", false);
```

In Lua, `state.get` / `state.set` read and write the same group.

**Opening a document in a new tab.** Before: `invokeMethod(edit,
"SlotNewCustomTab")` and `setTabText` on the host's tab widget. After:
`Commands().Invoke<gf::cmd::host::DocumentOpen>` with the content as a `gf::cmd::Blob`.

**A document type of your own.** Before: `RegisterTabPageView`,
`register_file_extension` with its `FILE_EXT_*` events, and an
`EDIT_TAB_TYPE_*_OP_SAVE_FILE` handler. After: a `DocumentWidget`
registered with `RegisterNativeWidgetFactory`, mounted on
`ui.anchor.editor { document_type = ..., extensions = { ... } }`. The host
opens those files, and saves through your `Save` and `PrepareSave`.

**Showing the raw source.** Before: `AdoptSourceView(QWidget*)` took the
host's editor. After: the page's own switcher shows the source; ask for it
with `ShowSource(true)`, and keep it read-only with `SourceLockReason`.

### More of the module-facing API

Things the sections above do not show, each documented in its header:

- **Commands** (`GFSDKCommand.hpp`, `GFModuleCommand.h`). A handler may take a
  `gf::cmd::Reply<Result>` as a third argument and answer later, from any
  thread. A command type may define `static auto State(const CommandContext&)
  -> uint32_t` to say whether it is enabled, visible or checked; `kCheckable`
  in its `kMeta` flags makes it a toggle. A long handler polls
  `ctx.Cancelled()`. `Commands()` also offers `InvokeDynamic` (by id, CBOR
  arguments), `Cancel`, `Describe` and `List`.
- **Events** (`GFModuleEvent.h`). `event.Bytes(key)` for a binary parameter
  (no base64 round trip), `event.Require(key, out)` for a mandatory one,
  `event.Params()` for all of them; `event.Answer()` gives the handle a
  deferred handler answers through (`Ok`, `Fail`, `Send`, `Answered`).
- **Facts** (`GFModule.h`). `GFModuleId()`, `GFModuleVersion()`,
  `GFModuleHasCapability(name)`, `GFModuleIsVerified()`.
- **Logging** (`GFModuleLog.h`). `MLogTrace` through `MLogError` take one
  QString; `LOG_*` take a format string and `FLOG_*` a format string with
  `%1`-style arguments.

### Translations

Wrap strings with `QCoreApplication::translate("GTrC", "...")` (or `GC_TR`
for a string that must stay untranslated until later, such as a command
title). `GTrC` is a single, fixed translation
context declared by `GFModule.h`; it is unrelated to a module's own
`translation_context` field in `module.json`, which only names the `.qm`
file. There is nothing left to register: `gf_module_runtime` hands the host
the module's translator during activation, before any event subscription and
before `OnActivate()` runs, and the host installs it on the GUI thread. It is
in place by the time anything the module mounts is shown -- but not
necessarily while `OnActivate()` itself runs, so translate there with
`GC_TR` and let the host translate later.

`.ts` files go in `ts/<translation_context>.<locale>.ts` and are embedded as
Qt resources under `:/i18n/`. The locale set is not chosen per module — it
comes from the shared `GPGFRONTEND_SUPPORTED_LOCALES` list (see
[Build Notes](#build-notes)), so the module always offers the same languages
as the application. After adding or changing translatable strings, run
[`scripts/update_translations.sh`](../scripts/update_translations.sh) to sync
every locale's `.ts`.

## Packaging, Signing & Distribution

Every module ships as a signed `.gfmodule` package (a manifest and its Ed25519
signature, plus, for an external module, the publisher public key that
signature was made with) and the native shared library it names. The package
format, verification rules, and trust model live in
[`src/core/module/`](../src/core/module); this section only summarises the
parts that matter for building and distributing a module.

There are three tools, one per trust role:

| Tool                    | Role                                                                     |
| ----------------------- | ------------------------------------------------------------------------ |
| `gf_module_keygen`      | the integrated build identity: one ephemeral key per build tree          |
| `gf_module_packager`    | builds, reseals and verifies integrated packages                         |
| `gf_module_externalize` | turns a verified integrated package into a publisher-signed external one |

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
2. In the **Module Controller**, the user trusts the publisher key the module
   was signed with ("signatures by this key are worth considering") and then
   enables that specific module ("run this one"). These are deliberately two
   separate decisions: trusting a key applies to every module that key ever
   signs, so collapsing it with "run this one" into a single click would
   make the first of them too easy to grant by accident.
3. Both take effect on the next restart.

The publisher key inside the package only names the signer. It never grants
trust by itself: trust comes only from the user's decision about that exact
key. A publisher name or URL in the manifest is an unverified claim. The
build's own integrated key is refused wherever a publisher key is expected.

#### Publishing an external module

External modules are made from integrated ones, never built directly. The
integrated package is the proof of what was built; externalizing it is a
trust-domain transition from "this build signed it" to "this publisher signs
it", done once, as the last release step.

1. **Create a publisher key**, once. It is your lasting identity, so keep the
   secret file safe and publish the `.pub` file (or its fingerprint) through a
   channel your users trust:

   ```bash
   gf_module_externalize new-key --out publisher.key   # writes publisher.key (0600) + publisher.key.pub
   gf_module_externalize fingerprint --key publisher.key.pub
   ```

   The key files are typed text files. A raw 32-byte seed, such as a build
   tree's `module-build.seed`, is refused, so the build key can never be used
   as a publisher key by mistake.

2. **Build with native binding required**, so the integrated descriptor binds
   the exact bytes of its native. Finish every step that rewrites the native
   (strip, `patchelf`, deploy tools) and the integrated reseal first:

   ```bash
   cmake -B build -DGPGFRONTEND_INTEGRATED_MODULE_NATIVE_BINDING=REQUIRED
   ```

3. **Externalize** the module's namespace directory:

   ```bash
   gf_module_externalize \
     --input build/artifacts/modules/<key> \
     --publisher-key publisher.key \
     --output-root dist \
     [--publisher-name "Example"] [--publisher-url https://example.com]
   ```

   The input is first verified as an integrated module of this build, and
   refused if its native is unbound or its `native/` directory holds anything
   besides the entry native. The output is `dist/<key>/module.gfmodule` plus
   `dist/<key>/native/<entry>`, ready to drop into a user's `mods/`. Every
   manifest field is carried over unchanged except the optional publisher
   metadata; the tool refuses a manifest field it has no rule for. It never
   overwrites an existing output.

4. **Do not modify the output.** The descriptor binds the native's bytes, and
   there is no external reseal: that would need your secret key wherever the
   rewrite happened. The one exception is Authenticode-signing a Windows
   native, whose binding digest excludes the signature by design.

`gf_module_packager verify-module-set` checks integrated trees only, and
refuses an externalized namespace.

### macOS has no external modules

External modules are refused unconditionally on macOS: Library Validation
means the process cannot load a `dylib` that does not carry this
application's own Team ID, and a third-party module by definition does not.
This is a platform policy, not a missing feature — admitting one past
GpgFrontend's own checks would still have it refused by `dyld`.

## Licensing

All modules in this directory are released under **GPL-3.0-or-later**,
consistent with the main GpgFrontend project.
