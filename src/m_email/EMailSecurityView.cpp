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

#include <QApplication>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "EMailHelper.h"
#include "EMailViewStyle.h"

namespace {

constexpr int kColItem = 0;
constexpr int kColValue = 1;

// Mirrors GpgFrontend::GpgKeyStatus, which GFGpgKeyBrief::usability reports.
enum KeyUsability : int {
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

auto ValidityIsGood(int validity) -> bool { return validity <= 2; }

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
  layout->addWidget(headline_);

  tree_ = new QTreeWidget(this);
  tree_->setUniformRowHeights(true);
  tree_->setAlternatingRowColors(true);
  tree_->setHeaderLabels({tr("Item"), tr("Detail")});
  tree_->header()->setStretchLastSection(true);
  tree_->header()->setSectionResizeMode(kColItem,
                                        QHeaderView::ResizeToContents);
  layout->addWidget(tree_, 1);
}

void EMailSecurityView::Clear() {
  tree_->clear();
  headline_->clear();
}

auto EMailSecurityView::add_group(const QString& title) -> QTreeWidgetItem* {
  auto* group = new QTreeWidgetItem(tree_);
  group->setText(kColItem, title);
  auto font = group->font(kColItem);
  font.setBold(true);
  group->setFont(kColItem, font);
  group->setFirstColumnSpanned(true);
  return group;
}

void EMailSecurityView::add_signature_section(
    const QList<EMailSignatureRegion>& regions,
    const QList<EMailSignatureResult>& results) {
  if (regions.isEmpty()) return;

  auto* group = add_group(tr("Signatures"));
  const auto muted = EMailMutedColor(this);

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
      none->setText(kColValue,
                    tr("this section is signed, but nothing has verified it "
                       "yet"));
      none->setForeground(kColValue, muted);
    }

    for (const auto& result : mine) {
      auto* item = new QTreeWidgetItem(region_item);
      item->setText(kColItem,
                    result.uid.isEmpty() ? tr("Unknown signer") : result.uid);
      item->setText(kColValue, DescribeValidity(result.validity));
      item->setForeground(kColValue, ValidityIsGood(result.validity)
                                         ? EMailAccentColor(this, true)
                                         : EMailWarningColor(this));

      const auto detail = [&](const QString& name, const QString& value) {
        if (value.isEmpty()) return;
        auto* row = new QTreeWidgetItem(item);
        row->setText(kColItem, name);
        row->setText(kColValue, value);
        row->setForeground(kColItem, muted);
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
        warn->setForeground(kColValue, EMailWarningColor(this));
      }

      for (const auto& warning : result.warnings) {
        auto* warn = new QTreeWidgetItem(item);
        warn->setText(kColItem, tr("Warning"));
        warn->setText(kColValue, warning);
        warn->setForeground(kColValue, EMailWarningColor(this));
      }
    }
  }
}

void EMailSecurityView::add_recipient_section(
    const QList<EMailRecipientRow>& recipients) {
  if (recipients.isEmpty()) return;

  auto* group = add_group(tr("Recipients"));
  const auto muted = EMailMutedColor(this);

  for (const auto& row : recipients) {
    auto* item = new QTreeWidgetItem(group);

    switch (row.match) {
      case RecipientMatch::kMATCHED:
        item->setText(kColItem, row.address);
        item->setText(kColValue, tr("addressed and encrypted to"));
        item->setForeground(kColValue, EMailAccentColor(this, true));
        break;

      case RecipientMatch::kADDRESSED_NOT_ENCRYPTED:
        // The only genuine warning in this section: this person was told the
        // message is for them and cannot open it.
        item->setText(kColItem, row.address);
        item->setText(kColValue,
                      tr("in %1, but NOT encrypted to - they cannot read this")
                          .arg(row.header_field));
        item->setForeground(kColValue, EMailWarningColor(this));
        break;

      case RecipientMatch::kENCRYPTED_NOT_ADDRESSED:
        // Ordinary and usually correct: a blind copy, an archive key, or the
        // sender's own key. Reported, not flagged.
        item->setText(kColItem, row.info.uid.isEmpty()
                                    ? tr("Key %1").arg(row.info.key_id)
                                    : row.info.uid);
        item->setText(kColValue,
                      tr("encrypted to, but not in the visible headers"));
        item->setForeground(kColValue, muted);
        break;

      case RecipientMatch::kHIDDEN_RECIPIENT:
        item->setText(kColItem, tr("Hidden recipient"));
        item->setText(kColValue,
                      tr("the sender chose not to record who this is"));
        item->setForeground(kColValue, muted);
        break;
    }

    if (!row.info.fingerprint.isEmpty()) {
      auto* fpr = new QTreeWidgetItem(item);
      fpr->setText(kColItem, tr("Fingerprint"));
      fpr->setText(kColValue, row.info.fingerprint);
      fpr->setForeground(kColItem, muted);
    }

    if (row.info.algo_is_primary_key) {
      auto* note = new QTreeWidgetItem(item);
      note->setText(kColItem, tr("Note"));
      note->setText(kColValue,
                    tr("the engine reported the primary key rather than the "
                       "encryption subkey actually used"));
      note->setForeground(kColValue, muted);
    }
  }
}

void EMailSecurityView::add_key_section(const QStringList& addresses,
                                        int channel) {
  if (addresses.isEmpty()) return;

  auto* group = add_group(tr("Keys for these addresses"));
  const auto muted = EMailMutedColor(this);

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
      item->setForeground(kColValue, EMailWarningColor(this));
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
      key_item->setForeground(kColValue, brief.usability == kUSABLE
                                             ? EMailAccentColor(this, true)
                                             : EMailWarningColor(this));

      auto* fpr = new QTreeWidgetItem(key_item);
      fpr->setText(kColItem, tr("Fingerprint"));
      fpr->setText(kColValue, QString::fromUtf8(brief.fingerprint));
      fpr->setForeground(kColItem, muted);

      // Identity binding, kept as its own row. A usable key carrying this
      // address only on a secondary or revoked UID is exactly the case a
      // single combined verdict would bury.
      auto* identity = new QTreeWidgetItem(key_item);
      identity->setText(kColItem, tr("Identity"));
      if (brief.matched_uid_revoked != 0) {
        identity->setText(
            kColValue, tr("this address is on a REVOKED user ID of the key"));
        identity->setForeground(kColValue, EMailWarningColor(this));
      } else if (brief.matched_uid_is_primary != 0) {
        identity->setText(kColValue, tr("this address is the key's primary "
                                        "user ID"));
        identity->setForeground(kColValue, EMailAccentColor(this, true));
      } else {
        identity->setText(kColValue,
                          tr("this address is a secondary user ID of the key"));
        identity->setForeground(kColValue, muted);
      }

      if (brief.can_encrypt == 0) {
        auto* note = new QTreeWidgetItem(key_item);
        note->setText(kColItem, tr("Note"));
        note->setText(kColValue, tr("this key cannot be used for encryption"));
        note->setForeground(kColValue, EMailWarningColor(this));
      }
    }

    GFGpgFreeKeyBriefs(briefs, count);
  }
}

void EMailSecurityView::add_findings_section(
    const QList<EMailFinding>& findings) {
  if (findings.isEmpty()) return;

  auto* group = add_group(tr("What stands out"));

  for (const auto& finding : findings) {
    auto* item = new QTreeWidgetItem(group);
    item->setText(kColItem, finding.title);
    item->setText(kColValue, finding.detail);

    switch (finding.level) {
      case EMailFindingLevel::kRISK:
        // The only level painted as danger. Reserved for things that are
        // actually wrong or built to deceive.
        item->setForeground(kColItem, EMailThemeColor(this, &GFUIDangerColor));
        break;
      case EMailFindingLevel::kWARN:
        item->setForeground(kColItem, EMailWarningColor(this));
        break;
      case EMailFindingLevel::kNOTE:
        item->setForeground(kColItem, EMailMutedColor(this));
        break;
    }
  }
}

void EMailSecurityView::SetMessage(EMailSecurityState state,
                                   const QList<EMailSignatureRegion>& regions,
                                   const QList<EMailSignatureResult>& results,
                                   const QList<EMailRecipientRow>& recipients,
                                   const QStringList& addresses, int channel,
                                   const QList<EMailFinding>& findings) {
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

  auto palette = headline_->palette();
  palette.setColor(QPalette::WindowText,
                   state == EMailSecurityState::kMALFORMED_PGP
                       ? EMailWarningColor(this)
                       : EMailMutedColor(this));
  headline_->setPalette(palette);

  // Findings first: if something about this message is deceptive, that is the
  // thing to read before any of the detail below it.
  add_findings_section(findings);
  add_signature_section(regions, results);
  add_recipient_section(recipients);
  add_key_section(addresses, channel);

  tree_->expandAll();
}
