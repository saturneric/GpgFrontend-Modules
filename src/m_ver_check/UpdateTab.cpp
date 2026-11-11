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
#include "UpdateChecker.h"
#include "UpdatePresentation.h"
#include "VersionCheckingModule.h"

namespace {

constexpr auto kDownloadPage =
    "https://www.gpgfrontend.bktus.com/overview/downloads/";

/// The update this process has put on disk, by its sha256: a dialog opened
/// again offers the restart, not the same download a second time.
QString g_installed_sha256;  // NOLINT(*-avoid-non-const-global-variables)

/// One icon per tone, for the header and the status line alike.
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
    case UpdateTone::kError:
      return style->standardIcon(QStyle::SP_MessageBoxCritical);
    case UpdateTone::kNeutral:
      break;
  }
  return style->standardIcon(QStyle::SP_MessageBoxInformation);
}

/// A label for text from the server or the build: never read as markup.
auto PlainLabel(const QString& text, QWidget* parent) -> QLabel* {
  auto* label = new QLabel(text, parent);
  label->setTextFormat(Qt::PlainText);
  label->setWordWrap(true);
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  return label;
}

/// A hash, a key or a file name can break after a dash, so a long one
/// wraps instead of widening the dialog.
auto Breakable(QString text) -> QString {
  text.replace(u'-', QStringLiteral("-\u200b"));
  return text;
}

auto MonoLabel(const QString& text, const QString& full, QWidget* parent)
    -> QLabel* {
  auto* label = PlainLabel(text, parent);
  label->setWordWrap(false);  // short already: a hash cut in two misleads
  label->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  label->setToolTip(full);
  return label;
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

/// A small heading over a group of rows.
auto Caption(const QString& text, QWidget* parent) -> QLabel* {
  auto* label = new QLabel(text, parent);
  auto font = label->font();
  font.setBold(true);
  label->setFont(font);
  return label;
}

auto Separator(QWidget* parent) -> QFrame* {
  auto* line = new QFrame(parent);
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  return line;
}

/// The first column of a details row, the same width on every row so the
/// text lines up: a mark's icon, a dash for what did not run, a circle for
/// what was not checked, nothing for the release server's rows.
auto MarkLabel(DetailRow::Mark mark, QWidget* parent) -> QLabel* {
  auto* label = new QLabel(parent);
  const auto size = parent->style()->pixelMetric(QStyle::PM_SmallIconSize);
  label->setFixedWidth(size);
  label->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
  label->setForegroundRole(QPalette::PlaceholderText);
  switch (mark) {
    case DetailRow::Mark::kVerified:
      label->setPixmap(parent->style()
                           ->standardIcon(QStyle::SP_DialogApplyButton)
                           .pixmap(size, size));
      break;
    case DetailRow::Mark::kFailed:
      label->setPixmap(parent->style()
                           ->standardIcon(QStyle::SP_MessageBoxCritical)
                           .pixmap(size, size));
      break;
    case DetailRow::Mark::kNotRun:
      label->setText(QStringLiteral("\u2013"));
      break;
    case DetailRow::Mark::kNotChecked:
      label->setText(QStringLiteral("\u25CB"));
      break;
    case DetailRow::Mark::kNone:
      break;
  }
  return label;
}

/// A details row's text: the title, then its value in the secondary
/// colour and its key ids or hashes in the fixed-width font, on the same
/// line where they fit.
auto RowText(const DetailRow& row, const QPalette& palette) -> QString {
  auto text = row.title.toHtmlEscaped();
  const auto muted = palette.color(QPalette::PlaceholderText).name();
  if (!row.value.isEmpty()) {
    text += QStringLiteral(" &nbsp;<span style=\"color:%1\">%2</span>")
                .arg(muted, row.value.toHtmlEscaped());
  }
  if (!row.mono.isEmpty()) {
    text += QStringLiteral(" <span style=\"font-family:'%1'\">%2</span>")
                .arg(QFontDatabase::systemFont(QFontDatabase::FixedFont)
                         .family()
                         .toHtmlEscaped(),
                     row.mono.toHtmlEscaped());
  }
  return text;
}

/// Empties a form, its labels and values with it.
void ClearForm(QFormLayout* form) {
  while (form->rowCount() > 0) form->removeRow(0);
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

  // This build, and what it is offered, side by side: plain label/value
  // rows, no frames.
  auto* build_column = new QWidget(this);
  build_caption_ = Caption(tr("This build"), build_column);
  build_form_ = new QFormLayout();
  auto* build_layout = new QVBoxLayout(build_column);
  build_layout->setContentsMargins(0, 0, 0, 0);
  build_layout->addWidget(build_caption_);
  build_layout->addLayout(build_form_);
  build_layout->addStretch();

  offer_column_ = new QWidget(this);
  offer_form_ = new QFormLayout();
  auto* offer_layout = new QVBoxLayout(offer_column_);
  offer_layout->setContentsMargins(0, 0, 0, 0);
  offer_layout->addWidget(Caption(tr("Offered"), offer_column_));
  offer_layout->addLayout(offer_form_);
  offer_layout->addStretch();

  auto* blocks = new QHBoxLayout();
  blocks->setSpacing(24);
  blocks->addWidget(build_column, 1);
  blocks->addWidget(offer_column_, 1);

  // A beta the user did not ask for: the setting is in Settings > Updates.
  beta_hint_ = PlainLabel({}, this);
  beta_hint_->setForegroundRole(QPalette::PlaceholderText);
  beta_hint_->hide();

  notes_ = new QTextBrowser(this);
  // Notes are text the release server signed, but a link in them is still
  // a link: it opens only to a host release/product.json allows, and never
  // inside the browser itself.
  notes_->setOpenLinks(false);
  notes_->setOpenExternalLinks(false);
  // The one part of the page that scrolls, and takes what height there is:
  // on the window's own background, one light frame, at least five lines.
  notes_->setFrameShape(QFrame::StyledPanel);
  notes_->setFrameShadow(QFrame::Plain);
  notes_->setMinimumHeight(notes_->fontMetrics().lineSpacing() * 5 +
                           2 * notes_->frameWidth());
  fill_notes_palette();
  notes_->hide();
  connect(notes_, &QTextBrowser::anchorClicked, this, [](const QUrl& url) {
    if (IsAllowedNotesLink(url, NotesLinkHosts())) {
      QDesktopServices::openUrl(url);
    }
  });

  // One short line on the update information; its details on request.
  status_icon_ = new QLabel(this);
  status_text_ = new QLabel(this);
  details_toggle_ = new QToolButton(this);
  details_toggle_->setText(tr("Details"));
  details_toggle_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  details_toggle_->setAutoRaise(true);
  auto* status_line = new QHBoxLayout();
  status_line->addWidget(status_icon_);
  status_line->addWidget(status_text_, 1);
  status_line->addWidget(details_toggle_);

  details_ = new QWidget(this);
  details_layout_ = new QVBoxLayout(details_);
  details_layout_->setContentsMargins(0, 0, 0, 0);

  // One row of buttons.
  copy_report_ = new QPushButton(tr("Copy report"), this);
  connect(copy_report_, &QPushButton::clicked, this,
          [this] { QGuiApplication::clipboard()->setText(ReportText(view_)); });
  download_btn_ = new QPushButton(this);
  download_btn_->setDefault(true);
  check_btn_ = new QPushButton(this);
  auto* close = new QPushButton(tr("Close"), this);
  connect(close, &QPushButton::clicked, this, [this] { window()->close(); });
  auto* buttons = new QHBoxLayout();
  buttons->addWidget(copy_report_);
  buttons->addStretch();
  buttons->addWidget(download_btn_);
  buttons->addWidget(check_btn_);
  buttons->addWidget(close);

  const bool open = DetailsOpenSetting();
  details_toggle_->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
  details_->setVisible(open);
  copy_report_->setVisible(open);

  auto* layout = new QVBoxLayout(this);
  layout->addLayout(header);
  layout->addSpacing(8);
  layout->addLayout(blocks);
  layout->addWidget(beta_hint_);
  layout->addWidget(notes_, 1);
  layout->addSpacing(4);
  layout->addWidget(Separator(this));
  layout->addLayout(status_line);
  layout->addWidget(details_);
  layout->addWidget(Separator(this));
  layout->addLayout(buttons);
  layout->addStretch();

  // Wide enough that the status line and the two columns fit unwrapped;
  // the height follows the content (see fit_window()).
  setMinimumWidth(fontMetrics().averageCharWidth() * 92);

  connect(details_toggle_, &QToolButton::clicked, this, [this] {
    const bool open = !details_->isVisible();
    details_toggle_->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    details_->setVisible(open);
    copy_report_->setVisible(open);
    SetDetailsOpenSetting(open);
    fit_window();
  });

  connect(check_btn_, &QPushButton::clicked, this, [this] {
    if (!checker_.isNull()) checker_->Start(CheckMode::kForce);
  });

  connect(download_btn_, &QPushButton::clicked, this,
          &UpdateTab::on_main_button);

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
    if (!isVisible()) return;
    auto* frame = window();
    frame->adjustSize();
    // Wrapped text needs the height for the width it got, which
    // adjustSize() does not always ask for.
    int height = frame->hasHeightForWidth()
                     ? frame->heightForWidth(frame->width())
                     : frame->height();
    // Never taller than most of the screen: the release notes give way down
    // to their few lines, they scroll; the details never do.
    if (const auto* screen = frame->screen(); screen != nullptr) {
      const int give = notes_->isVisible() ? notes_->sizeHint().height() -
                                                 notes_->minimumHeight()
                                           : 0;
      height = std::max(
          std::min(height, screen->availableGeometry().height() * 4 / 5),
          height - std::max(give, 0));
    }
    frame->resize(frame->width(), height);
  });
}

void UpdateTab::apply(const UpdateView& view) {
  view_ = view;
  const auto icon_size = style()->pixelMetric(QStyle::PM_MessageBoxIconSize);
  icon_->setPixmap(ToneIcon(style(), view.tone).pixmap(icon_size, icon_size));

  headline_->setText(view.headline);
  detail_->setText(view.detail);
  detail_->setVisible(!view.detail.isEmpty());
  notice_->setText(view.notice);
  notice_->setVisible(!view.notice.isEmpty());

  ClearForm(build_form_);
  build_form_->addRow(tr("Version"), PlainLabel(view.build_version, this));
  build_form_->addRow(tr("Type"), PlainLabel(view.build_kind, this));
  if (!view.build_commit.isEmpty()) {
    build_form_->addRow(tr("Commit"),
                        MonoLabel(view.build_commit, view.build_commit, this));
  }
  build_form_->addRow(tr("Official build"),
                      PlainLabel(view.build_official, this));
  if (!view.newest.isEmpty()) {
    build_form_->addRow(view.newest_label, PlainLabel(view.newest, this));
  }

  ClearForm(offer_form_);
  offer_column_->setVisible(view.show_offer);
  if (view.show_offer) {
    offer_form_->addRow(tr("Version"), PlainLabel(view.offer_version, this));
    if (!view.offer_file.isEmpty()) {
      offer_form_->addRow(tr("File"),
                          PlainLabel(Breakable(view.offer_file), this));
    }
    if (!view.offer_size.isEmpty()) {
      offer_form_->addRow(tr("Size"), PlainLabel(view.offer_size, this));
    }
    if (!view.offer_sha256.isEmpty()) {
      auto* hash = new QHBoxLayout();
      hash->addWidget(
          MonoLabel(ShortHash(view.offer_sha256), view.offer_sha256, this));
      hash->addWidget(CopyButton(view.offer_sha256, this));
      hash->addStretch();
      offer_form_->addRow(tr("SHA-256"), hash);
    }
  }
  beta_hint_->setText(view.newer_beta);
  beta_hint_->setVisible(!view.newer_beta.isEmpty());

  notes_->setVisible(view.show_notes);
  if (view.show_notes) {
    notes_->setMarkdown(view.notes);
    RestrictNotesLinks(notes_->document(), NotesLinkHosts());
    TameNotesHeadings(notes_->document(), notes_->font().pointSizeF());
    // The platform's usual list indent, not the document's wide default.
    notes_->document()->setIndentWidth(
        notes_->fontMetrics().averageCharWidth() * 4);
  } else {
    notes_->clear();
  }

  const auto small = style()->pixelMetric(QStyle::PM_SmallIconSize);
  status_icon_->setPixmap(
      ToneIcon(style(), view.verification_tone).pixmap(small, small));
  status_text_->setText(view.verification);
  fill_details(view);

  download_btn_->setText(view.download_label);
  download_btn_->setEnabled(view.download_enabled);
  download_btn_->setVisible(view.show_download);
  download_btn_->setIcon(QIcon::fromTheme(
      view.action == UpdateView::Action::kRestart   ? "view-refresh"
      : view.action == UpdateView::Action::kInstall ? "system-software-update"
                                                    : "download"));
  check_btn_->setText(view.check_label);
  check_btn_->setEnabled(view.check_enabled);
  // Only where asking again can help; once installed, a restart comes
  // first.
  check_btn_->setVisible(view.show_check &&
                         view.action != UpdateView::Action::kRestart);

  fit_window();
}

void UpdateTab::fill_details(const UpdateView& view) {
  // Rebuilt whole: each check may have run other steps.
  while (auto* item = details_layout_->takeAt(0)) {
    delete item->widget();
    delete item;
  }
  const auto add_rows = [this](const QList<DetailRow>& rows) {
    auto* grid = new QGridLayout();
    grid->setColumnStretch(1, 1);
    grid->setVerticalSpacing(4);
    for (int i = 0; i < rows.size(); ++i) {
      const auto& row = rows[i];
      grid->addWidget(MarkLabel(row.mark, details_), i, 0);
      auto* text = new QLabel(RowText(row, palette()), details_);
      text->setWordWrap(true);
      text->setTextInteractionFlags(Qt::TextSelectableByMouse);
      text->setToolTip(row.full);
      grid->addWidget(text, i, 1);
      if (!row.copy.isEmpty()) {
        grid->addWidget(CopyButton(row.copy, details_), i, 2, Qt::AlignTop);
      }
    }
    auto* holder = new QWidget(details_);
    holder->setLayout(grid);
    grid->setContentsMargins(0, 0, 0, 0);
    details_layout_->addWidget(holder);
  };

  if (view.device_rows.isEmpty()) {
    auto* empty =
        PlainLabel(tr("The details appear after the next check."), details_);
    empty->setForegroundRole(QPalette::PlaceholderText);
    details_layout_->addWidget(empty);
    return;
  }
  details_layout_->addWidget(Caption(tr("Checked on this device"), details_));
  add_rows(view.device_rows);
  if (!view.host_rows.isEmpty()) {
    details_layout_->addWidget(
        Caption(tr("Checked by the release server"), details_));
    add_rows(view.host_rows);
  }
  if (!view.security_url.isEmpty()) {
    auto* link = new QLabel(QStringLiteral("<a href=\"%1\">%2</a>")
                                .arg(view.security_url.toHtmlEscaped(),
                                     tr("How to verify the download yourself")),
                            details_);
    link->setOpenExternalLinks(true);
    details_layout_->addWidget(link);
  }
}

void UpdateTab::changeEvent(QEvent* event) {
  QWidget::changeEvent(event);
  // The rows' secondary colour comes from the palette: a theme switch
  // redraws them.
  if (event->type() == QEvent::PaletteChange) {
    fill_notes_palette();
    fill_details(view_);
  }
}

void UpdateTab::fill_notes_palette() {
  // The notes sit on the window's background like the rest of the page;
  // a text view paints its own with Base.
  auto notes = notes_->palette();
  notes.setColor(QPalette::Base, palette().color(QPalette::Window));
  notes_->setPalette(notes);
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
