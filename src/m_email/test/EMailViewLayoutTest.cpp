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

#include <gtest/gtest.h>

#include "EMailModel.h"
#include "EMailViewLayout.h"

// The details inspector's rules, checked without a widget. The view itself
// cannot be built in a test here -- these bodies run off the GUI thread -- so
// the decisions it makes are kept in a file that can be.

namespace {

TEST(EMailViewLayoutTest, NothingRememberedOpensAtTheDefaultSize) {
  const auto size = EMailClampDetailsSize(0, 0);
  EXPECT_EQ(size.first, kEMailDetailsDefaultWidth);
  EXPECT_EQ(size.second, kEMailDetailsDefaultHeight);
}

TEST(EMailViewLayoutTest, AnUnusableRememberedSizeIsBroughtBack) {
  // A size measured on a screen that is not the one it is being restored onto.
  // Too small to show a tab bar and a row, the window cannot be recovered from
  // by the person looking at it.
  const auto size = EMailClampDetailsSize(40, 10);
  EXPECT_EQ(size.first, kEMailDetailsMinWidth);
  EXPECT_EQ(size.second, kEMailDetailsMinHeight);
}

TEST(EMailViewLayoutTest, ARememberedSizeThatFitsIsKeptExactly) {
  const auto size = EMailClampDetailsSize(900, 700);
  EXPECT_EQ(size.first, 900);
  EXPECT_EQ(size.second, 700);
}

TEST(EMailViewLayoutTest, ASizeIsLargeEnoughInOneDimensionAtATime) {
  // Width fine, height unusable: only the height gives way.
  const auto size = EMailClampDetailsSize(900, 10);
  EXPECT_EQ(size.first, 900);
  EXPECT_EQ(size.second, kEMailDetailsMinHeight);
}

TEST(EMailViewLayoutTest, ARememberedTabOutlivingItsTabsLandsOnARealOne) {
  EXPECT_EQ(EMailClampDetailsTab(2, 3), 2);
  EXPECT_EQ(EMailClampDetailsTab(7, 3), 2);
  EXPECT_EQ(EMailClampDetailsTab(-1, 3), 0);

  // No tabs at all: still an index something can be told to select.
  EXPECT_EQ(EMailClampDetailsTab(2, 0), 0);
}

TEST(EMailViewLayoutTest, CrowdedActionRowsDropTheirWording) {
  EXPECT_TRUE(EMailShouldCompactActions(500, false));
  EXPECT_FALSE(EMailShouldCompactActions(1000, false));
  EXPECT_FALSE(EMailShouldCompactActions(1000, true));
}

TEST(EMailViewLayoutTest, TheActionDensityThresholdDoesNotFlap) {
  // The window edge is dragged across this range a pixel at a time, and the
  // labels must not blink on and off as it goes.
  for (int width = kEMailCompactActionsWidth; width < kEMailRoomyActionsWidth;
       ++width) {
    EXPECT_FALSE(EMailShouldCompactActions(width, false)) << width;
    EXPECT_TRUE(EMailShouldCompactActions(width, true)) << width;
  }
}

TEST(EMailViewLayoutTest, SecurityIsOnlyReDerivedWhereItCanBeSeen) {
  EXPECT_TRUE(EMailNeedsSecurityRefresh(true, true));

  // The window is shut, or another tab is in front: in both cases the next
  // visit picks the work up, which is where it is normally triggered anyway.
  EXPECT_FALSE(EMailNeedsSecurityRefresh(false, true));
  EXPECT_FALSE(EMailNeedsSecurityRefresh(true, false));
  EXPECT_FALSE(EMailNeedsSecurityRefresh(false, false));
}

// ------------------------------------------------------ attachment section

auto Bar(int count, int selected, bool editable, bool expanded) {
  return EMailAttachmentBar({count, selected, editable, expanded});
}

TEST(EMailViewLayoutTest, NoAttachmentsTakeNoRoomInEitherMode) {
  // Composing, Attach is on the action row; the bar has nothing to say.
  for (const bool editable : {false, true}) {
    const auto s = Bar(0, 0, editable, true);
    EXPECT_FALSE(s.section_visible);
    EXPECT_FALSE(s.list_visible);
    EXPECT_FALSE(s.toggle_visible);
  }
}

TEST(EMailViewLayoutTest, OneAttachmentIsACompleteCompactRow) {
  const auto s = Bar(1, 0, false, false);
  EXPECT_TRUE(s.section_visible);
  EXPECT_FALSE(s.list_visible) << "one row, not a table";
  EXPECT_TRUE(s.toggle_visible);
  // The one part is the target without a selection.
  EXPECT_TRUE(s.can_open);
  EXPECT_TRUE(s.can_save);
}

TEST(EMailViewLayoutTest, SeveralAttachmentsExpandIntoTheList) {
  EXPECT_FALSE(Bar(3, 0, false, false).list_visible);
  EXPECT_TRUE(Bar(3, 0, false, false).toggle_visible);
  EXPECT_TRUE(Bar(3, 0, false, true).list_visible);
}

TEST(EMailViewLayoutTest, ReadOnlyMenusLeaveOutEditActions) {
  for (const auto& s : {Bar(1, 1, false, true), Bar(4, 2, false, true),
                        Bar(4, 0, false, false)}) {
    EXPECT_FALSE(s.show_attach) << "left out, not greyed";
    EXPECT_FALSE(s.show_remove);
    EXPECT_TRUE(s.show_save_all);
    EXPECT_TRUE(s.can_save_all) << "saving out is what a received part is for";
    EXPECT_TRUE(s.show_open);
    EXPECT_TRUE(s.show_save);
  }
}

TEST(EMailViewLayoutTest, ComposingMenusOfferAttachAndRemove) {
  const auto s = Bar(2, 0, true, false);
  EXPECT_TRUE(s.show_attach);
  EXPECT_TRUE(s.can_attach);
  EXPECT_TRUE(s.show_remove);
  EXPECT_FALSE(s.can_remove) << "a selection in a collapsed list is hidden";
  EXPECT_TRUE(Bar(2, 1, true, true).can_remove);
  EXPECT_TRUE(Bar(1, 0, true, false).can_remove)
      << "the one part is the target without a selection";
  EXPECT_FALSE(Bar(0, 0, true, false).show_remove) << "nothing to remove";
  EXPECT_TRUE(Bar(0, 0, true, false).show_attach);
}

TEST(EMailViewLayoutTest, MenuEntriesFollowTheTargets) {
  const auto none = Bar(3, 0, true, true);
  EXPECT_FALSE(none.can_open);
  EXPECT_FALSE(none.can_save);
  EXPECT_FALSE(none.can_remove);
  EXPECT_TRUE(none.can_save_all);

  const auto one = Bar(3, 1, true, true);
  EXPECT_TRUE(one.can_open);
  EXPECT_TRUE(one.can_save);
  EXPECT_TRUE(one.can_remove);

  const auto two = Bar(3, 2, true, true);
  EXPECT_FALSE(two.can_open) << "Open takes exactly one";
  EXPECT_TRUE(two.can_save);
  EXPECT_TRUE(two.can_remove);
}

// ------------------------------------------------------ action row

/// is_message, draft, editable, lock offered, lock on, attachments.
auto Actions(bool is_message, bool draft, bool editable, bool offered,
             bool read_only, int attachments) {
  return EMailActionBar(
      {is_message, draft, editable, offered, read_only, attachments});
}

TEST(EMailViewLayoutTest, AReceivedMessageIsAnsweredFromTheRow) {
  const auto s = Actions(true, false, false, false, false, 0);
  EXPECT_TRUE(s.reply_visible) << "Reply, Reply All and Forward";
  EXPECT_FALSE(s.send_visible);
  EXPECT_FALSE(s.attach_visible);
  EXPECT_FALSE(s.more_reply) << "not twice";
  EXPECT_TRUE(s.more_send) << "still reachable, in More";
  EXPECT_TRUE(s.more_details);
}

TEST(EMailViewLayoutTest, ANewDraftIsSentFromTheRow) {
  const auto s = Actions(false, true, true, false, false, 0);
  EXPECT_FALSE(s.reply_visible) << "nothing to reply to: hidden, not greyed";
  EXPECT_FALSE(s.more_reply) << "not even in More";
  EXPECT_TRUE(s.send_visible);
  EXPECT_TRUE(s.attach_visible);
  EXPECT_FALSE(s.more_send) << "not twice";
  EXPECT_FALSE(s.more_read_only);
  EXPECT_TRUE(s.more_details);
}

TEST(EMailViewLayoutTest, AReplyBeingWrittenIsADraftThatCanStillBeAnswered) {
  // Parsed as a message, but never sent: a reply or a forward in progress.
  const auto s = Actions(true, true, true, true, false, 0);
  EXPECT_TRUE(s.send_visible);
  EXPECT_TRUE(s.attach_visible);
  EXPECT_FALSE(s.reply_visible);
  EXPECT_TRUE(s.more_reply) << "reachable, out of the way";
  EXPECT_FALSE(s.more_read_only) << "there is no evidence in a draft to lock";
}

TEST(EMailViewLayoutTest, ALockedDraftHasNoAttachButStillSends) {
  // Signed or encrypted by its author, before sending.
  const auto s = Actions(true, true, false, false, false, 1);
  EXPECT_TRUE(s.send_visible);
  EXPECT_FALSE(s.attach_visible);
  EXPECT_TRUE(s.attachments_menu_visible);
}

TEST(EMailViewLayoutTest, TheAttachmentsMenuExistsOnlyWithAttachments) {
  EXPECT_FALSE(
      Actions(true, false, false, false, false, 0).attachments_menu_visible);
  EXPECT_TRUE(
      Actions(true, false, false, false, false, 2).attachments_menu_visible);
  EXPECT_TRUE(
      Actions(false, true, true, false, false, 1).attachments_menu_visible);
}

TEST(EMailViewLayoutTest, ReadOnlyIsOfferedWhereItChangesSomething) {
  EXPECT_TRUE(Actions(true, false, true, true, false, 0).more_read_only);
  EXPECT_FALSE(Actions(true, false, false, false, false, 0).more_read_only)
      << "a signed or encrypted message is protected already";
  EXPECT_TRUE(Actions(true, false, false, false, true, 0).more_read_only)
      << "while it is on, it can always be turned off";
}

TEST(EMailViewLayoutTest, ReadOnlyIsAStatusLabelWhileOn) {
  EXPECT_TRUE(Actions(true, false, false, true, true, 0).read_only_label);
  EXPECT_FALSE(Actions(true, false, true, true, false, 0).read_only_label);
}

TEST(EMailViewLayoutTest, EveryCommandIsReachableInEveryMode) {
  for (const bool is_message : {false, true}) {
    for (const bool draft : {false, true}) {
      if (!is_message && !draft) continue;  // not a message is always a draft
      for (const bool editable : {false, true}) {
        const auto s = Actions(is_message, draft, editable, true, false, 1);
        EXPECT_TRUE(s.send_visible || s.more_send) << "Send";
        EXPECT_TRUE(s.more_details) << "Details";
        if (is_message) {
          EXPECT_TRUE(s.reply_visible || s.more_reply) << "Reply";
        }
        EXPECT_FALSE(s.reply_visible && s.more_reply) << "never twice";
        EXPECT_FALSE(s.send_visible && s.more_send) << "never twice";
      }
    }
  }
}

TEST(EMailViewLayoutTest, TheSecurityStatusSaysOnlyWhatIsKnown) {
  EXPECT_EQ(EMailSecurityStatus(EMailBadgeState::kENCRYPTED_ONLY, true).text,
            QStringLiteral("Encrypted"));
  EXPECT_EQ(
      EMailSecurityStatus(EMailBadgeState::kSIGNED_UNVERIFIED, false).text,
      QStringLiteral("Signature not checked"));
  EXPECT_EQ(EMailSecurityStatus(EMailBadgeState::kSIGNED_UNVERIFIED, true).text,
            QStringLiteral("Encrypted, signature not checked"));
  EXPECT_EQ(EMailSecurityStatus(EMailBadgeState::kSIGNED_GOOD, false).text,
            QStringLiteral("Signature verified"));
  EXPECT_EQ(EMailSecurityStatus(EMailBadgeState::kSIGNED_BAD, false).text,
            QStringLiteral("Bad signature"));
  EXPECT_EQ(EMailSecurityStatus(EMailBadgeState::kNOT_PROTECTED, false).text,
            QStringLiteral("Not signed or encrypted"));
  // A verdict against the signature never wears the signature's icon.
  for (const auto badge :
       {EMailBadgeState::kSIGNED_BAD, EMailBadgeState::kSIGNED_ERROR,
        EMailBadgeState::kSIGNED_MISMATCH, EMailBadgeState::kMALFORMED}) {
    EXPECT_EQ(EMailSecurityStatus(badge, false).icon,
              QStringLiteral(":/icons/warning.png"));
  }
}

TEST(EMailViewLayoutTest, TargetsAreWhatTheUserCanSee) {
  EXPECT_EQ(EMailAttachmentTargets(1, {}, false), QList<int>{0});
  EXPECT_EQ(EMailAttachmentTargets(1, {}, true), QList<int>{0});
  EXPECT_TRUE(EMailAttachmentTargets(3, {0, 2}, false).isEmpty())
      << "a hidden selection is kept, not acted on";
  EXPECT_EQ(EMailAttachmentTargets(3, {2, 0}, true), (QList<int>{2, 0}));
  EXPECT_EQ(EMailAttachmentTargets(3, {1, 1, 7, -1}, true), QList<int>{1})
      << "duplicates and stale rows are dropped";
  EXPECT_TRUE(EMailAttachmentTargets(0, {0}, true).isEmpty());
}

TEST(EMailViewLayoutTest, CollapsingChangesOnlyWhatIsShown) {
  // The same counts and selection, collapsed and expanded: nothing about the
  // attachments themselves is an input, so nothing about them can change.
  const auto open = Bar(3, 2, true, true);
  const auto shut = Bar(3, 2, true, false);
  EXPECT_TRUE(open.list_visible);
  EXPECT_FALSE(shut.list_visible);
  EXPECT_EQ(open.section_visible, shut.section_visible);
  EXPECT_EQ(open.can_save_all, shut.can_save_all);
  EXPECT_EQ(EMailAttachmentTargets(3, {0, 1}, true).size(), 2)
      << "reopening brings the kept selection back as the target";
}

// ------------------------------------------------------ surface order

TEST(EMailViewLayoutTest, TheNoticeSitsBelowTheBodyAndAttachments) {
  const auto at = [](EMailSurfacePart part) {
    for (size_t i = 0; i < kEMailSurfaceOrder.size(); ++i) {
      if (kEMailSurfaceOrder[i] == part) return static_cast<int>(i);
    }
    return -1;
  };
  ASSERT_NE(at(EMailSurfacePart::kNOTICE), -1);
  EXPECT_LT(at(EMailSurfacePart::kBODY), at(EMailSurfacePart::kATTACHMENTS));
  EXPECT_LT(at(EMailSurfacePart::kATTACHMENTS), at(EMailSurfacePart::kNOTICE));
  EXPECT_EQ(at(EMailSurfacePart::kNOTICE),
            static_cast<int>(kEMailSurfaceOrder.size()) - 1)
      << "last in the view, directly above the Status Panel";
}

}  // namespace
