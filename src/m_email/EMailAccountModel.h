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

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

/**
 * @brief How a transport reaches its server.
 *
 * Deliberately three values and not a free-form "port + a checkbox". The
 * security of the connection is the thing being chosen; the port follows from
 * it. A port number never implies a security level -- 143 means STARTTLS on
 * 143, not cleartext on 143.
 */
enum class MailTlsMode : uint8_t {
  kIMPLICIT = 0,  ///< TLS from the first byte (993 / 465)
  kSTARTTLS,      ///< negotiate on the cleartext port (143 / 587)
  kNONE,          ///< cleartext; only ever legal for a loopback host
};

auto MailTlsModeToString(MailTlsMode mode) -> QString;
auto MailTlsModeFromString(const QString& s) -> MailTlsMode;

/**
 * @brief Whether a host may legally be spoken to in the clear.
 *
 * The whole of the plaintext policy, in one function, so it cannot drift
 * between the settings page and the transports. Only loopback qualifies, and
 * there is deliberately no override for anything else: an override that exists
 * is eventually used to work around a certificate problem it was never meant
 * to solve.
 *
 * @param host hostname or literal address as the user typed it
 * @return true when kNONE is a permissible choice for @p host
 */
auto MailHostAllowsCleartext(const QString& host) -> bool;

/**
 * @brief The port a transport uses when the user has not overridden it.
 *
 * @param imap true for IMAP, false for SMTP
 * @param mode the chosen security
 * @return the well-known port, or 0 when @p mode has none
 */
auto MailDefaultPort(bool imap, MailTlsMode mode) -> quint16;

/**
 * @brief One transport of an account.
 *
 * IMAP and SMTP are configured independently and either may be absent: an
 * account is a grouping for the user's convenience, not a claim that both
 * protocols exist. Nothing may require one because the other is present.
 */
struct MailTransportConfig {
  bool enabled{false};
  QString host;
  /// 0 means "whatever MailDefaultPort says", which is the normal case. A
  /// non-zero value is an Advanced override and is preserved verbatim.
  quint16 port{0};
  MailTlsMode tls{MailTlsMode::kIMPLICIT};
  QString username;

  /// Fingerprint of the one certificate the user chose to trust for this
  /// transport, lowercase hex, empty when none. A pin is not "ignore TLS
  /// errors": it excuses an untrusted or self-signed chain and nothing else,
  /// so expiry and hostname mismatch still fail with one in place.
  QString pinned_cert_sha256;

  [[nodiscard]] auto EffectivePort(bool imap) const -> quint16 {
    return port != 0 ? port : MailDefaultPort(imap, tls);
  }

  [[nodiscard]] auto ToJson() const -> QJsonObject;
  static auto FromJson(const QJsonObject& json) -> MailTransportConfig;
};

/**
 * @brief A configured mail account.
 *
 * Holds no password. @ref secret_ref names an entry in the credential store;
 * the secret itself never appears in this struct, is never serialized with it,
 * and so cannot leak through a settings export or a log line that prints one
 * of these.
 */
struct MailAccountConfig {
  /// Stable identity of the account, minted once. Also the credential key, so
  /// it must not change when the user renames or re-hosts the account.
  QString id;

  QString display_name;
  QString address;
  QString reply_to;
  /// Fingerprint of the key to offer by default when signing from this
  /// identity. Advisory: the user can always choose another.
  QString default_key_fpr;

  MailTransportConfig imap;
  MailTransportConfig smtp;

  /// Folder to look in for a Sent copy, when the server does not advertise one
  /// via RFC 6154 SPECIAL-USE. Empty means "discover it", which is the normal
  /// case; this exists because discovery genuinely fails on some servers.
  QString sent_folder_override;

  /// Rows per page in the message picker. Clamped to the allowed set on read,
  /// so a hand-edited settings file cannot ask for an unbounded page.
  int page_size{50};

  [[nodiscard]] auto Label() const -> QString;

  /// Whether this account is coherent enough to be used at all.
  [[nodiscard]] auto IsUsable() const -> bool;

  [[nodiscard]] auto ToJson() const -> QJsonObject;
  static auto FromJson(const QJsonObject& json) -> MailAccountConfig;
};

/// The page sizes the picker offers. Small, fixed, and the only listing knob.
auto MailAllowedPageSizes() -> QList<int>;

/// Nearest allowed page size to @p requested.
auto MailClampPageSize(int requested) -> int;

/**
 * @brief Most rows one picker session will ever retain.
 *
 * A ceiling on the session, not on the mailbox: on reaching it the picker stops
 * loading and asks the user to narrow with a search, rather than growing until
 * something gives.
 */
constexpr int kMailMaxRetainedRows = 1000;

/// Rows loaded before the user asks for more.
constexpr int kMailDefaultPageSize = 50;

/**
 * @brief Largest message this application will load, encoded.
 *
 * One number, applied identically to opening a file and to fetching over IMAP,
 * so the two can never disagree about what is too big. It is a limit on the
 * *encoded* message: base64 inflates an attachment by about 4/3, so this admits
 * roughly three quarters of its value in actual attachment bytes.
 */
constexpr qint64 kMailMaxMessageSize = 64LL * 1024 * 1024;
