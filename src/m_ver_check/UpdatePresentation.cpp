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

#include "UpdatePresentation.h"

#include <QCoreApplication>
#include <QLocale>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QUrl>
#include <algorithm>

#include "ReleaseCheck.h"
#include "ReleaseIndex.h"
#include "SemVer.h"

// Every string here uses the literal context "UpdateTab", shared with the
// dialog's own tr(): lupdate files them together, and a named constant would
// hide them from it.

namespace {

/// A date that reads one way only.
auto Date(const QDateTime& t) -> QString {
  // ISO: "11/6/28" reads three ways.
  return t.toUTC().date().toString(Qt::ISODate);
}

/// Why @p step failed, as the end of a sentence ("... couldn't be verified:
/// <this>").
auto ReasonText(CheckStepId id, const CheckStep& step) -> QString {
  const auto when = Date(QDateTime::fromString(step.detail, Qt::ISODate));
  switch (step.reason) {
    case StepReason::kUnreachable:
      return QCoreApplication::translate("UpdateTab",
                                         "no update server answered");
    case StepReason::kBadSignature:
      return QCoreApplication::translate(
          "UpdateTab", "the update index's signature is not valid");
    case StepReason::kKeyNotListed:
      // The key's id is in its row.
      return QCoreApplication::translate(
          "UpdateTab",
          "the update index is signed by a key that is not "
          "trusted");
    case StepReason::kExpired:
      if (id == CheckStepId::kRoot) {
        return QCoreApplication::translate("UpdateTab",
                                           "the root of trust expired on %1")
            .arg(when);
      }
      if (id == CheckStepId::kIndexKeys) {
        return QCoreApplication::translate("UpdateTab",
                                           "the signing key list expired on %1")
            .arg(when);
      }
      return QCoreApplication::translate("UpdateTab",
                                         "the update index expired on %1")
          .arg(when);
    case StepReason::kRollback:
      return QCoreApplication::translate(
                 "UpdateTab",
                 "the update index is older than one seen before (serial "
                 "%1, %2 seen before)")
          .arg(step.detail.section(u' ', 1, 1),
               step.detail.section(u' ', 0, 0));
    case StepReason::kRootWalkFailed:
      return QCoreApplication::translate(
          "UpdateTab", "a newer root of trust could not be verified");
    case StepReason::kListRejected:
      return QCoreApplication::translate(
          "UpdateTab", "the signing key list could not be verified");
    case StepReason::kHashMismatch:
      return id == CheckStepId::kNotes
                 ? QCoreApplication::translate(
                       "UpdateTab",
                       "the release notes don't match the update index")
                 : QCoreApplication::translate(
                       "UpdateTab",
                       "the release details don't match the update index");
    case StepReason::kOversized:
      return QCoreApplication::translate(
          "UpdateTab", "an answer from the server was too large");
    case StepReason::kMalformed:
      return QCoreApplication::translate("UpdateTab",
                                         "the update index could not be read");
    case StepReason::kNotFound:
      return QCoreApplication::translate(
          "UpdateTab", "this build's commit is not among the official builds");
    case StepReason::kNoTrust:
      return QCoreApplication::translate(
          "UpdateTab", "this build was made without a built-in root of trust");
    case StepReason::kNone:
      break;
  }
  return {};
}

/// A fragment as a sentence of its own.
auto Sentence(const QString& fragment) -> QString {
  if (fragment.isEmpty()) return {};
  return fragment.left(1).toUpper() + fragment.mid(1) + u'.';
}

/// A failed step's reason, short, for its row: the sentence is the
/// header's (ReasonText()).
auto ReasonShort(const CheckStep& step) -> QString {
  switch (step.reason) {
    case StepReason::kUnreachable:
      return QCoreApplication::translate("UpdateTab", "No server answered");
    case StepReason::kBadSignature:
      return QCoreApplication::translate("UpdateTab", "Signature not valid");
    case StepReason::kKeyNotListed:
      return QCoreApplication::translate("UpdateTab",
                                         "Signed by a key that isn't trusted");
    case StepReason::kExpired:
      return QCoreApplication::translate("UpdateTab", "Expired on %1")
          .arg(Date(QDateTime::fromString(step.detail, Qt::ISODate)));
    case StepReason::kRollback:
      return QCoreApplication::translate(
                 "UpdateTab", "Older than one seen before (serial %1, %2 seen)")
          .arg(step.detail.section(u' ', 1, 1),
               step.detail.section(u' ', 0, 0));
    case StepReason::kRootWalkFailed:
      return QCoreApplication::translate("UpdateTab",
                                         "A newer root didn't verify");
    case StepReason::kListRejected:
      return QCoreApplication::translate("UpdateTab", "Didn't verify");
    case StepReason::kHashMismatch:
      return QCoreApplication::translate("UpdateTab",
                                         "Doesn't match the update index");
    case StepReason::kOversized:
      return QCoreApplication::translate("UpdateTab", "Answer too large");
    case StepReason::kMalformed:
      return QCoreApplication::translate("UpdateTab", "Not readable");
    case StepReason::kNotFound:
      return QCoreApplication::translate("UpdateTab",
                                         "Not among the official builds");
    case StepReason::kNoTrust:
      return QCoreApplication::translate("UpdateTab",
                                         "Not included in this build");
    case StepReason::kNone:
      break;
  }
  return {};
}

/// The row of a step: "Verified" and @p facts when it verified, its reason
/// when it failed; nothing for a step that does not apply.
auto StepRow(const CheckStep& step, const QString& title, const QString& facts,
             const QString& tip = {}) -> std::optional<DetailRow> {
  switch (step.status) {
    case StepStatus::kVerified:
      return DetailRow{DetailRow::Mark::kVerified, title,
                       QCoreApplication::translate("UpdateTab", "Verified"),
                       facts, tip};
    case StepStatus::kFailed:
      // An HTTP status says something to whoever reads the report; a key
      // id is in the tooltip.
      return DetailRow{
          DetailRow::Mark::kFailed, title, ReasonShort(step),
          step.detail.startsWith(u"HTTP ") ? step.detail : QString(),
          step.reason == StepReason::kKeyNotListed
              ? QCoreApplication::translate("UpdateTab", "Signed by %1")
                    .arg(step.detail)
              : QString()};
    case StepStatus::kNotRun:
      return DetailRow{DetailRow::Mark::kNotRun, title,
                       QCoreApplication::translate("UpdateTab", "Not run")};
    case StepStatus::kNotApplicable:
      break;
  }
  return std::nullopt;
}

/// Facts as the secondary line shows them.
auto Facts(const QStringList& facts) -> QString {
  return facts.join(QStringLiteral(" · "));
}

/// The rows of what this device checked, from the last check's record, with
/// @p file (the offer's download) in its place.
auto DeviceRows(const UpdateSnapshot& s, const CheckRecord& r,
                const std::optional<DetailRow>& file) -> QList<DetailRow> {
  QList<std::optional<DetailRow>> steps;

  steps << StepRow(
      r.Step(CheckStepId::kRoot),
      QCoreApplication::translate("UpdateTab", "Root of trust"),
      Facts({r.root_version == r.root_built_in || !s.root_taken_at.isValid()
                 ? QCoreApplication::translate("UpdateTab",
                                               "Version %1 (built in)")
                       .arg(r.root_version)
                 : QCoreApplication::translate(
                       "UpdateTab", "Version %1 (updated %2 from version %3)")
                       .arg(r.root_version)
                       .arg(Date(s.root_taken_at))
                       .arg(r.root_built_in),
             QCoreApplication::translate("UpdateTab", "Valid until %1")
                 .arg(Date(r.root_expires)),
             QCoreApplication::translate("UpdateTab",
                                         "Signed by %1 of %2 root keys")
                 .arg(r.root_signers.size())
                 .arg(r.root_keys)}),
      QCoreApplication::translate("UpdateTab", "Root keys: %1")
          .arg(r.root_signers.join(QStringLiteral(", "))));

  steps << StepRow(
      r.Step(CheckStepId::kIndexKeys),
      QCoreApplication::translate("UpdateTab", "Signing key list"),
      Facts({QCoreApplication::translate("UpdateTab", "Version %1")
                 .arg(r.list_version),
             QCoreApplication::translate("UpdateTab", "%n key(s)", "",
                                         r.list_keys),
             QCoreApplication::translate("UpdateTab", "Valid until %1")
                 .arg(Date(r.list_expires))}),
      QCoreApplication::translate("UpdateTab", "Signed by %1")
          .arg(r.list_signer));

  steps << StepRow(
      r.Step(CheckStepId::kIndex),
      QCoreApplication::translate("UpdateTab", "Update index"),
      Facts({QCoreApplication::translate("UpdateTab", "Serial %1")
                 .arg(r.index_serial),
             r.mirror,
             QCoreApplication::translate("UpdateTab", "Valid until %1")
                 .arg(Date(r.index_expires)),
             QCoreApplication::translate("UpdateTab", "No rollback")}),
      QCoreApplication::translate("UpdateTab", "Signed by %1")
          .arg(r.index_key));

  // The release details and its notes: one row, the worse of the two.
  const auto& descriptor = r.Step(CheckStepId::kDescriptor);
  const auto& notes = r.Step(CheckStepId::kNotes);
  const auto& worse = descriptor.status == StepStatus::kFailed ? descriptor
                      : notes.status == StepStatus::kFailed    ? notes
                      : descriptor.status == StepStatus::kVerified ||
                              descriptor.status == StepStatus::kNotRun
                          ? descriptor
                          : notes;
  QStringList hashes;
  if (!r.descriptor_sha256.isEmpty()) {
    hashes << QCoreApplication::translate("UpdateTab", "Release details: %1")
                  .arg(r.descriptor_sha256);
  }
  if (!r.notes_sha256.isEmpty()) {
    hashes << QCoreApplication::translate("UpdateTab", "Release notes: %1")
                  .arg(r.notes_sha256);
  }
  steps << StepRow(
      worse,
      QCoreApplication::translate("UpdateTab", "Release details and notes"),
      QCoreApplication::translate("UpdateTab",
                                  "Match the hashes in the update index"),
      hashes.join(u'\n'));

  steps << file;

  steps << StepRow(r.Step(CheckStepId::kCommit),
                   QCoreApplication::translate("UpdateTab", "Official build"),
                   QCoreApplication::translate("UpdateTab", "Confirmed by %1")
                       .arg(r.marker_base));

  // After a failure, what came later is one row: it did not run, or it
  // describes a result this check did not produce.
  QList<DetailRow> rows;
  bool failed = false;
  bool collapsed = false;
  for (const auto& row : steps) {
    if (!row) continue;
    if (failed && row->mark != DetailRow::Mark::kFailed) {
      if (!collapsed) {
        rows << DetailRow{
            DetailRow::Mark::kNotRun,
            QCoreApplication::translate("UpdateTab", "Later steps not run")};
        collapsed = true;
      }
      continue;
    }
    failed = failed || row->mark == DetailRow::Mark::kFailed;
    rows << *row;
  }
  return rows;
}

/// A fingerprint as people read one out: in blocks of four.
auto Grouped(const QString& fingerprint) -> QString {
  QStringList blocks;
  for (qsizetype i = 0; i < fingerprint.size(); i += 4) {
    blocks << fingerprint.mid(i, 4);
  }
  return blocks.join(u' ');
}

/// "118 MiB" from 10 MiB up, "7.3 MiB" below.
auto SizeText(qint64 bytes) -> QString {
  return QLocale().formattedDataSize(bytes,
                                     bytes >= 10LL * 1024 * 1024 ? 0 : 1);
}

/// The first step that failed to verify the update information: a release
/// detail or notes nobody served only costs the download link, and the
/// commit marker is the verdict's to report.
auto FirstFailure(const CheckRecord& r)
    -> std::optional<std::pair<CheckStepId, CheckStep>> {
  for (const auto id :
       {CheckStepId::kRoot, CheckStepId::kIndexKeys, CheckStepId::kIndex,
        CheckStepId::kDescriptor, CheckStepId::kNotes}) {
    const auto& step = r.Step(id);
    if (step.status != StepStatus::kFailed) continue;
    if ((id == CheckStepId::kDescriptor || id == CheckStepId::kNotes) &&
        step.reason == StepReason::kUnreachable) {
      continue;
    }
    return std::make_pair(id, step);
  }
  return std::nullopt;
}

/// How the series' status reads after its number.
auto SeriesStatusText(const QString& status) -> QString {
  if (status == u"stable") {
    return QCoreApplication::translate("UpdateTab", "stable");
  }
  if (status == u"mainline") {
    return QCoreApplication::translate("UpdateTab", "mainline");
  }
  if (status == u"legacy") {
    return QCoreApplication::translate("UpdateTab", "security fixes only");
  }
  if (status == u"eol") {
    return QCoreApplication::translate("UpdateTab", "end of life");
  }
  return {};
}

/// Where a build that is not offered a file is updated from; empty for a
/// build that downloads its own (SelectArtifact()).
auto PackageSource(const QString& install_kind) -> QString {
  if (install_kind == u"msix") {
    return QCoreApplication::translate("UpdateTab", "the Microsoft Store");
  }
  if (install_kind == u"mas") {
    return QCoreApplication::translate("UpdateTab", "the Mac App Store");
  }
  if (install_kind == u"flatpak") {
    return QCoreApplication::translate("UpdateTab", "Flatpak");
  }
  if (install_kind == u"distro") {
    return QCoreApplication::translate("UpdateTab",
                                       "your distribution's packages");
  }
  if (install_kind == u"android") {
    return QCoreApplication::translate("UpdateTab",
                                       "the store it was installed from");
  }
  return {};
}

/// A platform requirement ("macos 13.0") as a person reads it.
auto RequirementText(const QString& unmet) -> QString {
  const auto key = unmet.section(u' ', 0, 0);
  const auto version = unmet.section(u' ', 1);
  if (key == u"macos") return QStringLiteral("macOS %1").arg(version);
  if (key == u"windows") return QStringLiteral("Windows %1").arg(version);
  if (key == u"linux_glibc") return QStringLiteral("glibc %1").arg(version);
  return unmet;
}

}  // namespace

auto ShortHash(const QString& hash) -> QString {
  if (hash.size() <= 16) return hash;
  return hash.left(8) + QStringLiteral("…") + hash.right(8);
}

auto PresentInstall(UpdateView view, const InstallState& install)
    -> UpdateView {
  if (!view.installable) return view;
  using Stage = InstallState::Stage;
  view.show_download = true;
  view.download_enabled = true;
  view.action = UpdateView::Action::kInstall;
  // Checked here, against the descriptor, before it replaces anything.
  DetailRow file{
      DetailRow::Mark::kNotRun,
      QCoreApplication::translate("UpdateTab", "Downloaded file"),
      QCoreApplication::translate("UpdateTab", "Not run"),
      QCoreApplication::translate(
          "UpdateTab", "Checked against its size and SHA-256 after download"),
      {},
      true};

  switch (install.stage) {
    case Stage::kIdle:
      view.download_label =
          QCoreApplication::translate("UpdateTab", "Install update");
      break;
    case Stage::kDownloading: {
      const auto total = install.total > 0 ? install.total : view.download_size;
      const int percent = total > 0
                              ? static_cast<int>(std::clamp<qint64>(
                                    install.received * 100 / total, 0, 100))
                              : 0;
      view.download_label =
          QCoreApplication::translate("UpdateTab", "Downloading… %1%")
              .arg(percent);
      view.download_enabled = false;
      view.check_enabled = false;
      break;
    }
    case Stage::kInstalled:
      view.tone = UpdateTone::kGood;
      view.headline = QCoreApplication::translate(
                          "UpdateTab", "Nightly build %1 is installed")
                          .arg(DisplayVersion(view.download_version));
      view.detail =
          install.can_restart
              ? QCoreApplication::translate(
                    "UpdateTab", "Restart GpgFrontend to start using it.")
              : QCoreApplication::translate(
                    "UpdateTab",
                    "Close and reopen GpgFrontend to start using it.");
      view.notice.clear();
      view.action = UpdateView::Action::kRestart;
      view.download_label =
          QCoreApplication::translate("UpdateTab", "Restart now");
      view.show_download = install.can_restart;
      view.check_enabled = false;
      file.mark = DetailRow::Mark::kVerified;
      file.result = QCoreApplication::translate("UpdateTab", "Verified");
      file.facts =
          Facts({QCoreApplication::translate(
                     "UpdateTab", "Size and SHA-256 matched after download"),
                 SizeText(view.download_size)});
      break;
    case Stage::kFailed:
      view.notice = install.error;
      view.download_label =
          QCoreApplication::translate("UpdateTab", "Try again");
      file.mark = DetailRow::Mark::kFailed;
      file.result = install.error;
      break;
  }
  for (auto& row : view.device_rows) {
    if (row.download) row = file;
  }
  return view;
}

auto DisplayVersion(const QString& version) -> QString {
  auto v = version.trimmed();
  if (v.startsWith(u'v') || v.startsWith(u'V')) v.remove(0, 1);
  const auto plus = v.indexOf(u'+');
  return plus < 0 ? v : v.left(plus);
}

auto RelativeTime(const QDateTime& then, const QDateTime& now) -> QString {
  const auto minutes = then.secsTo(now) / 60;
  if (minutes < 1) return QCoreApplication::translate("UpdateTab", "just now");

  const int m = static_cast<int>(minutes);
  if (minutes < 60) {
    return QCoreApplication::translate("UpdateTab", "%n minute(s) ago", "", m);
  }

  const int h = static_cast<int>(minutes / 60);
  if (h < 24) {
    return QCoreApplication::translate("UpdateTab", "%n hour(s) ago", "", h);
  }

  return QLocale().toString(then, QLocale::ShortFormat);
}

auto SeriesNoticeText(const SoftwareVersion::Notice& notice) -> QString {
  if (notice.series.isEmpty()) return {};
  const auto latest = DisplayVersion(notice.successor_latest);
  if (notice.status == u"legacy") {
    return notice.successor_runs_here
               ? QCoreApplication::translate(
                     "UpdateTab",
                     "The %1 series gets security fixes only, until %2. "
                     "GpgFrontend %3 is the current release.")
                     .arg(notice.series, notice.eol, latest)
               : QCoreApplication::translate(
                     "UpdateTab",
                     "The %1 series gets security fixes only, until %2.")
                     .arg(notice.series, notice.eol);
  }
  return notice.successor_runs_here
             ? QCoreApplication::translate(
                   "UpdateTab",
                   "The %1 series no longer gets fixes. GpgFrontend %2 is "
                   "the current release.")
                   .arg(notice.series, latest)
             : QCoreApplication::translate(
                   "UpdateTab",
                   "The %1 series no longer gets fixes, and the current "
                   "release does not run on this system.")
                   .arg(notice.series);
}

void TameNotesHeadings(QTextDocument* document, qreal body_points) {
  for (auto block = document->begin(); block.isValid(); block = block.next()) {
    const int level = block.blockFormat().headingLevel();
    if (level <= 0) continue;
    // The importer sizes headings by an adjustment; the point size below is
    // what they are to be.
    const auto tame = [&](QTextCharFormat format) {
      format.clearProperty(QTextFormat::FontSizeAdjustment);
      format.setFontWeight(QFont::Bold);
      format.setFontPointSize(level <= 2 ? body_points + 1 : body_points);
      return format;
    };
    QTextCursor cursor(document);
    cursor.setPosition(block.position());
    cursor.setBlockCharFormat(tame(block.charFormat()));
    for (auto it = block.begin(); !it.atEnd(); ++it) {
      const auto fragment = it.fragment();
      cursor.setPosition(fragment.position());
      cursor.setPosition(fragment.position() + fragment.length(),
                         QTextCursor::KeepAnchor);
      cursor.setCharFormat(tame(fragment.charFormat()));
    }
  }
}

void RestrictNotesLinks(QTextDocument* document, const QStringList& hosts) {
  for (auto block = document->begin(); block.isValid(); block = block.next()) {
    for (auto it = block.begin(); !it.atEnd(); ++it) {
      const auto fragment = it.fragment();
      const auto format = fragment.charFormat();
      if (!format.isAnchor() ||
          IsAllowedNotesLink(QUrl(format.anchorHref()), hosts)) {
        continue;
      }
      // Kept as text, in the paragraph's own look: nothing to click, and
      // nothing that looks clickable.
      QTextCharFormat plain = block.charFormat();
      plain.setFontWeight(format.fontWeight());
      plain.setFontItalic(format.fontItalic());
      QTextCursor cursor(document);
      cursor.setPosition(fragment.position());
      cursor.setPosition(fragment.position() + fragment.length(),
                         QTextCursor::KeepAnchor);
      cursor.setCharFormat(plain);
    }
  }
}

namespace {

/// The status line and the buttons, from the verdict alone.
auto PresentStatus(const UpdateSnapshot& s) -> UpdateView {
  UpdateView v;
  v.check_enabled = !s.checking;
  v.check_label = s.checking
                      ? QCoreApplication::translate("UpdateTab", "Checking…")
                  : !s.last_good && !s.last_attempt.isValid()
                      ? QCoreApplication::translate("UpdateTab", "Check now")
                      : QCoreApplication::translate("UpdateTab", "Check again");

  if (!s.last_good) {
    if (s.checking) {
      v.headline =
          QCoreApplication::translate("UpdateTab", "Checking for updates…");
    } else if (s.last_attempt_failed) {
      // A refusal or a build without trust is worded by PresentUpdate().
      v.tone = UpdateTone::kAttention;
      v.headline = QCoreApplication::translate(
          "UpdateTab", "Couldn't reach the update server");
      v.detail = QCoreApplication::translate("UpdateTab",
                                             "No update server answered.");
    } else {
      v.headline =
          QCoreApplication::translate("UpdateTab", "No update information yet");
      v.detail = QCoreApplication::translate(
          "UpdateTab",
          "Check now to see whether a newer GpgFrontend is available.");
    }
    return v;
  }

  const auto& r = *s.last_good;
  const auto latest = DisplayVersion(r.list.latest.version);
  const auto current = DisplayVersion(r.current_version);
  // A nightly follows the signed nightly channel, a release build its
  // series' releases in the same signed index; the wording says which.
  const bool nightly = r.nightly;

  switch (Decide(r)) {
    case Verdict::kUpdateAvailable:
      v.tone = UpdateTone::kUpdate;
      v.headline = nightly ? QCoreApplication::translate(
                                 "UpdateTab", "Nightly build %1 is available")
                                 .arg(latest)
                           : QCoreApplication::translate(
                                 "UpdateTab", "GpgFrontend %1 is available")
                                 .arg(latest);
      v.detail = QCoreApplication::translate("UpdateTab", "You are using %1.")
                     .arg(current);
      v.show_download = true;
      v.download_version = r.list.latest.version;
      // The version is in the headline: the button only says what it does.
      v.download_label = QCoreApplication::translate("UpdateTab", "Download");
      // Only a link the checker made from a verified descriptor. In place
      // only for a nightly: a stable update is installed by its installer.
      if (r.list.latest.html_url.startsWith(u"https://")) {
        v.download_url = r.list.latest.html_url;
        v.download_sha256 = r.list.latest.download_sha256;
        v.download_size = r.list.latest.download_size;
        v.installable =
            nightly && IsSha256(v.download_sha256) && v.download_size > 0;
      }
      v.notes = r.list.latest.notes.trimmed();
      v.show_notes = !v.notes.isEmpty();
      break;
    case Verdict::kWithdrawnOrUnreleased:
      v.tone = UpdateTone::kAttention;
      if (nightly) {
        v.headline = QCoreApplication::translate(
            "UpdateTab", "This nightly build has been withdrawn");
        v.detail = QCoreApplication::translate(
            "UpdateTab", "Move to the newest nightly build.");
        break;
      }
      v.headline = QCoreApplication::translate(
          "UpdateTab", "This version has been withdrawn");
      v.detail = QCoreApplication::translate(
          "UpdateTab", "Move to the latest release of its series.");
      break;
    case Verdict::kUnofficialBuild:
      v.tone = UpdateTone::kAttention;
      v.headline = QCoreApplication::translate(
          "UpdateTab", "This build isn't from the official repository");
      v.detail = nightly
                     ? QCoreApplication::translate(
                           "UpdateTab",
                           "Its source commit isn't among the published "
                           "builds. That is expected for self-built or "
                           "modified versions.")
                     : QCoreApplication::translate(
                           "UpdateTab",
                           "Its source commit wasn't found upstream. That is "
                           "expected for self-built or modified versions.");
      break;
    case Verdict::kUpToDate:
      if (r.other_series) {
        // A release branch's nightly: main's channel is another series, and
        // jumping there is not an update.
        v.headline = QCoreApplication::translate(
            "UpdateTab", "This is a release branch build");
        v.detail = QCoreApplication::translate(
                       "UpdateTab",
                       "%1 is a test build of a fix release. It is not offered "
                       "updates; install the release when it is out.")
                       .arg(current);
        break;
      }
      v.tone = UpdateTone::kGood;
      v.headline =
          QCoreApplication::translate("UpdateTab", "You're up to date");
      if (nightly) {
        v.detail = QCoreApplication::translate(
                       "UpdateTab", "%1 is the newest nightly build.")
                       .arg(current);
      } else {
        v.detail = r.list.outcome == ListOutcome::kFound
                       ? QCoreApplication::translate(
                             "UpdateTab", "%1 is the latest release.")
                             .arg(current)
                       : QCoreApplication::translate(
                             "UpdateTab", "No newer release was found for %1.")
                             .arg(current);
      }
      break;
    case Verdict::kUnknown:
      // last_good is usable by construction; kept for completeness.
      v.tone = UpdateTone::kAttention;
      v.headline = QCoreApplication::translate(
          "UpdateTab", "Couldn't reach the update server");
      break;
  }

  v.notice = SeriesNoticeText(r.notice);
  return v;
}

/// Puts @p line before what the notice already says.
void PrependNotice(UpdateView& v, const QString& line) {
  v.notice = v.notice.isEmpty() ? line : line + u'\n' + v.notice;
}

}  // namespace

auto PresentUpdate(const UpdateSnapshot& s, const QDateTime& now,
                   const UpdateChecker::Build& build) -> UpdateView {
  auto v = PresentStatus(s);

  const SoftwareVersion* r = s.last_good ? &*s.last_good : nullptr;
  const CheckRecord* record = s.last_check ? &*s.last_check : nullptr;
  const bool nightly = r != nullptr         ? r->nightly
                       : build.nightly >= 0 ? build.nightly == 1
                                            : IsNightlyBuild(build.version);
  const auto version = r != nullptr ? r->current_version : build.version;
  const auto commit = r != nullptr ? r->local_commit_hash : build.commit;
  const auto series = SoftwareVersion::VersionSeries(version);

  // How the last check went: the reason is said once, in the header.
  const auto failure = record != nullptr ? FirstFailure(*record) : std::nullopt;
  // Built without release/root: no check can succeed, and asking again
  // cannot help. A state of the build, not a failure.
  const bool untrusted =
      (!build.version.isEmpty() && !build.channel.IsConfigured()) ||
      (failure && failure->second.reason == StepReason::kNoTrust);
  const bool refused = !untrusted && failure &&
                       failure->second.reason != StepReason::kUnreachable;
  const bool unreached =
      !untrusted && !refused && (failure || s.last_attempt_failed);
  const auto attempted = RelativeTime(
      s.last_attempt.isValid() ? s.last_attempt
                               : (r != nullptr ? r->checked_at : now),
      now);

  if (untrusted) {
    v.tone = UpdateTone::kNeutral;
    v.headline = QCoreApplication::translate(
        "UpdateTab", "Updates aren't available for this build");
    v.detail = QCoreApplication::translate(
        "UpdateTab",
        "It was built without the release trust files. Official builds "
        "include them and check for updates automatically.");
    v.notice.clear();
    v.show_check = false;
  } else if (refused) {
    const auto why = ReasonText(failure->first, failure->second);
    if (r == nullptr) {
      v.tone = UpdateTone::kError;
      v.headline = QCoreApplication::translate(
          "UpdateTab", "Update information couldn't be verified");
      v.detail = Sentence(why);
    } else {
      PrependNotice(
          v, QCoreApplication::translate(
                 "UpdateTab", "Update information couldn't be verified: %1.")
                     .arg(why) +
                 (s.last_attempt_failed
                      ? u' ' + QCoreApplication::translate(
                                   "UpdateTab", "Showing the result from %1.")
                                   .arg(RelativeTime(r->checked_at, now))
                      : QString()));
    }
  } else if (unreached && r != nullptr) {
    PrependNotice(v, QCoreApplication::translate(
                         "UpdateTab",
                         "Couldn't reach the update server. Showing the "
                         "result from %1.")
                         .arg(RelativeTime(r->checked_at, now)));
  }

  // The status line: short, its icon in the same set as the header's.
  if (untrusted) {
    v.verification = QCoreApplication::translate("UpdateTab", "Not checked");
  } else if (s.checking) {
    v.verification = QCoreApplication::translate("UpdateTab", "Checking…");
  } else if (refused) {
    v.verification_tone = UpdateTone::kError;
    v.verification =
        QCoreApplication::translate("UpdateTab", "Not verified · checked %1")
            .arg(attempted);
  } else if (unreached) {
    v.verification_tone = UpdateTone::kAttention;
    v.verification = QCoreApplication::translate(
                         "UpdateTab", "Server not reached · checked %1")
                         .arg(attempted);
  } else if (r != nullptr) {
    v.verification_tone = UpdateTone::kGood;
    v.verification =
        QCoreApplication::translate("UpdateTab", "Verified · checked %1")
            .arg(RelativeTime(r->checked_at, now));
  } else {
    v.verification =
        QCoreApplication::translate("UpdateTab", "Not checked yet");
  }

  // This build.
  v.build_version = DisplayVersion(version);
  const auto status =
      record != nullptr ? SeriesStatusText(record->series_status) : QString();
  v.build_kind =
      nightly ? QCoreApplication::translate("UpdateTab", "Nightly")
      : status.isEmpty()
          ? QCoreApplication::translate("UpdateTab", "Release · series %1")
                .arg(series)
          : QCoreApplication::translate("UpdateTab", "Release · series %1 (%2)")
                .arg(series, status);
  v.build_commit = commit.left(12);
  v.build_official = QCoreApplication::translate("UpdateTab", "Not checked");
  if (r != nullptr) {
    switch (r->commit_fact) {
      case RemoteFact::kConfirmed:
        v.build_official = QCoreApplication::translate("UpdateTab", "Yes");
        break;
      case RemoteFact::kNotFound:
        v.build_official = QCoreApplication::translate(
            "UpdateTab", "No, not from the official repository");
        break;
      case RemoteFact::kUnknown:
        v.build_official =
            QCoreApplication::translate("UpdateTab", "Not confirmed");
        break;
    }
  }

  // What is offered, or else the newest there is.
  if (r != nullptr) {
    const auto& latest = r->list.latest;
    if (Decide(*r) == Verdict::kWithdrawnOrUnreleased &&
        r->list.outcome == ListOutcome::kFound &&
        SemVerCompare(latest.version, r->current_version) != 0) {
      // Withdrawn: the newest of its line is the way out.
      v.detail = QCoreApplication::translate("UpdateTab",
                                             "Move to %1, the newest version.")
                     .arg(DisplayVersion(latest.version));
      v.show_download = true;
      v.download_version = latest.version;
      v.download_label = QCoreApplication::translate("UpdateTab", "Download");
      if (latest.html_url.startsWith(u"https://")) {
        v.download_url = latest.html_url;
        v.download_sha256 = latest.download_sha256;
        v.download_size = latest.download_size;
      }
    }
    if (v.show_download) {
      v.show_offer = true;
      v.offer_version = DisplayVersion(latest.version);
      if (!v.download_url.isEmpty()) {
        v.offer_file = QUrl(v.download_url).fileName();
        if (v.download_size > 0) {
          v.offer_size = SizeText(v.download_size);
        }
        v.offer_sha256 = v.download_sha256;
      }
    } else if (r->list.outcome == ListOutcome::kFound) {
      v.newest_label =
          nightly ? QCoreApplication::translate("UpdateTab", "Newest nightly")
                  : QCoreApplication::translate("UpdateTab", "Newest in %1")
                        .arg(series);
      v.newest = DisplayVersion(latest.version);
    }
  }
  if (record != nullptr && !nightly && !record->newer_beta.isEmpty()) {
    v.newer_beta = QCoreApplication::translate("UpdateTab",
                                               "A newer beta, %1, is available")
                       .arg(DisplayVersion(record->newer_beta));
  }

  // A store, a distribution or Flatpak updates this build, not a download.
  const auto source = PackageSource(build.platform.install_kind);
  if (!source.isEmpty() && v.show_download) {
    v.show_download = false;
    v.installable = false;
    v.notice += (v.notice.isEmpty() ? QString() : QStringLiteral("\n")) +
                QCoreApplication::translate(
                    "UpdateTab", "Updates for this build come from %1.")
                    .arg(source);
  }

  // A series no longer supported: its successor, where it runs here.
  if (r != nullptr && !r->notice.series.isEmpty() && source.isEmpty() &&
      !v.show_download && !r->notice.successor_latest.isEmpty()) {
    const auto successor = DisplayVersion(r->notice.successor_latest);
    if (r->notice.successor_runs_here) {
      v.show_download = true;  // the downloads page: no descriptor read
      v.download_version = r->notice.successor_latest;
      v.download_label = QCoreApplication::translate("UpdateTab", "Download");
    } else {
      v.notice +=
          u'\n' +
          (r->notice.unmet.isEmpty()
               ? QCoreApplication::translate(
                     "UpdateTab",
                     "GpgFrontend %1 does not support this system.")
                     .arg(successor)
               : QCoreApplication::translate(
                     "UpdateTab",
                     "GpgFrontend %1 needs %2 or later, which this system "
                     "doesn't have or couldn't confirm.")
                     .arg(successor, RequirementText(r->notice.unmet)));
    }
  }

  // The details: what this device checked, then what the server did.
  if (untrusted) {
    v.device_rows << DetailRow{
        DetailRow::Mark::kNotChecked,
        QCoreApplication::translate("UpdateTab", "Root of trust"),
        QCoreApplication::translate("UpdateTab", "Not included in this build")};
  } else if (record != nullptr) {
    std::optional<DetailRow> file;
    if (v.show_download) {
      // The hash to compare it with is in the Offered block, once.
      file = DetailRow{
          DetailRow::Mark::kNotChecked,
          QCoreApplication::translate("UpdateTab", "Downloaded file"),
          QCoreApplication::translate("UpdateTab", "Not checked"),
          QCoreApplication::translate(
              "UpdateTab",
              "Opens in your browser. Compare it with the SHA-256 above."),
          {},
          true};
    }
    v.device_rows = DeviceRows(s, *record, file);

    // What the trusted root's host_policy names, as it names it.
    if (!record->repository.isEmpty()) {
      QStringList built{record->repository, record->workflow.section(u'/', -1)};
      if (!nightly && v.show_offer) {
        built << QCoreApplication::translate("UpdateTab", "Tag v%1")
                     .arg(v.offer_version);
      }
      built.removeAll(QString());
      v.host_rows << DetailRow{
          DetailRow::Mark::kNone,
          QCoreApplication::translate("UpdateTab", "Built by GitHub Actions"),
          {},
          Facts(built),
          record->workflow};
      if (!nightly && !record->pgp_fingerprints.isEmpty()) {
        QStringList grouped;
        for (const auto& fpr : record->pgp_fingerprints) {
          grouped << Grouped(fpr);
        }
        v.host_rows << DetailRow{
            DetailRow::Mark::kNone,
            QCoreApplication::translate("UpdateTab",
                                        "Signed by the maintainer"),
            QCoreApplication::translate("UpdateTab", "OpenPGP"),
            grouped.join(QStringLiteral(" · ")),
            record->pgp_fingerprints.join(u'\n')};
        if (build.platform.os == u"windows" &&
            !record->authenticode_subject.isEmpty()) {
          v.host_rows << DetailRow{
              DetailRow::Mark::kNone,
              QCoreApplication::translate("UpdateTab",
                                          "Windows installers signed"),
              QCoreApplication::translate("UpdateTab", "Authenticode"),
              record->authenticode_subject};
        }
      }
      v.security_url =
          QStringLiteral("https://github.com/%1/blob/main/SECURITY.md")
              .arg(record->repository);
    }
  }

  v.report_head << QStringLiteral("GpgFrontend %1").arg(v.build_version)
                << v.build_kind;
  if (!commit.isEmpty()) {
    v.report_head
        << QCoreApplication::translate("UpdateTab", "Commit %1").arg(commit);
  }
  if (v.show_offer) {
    // The full hash, once, for comparing a download by hand.
    QStringList offer{QCoreApplication::translate("UpdateTab", "Offered %1")
                          .arg(v.offer_version),
                      v.offer_file, v.offer_size,
                      v.offer_sha256.isEmpty()
                          ? QString()
                          : QStringLiteral("SHA-256 %1").arg(v.offer_sha256)};
    offer.removeAll(QString());
    v.report_head << offer.join(QStringLiteral(" · "));
  }
  if (s.last_attempt.isValid()) {
    v.report_head << QCoreApplication::translate("UpdateTab", "Checked %1")
                         .arg(s.last_attempt.toUTC().toString(Qt::ISODate));
  }
  return v;
}

auto ReportText(const UpdateView& view) -> QString {
  const auto mark = [](DetailRow::Mark m) -> QString {
    switch (m) {
      case DetailRow::Mark::kVerified:
        return QCoreApplication::translate("UpdateTab", "verified");
      case DetailRow::Mark::kFailed:
        return QCoreApplication::translate("UpdateTab", "failed");
      case DetailRow::Mark::kNotRun:
        return QCoreApplication::translate("UpdateTab", "not run");
      case DetailRow::Mark::kNotChecked:
        return QCoreApplication::translate("UpdateTab", "not checked");
      case DetailRow::Mark::kNone:
        break;
    }
    return {};
  };
  const auto text = [](const DetailRow& row) {
    // Everything, key ids and full hashes too: the report is for comparing.
    QStringList parts{row.result, row.facts, row.tip};
    parts.removeAll(QString());
    return parts.join(QStringLiteral(" · "))
        .replace(u'\n', QStringLiteral(" · "));
  };

  QStringList out = view.report_head;
  out << view.headline << view.detail;
  if (!view.notice.isEmpty()) out << view.notice;
  out << view.verification << QString()
      << QCoreApplication::translate("UpdateTab", "Checked on this device");
  for (const auto& row : view.device_rows) {
    out << QStringLiteral("[%1] %2: %3")
               .arg(mark(row.mark), row.title, text(row));
  }
  if (!view.host_rows.isEmpty()) {
    out << QString()
        << QCoreApplication::translate("UpdateTab",
                                       "Checked by the release server");
    for (const auto& row : view.host_rows) {
      out << QStringLiteral("- %1: %2").arg(row.title, text(row));
    }
  }
  return out.join(u'\n') + u'\n';
}
