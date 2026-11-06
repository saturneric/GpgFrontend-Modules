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

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <optional>

/**
 * @file Minisign.h
 * @brief Verifies a minisign signature against pinned public keys.
 *
 * The update index is signed with minisign (scripts/release/gfrelease), so
 * anyone can check it with the minisign tool, and this checks it the same
 * way with libsodium: Ed25519 over the BLAKE2b-512 hash of the content ("ED",
 * what minisign makes) or over the content itself (the legacy "Ed"), and
 * Ed25519 over that signature followed by the trusted comment, which is what
 * makes the trusted comment trustworthy. The key id in the signature must be
 * one of the pinned keys'.
 */

/// The trusted comment of @p signature_file when it verifies @p message
/// under one of @p public_keys (each the base64 line of a minisign .pub);
/// nullopt for anything else: another key, other bytes, a malformed file.
auto MinisignVerify(const QStringList& public_keys, const QByteArray& message,
                    const QByteArray& signature_file) -> std::optional<QString>;
