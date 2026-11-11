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

#include "RootTrust.h"
#include "SoftwareVersion.h"

class QUrl;

/**
 * @file ReleaseIndex.h
 * @brief The signed update index, read with no network and no Host.
 *
 * scripts/release/gfrelease publishes <base><product>/v1/index.json and its
 * minisign signature. Nothing in it counts until the signature verifies
 * against a key the trusted index-keys list names (RootTrust.h, Minisign.h);
 * this file reads it after that, and maps
 * it onto the facts Decide() and everything downstream work from.
 *
 * Three lines, one chain of trust each the same: the nightly channel (main's
 * builds), the beta channel (a release branch's builds, offered to those who
 * ask for it), and the stable releases of each series. The beta channel is
 * listed exactly as the nightly one is, and read the same way. A release
 * descriptor is trusted through the
 * hash the index states for it, its files through the hashes the descriptor
 * states. A stable release is listed only after the release server checked
 * its maintainer's OpenPGP signatures; the client checks the index's
 * minisign signature alone, never OpenPGP.
 */

/// The trust a build starts from: the newest root and index-keys list of
/// the tree it was built from (release/root/, release/index-keys/).
struct EmbeddedTrust {
  SignedFile root;
  SignedFile index_keys;
};

/// Where the index and builds are, and the trust its signature rests on.
struct ReleaseChannel {
  QString product;           ///< the product slug, e.g. "gpgfrontend"
  QStringList update_bases;  ///< https://.../ ; tried in order
  QStringList build_bases;   ///< https://.../ ; tried in order
  EmbeddedTrust trust;

  [[nodiscard]] auto IsConfigured() const -> bool {
    return !product.isEmpty() && !update_bases.isEmpty() &&
           !trust.root.data.isEmpty() && !trust.index_keys.data.isEmpty();
  }
};

/// What the index says of one release series (release/policy.toml).
struct SeriesFacts {
  QString status;     ///< "mainline", "stable", "legacy" or "eol"
  QString latest;     ///< its newest listed release; empty when none
  QString successor;  ///< a legacy or eol series: where its users go
  QString eol;        ///< a legacy series: its last day of fixes
  /// The oldest system its builds run on: "windows" (an NT version such as
  /// "10.0.17763"), "macos", "linux_glibc", "android_api".
  QHash<QString, QString> platforms;

  /// Its beta: its release branch's newest build (X.Y.Z-alpha.N, Z > 0),
  /// newer than `latest`; empty when there is none. Only ever a build of this
  /// series. Its descriptor is listed as the nightly's is, and verified the
  /// same way; it may be empty when the index names none.
  QString beta;
  QString beta_descriptor;
  QString beta_descriptor_sha256;
};

/// One listed release: its descriptor, and its notes, each pinned by the
/// sha256 the signed index states, so fetching them is as trusted as it.
struct ReleaseEntry {
  QString descriptor;
  QString descriptor_sha256;
  QString notes;  ///< empty when it is listed without notes
  QString notes_sha256;
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

  /// By "X.Y". Each series' beta is its own (SeriesFacts::beta): a build is
  /// only ever offered the beta of the series it runs.
  QHash<QString, SeriesFacts> series;
  QHash<QString, ReleaseEntry> releases;  ///< by version, as published
};

/// The machine a download is for.
struct Platform {
  QString os;    ///< "linux", "windows", "macos"
  QString arch;  ///< "x86_64", "arm64" (aarch64 is spelled arm64)
  /// "15.3" for macOS, "10.0.19045" (NT version and build) for Windows; may
  /// be empty elsewhere.
  QString os_version;
  /// How the running build was packaged (GFAppInstallKind); empty from a
  /// Host too old to say.
  QString install_kind;
  QString glibc;  ///< "2.39" on Linux with glibc; empty when unknown
};

/// A path inside a product's tree that cannot reach outside it: relative, no
/// scheme, no "..", no empty, dot or percent-encoded segment.
auto IsSafeRelativePath(const QString& path) -> bool;

/// A SHA-256 as the index and descriptors spell it: 64 lowercase hex digits.
auto IsSha256(const QString& hex) -> bool;

/// The SHA-256 of @p data, spelled as IsSha256() wants it.
auto Sha256Hex(const QByteArray& data) -> QString;

/// The file to download on @p platform, from a release descriptor, with what
/// the descriptor says of it; an empty path when none fits.
struct DownloadChoice {
  QString path;     ///< product-relative; empty when no file fits
  QString sha256;   ///< lowercase hex, or empty when the descriptor gives none
  qint64 size = 0;  ///< bytes, or 0 when the descriptor gives none
};

/**
 * @brief What to download on @p platform: the file an in-place update checks
 * against its sha256 and size, or the one "Download" leads to.
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
auto SelectArtifact(const QByteArray& descriptor, const Platform& platform)
    -> DownloadChoice;

/// A build of the nightly identity follows the nightly channel and nothing
/// else. Until the Host tells it outright, a nightly is a build whose version
/// is an untagged one: "X.Y.Z-alpha.N" or "...-beta.K.dev.N".
auto IsNightlyBuild(const QString& version) -> bool;

/// The longest an index may say it is valid, from `generated_at` to
/// `expires`. The host signs for 14 days; one signed for longer is refused, so
/// no index, however obtained, can be replayed to a client for long.
inline constexpr qint64 kMaxIndexLifetimeSeconds = 60LL * 24 * 60 * 60;

/**
 * @brief The facts of an index whose signature verified, or nullopt.
 *
 * Refused: another schema, another product (in the document or in the
 * trusted comment, which must be exactly "product=<slug> serial=<n>"), a
 * serial that disagrees with the comment, an index without its
 * `generated_at`, one valid for longer than kMaxIndexLifetimeSeconds, and one
 * expired by more than kExpirySkewSeconds at @p now. @p expired gets the
 * expiry of an index refused for that reason alone.
 *
 * Each series' beta is its `series.<X.Y>.beta`. An index from before that
 * entry names one beta for all (`channels.beta` and the top-level `beta`):
 * it counts as the beta of its own series only.
 */
auto ParseIndex(const QByteArray& json, const QString& trusted_comment,
                const QString& product, const QDateTime& now,
                QDateTime* expired = nullptr) -> std::optional<IndexFacts>;

/// What a build is offered, and where the offer's files and notes are listed.
struct ChannelChoice {
  ListResult list;  ///< kFound with the version offered, or kNoMatch
  QString descriptor;
  QString descriptor_sha256;
  QString notes;
  QString notes_sha256;
  /// A nightly of a release branch (X.Y.Z-alpha.N with Z > 0) whose series
  /// has no beta: it is offered nothing, rather than a jump to another
  /// series.
  bool other_series = false;
  /// For a release build that does not follow betas: the beta it would be
  /// offered if it did (newer than it runs and than the series' latest);
  /// empty otherwise.
  QString newer_beta;
};

/**
 * @brief What @p current is offered from the index.
 *
 * The nightly identity of main (X.Y.0-alpha.N) follows the nightly channel,
 * across a cut too (2.3.0-alpha.N to 2.4.0-alpha.M). A release branch's
 * nightly (X.Y.Z-alpha.N with Z > 0; cut.sh branches at X.Y.0) follows its
 * series' beta. Every other build follows the newest release of its own
 * series, as the index lists it (withdrawn ones never are); with
 * @p follow_beta, its series' beta too when it is newer than both that
 * release and @p current. No build is ever offered another series' beta.
 */
auto ChooseUpdate(const IndexFacts& facts, const QString& current, bool nightly,
                  bool follow_beta = false) -> ChannelChoice;

/// Its series is no longer the one users are meant to be on: what the
/// update page says about it (SoftwareVersion::Notice, which keeps it).
using SeriesNotice = SoftwareVersion::Notice;

/// The notice for a release build of a legacy or eol series; empty (no
/// series) for a supported series, an unknown one and any nightly.
auto NoticeFor(const IndexFacts& facts, const QString& current,
               const Platform& platform) -> SeriesNotice;

/// Whether @p platform meets @p required (SeriesFacts::platforms). A
/// requirement for this system that cannot be checked here is not met.
/// @p unmet gets the requirement that is not ("macos 13.0"); empty when
/// this system is one no requirement is stated for.
auto MeetsPlatform(const QHash<QString, QString>& required,
                   const Platform& platform, QString* unmet = nullptr) -> bool;

/// kNotFound when @p current is withdrawn (the index says so; a missing
/// object never does), kConfirmed otherwise.
auto WithdrawnFact(const IndexFacts& facts, const QString& current)
    -> RemoteFact;

/// A release build: kNotFound when withdrawn, kConfirmed when the index
/// lists it, kUnknown otherwise (a release from before the index, 2.1.4 and
/// older, is not listed and is not therefore unreleased).
auto ReleaseFact(const IndexFacts& facts, const QString& current) -> RemoteFact;

/// Whether release notes may open @p url: https, and a host (or host/path
/// prefix) of @p hosts, release/product.json's notes_link_hosts. "*.x.com"
/// matches any subdomain of x.com.
auto IsAllowedNotesLink(const QUrl& url, const QStringList& hosts) -> bool;

/// `GET <builds>/<product>/commits/<sha>`: the signing host writes one for
/// every build it published. 200 confirms, 404 denies, the rest says nothing.
auto ParseCommitMarker(int http_status) -> RemoteFact;
