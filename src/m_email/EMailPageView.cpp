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

#include "EMailPageView.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "EMailHelper.h"
#include "GFModuleCommonUtils.hpp"

namespace {

// The application's own colours, reached through the SDK so this panel follows
// the user's theme and reads as part of the program rather than as a plug-in
// that picked its own greys.
auto ThemeColor(QWidget* w, uint32_t (*getter)(void*)) -> QColor {
  const auto rgba = getter(w);
  return rgba == 0 ? w->palette().color(QPalette::WindowText)
                   : QColor::fromRgba(rgba);
}

auto MutedColor(QWidget* w) -> QColor {
  return ThemeColor(w, &GFUIMutedTextColor);
}

auto AccentColor(QWidget* w, bool positive) -> QColor {
  const auto rgba = GFUIAccentColor(w, positive ? 1 : 0);
  return rgba == 0 ? w->palette().color(QPalette::WindowText)
                   : QColor::fromRgba(rgba);
}

/// A size written the way the rest of the application writes it.
auto HumanSize(qint64 bytes) -> QString {
  return UnStrDup(GFUIHumanSize(bytes));
}

constexpr int kColName = 0;
constexpr int kColType = 1;
constexpr int kColSize = 2;
constexpr int kColSigned = 3;

auto GuessMimeType(const QString& path) -> QString {
  static const QMap<QString, QString> kByExtension = {
      {"txt", "text/plain"},
      {"html", "text/html"},
      {"pdf", "application/pdf"},
      {"png", "image/png"},
      {"jpg", "image/jpeg"},
      {"jpeg", "image/jpeg"},
      {"gif", "image/gif"},
      {"zip", "application/zip"},
      {"asc", "application/pgp-keys"},
      {"eml", "message/rfc822"}};
  const auto suffix = QFileInfo(path).suffix().toLower();
  return kByExtension.value(suffix, "application/octet-stream");
}

}  // namespace

EMailPageView::EMailPageView(QWidget* parent) : QWidget(parent) { build_ui(); }

void EMailPageView::build_ui() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(8, 6, 8, 6);
  layout->setSpacing(6);

  auto* form = new QFormLayout();
  form->setLabelAlignment(Qt::AlignRight);
  form->setHorizontalSpacing(8);
  form->setVerticalSpacing(3);
  form->setContentsMargins(0, 0, 0, 0);

  from_edit_ = new QLineEdit(this);
  to_edit_ = new QLineEdit(this);
  cc_edit_ = new QLineEdit(this);
  bcc_edit_ = new QLineEdit(this);
  subject_edit_ = new QLineEdit(this);

  // Separator is shown rather than assumed: the old dialog explained it in a
  // tips label the user had to find first.
  const auto multi = tr("separate several addresses with \";\"");
  to_edit_->setPlaceholderText(multi);
  cc_edit_->setToolTip(multi);
  bcc_edit_->setToolTip(multi);

  // Captions are quieter than the values beside them, so the eye lands on the
  // addresses rather than on the word "From".
  const auto caption = [this](const QString& text) {
    auto* label = new QLabel(text, this);
    auto palette = label->palette();
    palette.setColor(QPalette::WindowText, MutedColor(this));
    label->setPalette(palette);
    return label;
  };

  form->addRow(caption(tr("From:")), from_edit_);

  // "To" carries the toggle for the two rows most messages never use, so the
  // common case is four rows rather than six. The toggle only shows and hides:
  // it used to clear the fields as well, which threw away addresses whenever
  // the row was collapsed by accident.
  auto* to_row = new QWidget(this);
  auto* to_row_layout = new QHBoxLayout(to_row);
  to_row_layout->setContentsMargins(0, 0, 0, 0);
  to_row_layout->setSpacing(6);
  to_row_layout->addWidget(to_edit_, 1);

  cc_bcc_toggle_ = new QToolButton(this);
  cc_bcc_toggle_->setText(tr("Cc/Bcc"));
  cc_bcc_toggle_->setCheckable(true);
  cc_bcc_toggle_->setAutoRaise(true);
  cc_bcc_toggle_->setToolTip(tr("Show carbon copy and blind carbon copy"));
  to_row_layout->addWidget(cc_bcc_toggle_);

  form->addRow(caption(tr("To:")), to_row);
  form->addRow(caption(tr("Cc:")), cc_edit_);
  form->addRow(caption(tr("Bcc:")), bcc_edit_);

  header_form_ = form;
  connect(cc_bcc_toggle_, &QToolButton::toggled, this,
          [this](bool on) { set_cc_bcc_visible(on); });
  set_cc_bcc_visible(false);

  // The subject is the message's title, so it carries the weight of one.
  auto subject_font = subject_edit_->font();
  subject_font.setBold(true);
  subject_edit_->setFont(subject_font);
  form->addRow(caption(tr("Subject:")), subject_edit_);

  layout->addLayout(form);

  // A hairline between the envelope and the message, so the two stop running
  // together into one undifferentiated column of boxes.
  auto* rule = new QFrame(this);
  rule->setFrameShape(QFrame::HLine);
  rule->setFrameShadow(QFrame::Plain);
  auto rule_palette = rule->palette();
  rule_palette.setColor(QPalette::WindowText,
                        ThemeColor(this, &GFUIBorderColor));
  rule->setPalette(rule_palette);
  rule->setFixedHeight(1);
  layout->addWidget(rule);

  body_edit_ = new QPlainTextEdit(this);
  body_edit_->setPlaceholderText(tr("Write your message here."));
  layout->addWidget(body_edit_, 1);

  attachment_heading_ = new QLabel(this);
  {
    auto palette = attachment_heading_->palette();
    palette.setColor(QPalette::WindowText, MutedColor(this));
    attachment_heading_->setPalette(palette);
    auto font = attachment_heading_->font();
    font.setPointSizeF(font.pointSizeF() * 0.92);
    attachment_heading_->setFont(font);
  }
  attachment_heading_->setVisible(false);
  layout->addWidget(attachment_heading_);

  attachment_list_ = new QTreeWidget(this);
  attachment_list_->setRootIsDecorated(false);
  attachment_list_->setAlternatingRowColors(true);
  attachment_list_->setUniformRowHeights(true);
  attachment_list_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  attachment_list_->setHeaderLabels(
      {tr("Name"), tr("Type"), tr("Size"), tr("Signed")});
  attachment_list_->header()->setStretchLastSection(false);
  attachment_list_->header()->setSectionResizeMode(kColName,
                                                   QHeaderView::Stretch);
  attachment_list_->setMaximumHeight(160);
  attachment_list_->setVisible(false);
  layout->addWidget(attachment_list_);

  unsigned_notice_ = new QLabel(this);
  unsigned_notice_->setWordWrap(true);
  unsigned_notice_->setVisible(false);
  {
    auto palette = unsigned_notice_->palette();
    palette.setColor(QPalette::WindowText, ThemeColor(this, &GFUIWarningColor));
    unsigned_notice_->setPalette(palette);
    auto font = unsigned_notice_->font();
    font.setPointSizeF(font.pointSizeF() * 0.92);
    unsigned_notice_->setFont(font);
  }
  layout->addWidget(unsigned_notice_);

  auto* buttons = new QHBoxLayout();
  add_button_ = new QPushButton(tr("Attach File..."), this);
  remove_button_ = new QPushButton(tr("Remove"), this);
  save_button_ = new QPushButton(tr("Save..."), this);
  save_all_button_ = new QPushButton(tr("Save All..."), this);
  buttons->addWidget(add_button_);
  buttons->addWidget(remove_button_);
  buttons->addStretch();
  buttons->addWidget(save_button_);
  buttons->addWidget(save_all_button_);
  layout->addLayout(buttons);

  connect(add_button_, &QPushButton::clicked, this,
          &EMailPageView::slot_add_attachment);
  connect(remove_button_, &QPushButton::clicked, this,
          &EMailPageView::slot_remove_attachment);
  connect(save_button_, &QPushButton::clicked, this,
          &EMailPageView::slot_save_attachment);
  connect(save_all_button_, &QPushButton::clicked, this,
          &EMailPageView::slot_save_all_attachments);
  connect(attachment_list_, &QTreeWidget::itemSelectionChanged, this,
          &EMailPageView::slot_selection_changed);

  for (auto* edit :
       {from_edit_, to_edit_, cc_edit_, bcc_edit_, subject_edit_}) {
    connect(edit, &QLineEdit::textEdited, this, [this]() { mark_dirty(); });
  }
  connect(body_edit_, &QPlainTextEdit::textChanged, this, [this]() {
    // textChanged also fires while loading, which must not count as an edit.
    if (!loading_) mark_dirty();
  });

  slot_selection_changed();
}

void EMailPageView::set_cc_bcc_visible(bool visible) {
  if (header_form_ == nullptr) return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  header_form_->setRowVisible(cc_edit_, visible);
  header_form_->setRowVisible(bcc_edit_, visible);
#else
  // Older Qt has no setRowVisible; hiding both cells collapses the row.
  const auto hide_row = [this](QWidget* field) {
    if (auto* label = header_form_->labelForField(field)) {
      label->setVisible(visible);
    }
    field->setVisible(visible);
  };
  hide_row(cc_edit_);
  hide_row(bcc_edit_);
#endif

  if (cc_bcc_toggle_ != nullptr && cc_bcc_toggle_->isChecked() != visible) {
    QSignalBlocker blocker(cc_bcc_toggle_);
    cc_bcc_toggle_->setChecked(visible);
  }
}

void EMailPageView::mark_dirty() {
  if (loading_) return;
  const bool was_clean = !dirty_;
  dirty_ = true;
  // Once per load is enough: the host only needs to learn that the tab became
  // modified, and it stays modified until it is saved.
  if (was_clean) emit SignalContentModified();
}

void EMailPageView::LoadFromSource(const QByteArray& source) {
  loading_ = true;

  message_ = EMailMetaData{};

  vmime::shared_ptr<vmime::message> parsed;
  if (CheckIfEMLMessage(source, parsed)) {
    GetEMLMetaData(parsed, message_);
    if (ExtractParts(parsed, message_) != 0) {
      MLogWarn("message exceeds the supported parsing limits");
    }
  } else {
    // Not a message yet -- a new tab, or something the user is still typing.
    // Treat the whole document as the body so nothing is lost.
    message_.body = source;
  }

  refresh_fields();
  refresh_attachments();

  loading_ = false;

  // A freshly loaded view matches the document exactly, which is what lets the
  // host hand the original bytes to a verify untouched.
  dirty_ = false;
}

void EMailPageView::refresh_fields() {
  from_edit_->setText(message_.from);
  to_edit_->setText(message_.to.join("; "));
  cc_edit_->setText(message_.cc.join("; "));
  bcc_edit_->setText(message_.bcc.join("; "));
  subject_edit_->setText(message_.subject);
  body_edit_->setPlainText(QString::fromUtf8(message_.body));

  // Collapsing these rows is about the common case of not using them. A
  // message that actually has a Cc or Bcc must not have it hidden.
  if (!message_.cc.isEmpty() || !message_.bcc.isEmpty()) {
    set_cc_bcc_visible(true);
  }
}

void EMailPageView::collect_fields() {
  const auto split = [](const QString& s) {
    QStringList out;
    for (const auto& part : s.split(';', Qt::SkipEmptyParts)) {
      const auto trimmed = part.trimmed();
      if (!trimmed.isEmpty()) out.append(trimmed);
    }
    return out;
  };

  message_.from = from_edit_->text().trimmed();
  message_.to = split(to_edit_->text());
  message_.cc = split(cc_edit_->text());
  message_.bcc = split(bcc_edit_->text());
  message_.subject = subject_edit_->text();
  message_.body = body_edit_->toPlainText().toUtf8();
}

void EMailPageView::refresh_attachments() {
  attachment_list_->clear();

  bool any_unsigned = false;
  bool any_signed_info = false;

  const auto muted = MutedColor(this);

  for (const auto& att : message_.attachments) {
    auto* item = new QTreeWidgetItem(attachment_list_);
    item->setText(kColName,
                  SanitizeAttachmentFileName(att.filename, att.mime_type));

    // A key is not a document and the action it affords is a different one, so
    // telling them apart should not take a reading of the type column.
    item->setIcon(kColName, QIcon(att.is_openpgp_key ? ":/icons/key.png"
                                                     : ":/icons/misc_doc.png"));

    // Type and size describe the name; they are not values in their own right.
    item->setText(kColType, att.mime_type);
    item->setForeground(kColType, muted);
    item->setText(kColSize, HumanSize(att.data.size()));
    item->setForeground(kColSize, muted);
    item->setTextAlignment(kColSize, Qt::AlignRight | Qt::AlignVCenter);

    if (att.inside_signed_part) {
      item->setText(kColSigned, tr("signed"));
      item->setForeground(kColSigned, AccentColor(this, true));
      any_signed_info = true;
    } else {
      // Not painted red: nothing here is broken and nothing is irreversible.
      // The part simply arrived without the signature vouching for it, which
      // is a caveat rather than an alarm.
      item->setText(kColSigned, tr("unsigned"));
      item->setForeground(kColSigned, ThemeColor(this, &GFUIWarningColor));
      any_unsigned = true;
    }

    // The name as it arrived, in full, so a sanitized display name never hides
    // what the sender actually sent.
    item->setToolTip(kColName, att.filename);
  }

  // Only say it when it means something: before a verify has run nothing is
  // marked signed, and claiming everything is unsigned would be noise.
  const bool meaningful = any_unsigned && any_signed_info;
  unsigned_notice_->setVisible(meaningful);
  if (meaningful) {
    unsigned_notice_->setText(
        tr("Some parts are outside the signed section of this message. "
           "They are not covered by the signature and could have been added "
           "by anyone."));
  }

  const bool has_any = !message_.attachments.isEmpty();

  // An empty table is a large blank box that reads as a fault and takes the
  // room the body wants, so the list only exists once there is something in
  // it. "Attach File..." stays available either way.
  attachment_list_->setVisible(has_any);
  attachment_heading_->setVisible(has_any);
  if (has_any) {
    const auto count = static_cast<int>(message_.attachments.size());
    attachment_heading_->setText(count == 1 ? tr("1 attachment")
                                            : tr("%1 attachments").arg(count));
  }

  // "Signed" is only an answer on a message that was received and verified.
  // While composing, nothing has been signed yet and a column of "no" would
  // be alarming rather than informative.
  attachment_list_->setColumnHidden(kColSigned, !any_signed_info);

  // Grow with the content instead of always reserving the maximum: a single
  // attachment should not claim the same space as eight.
  if (has_any) {
    const auto row_height = attachment_list_->sizeHintForRow(0) > 0
                                ? attachment_list_->sizeHintForRow(0)
                                : 20;
    const auto header_height = attachment_list_->header()->height();
    const auto wanted =
        header_height +
        row_height * static_cast<int>(message_.attachments.size()) + 8;
    attachment_list_->setFixedHeight(qMin(wanted, 160));
  }

  attachment_list_->resizeColumnToContents(kColType);
  attachment_list_->resizeColumnToContents(kColSize);
  if (any_signed_info) attachment_list_->resizeColumnToContents(kColSigned);
  slot_selection_changed();
}

auto EMailPageView::SaveToSource() -> QByteArray {
  collect_fields();

  QString eml;
  if (BuildMimeEML(message_, message_.body, message_.attachments, eml) != 0) {
    MLogWarn("failed to serialize message: " + eml);
    // Returning the body alone would silently discard the headers and every
    // attachment, so keep what the document already had instead.
    return message_.body;
  }

  dirty_ = false;
  return eml.toUtf8();
}

auto EMailPageView::IsDirty() -> bool { return dirty_; }

void EMailPageView::WipeContent() {
  // Overwrite before releasing: a decrypted body or attachment left in a freed
  // QByteArray would outlive the tab that was supposed to hold it.
  auto wipe = [](QByteArray& b) {
    if (!b.isEmpty()) b.fill('\0');
    b.clear();
  };

  wipe(message_.body);
  for (auto& att : message_.attachments) wipe(att.data);
  message_.attachments.clear();
  message_ = EMailMetaData{};

  loading_ = true;
  body_edit_->clear();
  from_edit_->clear();
  to_edit_->clear();
  cc_edit_->clear();
  bcc_edit_->clear();
  subject_edit_->clear();
  attachment_list_->clear();
  loading_ = false;

  dirty_ = false;
}

void EMailPageView::slot_selection_changed() {
  const auto selected = attachment_list_->selectedItems().size();
  const auto total = attachment_list_->topLevelItemCount();

  remove_button_->setEnabled(selected > 0);
  save_button_->setEnabled(selected > 0);
  save_all_button_->setEnabled(total > 0);

  // Hidden rather than permanently greyed out: on a message with no
  // attachments these two can never apply, and a row of dead buttons is just
  // clutter.
  save_button_->setVisible(total > 0);
  save_all_button_->setVisible(total > 0);
  remove_button_->setVisible(total > 0);
}

void EMailPageView::slot_add_attachment() {
  auto default_dir = UnStrDup(GFUIDefaultUserFilePath());
  if (default_dir.isEmpty()) default_dir = QDir::homePath();

  const auto paths =
      QFileDialog::getOpenFileNames(this, tr("Attach Files"), default_dir);
  if (paths.isEmpty()) return;

  for (const auto& path : paths) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      QMessageBox::warning(
          this, tr("Attach File"),
          tr("Cannot read %1:\n%2.").arg(path, file.errorString()));
      continue;
    }

    EMailAttachment att;
    att.filename = QFileInfo(path).fileName();
    att.mime_type = GuessMimeType(path);
    att.data = file.readAll();
    att.disposition = "attachment";
    // Composed here, so it will be inside whatever this message gets signed
    // with. Nothing is claimed about a signature that does not exist yet.
    att.inside_signed_part = false;
    message_.attachments.append(att);
  }

  refresh_attachments();
  mark_dirty();
}

void EMailPageView::slot_remove_attachment() {
  const auto selected = attachment_list_->selectedItems();
  if (selected.isEmpty()) return;

  // Collect indices first: removing while iterating would shift the rest.
  QList<int> rows;
  for (auto* item : selected) {
    rows.append(attachment_list_->indexOfTopLevelItem(item));
  }
  std::sort(rows.begin(), rows.end(), std::greater<>());

  for (const auto row : rows) {
    if (row >= 0 && row < message_.attachments.size()) {
      auto& data = message_.attachments[row].data;
      if (!data.isEmpty()) data.fill('\0');
      message_.attachments.removeAt(row);
    }
  }

  refresh_attachments();
  mark_dirty();
}

auto EMailPageView::write_attachment(const EMailAttachment& att,
                                     const QString& dir, const QString& name)
    -> bool {
  const QDir target(dir);
  const auto path = target.filePath(name);

  // The name has already been sanitized, so this is the second line rather
  // than the first: if anything ever slips through, the write still cannot
  // land outside the directory the user picked.
  const auto canonical_dir = QFileInfo(dir).canonicalFilePath();
  const auto resolved = QFileInfo(path).absolutePath();
  if (canonical_dir.isEmpty() ||
      QFileInfo(resolved).canonicalFilePath() != canonical_dir) {
    MLogWarn("refusing to write an attachment outside the chosen directory");
    return false;
  }

  // Atomic: a failure part-way through leaves no truncated file behind.
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) return false;
  if (file.write(att.data) != att.data.size()) return false;
  return file.commit();
}

void EMailPageView::slot_save_attachment() {
  const auto selected = attachment_list_->selectedItems();
  if (selected.isEmpty()) return;

  QList<EMailAttachment> chosen;
  for (auto* item : selected) {
    const auto row = attachment_list_->indexOfTopLevelItem(item);
    if (row >= 0 && row < message_.attachments.size()) {
      chosen.append(message_.attachments[row]);
    }
  }
  if (chosen.isEmpty()) return;

  auto default_dir = UnStrDup(GFUIDefaultUserFilePath());
  if (default_dir.isEmpty()) default_dir = QDir::homePath();

  if (chosen.size() == 1) {
    const auto suggested =
        QDir(default_dir)
            .filePath(SanitizeAttachmentFileName(chosen.front().filename,
                                                 chosen.front().mime_type));
    const auto path =
        QFileDialog::getSaveFileName(this, tr("Save Attachment"), suggested);
    if (path.isEmpty()) return;

    QSaveFile file(path);
    const bool ok =
        file.open(QIODevice::WriteOnly) &&
        file.write(chosen.front().data) == chosen.front().data.size() &&
        file.commit();
    if (!ok) {
      QMessageBox::warning(this, tr("Save Attachment"),
                           tr("Cannot write %1.").arg(path));
    }
    return;
  }

  const auto dir = QFileDialog::getExistingDirectory(
      this, tr("Save Attachments"), default_dir);
  if (dir.isEmpty()) return;

  const auto names =
      UniqueAttachmentFileNames(chosen, QDir(dir).entryList(QDir::Files));

  QStringList written;
  QStringList failed;
  for (int i = 0; i < chosen.size(); ++i) {
    if (write_attachment(chosen[i], dir, names[i])) {
      written.append(names[i]);
    } else {
      failed.append(names[i]);
    }
  }

  if (!failed.isEmpty()) {
    QMessageBox::warning(this, tr("Save Attachments"),
                         tr("Could not write: %1").arg(failed.join(", ")));
    return;
  }

  // The names are reported back because sanitizing or de-duplicating may have
  // changed them, and a silent rename is worse than a noisy one.
  const auto saved_count = written.size();
  const auto saved_line = saved_count == 1
                              ? tr("Saved 1 file:")
                              : tr("Saved %1 files:").arg(saved_count);
  QMessageBox::information(this, tr("Save Attachments"),
                           saved_line + "\n" + written.join("\n"));
}

void EMailPageView::slot_save_all_attachments() {
  if (message_.attachments.isEmpty()) return;

  attachment_list_->selectAll();
  slot_save_attachment();
}
