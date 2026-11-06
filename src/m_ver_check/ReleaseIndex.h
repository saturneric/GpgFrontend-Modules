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
#include <QString>
#include <QStringList>
#include <optional>

#include "SoftwareVersion.h"

/**
 * @file ReleaseIndex.h
 * @brief The signed update index, read with no network and no Host.
 *
 * scripts/release/gfrelease publishes <base><product>/v1/index.json and its
 * minisign signature. Nothing in it counts until the signature verifies
 * against a pinned key (Minisign.h); this file reads it after that, and maps
 * it onto the same facts a GitHub check yields, so Decide() and everything
 * downstream stay one model.
 */

/// Where the index and builds are, and the keys allowed to sign the index.
struct ReleaseChannel {
  QString product;           ///< the product slug, e.g. "gpgfrontend"
  QStringList update_bases;  ///< https://.../ ; tried in order
  QStringList build_bases;   ///< https://.../ ; tried in order
  QStringList index_keys;    ///< minisign public keys: current, next

  [[nodiscard]] auto IsConfigured() const -> bool {
    return !product.isEmpty() && !update_bases.isEmpty() &&
           !index_keys.isEmpty();
  }
};

/// What a verified index says that a client acts on.
struct IndexFacts {
  qint64 serial = 0;
  QDateTime generated_at;
  QDateTime expires;
  QString nightly_version;  ///< empty when there is no nightly
  QStringList withdrawn;    ///< versions, as published

  /// The nightly's descriptor (its files), as a product-relative path, and the
  /// sha256 the signed index states for it: fetching it is then as trusted
  /// as the index. Empty when the index names none.
  QString nightly_descriptor;
  QString nightly_descriptor_sha256;
};

/// The machine a download is for.
struct Platform {
  QString os;          ///< "linux", "windows", "macos"
  QString arch;        ///< "x86_64", "arm64" (aarch64 is spelled arm64)
  QString os_version;  ///< "15.3" for macOS; may be empty elsewhere
  /// How the running build was packaged (GFAppInstallKind); empty from a
  /// Host too old to say.
  QString install_kind;
};

/// A path inside a product's tree that cannot reach outside it: relative, no
/// scheme, no "..", no empty, dot or percent-encoded segment.
auto IsSafeRelativePath(const QString& path) -> bool;

/**
 * @brief The product-relative path of the file to download on @p platform,
 * from a release descriptor, or empty when none fits.
 *
 * The same kind of package as the running one: an AppImage for an AppImage,
 * the portable one for a portable build, the installer for an installed one.
 * A build a store, a distribution or Flatpak keeps up to date is offered no
 * file at all; one whose packaging is unknown ("source", or a Host too old to
 * say) is offered the installed package of its platform. Among several that
 * fit (macOS disk images built for different releases), the one built for
 * the newest macOS this machine runs; one built for a newer macOS than it
 * runs never fits.
 */
auto SelectDownload(const QByteArray& descriptor, const Platform& platform)
    -> QString;

/// A build of the nightly identity follows the nightly channel and nothing
/// else. Until the Host tells it outright, a nightly is a build whose version
/// is an untagged one: "X.Y.Z-alpha.N" or "...-beta.K.dev.N".
auto IsNightlyBuild(const QString& version) -> bool;

/// How long past `expires` an index is still taken: a client clock a little
/// wrong must not turn a good index into "unknown".
inline constexpr qint64 kExpirySkewSeconds = 24 * 60 * 60;

/**
 * @brief The facts of an index whose signature verified, or nullopt.
 *
 * Refused: another schema, another product (in the document or in the
 * trusted comment, which must be exactly "product=<slug> serial=<n>"), a
 * serial that disagrees with the comment, and an index expired by more than
 * kExpirySkewSeconds at @p now.
 */
auto ParseIndex(const QByteArray& json, const QString& trusted_comment,
                const QString& product, const QDateTime& now)
    -> std::optional<IndexFacts>;

/// The nightly channel as a release list: kFound with the channel's version,
/// kNoMatch when the index names no nightly. Decide() then compares.
auto NightlyList(const IndexFacts& facts) -> ListResult;

/// kNotFound when @p current is withdrawn (the index says so; a missing
/// object never does), kConfirmed otherwise.
auto WithdrawnFact(const IndexFacts& facts, const QString& current)
    -> RemoteFact;

/// `GET <builds>/<product>/commits/<sha>`: the signing host writes one for
/// every build it published. 200 confirms, 404 denies, the rest says nothing.
auto ParseCommitMarker(int http_status) -> RemoteFact;
