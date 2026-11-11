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
#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <optional>

/**
 * @file RootTrust.h
 * @brief Which keys may sign the update index, and why: the root and the
 * index-keys list (scripts/release/gfrelease/trust.py says the same in
 * Python).
 *
 * The root names the offline root keys and how many of them must sign the
 * next root, and the one key ("G") that signs the index-keys list; the list
 * names the keys that may sign v1/index.json. Every signature is minisign,
 * prehashed only, over the file's bytes as they are, with an exact trusted
 * comment: "product=<p> root=<N>" on a root (one file per root key,
 * <N>.root.json.<keyid>.minisig), "product=<p> index-keys=<V>" on a list.
 *
 * A root N+1 is taken only when enough of root N's keys and enough of its
 * own signed it, and it is exactly N+1. Signature files that do not verify
 * are not counted, never an error. Only the newest root must be unexpired.
 */

/// A version of a trust file, its signatures by key id, as fetched.
struct SignedFile {
  QByteArray data;
  QHash<QString, QByteArray> signatures;  ///< key id -> .minisig
};

/// A root whose signatures held.
struct TrustRoot {
  qint64 version = 0;
  QDateTime expires;
  QHash<QString, QString> keys;  ///< key id -> public key
  QStringList root_keyids;
  int threshold = 0;
  QStringList index_list_keyids;  ///< who signs the index-keys list (G)
  SignedFile file;                ///< as it was taken, to keep it
};

/// An index-keys list whose signature held.
struct IndexKeys {
  qint64 version = 0;
  QDateTime expires;
  QHash<QString, QString> keys;  ///< key id -> public key
  SignedFile file;
};

/// The root in @p file, signed by enough of its own root keys: what a client
/// starts from (the one built in, or one it took before). @p why says what
/// is wrong when there is nothing.
auto ParseRoot(const SignedFile& file, const QString& product,
               QString* why = nullptr) -> std::optional<TrustRoot>;

/// The root in @p file when it may follow @p trusted: version + 1, and
/// signed by enough root keys of both.
auto AcceptNextRoot(const TrustRoot& trusted, const SignedFile& file,
                    const QString& product, QString* why = nullptr)
    -> std::optional<TrustRoot>;

/// The index-keys list in @p file when G of @p root signed it (its one
/// signature, in file.signatures under any key) and it is not older than
/// @p floor.
auto ParseIndexKeys(const TrustRoot& root, const SignedFile& file,
                    const QString& product, qint64 floor,
                    QString* why = nullptr) -> std::optional<IndexKeys>;

/// Past @p expires by more than a clock's error (kExpirySkewSeconds).
auto IsTrustExpired(const QDateTime& expires, const QDateTime& now) -> bool;

/// What an index, not yet verified, says it is signed under: where to walk
/// to before its signature is checked. 0 for an index from before it said.
struct TrustHints {
  qint64 root_version = 0;
  qint64 index_keys_version = 0;
};
auto ReadTrustHints(const QByteArray& index) -> TrustHints;

/// The root keys @p root names, read without checking anything: which
/// signature files to fetch for it.
auto NamedRootKeys(const QByteArray& root) -> QStringList;

/// How far one check walks the roots.
inline constexpr int kMaxRootSteps = 32;
