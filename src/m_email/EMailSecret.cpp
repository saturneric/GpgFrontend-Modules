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

#include "EMailSecret.h"

#include <QByteArray>
#include <QString>
#include <cstring>

#include "GFModuleCommonUtils.hpp"

namespace {

/// Overwrites in a way the compiler may not elide.
///
/// A plain memset over memory that is about to be freed is dead-store-
/// eliminable, and optimisers do eliminate it. The volatile pointer is what
/// stops that; it is the reason this is not simply std::fill.
void SecureZero(char* data, size_t size) {
  if (data == nullptr || size == 0) return;
  volatile char* p = data;
  while (size-- > 0) *p++ = 0;
}

}  // namespace

void EMailSecret::Wipe() {
  if (bytes_.empty()) return;
  SecureZero(bytes_.data(), bytes_.size());
  bytes_.clear();
  bytes_.shrink_to_fit();
}

auto EMailSecret::AdoptCString(char* source) -> std::shared_ptr<EMailSecret> {
  auto secret = std::make_shared<EMailSecret>();
  if (source == nullptr) return secret;

  const auto length = std::strlen(source);
  secret->bytes_.assign(source, source + length);

  // The SDK's buffer is wiped and returned to the secure allocator it came
  // from, so the secret exists in exactly one place from here on.
  SecureZero(source, length);
  GFSecFreeMemory(static_cast<void*>(source));
  return secret;
}

auto EMailSecret::CopyFrom(const QString& text) -> std::shared_ptr<EMailSecret> {
  auto secret = std::make_shared<EMailSecret>();

  // A freshly built temporary, so its buffer is unshared and overwriting it
  // reaches the bytes rather than detaching a copy -- which is exactly the
  // distinction this class exists to make. The QString the caller holds is
  // still beyond reach; that is theirs to let go of quickly.
  auto utf8 = text.toUtf8();
  secret->bytes_.assign(utf8.constData(), utf8.constData() + utf8.size());
  SecureZero(utf8.data(), static_cast<size_t>(utf8.size()));
  return secret;
}

auto EMailSecret::ToSecureCString() const -> char* {
  auto* buffer = static_cast<char*>(
      GFSecAllocateMemory(static_cast<uint32_t>(bytes_.size() + 1)));
  if (buffer == nullptr) return nullptr;

  if (!bytes_.empty()) std::memcpy(buffer, bytes_.data(), bytes_.size());
  buffer[bytes_.size()] = '\0';
  return buffer;
}
