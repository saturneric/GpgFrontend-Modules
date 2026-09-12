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

#include "EMailAccountModel.h"

#include <QHostAddress>
#include <QJsonArray>

namespace {

constexpr auto kTlsImplicit = "implicit";
constexpr auto kTlsStartTls = "starttls";
constexpr auto kTlsNone = "none";

}  // namespace

auto MailTlsModeToString(MailTlsMode mode) -> QString {
  switch (mode) {
    case MailTlsMode::kIMPLICIT:
      return kTlsImplicit;
    case MailTlsMode::kSTARTTLS:
      return kTlsStartTls;
    case MailTlsMode::kNONE:
      return kTlsNone;
  }
  return kTlsImplicit;
}

auto MailTlsModeFromString(const QString& s) -> MailTlsMode {
  const auto v = s.trimmed().toLower();
  if (v == kTlsStartTls) return MailTlsMode::kSTARTTLS;
  if (v == kTlsNone) return MailTlsMode::kNONE;

  // Anything unrecognised reads as the safest mode rather than the nearest
  // one. A typo, a truncated file or a value from a newer build must not be
  // able to quietly downgrade a connection.
  return MailTlsMode::kIMPLICIT;
}

auto MailHostAllowsCleartext(const QString& host) -> bool {
  const auto h = host.trimmed().toLower();
  if (h.isEmpty()) return false;
  if (h == "localhost" || h == "localhost.localdomain") return true;

  // Judged as an address rather than by string prefix: "127.0.0.1.example.com"
  // is a remote host that merely looks local, and matching on text would hand
  // it a cleartext session.
  QHostAddress address(h);
  if (address.isNull()) return false;
  return address.isLoopback();
}

auto MailDefaultPort(bool imap, MailTlsMode mode) -> quint16 {
  switch (mode) {
    case MailTlsMode::kIMPLICIT:
      return imap ? 993 : 465;
    case MailTlsMode::kSTARTTLS:
      return imap ? 143 : 587;
    case MailTlsMode::kNONE:
      return imap ? 143 : 25;
  }
  return 0;
}

auto MailAllowedPageSizes() -> QList<int> { return {25, 50, 100, 200}; }

auto MailClampPageSize(int requested) -> int {
  const auto allowed = MailAllowedPageSizes();
  if (allowed.contains(requested)) return requested;

  // Snap down rather than to the nearest: a value between two steps is more
  // likely a guess than a demand, and the cheaper page is the safer answer.
  int best = allowed.first();
  for (const auto size : allowed) {
    if (size <= requested) best = size;
  }
  return best;
}

auto MailTransportConfig::ToJson() const -> QJsonObject {
  return QJsonObject{
      {"enabled", enabled},
      {"host", host},
      {"port", static_cast<int>(port)},
      {"tls", MailTlsModeToString(tls)},
      {"username", username},
      {"pinned_cert_sha256", pinned_cert_sha256},
  };
}

auto MailTransportConfig::FromJson(const QJsonObject& json)
    -> MailTransportConfig {
  MailTransportConfig config;
  config.enabled = json.value("enabled").toBool();
  config.host = json.value("host").toString().trimmed();
  config.tls = MailTlsModeFromString(json.value("tls").toString());
  config.username = json.value("username").toString();
  config.pinned_cert_sha256 =
      json.value("pinned_cert_sha256").toString().toLower();

  const auto port = json.value("port").toInt();
  config.port = port > 0 && port <= 65535 ? static_cast<quint16>(port) : 0;

  // A stored cleartext mode is re-checked against the host rather than
  // trusted. The file may predate the policy, or have been edited by hand;
  // either way a remote host must not come back cleartext.
  if (config.tls == MailTlsMode::kNONE &&
      !MailHostAllowsCleartext(config.host)) {
    config.tls = MailTlsMode::kIMPLICIT;
  }

  return config;
}

auto MailAccountConfig::Label() const -> QString {
  if (!display_name.trimmed().isEmpty() && !address.trimmed().isEmpty()) {
    return QString("%1 <%2>").arg(display_name.trimmed(), address.trimmed());
  }
  if (!address.trimmed().isEmpty()) return address.trimmed();
  if (!display_name.trimmed().isEmpty()) return display_name.trimmed();
  return {};
}

auto MailAccountConfig::IsUsable() const -> bool {
  if (address.trimmed().isEmpty()) return false;
  if (!imap.enabled && !smtp.enabled) return false;
  if (imap.enabled && imap.host.isEmpty()) return false;
  if (smtp.enabled && smtp.host.isEmpty()) return false;
  return true;
}

auto MailAccountConfig::ToJson() const -> QJsonObject {
  // No password and no secret of any kind: the credential lives in the
  // credential store under `id`, and this object is what gets written to
  // ordinary settings, logged, and exported.
  return QJsonObject{
      {"id", id},
      {"display_name", display_name},
      {"address", address},
      {"reply_to", reply_to},
      {"default_key_fpr", default_key_fpr},
      {"imap", imap.ToJson()},
      {"smtp", smtp.ToJson()},
      {"sent_folder_override", sent_folder_override},
      {"page_size", page_size},
  };
}

auto MailAccountConfig::FromJson(const QJsonObject& json) -> MailAccountConfig {
  MailAccountConfig config;
  config.id = json.value("id").toString();
  config.display_name = json.value("display_name").toString();
  config.address = json.value("address").toString().trimmed();
  config.reply_to = json.value("reply_to").toString().trimmed();
  config.default_key_fpr = json.value("default_key_fpr").toString();
  config.imap = MailTransportConfig::FromJson(json.value("imap").toObject());
  config.smtp = MailTransportConfig::FromJson(json.value("smtp").toObject());
  config.sent_folder_override =
      json.value("sent_folder_override").toString().trimmed();
  config.page_size =
      MailClampPageSize(json.value("page_size").toInt(kMailDefaultPageSize));
  return config;
}
