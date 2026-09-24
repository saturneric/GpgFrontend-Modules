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

#include "UpdateTab.h"

#include "GFModule.h"
#include "UpdateChecker.h"
#include "UpdatePresentation.h"
#include "VersionCheckingModule.h"

namespace {

constexpr auto kDownloadPage =
    "https://www.gpgfrontend.bktus.com/overview/downloads/";

auto ToneIcon(const QStyle* style, UpdateTone tone) -> QIcon {
  switch (tone) {
    case UpdateTone::kGood:
      return style->standardIcon(QStyle::SP_DialogApplyButton);
    case UpdateTone::kAttention:
      return style->standardIcon(QStyle::SP_MessageBoxWarning);
    case UpdateTone::kNeutral:
      break;
  }
  return style->standardIcon(QStyle::SP_MessageBoxInformation);
}

}  // namespace

UpdateTab::UpdateTab(QWidget* parent)
    : QWidget(parent), checker_(VersionChecker()) {
  icon_ = new QLabel(this);
  icon_->setAlignment(Qt::AlignTop);

  headline_ = new QLabel(this);
  headline_->setWordWrap(true);
  auto headline_font = headline_->font();
  headline_font.setBold(true);
  headline_font.setPointSizeF(headline_font.pointSizeF() * 1.25);
  headline_->setFont(headline_font);

  detail_ = new QLabel(this);
  detail_->setWordWrap(true);

  notice_ = new QLabel(this);
  notice_->setWordWrap(true);

  freshness_ = new QLabel(this);
  freshness_->setWordWrap(true);
  freshness_->setForegroundRole(QPalette::PlaceholderText);
  auto small_font = freshness_->font();
  small_font.setPointSizeF(small_font.pointSizeF() * 0.9);
  freshness_->setFont(small_font);

  auto* text_column = new QVBoxLayout();
  text_column->setSpacing(4);
  text_column->addWidget(headline_);
  text_column->addWidget(detail_);
  text_column->addWidget(notice_);
  text_column->addWidget(freshness_);

  auto* header = new QHBoxLayout();
  header->setSpacing(12);
  header->addWidget(icon_, 0, Qt::AlignTop);
  header->addLayout(text_column, 1);

  busy_ = new QProgressBar(this);
  busy_->setRange(0, 0);
  busy_->setTextVisible(false);
  busy_->setMaximumHeight(6);

  check_btn_ = new QPushButton(this);
  check_btn_->setIcon(QIcon::fromTheme("view-refresh"));

  download_btn_ = new QPushButton(this);
  download_btn_->setIcon(QIcon::fromTheme("download"));
  download_btn_->setDefault(true);

  auto* actions = new QHBoxLayout();
  actions->addWidget(busy_, 1);
  actions->addStretch();
  actions->addWidget(check_btn_);
  actions->addWidget(download_btn_);

  notes_toggle_ = new QToolButton(this);
  notes_toggle_->setText(tr("Release notes"));
  notes_toggle_->setCheckable(true);
  notes_toggle_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  notes_toggle_->setArrowType(Qt::RightArrow);
  notes_toggle_->setAutoRaise(true);

  notes_ = new QTextBrowser(this);
  notes_->setOpenExternalLinks(true);
  notes_->hide();

  auto* layout = new QVBoxLayout(this);
  layout->addLayout(header);
  layout->addSpacing(8);
  layout->addLayout(actions);
  layout->addSpacing(8);
  layout->addWidget(notes_toggle_, 0, Qt::AlignLeft);
  layout->addWidget(notes_, 1);
  layout->addStretch();

  connect(notes_toggle_, &QToolButton::toggled, this, [this](bool open) {
    notes_toggle_->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    notes_->setVisible(open);
  });

  connect(check_btn_, &QPushButton::clicked, this, [this] {
    if (!checker_.isNull()) checker_->Start(CheckMode::kForce);
  });

  connect(download_btn_, &QPushButton::clicked, this, [] {
    QDesktopServices::openUrl(QUrl(QString::fromLatin1(kDownloadPage)));
  });

  if (!checker_.isNull()) {
    connect(checker_, &UpdateChecker::Changed, this, &UpdateTab::render);
  }

  // Readable at once: a check that finished before this dialog existed has
  // left its answer in the checker, not in a signal already gone.
  render();
}

void UpdateTab::render() {
  if (checker_.isNull()) {
    UpdateView view;
    view.headline = tr("Update checking is unavailable");
    view.check_enabled = false;
    view.check_label = tr("Check now");
    apply(view);
    return;
  }
  apply(PresentUpdate(checker_->State(), QDateTime::currentDateTime()));
}

void UpdateTab::apply(const UpdateView& view) {
  const auto icon_size = style()->pixelMetric(QStyle::PM_MessageBoxIconSize);
  icon_->setPixmap(ToneIcon(style(), view.tone).pixmap(icon_size, icon_size));

  headline_->setText(view.headline);
  detail_->setText(view.detail);
  detail_->setVisible(!view.detail.isEmpty());
  notice_->setText(view.notice);
  notice_->setVisible(!view.notice.isEmpty());
  freshness_->setText(view.freshness);
  freshness_->setVisible(!view.freshness.isEmpty());

  busy_->setVisible(view.busy);
  check_btn_->setText(view.check_label);
  check_btn_->setEnabled(view.check_enabled);

  download_btn_->setText(view.download_label);
  download_btn_->setVisible(view.show_download);

  notes_toggle_->setVisible(view.show_notes);
  if (view.show_notes) {
    notes_->setMarkdown(view.notes);
    notes_->setVisible(notes_toggle_->isChecked());
  } else {
    notes_->clear();
    notes_->hide();
  }
}

void UpdateTab::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  render();

  // The Host's own switch, the one the setup wizard and this module's
  // settings page share. Off, the dialog still offers the button: pressing
  // it is the user asking.
  const auto prohibit = gf::sdk::Setting(GFModuleSdkContext(), GF_SETTING_HOST,
                                         "network/prohibit_update_check", false)
                            .toBool();
  if (!prohibit && !checker_.isNull()) checker_->Start(CheckMode::kIfStale);
}
