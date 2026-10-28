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

#include "NotePersistence.h"

using namespace gf::notes;
using namespace Qt::StringLiterals;

namespace {

/// Runs nothing: records each started job, finished by the test, in any order.
struct Harness {
  QList<Job> started;
  QList<std::pair<Job, bool>> done;
  NotePersistence p{
      [this](const Job& j) { started.append(j); },
      [this](const Job& j, bool ok, const QString&) { done.append({j, ok}); }};

  auto Snap(quint64 gen) -> Snapshot {
    return {gen, p.ProtectionRev(), nullptr};
  }
  auto Last() -> const Job& { return started.last(); }
  void Finish(bool ok = true) { p.Finished(started.last().id, ok); }
  /// Encrypt at @p gen, then save that revision to @p to.
  void Commit(quint64 gen, const QString& to) {
    p.Revise(Snap(gen), u"rev-%1"_s.arg(gen));
    p.Save(to);
  }
};

}  // namespace

TEST(NotePersistenceTest, EditsDuringASaveStayDirty) {
  Harness h;
  h.p.Edited(1, 0);
  h.Commit(1, "a.textpack.gpg");
  ASSERT_EQ(h.Last().kind, JobKind::kREVISION);
  h.Finish();
  ASSERT_EQ(h.Last().kind, JobKind::kSAVE);
  h.p.Edited(2, 100);  // while the save runs
  h.Finish();
  EXPECT_FALSE(h.p.IsClean()) << "2 is newer than what was saved";
  EXPECT_EQ(h.p.SavedGen(), 1U);
}

TEST(NotePersistenceTest, JobsRunInTheOrderAsked) {
  Harness h;
  h.p.Edited(1, 0);
  h.Commit(1, "a");
  h.p.SavePlain(h.Snap(1), "b.textpack");
  EXPECT_EQ(h.Last().kind, JobKind::kREVISION);
  h.Finish();
  EXPECT_EQ(h.Last().kind, JobKind::kSAVE);
  h.Finish();
  EXPECT_EQ(h.Last().kind, JobKind::kSAVE_PLAIN);
}

TEST(NotePersistenceTest, AFailedSaveChangesNothing) {
  Harness h;
  h.p.Opened(5, 1);
  h.p.Edited(6, 0);
  h.Commit(6, "a");
  h.Finish();       // revision
  h.Finish(false);  // the move fails
  EXPECT_FALSE(h.p.IsClean());
  EXPECT_EQ(h.p.SavedGen(), 5U);
  EXPECT_TRUE(h.p.Revision().exists) << "still there to try again";
}

TEST(NotePersistenceTest, AProtectionChangeLeavesTheTabDirty) {
  Harness h;
  h.p.Opened(1, 1);
  h.p.Edited(2, 0);
  h.Commit(2, "a");  // revision under rev 1
  h.p.ProtectionChanged(2);
  h.Finish();
  h.Finish();
  EXPECT_FALSE(h.p.IsClean()) << "saved under rev 1, now rev 2";
}

TEST(NotePersistenceTest, CompletionOrderCannotMarkNewerEditsClean) {
  Harness h;
  h.p.Edited(2, 1);
  h.Commit(2, "a");
  h.p.Edited(3, 2);
  h.Finish();  // revision 2
  h.Finish();  // save 2
  EXPECT_FALSE(h.p.IsClean());
  EXPECT_EQ(h.p.SavedGen(), 2U);
}

TEST(NotePersistenceTest, TheRecoveryRecordFollowsEditsButNotEveryKeystroke) {
  Harness h;
  EXPECT_FALSE(h.p.CaptureDue(0));
  h.p.Edited(1, 0);
  EXPECT_FALSE(h.p.CaptureDue(1999));
  EXPECT_TRUE(h.p.CaptureDue(2000)) << "2 s after the last edit";
  h.p.Captured(1);
  EXPECT_FALSE(h.p.CaptureDue(5000));

  Harness busy;
  for (int i = 1; i <= 12; ++i) {
    busy.p.Edited(i, (i - 1) * 1000);  // typing every second
    if ((i - 1) * 1000 < 10000) {
      EXPECT_FALSE(busy.p.CaptureDue((i - 1) * 1000)) << i;
    }
  }
  EXPECT_TRUE(busy.p.CaptureDue(10000)) << "10 s since the first edit";
}

TEST(NotePersistenceTest, SavingMovesTheLatestRevisionAndUsesItUp) {
  Harness h;
  h.p.Edited(1, 0);
  h.p.Revise(h.Snap(1), "r1");
  h.Finish();
  EXPECT_TRUE(h.p.RevisionCurrent());
  EXPECT_FALSE(h.p.IsClean()) << "a revision is not on disk yet";

  h.p.Edited(2, 10);
  EXPECT_FALSE(h.p.RevisionCurrent()) << "edited after the Encrypt";
  h.p.Revise(h.Snap(2), "r2");
  h.Finish();
  h.p.Save("dest");
  ASSERT_EQ(h.Last().kind, JobKind::kSAVE);
  EXPECT_EQ(h.Last().source, "r2") << "the latest one";
  EXPECT_EQ(h.Last().snapshot.edit_gen, 2U);
  h.Finish();
  EXPECT_TRUE(h.p.IsClean());
  EXPECT_FALSE(h.p.Revision().exists) << "moved, so gone";
}

TEST(NotePersistenceTest, SavingWithoutARevisionFailsAndChangesNothing) {
  Harness h;
  h.p.Edited(1, 0);
  h.p.Save("dest");
  EXPECT_TRUE(h.started.isEmpty()) << "nothing to move: nothing runs";
  ASSERT_EQ(h.done.size(), 1);
  EXPECT_FALSE(h.done.last().second);
  EXPECT_FALSE(h.p.IsClean());
}

TEST(NotePersistenceTest, AProtectionChangeMakesTheRevisionStale) {
  Harness h;
  h.p.Opened(1, 1);
  h.p.Revise(h.Snap(1), "r");
  h.Finish();
  EXPECT_TRUE(h.p.RevisionCurrent());
  h.p.ProtectionChanged(2);
  EXPECT_FALSE(h.p.RevisionCurrent());
}

TEST(NotePersistenceTest, ANoteNeverEncryptedIsSavedAsItIs) {
  Harness h;
  h.p.Edited(1, 0);
  h.p.SavePlain(h.Snap(1), "n.textpack");
  ASSERT_EQ(h.Last().kind, JobKind::kSAVE_PLAIN);
  EXPECT_EQ(h.Last().snapshot.edit_gen, 1U);
  h.p.Edited(2, 10);
  h.Finish();
  EXPECT_FALSE(h.p.IsClean()) << "edited while saving";
  EXPECT_EQ(h.p.SavedGen(), 1U);
  EXPECT_FALSE(h.p.Revision().exists);
}
