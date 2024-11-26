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

#include "EMailHelper.h"

#include <QRegularExpression>

#include "GFModuleCommonUtils.hpp"

auto IsValidMicalgFormat(const QString& prm_micalg_value) -> bool {
  QRegularExpression regex("^pgp-(\\w+)$");
  QRegularExpressionMatch match = regex.match(prm_micalg_value);
  return match.hasMatch();
}

auto FormatMailBox(const std::shared_ptr<vmime::mailbox>& m) -> QString {
  if (!m) return {"Unknown"};

  QString name = Q_SC(m->getName().getConvertedText(vmime::charsets::UTF_8));
  QString address =
      Q_SC(m->getEmail().toText().getConvertedText(vmime::charsets::UTF_8));

  if (!name.isEmpty()) {
    return QString("%1 <%2>").arg(name, address);
  }
  return address;
}

auto ExtractFieldValue(const vmime::shared_ptr<vmime::header>& header,
                       const QString& field_name) -> QString {
  auto field = header->getField(field_name.toStdString());
  if (!field) {
    FLOG_WARN("cannot get '%1' Field from header", field_name);
    return {};
  }

  auto field_value = field->getValue();
  if (!field_value) {
    FLOG_WARN("cannot get '%s' Field Value from header", field_name);
    return {};
  }

  return Q_SC(field_value->generate());
}

auto ExtractFieldValueMailBox(const vmime::shared_ptr<vmime::header>& header,
                              const QString& field_name) -> QString {
  auto field = header->getField(field_name.toStdString());
  if (!field) {
    FLOG_WARN("cannot get '%1' Field from header", field_name);
    return {};
  }

  auto field_value = field->getValue<vmime::mailbox>();
  if (!field_value) {
    FLOG_WARN("cannot get '%s' Field Value from header", field_name);
    return {};
  }

  return FormatMailBox(field_value);
}

auto ExtractFieldValueAddressList(
    const vmime::shared_ptr<vmime::header>& header,
    const QString& field_name) -> QString {
  auto field = header->getField(field_name.toStdString());
  if (!field) {
    FLOG_WARN("cannot get '%1' Field from header", field_name);
    return {};
  }

  auto field_value = field->getValue<vmime::addressList>();
  if (!field_value) {
    FLOG_WARN("cannot get '%s' Field Value from header", field_name);
    return {};
  }

  QStringList mailbox_list_strings;
  for (const auto& mailbox : field_value->toMailboxList()->getMailboxList()) {
    QString formatted_mailbox = FormatMailBox(mailbox);
    mailbox_list_strings.append(formatted_mailbox);
  }

  return mailbox_list_strings.join(", ");
}

auto ExtractFieldValueText(const vmime::shared_ptr<vmime::header>& header,
                           const QString& field_name) -> QString {
  auto field = header->getField(field_name.toStdString());
  if (!field) {
    FLOG_WARN("cannot get '%1' Field from header", field_name);
    return {};
  }

  auto field_value = field->getValue<vmime::text>();
  if (!field_value) {
    FLOG_WARN("cannot get '%s' Field Value from header", field_name);
    return {};
  }

  return Q_SC(field_value->getConvertedText(vmime::charsets::UTF_8));
}

auto ExtractFieldValueDateTime(const vmime::shared_ptr<vmime::header>& header,
                               const QString& field_name) -> QDateTime {
  auto field = header->getField(field_name.toStdString());
  if (!field) {
    FLOG_WARN("cannot get '%1' Field from header", field_name);
    return {};
  }

  auto field_value = field->getValue<vmime::datetime>();
  if (!field_value) {
    FLOG_WARN("cannot get '%s' Field Value from header", field_name);
    return {};
  }

  QDate date(field_value->getYear(), field_value->getMonth(),
             field_value->getDay());
  QTime time(field_value->getHour(), field_value->getMinute(),
             field_value->getSecond());
  field_value->getZone();

  auto zone = field_value->getZone();
  QDateTime datetime(date, time);
  datetime.setOffsetFromUtc(zone * 60);

  return datetime;
}
