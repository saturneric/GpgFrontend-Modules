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

#include <QList>
#include <QString>
#include <array>
#include <cstdint>
#include <utility>

/**
 * @file
 * @brief The e-mail view's layout decisions, with no widget behind them.
 *
 * Everything here is a rule about geometry or about when work is worth doing,
 * and none of it needs a QWidget to be true. Kept apart from EMailPageView.cpp
 * for exactly that reason: this module's tests link Qt Core only, and a widget
 * built in a test would be built off the GUI thread anyway. Rules that live
 * here are checked; the same rules written inline in the view would not be.
 */

enum class EMailBadgeState : uint8_t;

/// The smallest useful details window, and what it opens at when nothing has
/// been remembered. Below the minimum its lists are narrower than their own
/// column headings.
constexpr int kEMailDetailsMinWidth = 420;
constexpr int kEMailDetailsMinHeight = 320;
constexpr int kEMailDetailsDefaultWidth = 680;
constexpr int kEMailDetailsDefaultHeight = 520;

/// Below this the action row cannot carry five labelled actions as well as the
/// view controls and the security state.
constexpr int kEMailCompactActionsWidth = 640;

/// And the labels do not come back until there is room to spare, so dragging a
/// window edge across the threshold does not flap them on and off.
constexpr int kEMailRoomyActionsWidth = 700;

/// The settings keys the details window is remembered under.
constexpr auto kEMailDetailsWidthKey = "view/details_width";
constexpr auto kEMailDetailsHeightKey = "view/details_height";
constexpr auto kEMailDetailsTabKey = "view/details_tab";

/**
 * @brief What size the details window may actually open at.
 *
 * @param width  remembered width, or <= 0 for nothing remembered
 * @param height remembered height, or <= 0 for nothing remembered
 *
 * Never smaller than its minimum: a remembered size was measured on a screen
 * that is not necessarily the screen it is being restored onto, and a window
 * too small to show a tab bar and a row cannot be recovered from by the person
 * looking at it.
 */
auto EMailClampDetailsSize(int width, int height) -> std::pair<int, int>;

/**
 * @brief Which tab the details window may actually open on.
 *
 * A remembered index outlives the tabs it indexed: a build with one tab fewer
 * would otherwise open on nothing at all.
 */
auto EMailClampDetailsTab(int persisted, int count) -> int;

/**
 * @brief Whether the message actions should be shown as icons alone.
 *
 * @param surface_width the width the message surface actually has
 *
 * The actions give way rather than the view controls: which view you are
 * looking at, and whether the message is signed, are things to read at a
 * glance, while Reply and Forward are recognisable as icons and carry their
 * wording in a tooltip either way.
 */
auto EMailShouldCompactActions(int surface_width, bool currently_compact)
    -> bool;

/**
 * @brief Whether the Security tab is worth re-deriving right now.
 *
 * Verifying reads the message and writes nothing, but it can wait on the
 * agent, so it is only done for a tab someone is actually looking at. The one
 * definition of that condition, because it is asked in two places -- when the
 * window opens or changes tab, and when the keyring changes underneath it --
 * and the two drifting apart is how a freshly imported key fails to show up.
 */
auto EMailNeedsSecurityRefresh(bool details_visible, bool security_current)
    -> bool;

/// What the attachment section shows and offers, given the message's state.
struct EMailAttachmentBarInput {
  int count = 0;          ///< attachments in the message
  int selected = 0;       ///< rows selected in the (possibly hidden) list
  bool editable = false;  ///< the message may gain and lose parts
  bool expanded = false;  ///< the user asked for the full list
};

struct EMailAttachmentBarState {
  /// The bar itself is information only: what is attached, and Expand.
  bool section_visible = false;
  bool list_visible = false;    ///< the full table, not only the summary row
  bool toggle_visible = false;  ///< Expand / Collapse is offered

  /// The Attachments menu in the message's action row. Entries that cannot
  /// apply in this mode are left out rather than greyed; entries that apply
  /// but have nothing to act on yet are there, disabled.
  bool show_open = false;
  bool show_save = false;
  bool show_save_all = false;
  bool show_attach = false;
  bool show_remove = false;
  bool can_open = false;
  bool can_save = false;
  bool can_save_all = false;
  bool can_attach = false;
  bool can_remove = false;
};

/**
 * @brief Decides the attachment bar and menu from the counts and the mode.
 *
 * No attachments: no bar at all. Otherwise a summary row that can expand
 * into the list. The operations live in the action row's Attachments menu,
 * so the bar never repeats them; a read-only message's menu has no Attach or
 * Remove in it at all.
 */
auto EMailAttachmentBar(const EMailAttachmentBarInput& in)
    -> EMailAttachmentBarState;

/// What decides the message's action row.
struct EMailActionBarInput {
  bool is_message =
      false;  ///< parsed as a message: there is something to answer
  /// Still being written: not a message at all yet, or one that was never
  /// sent (it has no Message-ID; one is only minted when sending).
  bool draft = false;
  bool editable = false;           ///< not locked: may be changed in place
  bool read_only_offered = false;  ///< the Read-only lock would change anything
  bool read_only = false;          ///< the Read-only lock is on
  int attachments = 0;
};

struct EMailActionBarState {
  /// Directly on the row: what is done with this kind of document most.
  bool reply_visible = false;   ///< Reply, Reply All, Forward
  bool send_visible = false;    ///< Send, for a draft
  bool attach_visible = false;  ///< Attach File, for a draft
  bool attachments_menu_visible = false;

  /// In More: reachable, never in the way.
  bool more_details = false;
  bool more_reply = false;      ///< Reply, Reply All, Forward, for a draft
  bool more_send = false;       ///< Send, for a received message
  bool more_read_only = false;  ///< the Read-only lock

  /// A status, not a control: shown when the lock is on.
  bool read_only_label = false;
};

/**
 * @brief Decides which message actions are on the row and which are in More.
 *
 * A received message is read and answered: Reply, Reply All and Forward. A
 * draft is written and sent: Send and Attach File. Neither mode shows the
 * other's actions greyed out; what still applies (answering a draft that is
 * itself a message, sending a received one) is in More, with Details and the
 * Read-only lock where it would change something.
 */
auto EMailActionBar(const EMailActionBarInput& in) -> EMailActionBarState;

/// The security status as the action row states it.
struct EMailStatusLabel {
  QString text;
  QString icon;
};

/**
 * @brief The wording and icon for a message's security badge.
 *
 * @param encrypted the message was encrypted (so a signature is inside it)
 *
 * Says only what is known: a signature nobody has checked is "not checked",
 * never "signed" in the colour of a verified one.
 */
auto EMailSecurityStatus(EMailBadgeState badge, bool encrypted)
    -> EMailStatusLabel;

/**
 * @brief The rows a Save, Open or Remove acts on.
 *
 * A single attachment is the target whether or not the list is open. With
 * several, only a selection the user can see counts: a selection left behind
 * in a collapsed list is kept for when it opens again, but acting on rows
 * that are not on screen would surprise.
 */
auto EMailAttachmentTargets(int count, const QList<int>& selected_rows,
                            bool list_visible) -> QList<int>;

/// The lower parts of the message surface, top to bottom.
enum class EMailSurfacePart : uint8_t {
  kBODY,         ///< the message body, which takes the spare height
  kATTACHMENTS,  ///< the attachment section
  kNOTICE,       ///< the message's information banner (lock, HTML source)
};

/**
 * @brief The order the view stacks its lower parts in.
 *
 * The notice sits under the body and the attachments rather than between the
 * actions and the body: it is read once, and should not push every line of
 * the message down while it is being read or written. A notice that blocks
 * editing still has its action in the body itself (the locked panel's
 * Decrypt), so moving the sentence down does not move the way out.
 */
constexpr std::array<EMailSurfacePart, 3> kEMailSurfaceOrder = {
    EMailSurfacePart::kBODY, EMailSurfacePart::kATTACHMENTS,
    EMailSurfacePart::kNOTICE};
