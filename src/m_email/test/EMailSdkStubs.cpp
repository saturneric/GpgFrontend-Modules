// Stand-ins for the SDK symbols these translation units reference.
//
// The allocator stubs are not merely placeholders: they enforce the same
// ownership contract the real SDK does, so that handing a function memory from
// the wrong arena is reported here instead of aborting the process with
// munmap_chunk() at runtime.
#include <QSettings>
#include <QString>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>

namespace {
std::set<void*>& NormalArena() { static std::set<void*> a; return a; }
std::set<void*>& SecureArena() { static std::set<void*> a; return a; }
}  // namespace

int g_allocator_violations = 0;

namespace harness {
bool LastSaveUsedSecureValue = false;
}

extern "C" {

void* GFAllocateMemory(unsigned int n) {
  void* p = std::malloc(n);
  NormalArena().insert(p);
  return p;
}

void GFFreeMemory(void* p) {
  if (p == nullptr) return;
  if (NormalArena().erase(p) == 0) {
    std::printf("[FAIL] GFFreeMemory on a pointer it does not own\n");
    ++g_allocator_violations;
    return;
  }
  std::free(p);
}

void* GFSecAllocateMemory(unsigned int n) {
  void* p = std::malloc(n);
  SecureArena().insert(p);
  return p;
}

void GFSecFreeMemory(void* p) {
  if (p == nullptr) return;
  if (SecureArena().erase(p) == 0) {
    // Exactly the real crash: a pointer that never came from the secure
    // allocator handed to the secure deallocator.
    std::printf("[FAIL] GFSecFreeMemory on a pointer it does not own "
                "(this is the munmap_chunk abort)\n");
    ++g_allocator_violations;
    return;
  }
  std::free(p);
}

char* GFModuleStrDup(const char* s) {
  if (s == nullptr) return nullptr;
  auto n = std::strlen(s) + 1;
  char* d = static_cast<char*>(GFAllocateMemory(n));
  std::memcpy(d, s, n);
  return d;
}

char* GFModuleSecStrDup(const char* s) {
  if (s == nullptr) return nullptr;
  auto n = std::strlen(s) + 1;
  char* d = static_cast<char*>(GFSecAllocateMemory(n));
  std::memcpy(d, s, n);
  return d;
}

// Mirrors the real implementation's ownership behaviour: the key goes back to
// the ordinary allocator, the secret to the secure one.
int GFSecDurableCacheSave(const char* key, const char* value) {
  if (key == nullptr || value == nullptr) return -1;
  harness::LastSaveUsedSecureValue =
      SecureArena().count(const_cast<char*>(value)) != 0;
  GFFreeMemory(const_cast<char*>(key));
  GFSecFreeMemory(const_cast<char*>(value));
  return 0;
}

char* GFSecDurableCacheGet(const char* key) {
  GFFreeMemory(const_cast<char*>(key));
  return nullptr;
}

int GFSecDurableCacheRemove(const char* key) {
  GFFreeMemory(const_cast<char*>(key));
  return 0;
}

int GFAppKeyProtectionLevel() { return 1; }

void* GFUIGlobalSettings() {
  static QSettings* settings = nullptr;
  if (settings == nullptr) {
    settings = new QSettings("/tmp/gf-focus-harness.ini", QSettings::IniFormat);
  }
  return settings;
}

void GFModuleLogTrace(const char*) {}
void GFModuleLogDebug(const char*) {}
void GFModuleLogInfo(const char*) {}
void GFModuleLogWarn(const char*) {}
void GFModuleLogError(const char*) {}

const char* GFGetModuleID() { return "harness"; }
char* GFAppActiveLocale() { return GFModuleStrDup("en_US"); }
unsigned int GFUIMutedTextColor(void*) { return 0xFF808080; }
unsigned int GFUIBorderColor(void*) { return 0xFF808080; }
unsigned int GFUIWarningColor(void*) { return 0xFFFF8800; }
unsigned int GFUIDangerColor(void*) { return 0xFFFF0000; }
unsigned int GFUIAccentColor(void*, int) { return 0xFF0066CC; }
char* GFUIHumanSize(long long) { return GFModuleStrDup("0 B"); }
}
