// A fake host, for the translation units that reach the SDK.
//
// What changed, and why it is a better harness: this file used to define the
// SDK's exported C symbols by hand. The SDK is now module-side and stateless,
// so the thing to fake is the HOST TABLE it is handed. Every GFSDK* call these
// tests make therefore runs the real wrapper code and lands here, which is
// what lets a test assert that the wrappers behave -- something a hand-written
// stub of the wrapper itself could never show.
//
// The allocator stubs are not merely placeholders: they enforce the same
// arena contract the real host does, so handing a function memory from the
// wrong arena is reported here instead of aborting the process with
// munmap_chunk() at runtime.

#include <GFSDKBuildInfo.h>
#include <GFSDKContext.h>
#include <GFSDKHostApi.h>

#include <QSettings>
#include <QString>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>

namespace {

std::set<void*>& NormalArena() {
  static std::set<void*> a;
  return a;
}
std::set<void*>& SecureArena() {
  static std::set<void*> a;
  return a;
}

}  // namespace

int g_allocator_violations = 0;

namespace harness {
bool LastSaveUsedSecureValue = false;
}

namespace {

/* --- buffers and memory --------------------------------------------------- */

struct FakeBuffer {
  size_t size;
  char* data;
};

auto BufNew(GFHostContextRef, const void* data, size_t size) -> GFBufferRef {
  auto* b = new FakeBuffer{size, static_cast<char*>(std::malloc(size + 1))};
  if (size != 0 && data != nullptr) std::memcpy(b->data, data, size);
  b->data[size] = '\0';
  return reinterpret_cast<GFBufferRef>(b);
}

auto BufData(GFHostContextRef, GFBufferView buf) -> const void* {
  return buf == nullptr ? nullptr
                        : reinterpret_cast<const FakeBuffer*>(buf)->data;
}

auto BufSize(GFHostContextRef, GFBufferView buf) -> size_t {
  return buf == nullptr ? 0 : reinterpret_cast<const FakeBuffer*>(buf)->size;
}

void BufZeroize(GFHostContextRef, GFBufferRef buf) {
  if (buf == nullptr) return;
  auto* b = reinterpret_cast<FakeBuffer*>(buf);
  std::memset(b->data, 0, b->size);
}

void BufRelease(GFHostContextRef, GFBufferRef buf) {
  if (buf == nullptr) return;
  auto* b = reinterpret_cast<FakeBuffer*>(buf);
  // Release wipes, as the real one does: these tests carry passwords.
  std::memset(b->data, 0, b->size);
  std::free(b->data);
  delete b;
}

auto BufOutstanding(GFHostContextRef) -> size_t { return 0; }

auto MemAlloc(GFHostContextRef, int arena, uint32_t n) -> void* {
  auto* p = std::malloc(n);
  if (p == nullptr) return nullptr;
  (arena == GF_ARENA_SECURE ? SecureArena() : NormalArena()).insert(p);
  return p;
}

auto MemRealloc(GFHostContextRef, int, void* p, uint32_t n) -> void* {
  return std::realloc(p, n);
}

void MemFree(GFHostContextRef, int arena, void* p) {
  if (p == nullptr) return;
  auto& mine = arena == GF_ARENA_SECURE ? SecureArena() : NormalArena();
  if (mine.erase(p) == 0) {
    std::printf(
        "[FAIL] free from the wrong arena (this is the munmap_chunk abort)\n");
    ++g_allocator_violations;
    return;
  }
  std::free(p);
}

auto MemStrDup(GFHostContextRef ctx, int arena, const char* s) -> char* {
  if (s == nullptr) return nullptr;
  const auto n = std::strlen(s);
  auto* copy = static_cast<char*>(MemAlloc(ctx, arena, n + 1));
  if (copy == nullptr) return nullptr;
  std::memcpy(copy, s, n + 1);
  return copy;
}

const GFHostBufferApi kBuffer = {
    sizeof(GFHostBufferApi),
    &BufNew,
    &BufData,
    &BufSize,
    &BufZeroize,
    &BufRelease,
    &BufOutstanding,
    &MemAlloc,
    &MemRealloc,
    &MemFree,
    &MemStrDup,
};

/* --- log ------------------------------------------------------------------ */

void LogWrite(GFHostContextRef, int, const char*, int, const char*,
              const char*) {}
auto LogEnabled(GFHostContextRef, int) -> int { return 1; }

const GFHostLogApi kLog = {sizeof(GFHostLogApi), &LogWrite, &LogEnabled};

/* --- app ------------------------------------------------------------------ */

auto AppText(GFHostContextRef) -> const char* { return "test"; }
auto AppLocale(GFHostContextRef ctx) -> char* {
  return MemStrDup(ctx, GF_ARENA_NORMAL, "en_US");
}
auto AppZero(GFHostContextRef) -> int { return 0; }
auto AppKeyProtection(GFHostContextRef) -> int { return 1; }

const GFHostAppApi kApp = {
    sizeof(GFHostAppApi), &AppText, &AppText,          &AppText, &AppText,
    &AppLocale,           &AppZero, &AppKeyProtection,
};

/* --- ui ------------------------------------------------------------------- */

auto ThemeColor(GFHostContextRef, int role, void*) -> uint32_t {
  switch (role) {
    case GF_UI_COLOR_WARNING:
      return 0xFFFF8800;
    case GF_UI_COLOR_DANGER:
      return 0xFFFF0000;
    case GF_UI_COLOR_ACCENT_POSITIVE:
    case GF_UI_COLOR_ACCENT_NEGATIVE:
      return 0xFF0066CC;
    default:
      return 0xFF808080;
  }
}

auto UiNull(GFHostContextRef, const char*) -> void* { return nullptr; }
auto UiCreate(GFHostContextRef, QObjectFactory, void*) -> void* {
  return nullptr;
}
auto UiShow(GFHostContextRef, void*, void*) -> int { return 0; }
auto UiPath(GFHostContextRef ctx) -> GFBufferRef {
  return BufNew(ctx, "/tmp", 4);
}
auto UiRegSettings(GFHostContextRef, const GFUISettingsPageSpec*) -> int {
  return 0;
}
auto UiUnreg(GFHostContextRef, const char*) -> int { return 0; }
auto UiRegTab(GFHostContextRef, const GFUITabViewSpec*) -> int { return 0; }
auto UiRegExt(GFHostContextRef, const char*, const char*) -> int { return 0; }

const GFHostUiApi kUi = {
    sizeof(GFHostUiApi), &UiCreate, &UiNull,   &UiShow,  &ThemeColor, &UiPath,
    &UiRegSettings,      &UiUnreg,  &UiRegTab, &UiUnreg, &UiRegExt,
};

/* --- storage -------------------------------------------------------------- */

auto SettingsRoot(GFHostContextRef) -> void* {
  static QSettings* settings = nullptr;
  if (settings == nullptr) {
    settings = new QSettings("/tmp/gf-focus-harness.ini", QSettings::IniFormat);
  }
  return settings;
}

auto CacheGet(GFHostContextRef, int, const char*, GFBufferRef* out) -> int {
  if (out != nullptr) *out = nullptr;
  return -1;  // nothing stored: absence is reportable now
}

auto CacheSet(GFHostContextRef ctx, int store, const char*, GFBufferView value,
              int64_t) -> int {
  // The secret arrives as a buffer, which is the whole point: it never passes
  // through an allocation the module cannot wipe. Recorded so the credential
  // test can still assert the secure tier was the one used.
  harness::LastSaveUsedSecureValue =
      store == GF_STORE_SECURE_DURABLE && BufSize(ctx, value) != 0;
  return 0;
}

auto CacheRemove(GFHostContextRef, int, const char*) -> int { return 0; }
auto StateGetText(GFHostContextRef, const char*, const char*, GFBufferRef* out)
    -> int {
  if (out != nullptr) *out = nullptr;
  return -1;
}
auto StateSetText(GFHostContextRef, const char*, const char*, GFBufferView)
    -> int {
  return 0;
}
auto StateGetBool(GFHostContextRef, const char*, const char*, int*) -> int {
  return -1;
}
auto StateSetBool(GFHostContextRef, const char*, const char*, int) -> int {
  return 0;
}
auto StateChildren(GFHostContextRef, const char*, const char*,
                   GFStringListRef* out) -> int {
  if (out != nullptr) *out = nullptr;
  return -1;
}

const GFHostStorageApi kStorage = {
    sizeof(GFHostStorageApi),
    &SettingsRoot,
    &CacheGet,
    &CacheSet,
    &CacheRemove,
    &StateGetText,
    &StateSetText,
    &StateGetBool,
    &StateSetBool,
    &StateChildren,
};

/* --- the table ------------------------------------------------------------ */

struct FakeHostContext {
  int marker = 0;
};

auto TheTable() -> GFHostApi& {
  static FakeHostContext host_context;
  static GFHostApi host = [&]() {
    GFHostApi t{};
    t.struct_size = sizeof(GFHostApi);
    t.abi_version = GF_SDK_ABI_VERSION;
    t.module_id = "harness";
    t.context = reinterpret_cast<GFHostContextRef>(&host_context);
    t.buffer = &kBuffer;
    t.log = &kLog;
    t.app = &kApp;
    t.ui = &kUi;
    t.storage = &kStorage;
    return t;
  }();
  return host;
}

}  // namespace

/// The module's SDK context, which the runtime would normally own. These
/// tests link no runtime, so the harness provides it.
auto GFModuleSdkContext() -> GFSDKContext* {
  static GFSDKContext context = []() {
    GFSDKContext c{};
    c.struct_size = sizeof(GFSDKContext);
    c.abi_version = GF_SDK_ABI_VERSION;
    c.host = &TheTable();
    c.module_id = "harness";
    return c;
  }();
  return &context;
}

/// Identity, for the log macros. C++ linkage, as GFModule.h declares it.
const char* GFGetModuleID() { return "harness"; }
