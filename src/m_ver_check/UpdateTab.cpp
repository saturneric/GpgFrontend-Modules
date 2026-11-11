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

#include "AppImageUpdate.h"
#include "GFModule.h"
#include "UpdateChecker.h"
#include "UpdatePresentation.h"
#include "VersionCheckingModule.h"

namespace {

constexpr auto kDownloadPage =
    "https://www.gpgfrontend.bktus.com/overview/downloads/";

/// The update this process has put on disk, by its sha256: a dialog opened
/// again offers the restart, not the same download a second time.
QString g_installed_sha256;  // NOLINT(*-avoid-non-const-global-variables)

auto ToneIcon(const QStyle* style, UpdateTone tone) -> QIcon {
  switch (tone) {
    case UpdateTone::kGood:
      return style->standardIcon(QStyle::SP_DialogApplyButton);
    case UpdateTone::kUpdate:
      // Good news, not a warning: the theme's update icon where there is
      // one, the information sign otherwise.
      return QIcon::fromTheme(
          QStringLiteral("system-software-update"),
          style->standardIcon(QStyle::SP_MessageBoxInformation));
    case UpdateTone::kAttention:
      return style->standardIcon(QStyle::SP_MessageBoxWarning);
    case UpdateTone::kNeutral:
      break;
  }
  return style->standardIcon(QStyle::SP_MessageBoxInformation);
}

/// The Host's own switch, the one the setup wizard and this module's
/// settings page share. Off, the dialog still offers the button: pressing
/// it is the user asking. Absent means the wizard has not asked yet, which
/// is not a yes: the startup check reads it the same way.
auto UpdateCheckProhibited() -> bool {
  return gf::sdk::Setting(GFModuleSdkContext(), GF_SETTING_HOST,
                          kProhibitUpdateCheckKey, true)
      .toBool();
}

/// The icon of a step's mark; none for a step that did not run or does not
/// apply, whose text says so.
auto MarkIcon(const QStyle* style, DetailRow::Mark mark) -> QIcon {
  switch (mark) {
    case DetailRow::Mark::kVerified:
      return style->standardIcon(QStyle::SP_DialogApplyButton);
    case DetailRow::Mark::kFailed:
      return style->standardIcon(QStyle::SP_MessageBoxCritical);
    case DetailRow::Mark::kInfo:
      return style->standardIcon(QStyle::SP_MessageBoxInformation);
    case DetailRow::Mark::kNotRun:
    case DetailRow::Mark::kNotApplicable:
    case DetailRow::Mark::kNone:
      break;
  }
  return {};
}

/// A label for text from the server or the build: never read as markup.
auto PlainLabel(const QString& text, QWidget* parent) -> QLabel* {
  auto* label = new QLabel(text, parent);
  label->setTextFormat(Qt::PlainText);
  label->setWordWrap(true);
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  return label;
}

/// A hash, a key or a file name can break after a dash or a dot, so a long
/// one wraps instead of widening the dialog.
auto Breakable(QString text) -> QString {
  text.replace(u'-', QStringLiteral("-\u200b"));
  return text;
}

auto CopyButton(const QString& value, QWidget* parent) -> QToolButton* {
  auto* button = new QToolButton(parent);
  button->setText(QCoreApplication::translate("UpdateTab", "Copy"));
  button->setToolTip(value);
  button->setAutoRaise(true);
  QObject::connect(button, &QToolButton::clicked, button,
                   [value] { QGuiApplication::clipboard()->setText(value); });
  return button;
}

/// The rows of the details: a mark (when @p marked), the title, its lines,
/// and a Copy button for a row that offers a value.
auto RowsWidget(const QList<DetailRow>& rows, bool marked, QWidget* parent)
    -> QWidget* {
  auto* widget = new QWidget(parent);
  auto* grid = new QGridLayout(widget);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setColumnStretch(1, 1);
  const auto size = widget->style()->pixelMetric(QStyle::PM_SmallIconSize);
  for (int i = 0; i < rows.size(); ++i) {
    const auto& row = rows[i];
    if (marked) {
      auto* icon = new QLabel(widget);
      icon->setFixedWidth(size);
      icon->setPixmap(MarkIcon(widget->style(), row.mark).pixmap(size, size));
      grid->addWidget(icon, i, 0, Qt::AlignTop);
    }
    auto* title = new QLabel(row.title, widget);
    auto font = title->font();
    font.setBold(true);
    title->setFont(font);
    auto* lines = PlainLabel(Breakable(row.lines.join(u'\n')), widget);
    auto* text = new QVBoxLayout();
    text->setSpacing(0);
    text->addWidget(title);
    text->addWidget(lines);
    grid->addLayout(text, i, 1);
    if (!row.copy.isEmpty()) {
      grid->addWidget(CopyButton(row.copy, widget), i, 2, Qt::AlignTop);
    }
  }
  return widget;
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

  auto* text_column = new QVBoxLayout();
  text_column->setSpacing(4);
  text_column->addWidget(headline_);
  text_column->addWidget(detail_);
  text_column->addWidget(notice_);

  auto* header = new QHBoxLayout();
  header->setSpacing(12);
  header->addWidget(icon_, 0, Qt::AlignTop);
  header->addLayout(text_column, 1);

  // This build, and what it is offered, side by side.
  build_box_ = new QGroupBox(tr("This build"), this);
  build_text_ = PlainLabel({}, build_box_);
  (new QVBoxLayout(build_box_))->addWidget(build_text_);

  offer_box_ = new QGroupBox(this);
  offer_text_ = PlainLabel({}, offer_box_);
  copy_hash_ = new QToolButton(offer_box_);
  copy_hash_->setText(tr("Copy"));
  copy_hash_->setAutoRaise(true);
  connect(copy_hash_, &QToolButton::clicked, this, [this] {
    QGuiApplication::clipboard()->setText(view_.offer_sha256);
  });
  auto* offer_layout = new QVBoxLayout(offer_box_);
  offer_layout->addWidget(offer_text_);
  offer_layout->addWidget(copy_hash_, 0, Qt::AlignLeft);

  auto* blocks = new QHBoxLayout();
  blocks->addWidget(build_box_, 1);
  blocks->addWidget(offer_box_, 1);

  notes_ = new QTextBrowser(this);
  // Notes are text the release server signed, but a link in them is still
  // a link: it opens only to a host release/product.json allows, and never
  // inside the browser itself.
  notes_->setOpenLinks(false);
  notes_->setOpenExternalLinks(false);
  // A few lines in the page; the rest in a window of their own, so the
  // page never scrolls inside itself.
  notes_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  notes_->hide();
  connect(notes_, &QTextBrowser::anchorClicked, this, [](const QUrl& url) {
    if (IsAllowedNotesLink(url, NotesLinkHosts())) {
      QDesktopServices::openUrl(url);
    }
  });
  full_notes_ = new QLabel(
      QStringLiteral("<a href=\"#\">%1</a>").arg(tr("Full release notes")),
      this);
  full_notes_->hide();
  connect(full_notes_, &QLabel::linkActivated, this,
          &UpdateTab::show_full_notes);

  check_btn_ = new QPushButton(this);
  check_btn_->setIcon(QIcon::fromTheme("view-refresh"));

  download_btn_ = new QPushButton(this);
  download_btn_->setDefault(true);

  auto* actions = new QHBoxLayout();
  actions->addStretch();
  actions->addWidget(check_btn_);
  actions->addWidget(download_btn_);

  // Only where it changes anything: the nightly identity follows its own
  // channel (main's nightly, or a release branch's betas) either way.
  beta_ = new QCheckBox(tr("Also offer beta versions"), this);
  beta_->setToolTip(
      tr("Test builds of the next fix release, before it is out. They are "
         "less tested than releases."));
  beta_->setChecked(FollowBetaSetting());
  beta_->setVisible(!checker_.isNull() && !checker_->IsNightly());

  // One line on the update information, and its details on request.
  verified_icon_ = new QLabel(this);
  verification_ = new QLabel(this);
  verification_->setWordWrap(true);
  details_toggle_ = new QToolButton(this);
  details_toggle_->setText(tr("Details"));
  details_toggle_->setCheckable(true);
  details_toggle_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  details_toggle_->setAutoRaise(true);
  auto* verification_line = new QHBoxLayout();
  verification_line->addWidget(verified_icon_, 0, Qt::AlignTop);
  verification_line->addWidget(verification_, 1);
  verification_line->addWidget(details_toggle_, 0, Qt::AlignTop);

  details_ = new QWidget(this);
  device_box_ = new QGroupBox(tr("Checked on this device"), details_);
  new QVBoxLayout(device_box_);
  host_box_ = new QGroupBox(
      tr("Checked by the release server before publishing"), details_);
  new QVBoxLayout(host_box_);
  security_link_ = new QLabel(details_);
  security_link_->setOpenExternalLinks(true);
  auto* copy_report = new QPushButton(tr("Copy report"), details_);
  connect(copy_report, &QPushButton::clicked, this,
          [this] { QGuiApplication::clipboard()->setText(ReportText(view_)); });
  auto* details_footer = new QHBoxLayout();
  details_footer->addWidget(security_link_, 1);
  details_footer->addWidget(copy_report);
  auto* details_layout = new QVBoxLayout(details_);
  details_layout->setContentsMargins(0, 0, 0, 0);
  details_layout->addWidget(device_box_);
  details_layout->addWidget(host_box_);
  details_layout->addLayout(details_footer);

  const bool open = DetailsOpenSetting();
  details_toggle_->setChecked(open);
  details_toggle_->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
  details_->setVisible(open);

  auto* layout = new QVBoxLayout(this);
  layout->addLayout(header);
  layout->addSpacing(8);
  layout->addLayout(blocks);
  layout->addWidget(notes_);
  layout->addWidget(full_notes_, 0, Qt::AlignLeft);
  layout->addLayout(actions);
  layout->addWidget(beta_, 0, Qt::AlignLeft);
  layout->addSpacing(8);
  layout->addLayout(verification_line);
  layout->addWidget(details_);
  layout->addStretch();

  // Wide enough that a headline wraps at most once; the height follows the
  // content (see fit_window()).
  setMinimumWidth(fontMetrics().averageCharWidth() * 64);

  connect(details_toggle_, &QToolButton::toggled, this, [this](bool open) {
    details_toggle_->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    details_->setVisible(open);
    SetDetailsOpenSetting(open);
    fit_window();
  });

  connect(check_btn_, &QPushButton::clicked, this, [this] {
    if (!checker_.isNull()) checker_->Start(CheckMode::kForce);
  });

  connect(download_btn_, &QPushButton::clicked, this,
          &UpdateTab::on_main_button);

  // Applies to the next check, and makes the last result stale: the page
  // asks again at once where checking is allowed.
  connect(beta_, &QCheckBox::clicked, this, [this](bool checked) {
    SetFollowBetaSetting(checked);
    if (!UpdateCheckProhibited() && !checker_.isNull()) {
      checker_->Start(CheckMode::kIfStale);
    }
  });

  if (!checker_.isNull()) {
    connect(checker_, &UpdateChecker::Changed, this, [this] {
      // An update found while the page is open has been seen.
      if (isVisible()) checker_->Acknowledge();
      render();
    });
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
  auto view = PresentUpdate(checker_->State(), QDateTime::currentDateTime(),
                            checker_->GetBuild());
  // In place only where this machine allows it: an AppImage of the nightly
  // application, in a place the user can write.
  view.installable = view.installable && !SelfUpdateImage().isEmpty();
  if (install_.stage == InstallState::Stage::kIdle && view.installable &&
      view.download_sha256 == g_installed_sha256) {
    install_.stage = InstallState::Stage::kInstalled;
  }
  apply(PresentInstall(view, install_));
}

void UpdateTab::on_main_button() {
  switch (view_.action) {
    case UpdateView::Action::kOpen:
      QDesktopServices::openUrl(QUrl(view_.download_url.isEmpty()
                                         ? QString::fromLatin1(kDownloadPage)
                                         : view_.download_url));
      break;
    case UpdateView::Action::kInstall:
      install();
      break;
    case UpdateView::Action::kRestart:
      if (!RequestRestart()) {
        install_.can_restart = false;
        render();
      }
      break;
  }
}

void UpdateTab::install() {
  if (!installer_.isNull()) return;
  const auto image = SelfUpdateImage();
  if (image.isEmpty()) return;

  install_ = {};
  install_.stage = InstallState::Stage::kDownloading;
  installer_ = MakeAppImageInstaller(
      image,
      {QUrl(view_.download_url), view_.download_sha256, view_.download_size},
      this);
  connect(installer_, &AppImageInstaller::Progress, this,
          [this](qint64 received, qint64 total) {
            install_.received = received;
            install_.total = total;
            render();
          });
  const auto sha256 = view_.download_sha256;
  connect(installer_, &AppImageInstaller::Finished, this,
          [this, sha256](bool ok, const QString& error) {
            install_.stage = ok ? InstallState::Stage::kInstalled
                                : InstallState::Stage::kFailed;
            install_.error = error;
            if (ok) g_installed_sha256 = sha256;
            if (auto* done = installer_.data(); done != nullptr) {
              installer_.clear();  // "Try again" may start another at once
              done->deleteLater();
            }
            render();
          });
  render();
  installer_->Start();
}

void UpdateTab::fit_window() {
  // Deferred: the layout settles the new content first. The frame is the
  // window; this page fills it, so fitting the window fits the page. It
  // keeps its place on screen, and overrides a size remembered from a page
  // that showed something else.
  QTimer::singleShot(0, this, [this] {
    if (isVisible()) window()->adjustSize();
  });
}

void UpdateTab::fit_notes() {
  // Deferred like fit_window(): the document has its width only once laid
  // out. At most ten lines; a link to the rest when there is more.
  QTimer::singleShot(0, this, [this] {
    if (!notes_->isVisible()) return;
    const int cap = notes_->fontMetrics().lineSpacing() * 10;
    const int height = static_cast<int>(notes_->document()->size().height()) +
                       2 * notes_->frameWidth();
    notes_->setFixedHeight(std::min(height, cap));
    full_notes_->setVisible(height > cap);
    fit_window();
  });
}

void UpdateTab::show_full_notes() {
  auto* dialog = new QDialog(this);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowTitle(tr("Release notes"));
  auto* browser = new QTextBrowser(dialog);
  browser->setOpenLinks(false);
  browser->setMarkdown(view_.notes);
  RestrictNotesLinks(browser->document(), NotesLinkHosts());
  connect(browser, &QTextBrowser::anchorClicked, dialog, [](const QUrl& url) {
    if (IsAllowedNotesLink(url, NotesLinkHosts())) {
      QDesktopServices::openUrl(url);
    }
  });
  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
  connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
  auto* layout = new QVBoxLayout(dialog);
  layout->addWidget(browser);
  layout->addWidget(buttons);
  dialog->resize(fontMetrics().averageCharWidth() * 80,
                 fontMetrics().lineSpacing() * 30);
  dialog->show();
}

void UpdateTab::apply(const UpdateView& view) {
  const auto icon_size = style()->pixelMetric(QStyle::PM_MessageBoxIconSize);
  icon_->setPixmap(ToneIcon(style(), view.tone).pixmap(icon_size, icon_size));

  headline_->setText(view.headline);
  detail_->setText(view.detail);
  detail_->setVisible(!view.detail.isEmpty());
  notice_->setText(view.notice);
  notice_->setVisible(!view.notice.isEmpty());

  QStringList build{view.build_version, view.build_kind};
  if (!view.build_commit.isEmpty()) {
    build << tr("Commit %1").arg(view.build_commit);
  }
  if (!view.build_official.isEmpty()) build << view.build_official;
  build_text_->setText(build.join(u'\n'));
  build_box_->setVisible(!view.build_version.isEmpty());

  QStringList offer;
  if (view.show_offer) {
    offer_box_->setTitle(tr("Offered"));
    offer << view.offer_version;
    if (!view.offer_file.isEmpty()) offer << Breakable(view.offer_file);
    if (!view.offer_sha256.isEmpty()) {
      offer << tr("SHA-256 %1").arg(ShortHash(view.offer_sha256));
    }
  } else {
    offer_box_->setTitle(tr("Available"));
    offer << view.newest;
  }
  if (!view.newer_beta.isEmpty()) offer << view.newer_beta;
  offer.removeAll(QString());
  offer_text_->setText(offer.join(u'\n'));
  offer_text_->setToolTip(view.offer_sha256);
  copy_hash_->setVisible(view.show_offer && !view.offer_sha256.isEmpty());
  offer_box_->setVisible(!offer.isEmpty());

  check_btn_->setText(view.check_label);
  check_btn_->setEnabled(view.check_enabled);
  // Once installed, checking again has nothing to add before the restart.
  check_btn_->setVisible(view.action != UpdateView::Action::kRestart);

  view_ = view;
  download_btn_->setText(view.download_label);
  download_btn_->setEnabled(view.download_enabled);
  download_btn_->setVisible(view.show_download);
  download_btn_->setIcon(QIcon::fromTheme(
      view.action == UpdateView::Action::kRestart   ? "view-refresh"
      : view.action == UpdateView::Action::kInstall ? "system-software-update"
                                                    : "download"));

  notes_->setVisible(view.show_notes);
  full_notes_->hide();
  if (view.show_notes) {
    notes_->setMarkdown(view.notes);
    RestrictNotesLinks(notes_->document(), NotesLinkHosts());
    fit_notes();
  } else {
    notes_->clear();
  }

  const auto small = style()->pixelMetric(QStyle::PM_SmallIconSize);
  verified_icon_->setPixmap(
      ToneIcon(style(), view.verification_tone).pixmap(small, small));
  verification_->setText(view.verification);

  // The rows are rebuilt: each check may have run other steps.
  delete device_rows_;
  device_rows_ =
      view.device_rows.isEmpty()
          ? PlainLabel(tr("The details appear after the next check."),
                       device_box_)
          : RowsWidget(view.device_rows, true, device_box_);
  device_box_->layout()->addWidget(device_rows_);
  delete host_rows_;
  host_rows_ = RowsWidget(view.host_rows, false, host_box_);
  host_box_->layout()->addWidget(host_rows_);
  // Unmarked: what the server checked is never shown as checked here.
  host_box_->setVisible(!view.host_rows.isEmpty());
  security_link_->setText(
      view.security_url.isEmpty()
          ? QString()
          : QStringLiteral("<a href=\"%1\">%2</a>")
                .arg(view.security_url.toHtmlEscaped(),
                     tr("How to verify the download yourself")));

  fit_window();
}

void UpdateTab::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  // Opening the page is seeing whatever update it advertises: the badge on
  // the menu entry goes, until a still newer release turns up.
  if (!checker_.isNull()) checker_->Acknowledge();
  render();

  if (!UpdateCheckProhibited() && !checker_.isNull()) {
    checker_->Start(CheckMode::kIfStale);
  }
}
