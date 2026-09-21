/**
 * Copyright (C) 2021-2024 Saturneric <eric@bktus.com>
 *
 * This file is part of GpgFrontend.
 *
 * GpgFrontend is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GpgFrontend is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GpgFrontend. If not, see <https://www.gnu.org/licenses/>.
 *
 * The initial version of the source code is inherited from
 * the gpg4usb project, which is under GPL-3.0-or-later.
 *
 * All the source code of GpgFrontend was modified and released by
 * Saturneric <eric@bktus.com> starting on May 12, 2021.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

// Recording stand-ins for the crypto SDK symbols EMailBasicGpgOpera.cpp calls.
//
// These are not merely placeholders. The point of this target is to pin down
// exactly which OCTETS the module hands to GPG, because that -- not what the
// module parsed, hashed, or displayed -- is what a signature actually covers.
// So every entry point records its inputs verbatim and the tests assert byte
// equality against the slice the module claims it verified.
//
// GFModuleStrDup mirrors the REAL implementation, NUL truncation included
// (src/sdk/GFSDKBasic.cpp). Weakening it here would hide the very class of bug
// this target exists to catch.

#include "EMailCryptoRecorder.h"

//
#include <GFSDKGpg.h>

#include <QByteArray>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>

namespace {
auto NormalArena() -> std::set<void*>& {
  static std::set<void*> a;
  return a;
}
auto SecureArena() -> std::set<void*>& {
  static std::set<void*> a;
  return a;
}
}  // namespace

int g_allocator_violations = 0;

namespace crypto_recorder {

Recording g_recording;

void Reset() { g_recording = Recording{}; }

auto OutstandingAllocations() -> int {
  return static_cast<int>(NormalArena().size() + SecureArena().size());
}

auto Get() -> Recording& { return g_recording; }

}  // namespace crypto_recorder

using crypto_recorder::g_recording;

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
    std::printf("[FAIL] GFSecFreeMemory on a pointer it does not own\n");
    ++g_allocator_violations;
    return;
  }
  std::free(p);
}

// Deliberately identical to the shipping implementation, truncation and all.
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

void GFModuleLogTrace(const char*) {}
void GFModuleLogDebug(const char*) {}
void GFModuleLogInfo(const char*) {}
void GFModuleLogWarn(const char*) {}
void GFModuleLogError(const char*) {}
void GFModuleLogAt(const char*, int, const char*, int, const char*,
                   const char*) {}
int GFModuleLogEnabled(const char*, int) { return 1; }

char* GFAppActiveLocale() { return GFModuleStrDup("en_US"); }

char* GFGpgPublicKey(int, const char* key_id, int) {
  // Borrowed, not consumed: the real entry point stopped freeing its
  // arguments, and a stub that still frees would report a bogus allocator
  // violation for every correct call.
  (void)key_id;
  return GFModuleStrDup(
      "-----BEGIN PGP PUBLIC KEY BLOCK-----\nstub\n"
      "-----END PGP PUBLIC KEY BLOCK-----\n");
}

// --- the four crypto entry points -------------------------------------------
//
// Both forms are provided. The string forms mirror the real SDK exactly,
// truncation included, so a test can still demonstrate what the old boundary
// did; the N forms record the length they were given.

namespace {
// Mirrors GFBytesDup on the host side: byte-exact, NUL-terminated one past the
// end, with the true length reported separately.
auto BytesOut(const QByteArray& b, size_t* size) -> char* {
  auto* d = static_cast<char*>(GFAllocateMemory(b.size() + 1));
  std::memcpy(d, b.constData(), b.size());
  d[b.size()] = '\0';
  if (size != nullptr) *size = static_cast<size_t>(b.size());
  return d;
}
}  // namespace

int GFAnalyseVerifyResultInfoByCapsule(int, uint32_t, const char* capsule_id,
                                       const char** analyse, const char** cards,
                                       const char** info_json) {
  (void)capsule_id;  // borrowed, like every SDK argument
  if (analyse != nullptr) *analyse = GFModuleStrDup("");
  if (cards != nullptr) *cards = GFModuleStrDup("[]");

  // One good signature, so ParseSignatureResults has something to walk --
  // unless the test queued an answer for this call. Regions are analysed in
  // the order they are verified, so the queue is consumed the same way.
  if (info_json != nullptr) {
    auto& recording = crypto_recorder::Get();
    if (!recording.verify_info_json.isEmpty()) {
      const auto queued = recording.verify_info_json.takeFirst();
      *info_json = GFModuleStrDup(queued.constData());
    } else {
      *info_json = GFModuleStrDup(
          R"({"signatures":[{"status":"good","fingerprint":"DEADBEEF",)"
          R"("hash_algo":"SHA256","uid":"Test <t@example.com>"}]})");
    }
  }
  return 0;
}

}  // extern "C"

/* ===================================================================== *
 * Handle-based SDK: buffers and results
 *
 * Same recording behaviour as the struct-based stubs above, through the
 * API that replaced them. Two things are deliberately faithful to the real
 * implementation rather than convenient:
 *
 *   - every handle is tracked, so a double release or a foreign pointer is
 *     REPORTED here exactly as the real registry reports it, instead of
 *     silently "working" in tests and corrupting the heap in production;
 *   - a FAILED operation still hands back an owned result carrying the
 *     reason. That is the contract the old struct API got wrong by accident,
 *     and the leak tests depend on it being reproduced honestly.
 * ===================================================================== */

#include <GFSDKBuffer.h>
#include <GFSDKGpgResult.h>

#include <map>

namespace {

struct StubBuffer {
  QByteArray bytes;
};

struct StubResult {
  int status = GF_GPG_OK;
  uint32_t error = 0;
  GFBufferRef data = nullptr;
  QByteArray capsule_id;
  QByteArray error_string;
  QByteArray hash_algo;
};

auto LiveBuffers() -> std::set<void*>& {
  static std::set<void*> live;
  return live;
}
auto LiveResults() -> std::set<void*>& {
  static std::set<void*> live;
  return live;
}

/// Validate against the live set BEFORE dereferencing -- reading a magic word
/// out of a released handle would be the very use-after-free being guarded.
auto LiveBuf(GFBufferView b) -> StubBuffer* {
  if (b == nullptr) return nullptr;
  auto* raw = const_cast<void*>(static_cast<const void*>(b));
  if (LiveBuffers().count(raw) == 0) {
    std::printf("[FAIL] buffer handle is not live\n");
    ++g_allocator_violations;
    return nullptr;
  }
  return static_cast<StubBuffer*>(raw);
}

auto LiveRes(GFGpgResultRef r) -> StubResult* {
  if (r == nullptr) return nullptr;
  if (LiveResults().count(r) == 0) {
    std::printf("[FAIL] result handle is not live\n");
    ++g_allocator_violations;
    return nullptr;
  }
  return reinterpret_cast<StubResult*>(r);
}

auto NewResult() -> StubResult* {
  auto* r = new StubResult();
  LiveResults().insert(r);
  return r;
}

auto BufferBytes(GFBufferView b) -> QByteArray {
  auto* impl = LiveBuf(b);
  return impl == nullptr ? QByteArray() : impl->bytes;
}

auto KeyIdCount(const char* const* key_ids, size_t n) -> int {
  if (key_ids == nullptr) return 0;
  return static_cast<int>(n);
}

}  // namespace

extern "C" {

GFBufferRef GFBufferNewFromBytes(const void* data, size_t size) {
  if (data == nullptr && size != 0) return nullptr;
  auto* b = new StubBuffer{
      QByteArray(static_cast<const char*>(data), static_cast<int>(size))};
  LiveBuffers().insert(b);
  return reinterpret_cast<GFBufferRef>(b);
}

const void* GFBufferData(GFBufferView buf) {
  auto* impl = LiveBuf(buf);
  return impl == nullptr ? nullptr : impl->bytes.constData();
}

size_t GFBufferSize(GFBufferView buf) {
  auto* impl = LiveBuf(buf);
  return impl == nullptr ? 0 : static_cast<size_t>(impl->bytes.size());
}

void GFBufferZeroize(GFBufferRef buf) {
  auto* impl = LiveBuf(buf);
  if (impl != nullptr) impl->bytes.fill('\0');
}

void GFBufferRelease(GFBufferRef buf) {
  if (buf == nullptr) return;
  if (LiveBuffers().erase(buf) == 0) {
    std::printf("[FAIL] GFBufferRelease on a handle it does not own\n");
    ++g_allocator_violations;
    return;
  }
  delete reinterpret_cast<StubBuffer*>(buf);
}

size_t GFBufferOutstandingCount(const char*) { return LiveBuffers().size(); }

int GFGpgSign(int, const char* const* key_ids, size_t key_ids_size,
              GFBufferView in, int sign_mode, int, GFGpgResultRef* out) {
  if (out == nullptr) return -1;
  g_recording.sign.append({BufferBytes(in), sign_mode});
  (void)KeyIdCount(key_ids, key_ids_size);

  auto* r = NewResult();
  if (g_recording.fail_next) {
    r->status = GF_GPG_OP_FAILED;
    r->error_string = g_recording.fail_error_string;
    *out = reinterpret_cast<GFGpgResultRef>(r);
    return GF_GPG_OP_FAILED;
  }

  r->data = GFBufferNewFromBytes(g_recording.sign_output.constData(),
                                 g_recording.sign_output.size());
  r->hash_algo = "SHA256";
  r->capsule_id = "stub-capsule";
  r->error_string = "Success";
  *out = reinterpret_cast<GFGpgResultRef>(r);
  return GF_GPG_OK;
}

int GFGpgEncrypt(int, const char* const* key_ids, size_t key_ids_size,
                 GFBufferView in, int, GFGpgResultRef* out) {
  if (out == nullptr) return -1;
  g_recording.encrypt.append({BufferBytes(in)});
  (void)KeyIdCount(key_ids, key_ids_size);

  auto* r = NewResult();
  if (g_recording.fail_next) {
    r->status = GF_GPG_OP_FAILED;
    r->error_string = g_recording.fail_error_string;
    *out = reinterpret_cast<GFGpgResultRef>(r);
    return GF_GPG_OP_FAILED;
  }

  r->data = GFBufferNewFromBytes(g_recording.encrypt_output.constData(),
                                 g_recording.encrypt_output.size());
  r->capsule_id = "stub-capsule";
  r->error_string = "Success";
  *out = reinterpret_cast<GFGpgResultRef>(r);
  return GF_GPG_OK;
}

int GFGpgDecrypt(int, GFBufferView in, GFGpgResultRef* out) {
  if (out == nullptr) return -1;
  g_recording.decrypt.append({BufferBytes(in)});

  auto* r = NewResult();
  if (g_recording.fail_next) {
    r->status = GF_GPG_OP_FAILED;
    r->error_string = g_recording.fail_error_string;
    *out = reinterpret_cast<GFGpgResultRef>(r);
    return GF_GPG_OP_FAILED;
  }

  r->data = GFBufferNewFromBytes(g_recording.decrypt_output.constData(),
                                 g_recording.decrypt_output.size());
  r->capsule_id = "stub-capsule";
  r->error_string = "Success";
  *out = reinterpret_cast<GFGpgResultRef>(r);
  return GF_GPG_OK;
}

int GFGpgVerify(int, GFBufferView in, GFBufferView signature,
                GFGpgResultRef* out) {
  if (out == nullptr) return -1;
  g_recording.verify.append({BufferBytes(in), BufferBytes(signature)});

  auto* r = NewResult();
  if (g_recording.fail_next) {
    r->status = GF_GPG_OP_FAILED;
    r->error_string = g_recording.fail_error_string;
    *out = reinterpret_cast<GFGpgResultRef>(r);
    return GF_GPG_OP_FAILED;
  }

  r->capsule_id = "stub-capsule";
  r->error_string = "Success";
  *out = reinterpret_cast<GFGpgResultRef>(r);
  return GF_GPG_OK;
}

int GFGpgResultStatusOf(GFGpgResultRef r) {
  auto* impl = LiveRes(r);
  return impl == nullptr ? GF_GPG_BAD_REQUEST : impl->status;
}

uint32_t GFGpgResultError(GFGpgResultRef r) {
  auto* impl = LiveRes(r);
  return impl == nullptr ? 0 : impl->error;
}

GFBufferView GFGpgResultData(GFGpgResultRef r) {
  auto* impl = LiveRes(r);
  return impl == nullptr ? nullptr : impl->data;
}

const char* GFGpgResultCapsuleId(GFGpgResultRef r) {
  auto* impl = LiveRes(r);
  return impl == nullptr ? "" : impl->capsule_id.constData();
}

const char* GFGpgResultErrorString(GFGpgResultRef r) {
  auto* impl = LiveRes(r);
  return impl == nullptr ? "" : impl->error_string.constData();
}

const char* GFGpgResultHashAlgo(GFGpgResultRef r) {
  auto* impl = LiveRes(r);
  return impl == nullptr ? "" : impl->hash_algo.constData();
}

GFBufferRef GFGpgResultTakeData(GFGpgResultRef r) {
  auto* impl = LiveRes(r);
  if (impl == nullptr) return nullptr;
  auto* taken = impl->data;
  impl->data = nullptr;
  return taken;
}

void GFGpgResultRelease(GFGpgResultRef r) {
  if (r == nullptr) return;
  if (LiveResults().erase(r) == 0) {
    std::printf("[FAIL] GFGpgResultRelease on a handle it does not own\n");
    ++g_allocator_violations;
    return;
  }
  auto* impl = reinterpret_cast<StubResult*>(r);
  GFBufferRelease(impl->data);
  delete impl;
}

size_t GFGpgResultOutstandingCount(const char*) { return LiveResults().size(); }

}  // extern "C"

// C++ linkage, deliberately outside the extern "C" block above: GFModule.h
// declares it as an ordinary C++ function, and the log macros call it on every
// line. Defined here with C linkage it compiles cleanly and fails to link.
const char* GFGetModuleID() { return "harness"; }
