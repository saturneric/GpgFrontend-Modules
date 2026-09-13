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

#include "EMailSecurityView.h"

#include <GFSDKGpg.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <functional>

#include "EMailHelper.h"
#include "EMailViewStyle.h"

namespace {

constexpr int kColItem = 0;
constexpr int kColValue = 1;

/// Marks an address row that resolved to no key at all. Far past UserRole,
/// which the tree's own payloads use.
constexpr int kRoleMissingKey = Qt::UserRole + 20;

/// Which section a top-level row is, as one of the kSection* constants.
constexpr int kRoleSectionId = Qt::UserRole + 21;

// Mirrors GpgFrontend::GpgKeyStatus, which GFGpgKeyBrief::usability reports.
//
// Deliberately untyped: lupdate stops attributing tr() calls to the enclosing
// class once it meets an enum with an explicit underlying type in this file,
// so every string below one would silently drop out of the catalogues.
enum KeyUsability {
  kUSABLE = 0,
  kEXPIRING_SOON = 1,
  kEXPIRED = 2,
  kREVOKED = 3,
  kDISABLED = 4,
};

auto DescribeUsability(int usability) -> QString {
  switch (usability) {
    case kUSABLE:
      return QApplication::translate("EMailSecurityView", "usable");
    case kEXPIRING_SOON:
      return QApplication::translate("EMailSecurityView", "expiring soon");
    case kEXPIRED:
      return QApplication::translate("EMailSecurityView", "expired");
    case kREVOKED:
      return QApplication::translate("EMailSecurityView", "revoked");
    case kDISABLED:
      return QApplication::translate("EMailSecurityView", "disabled");
    default:
      return QApplication::translate("EMailSecurityView", "unknown");
  }
}

// Mirrors GpgFrontend::GpgSigValidity.
auto DescribeValidity(int validity) -> QString {
  switch (validity) {
    case 0:
      return QApplication::translate("EMailSecurityView", "valid");
    case 1:
      return QApplication::translate("EMailSecurityView", "valid, with issues");
    case 2:
      return QApplication::translate("EMailSecurityView",
                                     "valid, key not fully trusted");
    case 3:
      return QApplication::translate("EMailSecurityView", "invalid");
    case 4:
      return QApplication::translate("EMailSecurityView", "public key missing");
    case 5:
      return QApplication::translate("EMailSecurityView",
                                     "signing key revoked");
    case 6:
      return QApplication::translate("EMailSecurityView", "signature expired");
    case 7:
      return QApplication::translate("EMailSecurityView",
                                     "signing key expired");
    default:
      return QApplication::translate("EMailSecurityView", "unknown");
  }
}

/// The wording for one signature, once it has been judged.
///
/// Falls back to the raw validity description for the outcomes that have no
/// better name, so nothing is lost by routing through the badge.
auto DescribeSignature(EMailBadgeState badge, int validity) -> QString {
  switch (badge) {
    case EMailBadgeState::kSIGNED_BAD:
      // Named for what it is. "valid, with issues" -- the label validity 1
      // used to carry -- describes a bad signature as a near miss.
      return validity == 5
                 ? QApplication::translate("EMailSecurityView",
                                           "bad signature: signing key revoked")
                 : QApplication::translate("EMailSecurityView",
                                           "BAD signature: it does not match "
                                           "these bytes");
    case EMailBadgeState::kSIGNED_MISMATCH:
      return QApplication::translate("EMailSecurityView",
                                     "valid, but signed by another address");
    default:
      return DescribeValidity(validity);
  }
}

/// How loudly to show one signature.
auto ToneForSignature(EMailBadgeState badge) -> EMailTone {
  switch (ToneForBadge(badge)) {
    case EMailBadgeTone::kGOOD:
      return EMailTone::kGOOD;
    case EMailBadgeTone::kDANGER:
      return EMailTone::kDANGER;
    case EMailBadgeTone::kWARN:
    case EMailBadgeTone::kMUTED:
      break;
  }
  return EMailTone::kWARN;
}

}  // namespace

EMailSecurityView::EMailSecurityView(QWidget* parent) : QWidget(parent) {
  build_ui();
}

void EMailSecurityView::build_ui() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 6, 8, 6);
  layout->setSpacing(6);

  headline_ = new QLabel(this);
  headline_->setWordWrap(true);
  // One sentence describing the table beneath it. At full size above an empty
  // table it read as an orphaned caption rather than as a summary of anything.
  EMailMakeSecondary(headline_);
  layout->addWidget(headline_);

  tree_ = new QTreeWidget(this);
  // Findings and warnings are sentences, not labels, and a row that ends in an
  // ellipsis is a row the user cannot act on. Wrapping costs uniform row
  // heights, which is only a scrolling optimisation and is worth giving up for
  // text that can actually be read.
  tree_->setWordWrap(true);
  tree_->setTextElideMode(Qt::ElideNone);
  tree_->setUniformRowHeights(false);
  tree_->setAlternatingRowColors(true);
  tree_->setHeaderLabels({tr("Item"), tr("Detail")});
  tree_->header()->setStretchLastSection(true);
  tree_->header()->setSectionResizeMode(kColItem,
                                        QHeaderView::ResizeToContents);
  EMailPolishTree(tree_);
  // Copying a fingerprint out of this tab used to be impossible by any route:
  // the rows are not selectable as text and there was no menu.
  tree_->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(tree_, &QTreeWidget::customContextMenuRequested, this,
          &EMailSecurityView::show_row_menu);

  layout->addWidget(tree_, 1);

  // Takes the tree's place rather than a row of its own: exactly one of the
  // two is on screen at any time.
  empty_notice_ = EMailEmptyNotice(this, tr("No message is open."));
  layout->addWidget(empty_notice_, 1);
  tree_->setVisible(false);

  apply_colors();
}

void EMailSecurityView::apply_colors() {
  // The headline's colour is a function of the state, which is why the state
  // is kept: SetMessage's arguments are gone by the time a theme change
  // arrives, and a headline that had turned warning-coloured must not come
  // back muted.
  EMailSetLabelColor(headline_, state_ == EMailSecurityState::kMALFORMED_PGP
                                    ? EMailWarningColor(this)
                                    : EMailMutedColor(this));
  EMailSetLabelColor(empty_notice_, EMailMutedColor(this));
  EMailPaintTreeHeader(tree_);
  EMailRepaintTree(tree_);
}

void EMailSecurityView::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);
  if (EMailIsRestyle(event)) apply_colors();
}

void EMailSecurityView::Clear() {
  tree_->clear();
  headline_->clear();
  ShowNotice(tr("No message is open."));
}

void EMailSecurityView::ShowNotice(const QString& text) {
  tree_->clear();
  tree_->setVisible(false);
  // The headline describes a table that is not there, so it goes with it.
  headline_->setVisible(false);
  empty_notice_->setText(text);
  empty_notice_->setVisible(true);
}

void EMailSecurityView::show_row_menu(const QPoint& pos) {
  auto* item = tree_->itemAt(pos);
  if (item == nullptr) return;

  tree_->setCurrentItem(item);

  const auto value = item->text(kColValue);
  const auto label = item->text(kColItem);

  // The fingerprint of the key this row is about: the row itself when it is
  // the fingerprint row, and otherwise the one hanging under it. Offered
  // separately from Copy Value because the fingerprint is the thing anyone
  // comes to this tab to take away, and hunting for the child row to
  // right-click is a step with no purpose.
  const auto fingerprint = [this, item]() -> QString {
    if (item->text(kColItem) == tr("Fingerprint")) return item->text(kColValue);
    for (int i = 0; i < item->childCount(); ++i) {
      auto* child = item->child(i);
      if (child->text(kColItem) == tr("Fingerprint")) {
        return child->text(kColValue);
      }
    }
    return {};
  }();

  const bool missing_key = item->data(kColItem, kRoleMissingKey).toBool();

  QMenu menu(this);
  auto* copy_value = menu.addAction(tr("Copy Value"));
  copy_value->setEnabled(!value.isEmpty());

  QAction* copy_fpr = nullptr;
  if (!fingerprint.isEmpty()) {
    copy_fpr = menu.addAction(tr("Copy Fingerprint"));
  }

  QAction* copy_address = nullptr;
  QAction* import = nullptr;
  if (missing_key) {
    menu.addSeparator();
    copy_address = menu.addAction(tr("Copy Address"));
    if (message_carries_key_) {
      // The common shape of a first contact: the message that names an
      // address nothing is known about is also the message carrying the key
      // for it. Offered only here, because this is the row that is otherwise
      // a dead end.
      import = menu.addAction(tr("Import the Key in This Message"));
    }
  }

  // Only once something has already tried. Before that the tab runs a
  // verification on its own when it is opened, and offering to repeat work
  // that has not happened yet would be a control that means nothing.
  QAction* verify_again = nullptr;
  if (has_regions_ && verify_state_ != EMailVerifyState::kNOT_ATTEMPTED) {
    menu.addSeparator();
    verify_again = menu.addAction(tr("Verify Again"));
  }

  auto* chosen = menu.exec(tree_->viewport()->mapToGlobal(pos));
  if (chosen == nullptr) return;

  auto* clipboard = QApplication::clipboard();
  if (chosen == copy_value) {
    clipboard->setText(value);
  } else if (copy_fpr != nullptr && chosen == copy_fpr) {
    clipboard->setText(fingerprint);
  } else if (copy_address != nullptr && chosen == copy_address) {
    clipboard->setText(label);
  } else if (import != nullptr && chosen == import) {
    emit SignalImportMessageKeysRequested();
  } else if (verify_again != nullptr && chosen == verify_again) {
    emit SignalVerifyAgainRequested();
  }
}

auto EMailSecurityView::add_group(const QString& title, const QString& id)
    -> QTreeWidgetItem* {
  auto* group = new QTreeWidgetItem(tree_);
  group->setText(kColItem, title);
  auto font = group->font(kColItem);
  font.setBold(true);
  group->setFont(kColItem, font);
  group->setFirstColumnSpanned(true);
  // Stored so RevealSection() can find this row again. Not matched on the
  // title: that is translated, and the caller naming it is not.
  group->setData(kColItem, kRoleSectionId, id);
  return group;
}

void EMailSecurityView::RevealSection(const QString& section) {
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    auto* item = tree_->topLevelItem(i);
    if (item->data(kColItem, kRoleSectionId).toString() != section) continue;

    tree_->setCurrentItem(item);
    // PositionAtTop rather than EnsureVisible: the section's own rows are
    // underneath it, and the point of coming here was to read them.
    tree_->scrollToItem(item, QAbstractItemView::PositionAtTop);
    return;
  }

  // No such section in this message. Nothing to point at, and inventing a
  // selection elsewhere would send the user to the wrong rows.
}

void EMailSecurityView::add_signature_section(
    const QList<EMailSignatureRegion>& regions,
    const QList<EMailSignatureResult>& results, EMailVerifyState verify_state) {
  if (regions.isEmpty()) return;

  auto* group = add_group(tr("Signatures"), kSectionSignatures);
  for (const auto& region : regions) {
    auto* region_item = new QTreeWidgetItem(group);
    region_item->setText(kColItem,
                         tr("Signature %1").arg(region.region_id + 1));
    region_item->setText(
        kColValue, region.nesting_depth == 0
                       ? tr("covers %1").arg(EMailHumanSize(region.raw_length))
                       : tr("covers %1, nested %2 deep")
                             .arg(EMailHumanSize(region.raw_length))
                             .arg(region.nesting_depth));

    // Results are gathered by region id. Position in either list means
    // nothing: a region can produce several signatures, or none at all.
    QList<EMailSignatureResult> mine;
    for (const auto& result : results) {
      if (result.region_id == region.region_id) mine.append(result);
    }

    if (mine.isEmpty()) {
      auto* none = new QTreeWidgetItem(region_item);
      none->setText(kColItem, tr("Result"));
      // Said plainly rather than left blank: a signed range that nothing has
      // vouched for is a fact about the message, not a gap in the display.
      //
      // Which of the two it is matters. "Nothing has looked yet" is a state
      // that resolves on its own; "something looked and came back with
      // nothing" is a finding about the message, and the user can only tell
      // they are entitled to ask again if the two are not worded alike.
      none->setText(
          kColValue,
          verify_state == EMailVerifyState::kNOT_ATTEMPTED
              ? tr("this section is signed, but nothing has verified it yet")
              : tr("this section is signed, but verifying it produced no "
                   "result: the signing key may not be in your keyring"));
      EMailSetCellTone(none, kColValue, EMailTone::kMUTED, this);
    }

    for (const auto& result : mine) {
      auto* item = new QTreeWidgetItem(region_item);
      item->setText(kColItem,
                    result.uid.isEmpty() ? tr("Unknown signer") : result.uid);
      // Judged the same way the badge on the message surface is, so the two
      // can never disagree about the same signature.
      //
      // The tone matters as much as the words. Validity 1 is
      // GPGME_SIGSUM_RED -- a BAD signature -- and it used to be painted the
      // same amber as a missing key and labelled "valid, with issues", which
      // made a forgery read as a minor quibble. kDANGER exists for exactly
      // this and was not being used for any signature at all.
      const auto badge = BadgeForSignature(result, from_);
      item->setText(kColValue, DescribeSignature(badge, result.validity));
      EMailSetCellTone(item, kColValue, ToneForSignature(badge), this);

      if (badge == EMailBadgeState::kSIGNED_MISMATCH) {
        auto* warn = new QTreeWidgetItem(item);
        warn->setText(kColItem, tr("Different address"));
        warn->setText(
            kColValue,
            tr("the signature is valid, but the key belongs to %1 while this "
               "message says it is from %2. A valid signature says who signed "
               "the bytes, not who sent the message.")
                .arg(AddressOfUid(result.uid), AddressOfUid(from_)));
        EMailSetCellTone(warn, kColValue, EMailTone::kWARN, this);
      }

      const auto detail = [&](const QString& name, const QString& value) {
        if (value.isEmpty()) return;
        auto* row = new QTreeWidgetItem(item);
        row->setText(kColItem, name);
        row->setText(kColValue, value);
        EMailSetCellTone(row, kColItem, EMailTone::kMUTED, this);
      };

      detail(tr("Fingerprint"), result.fingerprint);
      detail(tr("Public key algorithm"), result.pubkey_algo);
      detail(tr("Signed at"), result.sign_time.isValid()
                                  ? QLocale().toString(result.sign_time)
                                  : QString());

      // The two hash values side by side, each labelled for what it is: one
      // is the sender's claim, the other is what was actually used.
      detail(tr("Declared hash (micalg)"), region.declared_micalg);
      detail(tr("Signature hash"), result.hash_algo);

      if (result.micalg_mismatch) {
        auto* warn = new QTreeWidgetItem(item);
        warn->setText(kColItem, tr("Hash mismatch"));
        warn->setText(kColValue,
                      tr("the message declares %1 but the signature used %2")
                          .arg(region.declared_micalg, result.hash_algo));
        EMailSetCellTone(warn, kColValue, EMailTone::kWARN, this);
      }

      for (const auto& warning : result.warnings) {
        auto* warn = new QTreeWidgetItem(item);
        warn->setText(kColItem, tr("Warning"));
        warn->setText(kColValue, warning);
        EMailSetCellTone(warn, kColValue, EMailTone::kWARN, this);
      }
    }
  }
}

void EMailSecurityView::add_recipient_section(
    const QList<EMailRecipientRow>& recipients) {
  if (recipients.isEmpty()) return;

  auto* group = add_group(tr("Recipients"), kSectionRecipients);
  for (const auto& row : recipients) {
    auto* item = new QTreeWidgetItem(group);

    switch (row.match) {
      case RecipientMatch::kMATCHED:
        item->setText(kColItem, row.address);
        item->setText(kColValue, tr("addressed and encrypted to"));
        EMailSetCellTone(item, kColValue, EMailTone::kGOOD, this);
        break;

      case RecipientMatch::kADDRESSED_NOT_ENCRYPTED:
        // The only genuine warning in this section: this person was told the
        // message is for them and cannot open it.
        item->setText(kColItem, row.address);
        item->setText(
            kColValue,
            tr("in %1, but not encrypted to it: they cannot read this")
                .arg(row.header_field));
        EMailSetCellTone(item, kColValue, EMailTone::kWARN, this);
        break;

      case RecipientMatch::kENCRYPTED_NOT_ADDRESSED:
        // Ordinary and usually correct: a blind copy, an archive key, or the
        // sender's own key. Reported, not flagged.
        item->setText(kColItem, row.info.uid.isEmpty()
                                    ? tr("Key %1").arg(row.info.key_id)
                                    : row.info.uid);
        item->setText(kColValue,
                      tr("encrypted to, but not in the visible headers"));
        EMailSetCellTone(item, kColValue, EMailTone::kMUTED, this);
        break;

      case RecipientMatch::kHIDDEN_RECIPIENT:
        item->setText(kColItem, tr("Hidden recipient"));
        item->setText(kColValue,
                      tr("the sender chose not to record who this is"));
        EMailSetCellTone(item, kColValue, EMailTone::kMUTED, this);
        break;
    }

    if (!row.info.fingerprint.isEmpty()) {
      auto* fpr = new QTreeWidgetItem(item);
      fpr->setText(kColItem, tr("Fingerprint"));
      fpr->setText(kColValue, row.info.fingerprint);
      EMailSetCellTone(fpr, kColItem, EMailTone::kMUTED, this);
    }

    if (row.info.algo_is_primary_key) {
      auto* note = new QTreeWidgetItem(item);
      note->setText(kColItem, tr("Note"));
      note->setText(kColValue,
                    tr("the engine reported the primary key rather than the "
                       "encryption subkey actually used"));
      EMailSetCellTone(note, kColValue, EMailTone::kMUTED, this);
    }
  }
}

void EMailSecurityView::add_key_section(const QStringList& addresses,
                                        int channel) {
  if (addresses.isEmpty()) return;

  auto* group = add_group(tr("Keys for these addresses"), kSectionKeys);
  QStringList seen;
  for (const auto& address : addresses) {
    const auto email = AddressOfUid(address);
    if (email.isEmpty() || seen.contains(email)) continue;
    seen.append(email);

    GFGpgKeyBrief* briefs = nullptr;
    int count = 0;
    GFGpgFindKeysByEmail(channel, email.toUtf8().constData(), &briefs, &count);

    auto* item = new QTreeWidgetItem(group);
    item->setText(kColItem, email);

    if (count == 0) {
      item->setText(kColValue, tr("no key found"));
      EMailSetCellTone(item, kColValue, EMailTone::kWARN, this);
      // Recorded on the row so the menu knows this is an address with nothing
      // behind it, which is the one case where importing is worth offering.
      item->setData(kColItem, kRoleMissingKey, true);
      GFGpgFreeKeyBriefs(briefs, count);
      continue;
    }

    item->setText(kColValue,
                  count == 1 ? tr("1 key") : tr("%1 keys").arg(count));

    for (int i = 0; i < count; ++i) {
      const auto& brief = briefs[i];

      auto* key_item = new QTreeWidgetItem(item);
      key_item->setText(kColItem, QString::fromUtf8(brief.uid));

      // Usability. Says whether the key can be used at all -- and nothing
      // whatsoever about whose key it is.
      key_item->setText(
          kColValue, tr("key is %1").arg(DescribeUsability(brief.usability)));
      EMailSetCellTone(
          key_item, kColValue,
          brief.usability == kUSABLE ? EMailTone::kGOOD : EMailTone::kWARN,
          this);

      auto* fpr = new QTreeWidgetItem(key_item);
      fpr->setText(kColItem, tr("Fingerprint"));
      fpr->setText(kColValue, QString::fromUtf8(brief.fingerprint));
      EMailSetCellTone(fpr, kColItem, EMailTone::kMUTED, this);

      // Identity binding, kept as its own row. A usable key carrying this
      // address only on a secondary or revoked UID is exactly the case a
      // single combined verdict would bury.
      auto* identity = new QTreeWidgetItem(key_item);
      identity->setText(kColItem, tr("Identity"));
      if (brief.matched_uid_revoked != 0) {
        identity->setText(
            kColValue, tr("this address is on a REVOKED user ID of the key"));
        EMailSetCellTone(identity, kColValue, EMailTone::kWARN, this);
      } else if (brief.matched_uid_is_primary != 0) {
        identity->setText(kColValue, tr("this address is the key's primary "
                                        "user ID"));
        EMailSetCellTone(identity, kColValue, EMailTone::kGOOD, this);
      } else {
        identity->setText(kColValue,
                          tr("this address is a secondary user ID of the key"));
        EMailSetCellTone(identity, kColValue, EMailTone::kMUTED, this);
      }

      if (brief.can_encrypt == 0) {
        auto* note = new QTreeWidgetItem(key_item);
        note->setText(kColItem, tr("Note"));
        note->setText(kColValue, tr("this key cannot be used for encryption"));
        EMailSetCellTone(note, kColValue, EMailTone::kWARN, this);
      }
    }

    GFGpgFreeKeyBriefs(briefs, count);
  }
}

void EMailSecurityView::add_findings_section(
    const QList<EMailFinding>& findings) {
  if (findings.isEmpty()) return;

  auto* group = add_group(tr("What stands out"), kSectionFindings);

  for (const auto& finding : findings) {
    auto* item = new QTreeWidgetItem(group);
    item->setText(kColItem, finding.title);
    item->setText(kColValue, finding.detail);

    switch (finding.level) {
      case EMailFindingLevel::kRISK:
        // The only level painted as danger. Reserved for things that are
        // actually wrong or built to deceive.
        EMailSetCellTone(item, kColItem, EMailTone::kDANGER, this);
        break;
      case EMailFindingLevel::kWARN:
        EMailSetCellTone(item, kColItem, EMailTone::kWARN, this);
        break;
      case EMailFindingLevel::kNOTE:
        EMailSetCellTone(item, kColItem, EMailTone::kMUTED, this);
        break;
    }
  }
}

void EMailSecurityView::SetMessage(
    EMailSecurityState state, const QList<EMailSignatureRegion>& regions,
    const QList<EMailSignatureResult>& results,
    const QList<EMailRecipientRow>& recipients, const QStringList& addresses,
    const QString& from, int channel, const QList<EMailFinding>& findings,
    EMailVerifyState verify_state, bool message_carries_key) {
  verify_state_ = verify_state;
  from_ = from;
  message_carries_key_ = message_carries_key;
  has_regions_ = !regions.isEmpty();
  // Kept because the headline's colour depends on it and these arguments are
  // gone by the time a theme change arrives.
  state_ = state;
  tree_->clear();

  switch (state) {
    case EMailSecurityState::kPLAIN:
      headline_->setText(tr("This message is not signed or encrypted."));
      break;
    case EMailSecurityState::kSIGNED:
      headline_->setText(tr("This message carries a signature."));
      break;
    case EMailSecurityState::kENCRYPTED:
      headline_->setText(tr("This message is encrypted."));
      break;
    case EMailSecurityState::kSIGNED_ENCRYPTED:
      headline_->setText(
          tr("This message is encrypted and carries a "
             "signature."));
      break;
    case EMailSecurityState::kMALFORMED_PGP:
      headline_->setText(
          tr("This message claims to use OpenPGP but its "
             "structure does not follow RFC 3156."));
      break;
  }

  apply_colors();

  // Findings first: if something about this message is deceptive, that is the
  // thing to read before any of the detail below it.
  add_findings_section(findings);
  add_signature_section(regions, results, verify_state);
  add_recipient_section(recipients);
  add_key_section(addresses, channel);

  // Wrapping handles the usual case; a tooltip is what still works when the
  // column is dragged narrow, and it costs one walk of a tree that is never
  // large.
  std::function<void(QTreeWidgetItem*)> add_tooltips =
      [&](QTreeWidgetItem* item) {
        for (int column = 0; column < tree_->columnCount(); ++column) {
          const auto text = item->text(column);
          if (!text.isEmpty()) item->setToolTip(column, text);
        }
        for (int i = 0; i < item->childCount(); ++i) {
          add_tooltips(item->child(i));
        }
      };
  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    add_tooltips(tree_->topLevelItem(i));
  }

  tree_->expandAll();

  // A message can be perfectly well formed and still give this tab nothing to
  // list -- no signature, no encryption, no findings. That is an answer, and
  // it reads better as one sentence than as an empty ruled box.
  const bool has_rows = tree_->topLevelItemCount() > 0;
  headline_->setVisible(true);
  tree_->setVisible(has_rows);
  empty_notice_->setVisible(!has_rows);
  if (!has_rows) {
    empty_notice_->setText(
        tr("There is nothing further to report about this message."));
  }
}
