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

char* GFAppActiveLocale() { return GFModuleStrDup("en_US"); }

void GFGpgFreeResult(void*) {}

char* GFGpgPublicKey(int, char* key_id, int) {
  GFFreeMemory(key_id);
  return GFModuleStrDup(
      "-----BEGIN PGP PUBLIC KEY BLOCK-----\nstub\n"
      "-----END PGP PUBLIC KEY BLOCK-----\n");
}

// --- the four crypto entry points -------------------------------------------
//
// Both forms are provided. The string forms mirror the real SDK exactly,
// truncation included, so a test can still demonstrate what the old boundary
// did; the N forms record the length they were given.

int GFGpgVerifyData(int, char* data, char* signature, GFGpgVerifyResult** ps) {
  g_recording.verify.append(
      {QByteArray(data == nullptr ? "" : data),
       QByteArray(signature == nullptr ? "" : signature)});

  auto* mem = static_cast<GFGpgVerifyResult*>(
      GFAllocateMemory(sizeof(GFGpgVerifyResult)));
  std::memset(mem, 0, sizeof(GFGpgVerifyResult));
  mem->capsule_id = GFModuleStrDup("stub-capsule");
  mem->error_string = GFModuleStrDup("Success");
  mem->gpgme_error = 0;
  *ps = mem;

  if (data != nullptr) GFFreeMemory(data);
  if (signature != nullptr) GFFreeMemory(signature);
  return 0;
}

int GFGpgVerifyDataN(int, const char* data, size_t data_size,
                     const char* signature, size_t signature_size,
                     GFGpgVerifyResult** ps) {
  g_recording.verify.append(
      {QByteArray(data, static_cast<int>(data_size)),
       QByteArray(signature, static_cast<int>(signature_size))});

  auto* mem = static_cast<GFGpgVerifyResult*>(
      GFAllocateMemory(sizeof(GFGpgVerifyResult)));
  std::memset(mem, 0, sizeof(GFGpgVerifyResult));
  mem->capsule_id = GFModuleStrDup("stub-capsule");
  mem->error_string = GFModuleStrDup("Success");
  mem->gpgme_error = 0;
  *ps = mem;
  return 0;
}

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

int GFGpgDecryptDataN(int, const char* data, size_t data_size,
                      GFGpgDecryptResult** ps) {
  g_recording.decrypt.append({QByteArray(data, static_cast<int>(data_size))});

  auto* mem = static_cast<GFGpgDecryptResult*>(
      GFAllocateMemory(sizeof(GFGpgDecryptResult)));
  std::memset(mem, 0, sizeof(GFGpgDecryptResult));
  mem->decrypted_data =
      BytesOut(g_recording.decrypt_output, &mem->decrypted_data_size);
  mem->capsule_id = GFModuleStrDup("stub-capsule");
  mem->error_string = GFModuleStrDup("Success");
  mem->gpgme_error = 0;
  *ps = mem;
  return 0;
}

int GFGpgDecryptData(int channel, char* data, GFGpgDecryptResult** ps) {
  const auto size = data == nullptr ? 0 : std::strlen(data);
  auto ret = GFGpgDecryptDataN(channel, data, size, ps);
  if (data != nullptr) GFFreeMemory(data);
  return ret;
}

int GFGpgSignDataN(int, char** key_ids, int key_ids_size, const char* data,
                   size_t data_size, int sign_mode, int, GFGpgSignResult** ps) {
  g_recording.sign.append(
      {QByteArray(data, static_cast<int>(data_size)), sign_mode});
  for (int i = 0; i < key_ids_size; ++i) GFFreeMemory(key_ids[i]);
  if (key_ids != nullptr) GFFreeMemory(key_ids);

  auto* mem =
      static_cast<GFGpgSignResult*>(GFAllocateMemory(sizeof(GFGpgSignResult)));
  std::memset(mem, 0, sizeof(GFGpgSignResult));
  mem->signature = BytesOut(g_recording.sign_output, &mem->signature_size);
  mem->hash_algo = GFModuleStrDup("SHA256");
  mem->capsule_id = GFModuleStrDup("stub-capsule");
  mem->error_string = GFModuleStrDup("Success");
  mem->gpgme_error = 0;
  *ps = mem;
  return 0;
}

int GFGpgSignData(int channel, char** key_ids, int key_ids_size, char* data,
                  int sign_mode, int ascii, GFGpgSignResult** ps) {
  const auto size = data == nullptr ? 0 : std::strlen(data);
  auto ret = GFGpgSignDataN(channel, key_ids, key_ids_size, data, size,
                            sign_mode, ascii, ps);
  if (data != nullptr) GFFreeMemory(data);
  return ret;
}

int GFGpgEncryptDataN(int, char** key_ids, int key_ids_size, const char* data,
                      size_t data_size, int, GFGpgEncryptionResult** ps) {
  g_recording.encrypt.append({QByteArray(data, static_cast<int>(data_size))});
  for (int i = 0; i < key_ids_size; ++i) GFFreeMemory(key_ids[i]);
  if (key_ids != nullptr) GFFreeMemory(key_ids);

  auto* mem = static_cast<GFGpgEncryptionResult*>(
      GFAllocateMemory(sizeof(GFGpgEncryptionResult)));
  std::memset(mem, 0, sizeof(GFGpgEncryptionResult));
  mem->encrypted_data =
      BytesOut(g_recording.encrypt_output, &mem->encrypted_data_size);
  mem->capsule_id = GFModuleStrDup("stub-capsule");
  mem->error_string = GFModuleStrDup("Success");
  mem->gpgme_error = 0;
  *ps = mem;
  return 0;
}

int GFGpgEncryptData(int channel, char** key_ids, int key_ids_size, char* data,
                     int ascii, GFGpgEncryptionResult** ps) {
  const auto size = data == nullptr ? 0 : std::strlen(data);
  auto ret =
      GFGpgEncryptDataN(channel, key_ids, key_ids_size, data, size, ascii, ps);
  if (data != nullptr) GFFreeMemory(data);
  return ret;
}

int GFAnalyseVerifyResultInfoByCapsule(int, gpgme_error_t, char* capsule_id,
                                       const char** analyse, const char** cards,
                                       const char** info_json) {
  if (capsule_id != nullptr) GFFreeMemory(capsule_id);
  if (analyse != nullptr) *analyse = GFModuleStrDup("");
  if (cards != nullptr) *cards = GFModuleStrDup("[]");
  // One good signature, so ParseSignatureResults has something to walk.
  if (info_json != nullptr) {
    *info_json = GFModuleStrDup(
        R"({"signatures":[{"status":"good","fingerprint":"DEADBEEF",)"
        R"("hash_algo":"SHA256","uid":"Test <t@example.com>"}]})");
  }
  return 0;
}

}  // extern "C"
