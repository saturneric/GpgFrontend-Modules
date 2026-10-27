# GpgFrontend Modules

A **module** is a plug-in for GpgFrontend. It is a small shared library
(`.so`, `.dll`, `.dylib`) that the application loads at startup. A module can
add menu entries, dialogs, settings pages and new document types, and it can
ask the application to sign, encrypt, decrypt and verify for it.

This folder holds the modules that ship with GpgFrontend. Read this page from
the top the first time. After that, use it as a reference.

## Contents

1. [Before you start](#before-you-start)
2. [The modules in this folder](#the-modules-in-this-folder)
3. [How a module works, in one picture](#how-a-module-works-in-one-picture)
4. [Tutorial: your first module](#tutorial-your-first-module)
5. [The rules that will bite you](#the-rules-that-will-bite-you)
6. [Reference](#reference)
7. [Packaging, signing and distribution](#packaging-signing-and-distribution)
8. [Licensing](#licensing)

## Before you start

- This folder is a git sub-repository. It must sit at `GpgFrontend/modules`.
- You **cannot build it on its own**. Compilers, SDK headers, Qt and output
  paths all come from the main GpgFrontend CMake project.
- Turn modules on when you configure the main project:

  ```bash
  cmake -B build -DGPGFRONTEND_BUILD_MODULES=ON
  ```

- You need **Qt 6.5 or newer**, the same Qt the application is built with.
  A module built against a different Qt major version will not load.
- Users can switch a module off in the application settings. In an archive
  install they can also delete its folder (see
  [where packages end up](#integrated-modules-the-ones-in-this-folder)).

## The modules in this folder

The full id of each module starts with `com.bktus.gpgfrontend.module.`, shown
here as `…`.

| Folder              | Module id                      | What it does                                     |
| ------------------- | ------------------------------ | ------------------------------------------------ |
| `m_ver_check`       | `…module.version_checking`     | Checks for new GpgFrontend releases.             |
| `m_gpg_info`        | `…module.gnupg_info_gathering` | Shows details about the local GnuPG install.     |
| `m_key_server_sync` | `…module.key_server_sync`      | Searches and syncs keys with HKP/VKS servers.    |
| `m_email`           | `…module.email`                | E-mail helpers built on vmime.                   |
| `m_pgp_inspect`     | `…module.pgp_inspect`          | Shows the packet structure of OpenPGP data.      |
| `m_im`              | `…module.im`                   | OpenPGP messages as single-line chat tokens.     |

```
modules/
├── src/
│   ├── m_ver_check/
│   ├── m_gpg_info/
│   ├── m_key_server_sync/
│   ├── m_email/
│   ├── m_pgp_inspect/
│   ├── m_im/
│   └── CMakeLists.txt      <- lists every module folder
└── CMakeLists.txt
```

**Want to copy something?** Start from these two:

- [`m_pgp_inspect`](src/m_pgp_inspect): small. One command, one dialog.
- [`m_key_server_sync`](src/m_key_server_sync): bigger. Commands with
  arguments, a dialog, a settings page, buttons on the key details page, and
  events.

## How a module works, in one picture

Every module folder has the same four parts:

```
my_module/
├── module.json       1. WHO the module is and WHAT it is allowed to do
├── CMakeLists.txt    2. how to build it (one function call)
├── MyModule.cpp      3. the C++ code: commands that do the work
└── ui/main.lua       4. a tiny script: WHERE the commands appear in the UI
```

The C++ code never touches the application's windows directly. Instead:

1. Your C++ code defines **commands**. A command is a named action with a
   title, like "Open Inspector". It does the work.
2. Your Lua script says **where** each command shows up: which menu, which
   context menu, which button row.
3. The application draws the menu entry. When the user clicks it, the
   application calls your command.

The module talks to the application only through the **SDK**, a set of C
functions (`GFGpgEncrypt`, `GFAppLocale`, ...). It cannot call into
GpgFrontend's own libraries, and the build stops you if you try.

Where the SDK lives (you do not need to touch these, `gf_add_module()` adds
them to your include path):

- [`src/sdk/`](../src/sdk): the `GFSDK*` functions, plus easier C++ wrappers
  in `GFSDK.hpp` under `gf::sdk::`. The full SDK manual is
  [`src/sdk/README.md`](../src/sdk/README.md).
- [`src/module_runtime/include/`](../src/module_runtime/include): `GFModule.h`
  and friends. This is the header you include. It gives you the event types,
  memory helpers, logging macros and the startup function.

## Tutorial: your first module

We will build a module that adds **Help → About My Module**. Clicking it
shows a message box saying "Hello."

### Step 1: Make the folder

```
modules/src/my_module/
```

Then open `modules/src/CMakeLists.txt` and add one line:

```cmake
add_subdirectory(my_module)
```

### Step 2: Write `module.json`

```json
{
  "schema_version": 1,
  "id": "com.example.my_module",
  "name": "MyModule",
  "description": "Says hello.",
  "author": "Your Name",
  "capabilities": ["ui"],
  "events": [],
  "commands": ["com.example.my_module.show_about"],
  "translation_context": "ModuleMyModule"
}
```

What each field means:

| Field                 | Meaning                                                                                                  |
| --------------------- | -------------------------------------------------------------------------------------------------------- |
| `id`                  | The module's unique name. Lower-case, dotted, like a reversed domain.                                    |
| `name`                | A short name shown to the user.                                                                          |
| `capabilities`        | What the module is **allowed** to use. See [Capabilities](#capabilities). Here: only `ui`.               |
| `events`              | Every event the module listens to. See [Events](#events). Here: none.                                    |
| `commands`            | Every command the module provides. Each one starts with the module `id` plus a dot.                      |
| `translation_context` | The name its translations are filed under. Pick something like `Module` + `name`.                        |
| `version`             | Optional. Defaults to the GpgFrontend version you build against. Bundled modules leave it out.           |
| `min_host_version`    | Optional. The oldest GpgFrontend that can run this module. Defaults to the version you build against.    |

This file is the **only** place the module's identity is written. The build
reads it to generate a C++ header, to set up translations, and to make the
signed package. Never copy the id into other places by hand: use the
generated `GF_MODULE_ID` instead.

> **Important:** the `events` and `commands` lists must match your C++ code
> **exactly**. List one you don't handle, or handle one you didn't list, and
> the module refuses to start.

### Step 3: Write the C++ code

```cpp
// MyModule.cpp
#include <GFModule.h>

#include <GFSDKHostCommands.hpp>  // commands the application itself offers

#include "GFModuleIdentity.h"  // GENERATED from module.json, gives GF_MODULE_ID
#include "GFModuleTr.h"        // GENERATED, gives GTrC for translations

// Write the type unqualified: lupdate misreads `gf::cmd::Meta kMeta{`.
using CommandMeta = gf::cmd::Meta;

// (A) Describe the command: its id, title, and that it runs on the GUI thread.
struct ShowAbout {
  static constexpr CommandMeta kMeta{
      GF_MODULE_ID ".show_about",       // must be listed in module.json
      GTrC::Noop("About My Module"),    // title shown in the menu
      "",                               // description
      "",                               // category
      0,
      gf::cmd::kNeedsGuiThread};
  using Args = gf::cmd::Unit;    // takes no arguments
  using Result = gf::cmd::Unit;  // returns nothing
};

// (B) What happens when the user clicks it.
auto DoShowAbout(const gf::cmd::CommandContext&, const gf::cmd::Unit&)
    -> gf::cmd::Outcome<gf::cmd::Unit> {
  // Ask the application to show a message box.
  Commands().Invoke<gf::cmd::host::AppMessage>(
      {gf::cmd::host::AppMessage::Severity::kInfo,
       GTrC::tr("About My Module"), GTrC::tr("Hello.")});
  return gf::cmd::Outcome<gf::cmd::Unit>::Success({});
}

// (C) Runs once when the module starts.
auto OnActivate() -> GFResult {
  LOG_I() << "MyModule activating";
  return GFResult::Ok();
}

// (D) The list of commands. Must match "commands" in module.json.
const std::array<gf::cmd::Binding, 1> kCommands = {
    gf::cmd::Bind<ShowAbout, &DoShowAbout>()};

// (E) Everything the application needs to know about this module.
const GFModuleHooks kHooks = {
    sizeof(GFModuleHooks),
    GF_MODULE_ID,
    GF_MODULE_VERSION,
    GF_MODULE_TRANSLATION_CONTEXT,
    &OnActivate,  // on_activate:   runs at start
    nullptr,      // on_deactivate: stop your own threads/timers here
    nullptr,      // on_unload:     final cleanup
    nullptr,      // events:        none (must match module.json)
    0,            // number of events
    kCommands.data(),
    kCommands.size(),
};

// (F) The entry point. Copy this exactly. Do not rename it.
extern "C" GF_MODULE_EXPORT auto GFModuleGetApi(uint32_t abi)
    -> const GFModuleApi* {
  return GFModuleRuntimeGetApi(abi, &kHooks);
}
```

Things to know about this code:

- **Every hook is optional.** Pass `nullptr` for any you don't need.
- **Order at startup:** the commands are registered, then `OnActivate()`
  runs, then your Lua script is loaded.
- **At shutdown you clean up nothing the application gave you.** It removes
  your commands, widgets, script, event subscriptions and translations by
  itself, before `on_deactivate` runs. Use `on_deactivate` only to stop
  things *you* started: threads, timers, network requests. If you started
  none, pass `nullptr`.
- **Why is `GFModuleGetApi` written by hand?** Nothing else in the module
  refers to it. Writing it out keeps the linker from throwing it away. The
  application looks for this exact name when it loads the library.

### Step 4: Write the UI script

Create `ui/main.lua`. It only says **where** the command appears:

```lua
-- ui/main.lua: places the command. No user-visible text belongs here.
local about = commands.get("com.example.my_module.show_about")

ui.action {
  id = "about",
  anchor = ui.anchor("main.menu.help"),  -- the Help menu
  command = about,
}
```

The menu title comes from the command (`GTrC::Noop("About My Module")`), not
from the script. All text the user sees lives in C++, so it can be
translated.

### Step 5: Write `CMakeLists.txt`

```cmake
gf_add_module(
  NAME        my_module
  QT          Core Widgets
  LUA_SCRIPTS ui/main.lua
  # UI_DIR      ui            # if you have Qt Designer .ui forms
  # LINK        some_target   # extra libraries, if any
  # RESOURCES   my.qrc        # extra Qt resource files
  # INCLUDE_DIRS some/dir     # extra include folders
  # SOURCES     a.cpp b.cpp   # default: every source file in this folder
)
```

That one call does **everything**:

- reads `module.json`,
- builds every source file in the folder (or only `SOURCES`, if you give it),
- generates `GFModuleIdentity.h` and `GFModuleTr.h`,
- sets up translations for every language GpgFrontend supports,
- embeds your Lua scripts (they are data, run by the application in a
  sandbox; never link a Lua interpreter yourself),
- builds and signs the `.gfmodule` package.

Moc, rcc and uic run automatically. There is nothing else to wire up.

### Step 6: Build and run

From the main GpgFrontend folder:

```bash
cmake -B build -DGPGFRONTEND_BUILD_MODULES=ON   # only needed the first time
cmake --build build -j$(nproc)
```

Start GpgFrontend. Open **Help**: you should see **About My Module**.

If you added new `.cpp` files to an existing folder later, run the configure
command again: the file list is collected at configure time.

### Step 7: Translations

Run this whenever you add or change `GTrC::tr(...)` or `GTrC::Noop(...)`
strings:

```bash
scripts/update_translations.sh
```

It creates or updates the `.ts` files for every language, in your module's
`ts/` folder. You never write them by hand. Details in
[Translations](#translations).

## The rules that will bite you

Read this list. Each item is a mistake someone already made.

1. **`module.json` and the code must agree.** The `events` list must equal
   your event table, and the `commands` list must equal your command table.
   Any difference and the module will not start.
2. **Declare every capability you use.** If you call `GFGpgDecrypt` without
   `"gpg"` in `capabilities`, the call does nothing, returns a failure, and
   logs a line telling you what to add.
3. **Don't declare what you don't use.** Users see the list. Extra
   permissions cost their trust for nothing.
4. **Always write `GTrC::tr("...")` or `GTrC::Noop("...")` literally.** No
   wrapper functions, no `#define`, no `QCoreApplication::translate` with
   another context. The translation tool cannot see through them, and those
   strings silently stay untranslated.
5. **Never link GpgFrontend's own libraries** (`gf_core`, `gf_ui`,
   `gf_host_api`). The build fails if you do. Need something the SDK does
   not have? Add it to the SDK (see [What a module may link](#what-a-module-may-link)).
6. **Strings you pass to the SDK stay yours. Strings the SDK returns are
   yours to free.** Wrap returned `char*` in `UDUP(...)`. See
   [Memory](#memory).
7. **`FLOG_*` uses `%1`, `%2`, not `%d`.** Better: use the stream form
   `LOG_I() << ...` and you can't get it wrong.
8. **Work on another thread? Take the context with you.** Grab
   `GFModuleSdkContext()` before you start the thread and pass it in.
9. **Don't change a packaged module after it is built.** The signature
   covers the exact bytes. See [Publishing](#publishing-an-external-module).

## Reference

### Capabilities

`capabilities` in `module.json` lists what your module may use. The
application enforces it: when your module starts, it only gets the
functions for the groups you declared. Calling anything else fails safely
and logs once.

| Name        | Enforced? | What it unlocks                                                         |
| ----------- | --------- | ----------------------------------------------------------------------- |
| `gpg`       | yes       | sign, encrypt, decrypt, verify, keys, key lists, result analysis        |
| `pgp`       | yes       | reading OpenPGP packet structure (no keyring, no engine)                |
| `ui`        | yes       | a UI script, commands, theme colours                                    |
| `ui.custom` | yes       | your own Qt widgets, mounted by the script. Needs `ui` too.             |
| `editor`    | yes       | reading the document the user has open                                 |
| `storage`   | yes       | settings, caches, the runtime value table                               |
| `process`   | yes       | running an external program                                             |
| `network`   | **no**    | only recorded and shown to the user (see below)                         |

Always available, never declared: buffers, memory, logging, events and
translations.

**Why isn't `network` enforced?** Your module opens sockets through Qt
directly. The application is not in between, so it cannot block it. The
name is still signed and shown to users so they know. It is kept apart from
the enforced names so the list never pretends to block something it can't.

If you forget a capability, the log tells you exactly what to do:

```
GFGpgDecrypt needs the "gpg" capability, which this module's signed manifest
does not declare. Add it to module.json's "capabilities" and rebuild; the
call did nothing.
```

A misspelled capability name stops the build at configure time.

### Commands

A command has an id, a title, a description and a category (all
`GTrC::Noop` strings), plus typed `Args` and `Result`. Run one with
`Commands().Invoke<C>(args)`. The application's own commands live in
`gf::cmd::host` (`GFSDKHostCommands.hpp`).

More options, documented in `GFSDKCommand.hpp` and `GFModuleCommand.h`:

- **Answer later:** take a `gf::cmd::Reply<Result>` as a third argument and
  reply from any thread.
- **Enabled / visible / checked:** add
  `static auto State(const gf::cmd::CommandContext&) -> uint32_t` to the
  command type. Real example: `OpenInspector` in
  [`PGPInspectModule.cpp`](src/m_pgp_inspect/PGPInspectModule.cpp).
- **Toggle:** put `kCheckable` in the `kMeta` flags.
- **Long work:** poll `ctx.Cancelled()` and stop when it is true.
- **Call by id:** `Commands()` also has `InvokeDynamic` (id + CBOR
  arguments), `Cancel`, `Describe` and `List`.

### Events

Events are messages the application sends you, like "the application has
finished loading". To listen to one:

1. Add its id to `events` in `module.json`.
2. Write a handler and put it in an event table.
3. Pass that table in `GFModuleHooks`.

```cpp
auto OnApplicationLoaded(const GFEvent& event) -> GFEventResult {
  LOG_I() << "the application is ready";
  return GFEventResult::Ok();
}

constexpr std::array<GFEventBinding, 1> kEvents = {{
    {"APPLICATION_LOADED", &OnApplicationLoaded},
}};

// in kHooks, replace the "nullptr, 0" for events with:
//   kEvents.data(), kEvents.size(),
```

There is no "subscribe" call: the table *is* the subscription.

**Which events exist?** All of them are listed, with what they mean, in
[`src/core/module/ModuleEventRegistry.cpp`](../src/core/module/ModuleEventRegistry.cpp).
An id not in that list is refused. For each event the list says:

- **Does your answer matter?** Most events only tell you something happened
  and ignore your answer. A few let your answer change the result: the six
  `EDIT_TAB_TYPE_<TYPE>_OP_*` crypto operations (your answer replaces the
  user's document) and the key-server requests.
- **May you answer later?** If yes, return `GFEventResult::Deferred()` now,
  and answer later with `event.Answer()`, from any thread, **once**. If your
  module stops before answering, the application answers "failed" for you
  and ignores your late answer. Real example: the key-server handlers in
  [`KeyServerSyncModule.cpp`](src/m_key_server_sync/KeyServerSyncModule.cpp).

A module can never cancel an operation through an event. Events only ever
carry data, never application objects.

Reading event data (see `GFModuleEvent.h`): `event.Bytes(key)` for binary
data, `event.Require(key, out)` for a value that must be there,
`event.Params()` for all of them. A deferred answer goes through
`event.Answer()` with `Ok`, `Fail`, `Send` or `Answered`.

### The context: every host call needs one

Almost every SDK function takes a **context** as its first argument. It
tells the application which module is calling.

```cpp
auto OnSomething(const GFEvent& e) -> GFEventResult {
  auto* ctx = e.Context();  // inside an event handler: from the event
  auto keys = gf::sdk::ExportKey(ctx, channel, key_id, true);
  LOG_I() << "exported" << keys.size() << "bytes";
  return GFEventResult::Ok();
}
```

Anywhere else, use `GFModuleSdkContext()`. It is safe on any thread. But if
you start work on another thread, **grab the context first and pass it
in**, rather than calling `GFModuleSdkContext()` inside the thread. That way
the code says plainly which module it belongs to.

Only two functions take no context, because they need nothing from the
application: `GFCompareSoftwareVersion` and `GFUIHumanSize`.

### Logging

Use the stream form. Five levels: `LOG_T` (trace), `LOG_D`, `LOG_I`,
`LOG_W`, `LOG_E` (error).

```cpp
LOG_W() << "open" << path << "failed:" << file.errorString();
LOG_T() << "option line:" << line << "fields:" << fields.size();
```

It prints anything `qDebug()` can print (enums, `bool`, `QByteArray`,
lists), and there is no format string to get wrong.

Older forms still work:

```cpp
LOG_INFO("started");                           // one fixed string
FLOG_INFO("started with %1 accounts", count);  // %1, %2 ... NOT %d
```

`FLOG_*` uses `QString::arg`. A `%d` is printed literally, and a value
`arg()` doesn't accept must be converted first. `MLogTrace` through
`MLogError` take a single `QString`.

Each line is tagged with your module and the source line:

```
[module.email] [W] EMailImapController.cpp:412 - imap connect failed
```

Trace lines are hidden by default. Start the app with `--log-level trace` to
see them.

### Memory

Two simple rules:

1. **What you pass in, you keep.** Hand the SDK a string literal or
   `qstr.toUtf8().constData()`. Do not make a copy for it. Nobody on the
   other side frees it.
2. **What comes back, you own.** Wrap a returned `char*` in `UDUP(...)` to
   turn it into a `QString` and free it in one go:

   ```cpp
   QString locale = UDUP(GFAppLocale(GFModuleSdkContext()));
   ```

   Use `USECDUP(...)` instead if it came from the secure (wiping) allocator.

**One exception:** when you build a whole struct and hand it over
(`GFModuleEvent`, `GFModuleEventParam`, `GFCommandExecuteContext`, ...), its
`char*` fields must be allocated with `DUP(...)`, because the whole struct
changes owner.

| Macro         | Use it for                                                         |
| ------------- | ------------------------------------------------------------------ |
| `UDUP(p)`     | a `char*` the SDK returned: makes a `QString`, frees `p`            |
| `USECDUP(p)`  | same, for a secret from the wiping allocator                       |
| `DUP("x")`    | a `char*` field in a struct you hand over                          |
| `SECDUP("x")` | same, for a secret                                                 |

`UDUP` does not accept a `QString` or `QByteArray`. That is on purpose: you
already own those, and freeing them through the SDK would corrupt memory.

### Runtime values

A shared table of small typed values (needs `storage`). Everyone can read
it. You can only **write** under your own module id (`GFModuleId()`). Keys
are lower-case. Text and bool are separate: reading a bool key as text is
just "not found".

```cpp
auto* ctx = GFModuleSdkContext();
gf::sdk::SetStateBool(ctx, GFModuleId(), "ready", true);
bool ready = gf::sdk::StateBool(ctx, GFModuleId(), "ready", /*fallback=*/false);

gf::sdk::SetStateText(ctx, GFModuleId(), "last_error", message);
QString v = gf::sdk::StateText(ctx, GFModuleId(), "last_error");
```

The application fills the `core` namespace itself: which engines work, each
module's state, every command, and usage counters. Read the numbers with
`gf::sdk::StateInt`:

```cpp
bool gnupg = gf::sdk::StateBool(ctx, "core", "engine.gnupg.supported");
qint64 runs = gf::sdk::StateInt(ctx, "core", "stats.operations.op_encrypt.run");
```

All `core` keys are listed in the [SDK README](../src/sdk/README.md#storage).

### Cache

One set of calls, three places to store:

| Tier                      | Where     | Wiped on free? |
| ------------------------- | --------- | -------------- |
| `GF_STORE_SESSION`        | memory    | no             |
| `GF_STORE_DURABLE`        | disk      | no             |
| `GF_STORE_SECURE_DURABLE` | disk      | yes            |

```cpp
auto* ctx = GFModuleSdkContext();
gf::sdk::SetCacheText(ctx, GF_STORE_SESSION, "key", "value");
gf::sdk::SetCacheText(ctx, GF_STORE_DURABLE, "token", "abc", /*ttl_seconds=*/3600);
QString v = gf::sdk::CacheText(ctx, GF_STORE_DURABLE, "token");
gf::sdk::RemoveCache(ctx, GF_STORE_DURABLE, "token");
```

### Settings

Each module has its own settings group. No casts, no `QSettings`:

```cpp
auto* ctx = GFModuleSdkContext();
bool check = gf::sdk::Setting(ctx, GF_SETTING_MODULE, "check_on_startup", true)
                 .toBool();
gf::sdk::SetSetting(ctx, GF_SETTING_MODULE, "check_on_startup", false);
```

In Lua, `state.get` and `state.set` read and write the same group.

### GPG operations

Needs `gpg`. Data goes in and out as `GFBuf` / `GFBufferView` (exact bytes,
not text). Results come back in a `GFGpgResult`, which frees itself.

```cpp
auto* ctx = GFModuleSdkContext();
auto channel = GFGpgCurrentChannel(ctx);   // the channel of the main window
auto in = GFBuf::Copy(ctx, plain_bytes);
QVector<const char*> key_ids = ...;         // one per recipient

GFGpgResult r(ctx);
if (GFGpgEncrypt(ctx, channel, key_ids.data(), key_ids.size(), in.View(),
                 /*ascii=*/1, r.Out()) != GF_GPG_OK) {
  // the call itself failed; r.ErrorString() says why
}
if (r.Error() != GF_GPG_ERR_NO_ERROR) {
  // the call ran, but the operation failed; r.ErrorString() again
}
auto encrypted = r.DataCopy();
```

`GFGpgSign`, `GFGpgDecrypt` and `GFGpgVerify` work the same way. Real,
complete examples with error handling:
[`EMailBasicGpgOpera.cpp`](src/m_email/EMailBasicGpgOpera.cpp).

Want the same details the application's own dialogs show (recipients,
signatures, validity)? Call
`gf::sdk::AnalyseResult(ctx, channel, operation, err, r.CapsuleId())`. It
works the same for both engines, GnuPG and rPGP.

### UI

The full manual (Lua API, every anchor, the sandbox, widget types) is in
[`src/sdk/README.md`](../src/sdk/README.md#ui-integration). The short
version:

- **Commands** (`ui`) do the work and hold all text.
- **The Lua script** (`ui`) puts commands on **anchors** (named places in the
  UI) with `ui.action`, and can react to a few UI events with
  `ui.subscribe`.
- **Native widgets** (`ui.custom`) are your own `QWidget`s. Derive from
  `gf::ui::DocumentWidget`, `SettingsWidget` or `DialogWidget`, register it
  in `OnActivate()`, and place it from the script with `ui.mount`. The
  application owns the frame around it (the dialog window, the settings
  page, the tab).

Example: your own settings page.

```cpp
auto OnActivate() -> GFResult {
  gf::ui::RegisterNativeWidget<MySettingsPage>(
      "settings", {GTrC::Noop("My Module"), GTrC::Noop("proxy,timeout")},
      [](const QCborMap&) { return new MySettingsPage(); });
  return GFResult::Ok();
}
```

```lua
ui.mount {
  id = "settings",
  anchor = ui.anchor.settings { section = "features" },
  widget = native.widget("settings"),
}
```

Register titles and keywords with `GTrC::Noop(...)`, untranslated. The
application translates them when it shows them, so they follow a language
change. Settings sections: `application`, `keys_engines`, `features`,
`system`.

#### UI recipes

**A menu entry.**

```lua
ui.action {
  id = "check",
  anchor = ui.anchor("main.menu.help"),
  command = commands.get("com.example.my_module.check"),
}
```

**A shortcut.** On a `main.menu.*` anchor, add `shortcut = "Ctrl+M"`. If the
key is already taken, it is dropped and a warning is logged.

**A button on the key details page.** Buttons are grouped by the command's
category, and the key is passed to your command.

```lua
ui.action {
  id = "publish",
  anchor = ui.anchor("key.details.actions"),
  command = publish,
  update = function(ctx)
    if not ctx.key then return { visible = false } end
    return { args = { key = ctx.key } }
  end,
}
```

**An entry in the key list's right-click menu.** `ctx.keys` holds the checked
keys if any are checked, otherwise the selected ones. `ctx.key` is also set
when there is exactly one. Your command's `Args` declares
`std::optional<QList<gf::cmd::KeyRef>> keys`.

```lua
ui.action {
  id = "refresh_selected",
  anchor = ui.anchor("key.list.context"),
  command = refresh,
  update = function(ctx)
    if #ctx.keys == 0 then return { visible = false } end
    return { args = { keys = ctx.keys } }
  end,
}
```

**A dialog.** Write a `DialogWidget`, mount it on a dialog anchor, and open it
with the application command `org.gpgfrontend.view.open` (it only opens your
own mounts).

```lua
local inspector = ui.mount {
  id = "inspector",
  anchor = ui.anchor.dialog {},
  widget = native.widget("inspector"),
}
```

**A settings page.** A `SettingsWidget` (`LoadSettings` / `ApplySettings`) and
a settings mount, as shown above.

**Open some content in a new tab.**
`Commands().Invoke<gf::cmd::host::DocumentOpen>` with the content as a
`gf::cmd::Blob`.

**Your own document type** (your own file extension, your own view). Write a
`DocumentWidget`, register it with `RegisterNativeWidgetFactory`, and mount
it on:

```lua
ui.anchor.editor { document_type = ..., extensions = { ... } }
```

The application opens those files and saves them through your `Save` and
`PrepareSave`. If your view has its own headers or attachments, add
`compact_status = true` to the anchor: the Status Panel then starts as a
thin bar while your tab is active, and opens again when a new result
arrives.

**Show the raw source** of your document: call `ShowSource(true)`. Keep it
read-only with `SourceLockReason`.

#### Upgrading old modules

Older modules got application objects (menus, windows, `QSettings*`) and
changed them directly. All of those entry points now refuse and log what to
use instead. The mapping:

| Old way                                                                      | New way                                           |
| ---------------------------------------------------------------------------- | ------------------------------------------------- |
| `MAINWINDOW_MENU_MOUNTED` + `RequireGui` + `addAction`                       | a command + `ui.action` on a menu anchor          |
| `KEY_PAIR_OPERA_MENU_CREATED`                                                | `ui.action` on `key.details.actions`              |
| `GUI_OBJECT`, `GFUIShowDialog(ctx, dlg, parent)`                             | `DialogWidget` + dialog mount + `org.gpgfrontend.view.open` |
| `RegisterSettingsPage`, `NETWORK_SETTINGS_TAB_*` events                      | `SettingsWidget` + settings mount                 |
| `GFStorageSettingsRoot` cast to `QSettings*`                                 | `gf::sdk::Setting` / `gf::sdk::SetSetting`        |
| `invokeMethod(edit, "SlotNewCustomTab")`, `setTabText`                       | `gf::cmd::host::DocumentOpen`                     |
| `RegisterTabPageView`, `register_file_extension`, `FILE_EXT_*`, `..._OP_SAVE_FILE` | `DocumentWidget` + `ui.anchor.editor { ... }` |
| `AdoptSourceView(QWidget*)`                                                  | `ShowSource(true)`, `SourceLockReason`            |

### Module facts

From `GFModule.h`: `GFModuleId()`, `GFModuleVersion()`,
`GFModuleHasCapability(name)`, `GFModuleIsVerified()`.

### Translations

1. Include the generated `GFModuleTr.h`.
2. Wrap text with `GTrC::tr("...")`.
3. For text that must be translated **later** (command titles, widget
   titles, anything in `OnActivate()`), use `GTrC::Noop("...")`. The
   application translates it when it shows it.
4. Run `scripts/update_translations.sh`.

`GTrC` uses your module's own `translation_context`, so your strings never
clash with another module's.

Always spell it out as `GTrC::tr` / `GTrC::Noop`. The translation tool
(lupdate) does not expand macros or follow helper functions. A local `Tr()`
wrapper, a `#define`, or `QCoreApplication::translate` with a different
context is **silently skipped**.

You register nothing. The runtime installs your translations before
`OnActivate()` runs. They are ready by the time anything you mount is shown,
but maybe **not** while `OnActivate()` itself runs. That is why text there
should be `GTrC::Noop`.

Files live at `ts/<translation_context>.<locale>.ts` and are embedded under
`:/i18n/`. You don't pick the languages: every module offers the same ones
as the application, set once as `GPGFRONTEND_SUPPORTED_LOCALES` in
[`cmake/Translations.cmake`](../cmake/Translations.cmake).

### What a module may link

`gf_add_module()` links two static libraries for you:

- **`gf_sdk`**: every `GFSDK*` function. It holds no state of its own.
- **`gf_module_runtime`**: your module's state: context, identity, hooks,
  event dispatch, translations.

You may **not** link `gf_core`, `gf_ui` or `gf_host_api`. Three checks stop
you:

- `gf_add_module()` fails at configure time if one of them shows up,
- the module is linked with `-Wl,--no-undefined`,
- `scripts/check_module_boundary.sh` checks the built library: it may import
  no `GF*` symbol and export nothing but `GFModuleGetApi`.

Need something the SDK doesn't offer? Add it to the SDK: a primitive in
`src/sdk/GFSDKHostApi.h`, its implementation in `src/sdk/host/`, and a
wrapper in `src/sdk/api/`.

## Packaging, signing and distribution

Every module ships as two things:

- a signed **`.gfmodule`** file: a manifest (made from `module.json`), its
  Ed25519 signature and, for external modules, the publisher's public key;
- the **native library** it names.

The exact format and trust rules are in
[`src/core/module/`](../src/core/module). This section covers what you need
to build and ship.

Three tools, one per job:

| Tool                    | Job                                                                   |
| ----------------------- | --------------------------------------------------------------------- |
| `gf_module_keygen`      | makes one throw-away signing key per build folder                     |
| `gf_module_packager`    | builds, re-signs and checks the bundled packages                      |
| `gf_module_externalize` | turns a checked bundled package into one signed by **you**, the publisher |

### Integrated modules (the ones in this folder)

**You do nothing.** The normal build makes, signs and installs the package.

The signing key is made once per build folder (by `gf_module_keygen`,
called during configure). It is compiled into the application as the thing
it trusts, and is never exported or reused. It only proves "this package
came from this build", not who wrote it.

Where packages end up (`build/artifacts/modules` in a developer build):

```
<modules-root>/<key>/module.gfmodule        # manifest + signature
<modules-root>/<key>/native/libgf_mod_xxx.so
```

`<key>` is made from the module id, for example
`com.bktus.gpgfrontend.module.email` → `email-a1b2c3d4e5f60718293a`. The
folder name is never trusted on its own: the application works it out again
from the signed id and refuses a package in the wrong folder.

### External modules (a user's own)

An external module lives in the user's profile, under `mods/<key>/`, with
the same layout as above. To run one, the user must:

1. Set **Settings → General → Module Discovery** to "Also Look For Modules
   I've Added". This only makes the application **look** in `mods/`.
   Finding a module never means running it.
2. In the **Module Controller**, first **trust the publisher key** the module
   was signed with, then **enable the module**. These are two separate
   choices on purpose: trusting a key covers every module that key ever
   signs, so it should never happen by accident in the same click.
3. Restart.

The key inside the package only says who signed it. It grants nothing by
itself; only the user's decision about that exact key does. A publisher name
or URL in the manifest is an unchecked claim. The build's own key is always
refused as a publisher key.

#### Publishing an external module

You never build an external module directly. You build it as an integrated
module first, then **externalize** it as the very last release step.

1. **Make a publisher key** (once, ever). This is your identity. Keep the
   secret file safe. Publish the `.pub` file, or its fingerprint, somewhere
   your users trust.

   ```bash
   gf_module_externalize new-key --out publisher.key   # writes publisher.key (mode 0600) + publisher.key.pub
   gf_module_externalize fingerprint --key publisher.key.pub
   ```

   A raw 32-byte seed (like a build folder's `module-build.seed`) is refused,
   so you can't use the build key by mistake.

2. **Build with native binding required.** This makes the package lock in
   the exact bytes of the library. Do every step that changes the library
   (strip, `patchelf`, deploy tools) and the integrated re-sign **before**
   step 3.

   ```bash
   cmake -B build -DGPGFRONTEND_INTEGRATED_MODULE_NATIVE_BINDING=REQUIRED
   ```

3. **Externalize** the module's folder:

   ```bash
   gf_module_externalize \
     --input build/artifacts/modules/<key> \
     --publisher-key publisher.key \
     --output-root dist \
     [--publisher-name "Example"] [--publisher-url https://example.com]
   ```

   It first checks that the input is a valid integrated module of this
   build, that its library is bound, and that `native/` holds nothing else.
   The result is `dist/<key>/module.gfmodule` and `dist/<key>/native/<lib>`,
   ready to copy into a user's `mods/`. All manifest fields are kept as they
   are, except the optional publisher name and URL. It refuses a manifest
   field it doesn't know, and never overwrites existing output.

4. **Do not touch the output.** The signature covers the library's exact
   bytes, and there is no way to re-sign without your secret key. The only
   exception: Authenticode-signing a Windows library is fine, because the
   check skips that signature by design.

`gf_module_packager verify-module-set` only checks integrated folders. It
refuses an externalized one.

### macOS: no external modules

macOS always refuses external modules. Its Library Validation only lets the
app load libraries signed with the app's own Team ID, and a third-party
module never has it. Even if GpgFrontend allowed it, the system loader
(`dyld`) would still refuse it.

## Licensing

All modules in this folder are released under **GPL-3.0-or-later**, like
the main GpgFrontend project.
