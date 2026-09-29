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

#include "EMailViewLayout.h"

#include <QCoreApplication>
#include <algorithm>

#include "EMailModel.h"

auto EMailClampDetailsSize(int width, int height) -> std::pair<int, int> {
  const auto wanted_width = width > 0 ? width : kEMailDetailsDefaultWidth;
  const auto wanted_height = height > 0 ? height : kEMailDetailsDefaultHeight;

  // Only a lower bound. There is no sensible upper one here: the window may
  // legitimately be as large as whatever screen it is opening on, and this
  // function does not know what that is.
  return {std::max(wanted_width, kEMailDetailsMinWidth),
          std::max(wanted_height, kEMailDetailsMinHeight)};
}

auto EMailClampDetailsTab(int persisted, int count) -> int {
  if (count <= 0) return 0;
  return std::clamp(persisted, 0, count - 1);
}

auto EMailShouldCompactActions(int surface_width, bool currently_compact)
    -> bool {
  // Hysteresis: without it the labels flicker on and off while a window edge
  // is being dragged across the threshold.
  return currently_compact ? surface_width < kEMailRoomyActionsWidth
                           : surface_width < kEMailCompactActionsWidth;
}

auto EMailNeedsSecurityRefresh(bool details_visible, bool security_current)
    -> bool {
  return details_visible && security_current;
}

auto EMailAttachmentBar(const EMailAttachmentBarInput& in)
    -> EMailAttachmentBarState {
  EMailAttachmentBarState s;
  const bool any = in.count > 0;

  s.section_visible = any;
  s.toggle_visible = any;
  s.list_visible = any && in.expanded;

  const auto targets = in.count == 1 ? 1 : (s.list_visible ? in.selected : 0);

  s.show_open = any;
  s.show_save = any;
  s.show_save_all = any;
  // Changing the parts is a mode, not a selection: a read-only message has
  // no Attach or Remove to offer, not even greyed out.
  s.show_attach = in.editable;
  s.show_remove = in.editable && any;

  s.can_open = targets == 1;
  s.can_save = targets > 0;
  s.can_save_all = any;
  s.can_attach = in.editable;
  s.can_remove = in.editable && targets > 0;
  return s;
}

auto EMailActionBar(const EMailActionBarInput& in) -> EMailActionBarState {
  EMailActionBarState s;
  const bool received = in.is_message && !in.draft;
  s.reply_visible = received;
  s.send_visible = !received;
  s.attach_visible = !received && in.editable;
  s.attachments_menu_visible = in.attachments > 0;

  s.more_details = true;
  s.more_reply = in.is_message && in.draft;
  s.more_send = received;
  // Offered only where it changes something, and always while it is on so it
  // can be turned off again.
  s.more_read_only = received && (in.read_only_offered || in.read_only);
  s.read_only_label = in.read_only;
  return s;
}

auto EMailSecurityStatus(EMailBadgeState badge, bool encrypted)
    -> EMailStatusLabel {
  // Translated under the view's own context, spelled out at every call so
  // lupdate sees each one: these are the view's strings, moved here only so
  // they can be checked.
  const auto warning = QStringLiteral(":/icons/warning.png");
  const auto signature = QStringLiteral(":/icons/signature.png");

  switch (badge) {
    case EMailBadgeState::kENCRYPTED_ONLY:
      return {QCoreApplication::translate("EMailPageView", "Encrypted"),
              QStringLiteral(":/icons/lock.png")};
    case EMailBadgeState::kSIGNED_UNVERIFIED:
      // What is actually known at this point: the message CARRIES a
      // signature. Whether it is any good is a separate question that has
      // not been asked yet, and the wording must not answer it.
      return {encrypted
                  ? QCoreApplication::translate(
                        "EMailPageView", "Encrypted, signature not checked")
                  : QCoreApplication::translate("EMailPageView",
                                                "Signature not checked"),
              signature};
    case EMailBadgeState::kSIGNED_GOOD:
      return {encrypted ? QCoreApplication::translate(
                              "EMailPageView", "Encrypted, signature verified")
                        : QCoreApplication::translate("EMailPageView",
                                                      "Signature verified"),
              signature};
    case EMailBadgeState::kSIGNED_MISMATCH:
      return {QCoreApplication::translate("EMailPageView",
                                          "Signed by a different address"),
              warning};
    case EMailBadgeState::kSIGNED_EXPIRED:
      return {QCoreApplication::translate("EMailPageView",
                                          "Signature or key expired"),
              warning};
    case EMailBadgeState::kSIGNED_UNKNOWN_KEY:
      return {QCoreApplication::translate("EMailPageView",
                                          "Signed by an unknown key"),
              warning};
    case EMailBadgeState::kSIGNED_BAD:
      return {QCoreApplication::translate("EMailPageView", "Bad signature"),
              warning};
    case EMailBadgeState::kSIGNED_ERROR:
      // Not the same claim as a bad signature, and not the same as one nobody
      // has looked at. The check was made and could not produce an answer,
      // which is the one outcome trying again might change.
      return {QCoreApplication::translate("EMailPageView",
                                          "Signature could not be checked"),
              warning};
    case EMailBadgeState::kMALFORMED:
      return {QCoreApplication::translate("EMailPageView",
                                          "Malformed OpenPGP structure"),
              warning};
    case EMailBadgeState::kNOT_PROTECTED:
      // Stated plainly and quietly. An unprotected message is the ordinary
      // case, not a fault, and painting it as a warning would train the user
      // to ignore the one that matters.
      break;
  }
  return {
      QCoreApplication::translate("EMailPageView", "Not signed or encrypted"),
      QStringLiteral(":/icons/email.png")};
}

auto EMailAttachmentTargets(int count, const QList<int>& selected_rows,
                            bool list_visible) -> QList<int> {
  if (count == 1) return {0};
  if (!list_visible) return {};
  QList<int> rows;
  for (const auto row : selected_rows) {
    if (row >= 0 && row < count && !rows.contains(row)) rows.append(row);
  }
  return rows;
}
