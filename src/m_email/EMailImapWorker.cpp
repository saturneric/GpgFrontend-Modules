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

#include "EMailImapWorker.h"

#include <QCoreApplication>
#include <vmime/net/imap/IMAPFolder.hpp>
#include <vmime/net/imap/IMAPSearchAttributes.hpp>
#include <vmime/security/defaultAuthenticator.hpp>

#include "EMailTlsSetup.h"
#include "GFModuleCommonUtils.hpp"

namespace {

/**
 * @brief An authenticator that refuses to hand over a password in the clear.
 *
 * The last line of defence, and the reason it is a separate class: vmime
 * connects and authenticates inside one call, so there is no moment between
 * the two where a caller could check. The authenticator callback is that
 * moment. By then STARTTLS has already run -- IMAPConnection does it before
 * authenticate() -- so the connection can be asked whether it is actually
 * encrypted, and the password simply is not produced if it is not.
 *
 * A policy failure therefore becomes a failed login rather than a leaked
 * credential.
 */
class SecureAuthenticator : public vmime::security::defaultAuthenticator {
 public:
  SecureAuthenticator(QString username, QString password, bool allow_cleartext)
      : username_(std::move(username)),
        password_(std::move(password)),
        allow_cleartext_(allow_cleartext) {}

  ~SecureAuthenticator() override { password_.fill(QChar('\0')); }

  auto getUsername() const -> const vmime::string override {
    return username_.toStdString();
  }

  auto getPassword() const -> const vmime::string override {
    if (!allow_cleartext_) {
      auto service = getService().lock();
      if (!EMailTlsSetup::IsSecured(service)) {
        LOG_ERROR("refusing to send mail password over an unencrypted link");
        throw vmime::exceptions::authentication_error(
            "refusing to authenticate over an unencrypted connection");
      }
    }
    return password_.toStdString();
  }

 private:
  QString username_;
  mutable QString password_;
  bool allow_cleartext_;
};

auto HeaderValue(const vmime::shared_ptr<const vmime::header>& header,
                 const char* field) -> QString {
  if (!header) return {};
  try {
    auto f = header->findField(field);
    if (!f) return {};
    return QString::fromStdString(f->getValue()->generate()).trimmed();
  } catch (...) {
    return {};
  }
}

/// Message-ID with the angle brackets stripped, for comparison.
auto NormalizeMessageId(const QString& raw) -> QString {
  auto value = raw.trimmed();
  if (value.startsWith('<')) value.remove(0, 1);
  if (value.endsWith('>')) value.chop(1);
  return value;
}

}  // namespace

struct EMailImapWorker::Impl {
  vmime::shared_ptr<vmime::net::session> session;
  vmime::shared_ptr<vmime::net::store> store;
  vmime::shared_ptr<vmime::net::folder> folder;
  QString folder_path;
  vmime::shared_ptr<EMailTimeoutHandlerFactory> timeouts;
  MailAccountConfig account;

  /// Folders as returned by a recursive LIST, kept rather than re-derived:
  /// a folder fetched by path carries empty attributes, so SPECIAL-USE is
  /// only visible on the objects the listing produced.
  std::vector<vmime::shared_ptr<vmime::net::folder>> listed;

  void Close() {
    try {
      if (folder && folder->isOpen()) folder->close(false);
    } catch (...) {
    }
    try {
      if (store && store->isConnected()) store->disconnect();
    } catch (...) {
    }
    // Released here, on the worker thread: these destructors close sockets and
    // must not run anywhere else.
    listed.clear();
    folder = nullptr;
    store = nullptr;
    session = nullptr;
    timeouts = nullptr;
    folder_path.clear();
  }
};

EMailImapWorker::EMailImapWorker(QObject* parent)
    : QObject(parent),
      impl_(new Impl),
      token_(std::make_shared<EMailCancelToken>()) {}

EMailImapWorker::~EMailImapWorker() {
  impl_->Close();
  delete impl_;
}

void EMailImapWorker::Connect(quint64 seq, const MailAccountConfig& account,
                              QString password) {
  impl_->Close();
  token_->Reset();
  EMailTlsSetup::ClearLastSeen();
  impl_->account = account;

  const auto& config = account.imap;
  const auto cleartext = config.tls == MailTlsMode::kNONE;

  // Belt and braces over the settings page: even a hand-edited configuration
  // cannot produce a cleartext session to a remote host.
  if (cleartext && !MailHostAllowsCleartext(config.host)) {
    password.fill(QChar('\0'));
    emit SignalFailed(seq, MailTlsRequiredError(config.host));
    return;
  }

  try {
    impl_->session = vmime::net::session::create();
    impl_->timeouts =
        vmime::make_shared<EMailTimeoutHandlerFactory>(token_, 30);

    const auto protocol = EMailTlsSetup::ProtocolName(true, config.tls);
    vmime::utility::url url(protocol.toStdString(), config.host.toStdString());
    url.setPort(config.EffectivePort(true));

    impl_->store = impl_->session->getStore(url);
    impl_->store->setTimeoutHandlerFactory(impl_->timeouts);

    EMailTlsSetup::Apply(impl_->session, impl_->store,
                         QString("store.%1").arg(protocol), config);

    impl_->store->setAuthenticator(vmime::make_shared<SecureAuthenticator>(
        config.username, password, cleartext));
    password.fill(QChar('\0'));

    impl_->store->connect();

    // After the fact as well as during: if a future vmime ever authenticated
    // before securing, this would still catch it before any mail moves.
    if (!cleartext && !EMailTlsSetup::IsSecured(impl_->store)) {
      impl_->Close();
      emit SignalFailed(seq, MailTlsRequiredError(config.host));
      return;
    }

    emit SignalConnected(seq);
  } catch (const vmime::exception& e) {
    password.fill(QChar('\0'));
    const auto cancelled =
        impl_->timeouts && impl_->timeouts->LastWasCancelled();
    auto error = ClassifyVmimeException(e, MailStage::kCONNECT, cancelled);
    impl_->Close();
    emit SignalFailed(seq, error);
  } catch (const std::exception& e) {
    password.fill(QChar('\0'));
    impl_->Close();
    emit SignalFailed(seq, MailInternalError(QString::fromUtf8(e.what())));
  }
}

void EMailImapWorker::ListFolders(quint64 seq) {
  if (!impl_->store) {
    emit SignalFailed(seq, MailInternalError("not connected"));
    return;
  }

  try {
    auto root = impl_->store->getRootFolder();
    impl_->listed = root->getFolders(true);

    QList<EMailFolderInfo> result;
    for (const auto& folder : impl_->listed) {
      const auto attributes = folder->getAttributes();

      EMailFolderInfo info;
      info.path = QString::fromStdString(
          folder->getFullPath().toString("/", vmime::charsets::UTF_8));
      info.display_name = QString::fromStdString(folder->getName().getBuffer());
      info.selectable =
          (attributes.getFlags() &
           vmime::net::folderAttributes::FLAG_NO_OPEN) == 0 &&
          (attributes.getType() &
           vmime::net::folderAttributes::TYPE_CONTAINS_MESSAGES) != 0;
      info.is_sent = attributes.getSpecialUse() ==
                     vmime::net::folderAttributes::SPECIALUSE_SENT;
      info.is_inbox = attributes.getSpecialUse() ==
                      vmime::net::folderAttributes::SPECIALUSE_INBOX;

      if (info.path.isEmpty()) continue;
      result.append(info);
    }

    emit SignalFolders(seq, result);
  } catch (const vmime::exception& e) {
    emit SignalFailed(
        seq, ClassifyVmimeException(
                 e, MailStage::kFOLDER,
                 impl_->timeouts && impl_->timeouts->LastWasCancelled()));
  }
}

namespace {

/// The attributes a listing needs and nothing more. PEEK is in the set so that
/// even the header fetch cannot set \Seen.
auto ListingAttributes() -> vmime::net::fetchAttributes {
  vmime::net::fetchAttributes attributes(
      vmime::net::fetchAttributes::ENVELOPE |
      vmime::net::fetchAttributes::SIZE | vmime::net::fetchAttributes::UID |
      vmime::net::fetchAttributes::FLAGS | vmime::net::fetchAttributes::PEEK);

  // IMAP is the only protocol that supports fetching a named header, and the
  // Message-ID is what Send & Confirm later matches on.
  attributes.add("Message-ID");
  return attributes;
}

auto SummarizeMessage(const vmime::shared_ptr<vmime::net::message>& message)
    -> EMailMessageSummary {
  EMailMessageSummary row;

  const auto uid_string = static_cast<vmime::string>(message->getUID());
  row.uid = QString::fromStdString(uid_string).toULongLong();
  row.size = static_cast<qint64>(message->getSize());

  auto header = message->getHeader();
  row.subject = HeaderValue(header, "Subject");
  row.from = HeaderValue(header, "From");
  row.message_id = NormalizeMessageId(HeaderValue(header, "Message-ID"));

  const auto date = HeaderValue(header, "Date");
  if (!date.isEmpty()) {
    row.date = QDateTime::fromString(date, Qt::RFC2822Date);
  }
  return row;
}

}  // namespace

void EMailImapWorker::ListMessages(quint64 seq, const QString& folder_path,
                                   quint64 before_uid, int page_size,
                                   int retained) {
  try {
    auto folder = OpenFolderReadOnly(folder_path);
    if (!folder) {
      emit SignalFailed(seq, MailInternalError("no folder"));
      return;
    }

    const auto total = static_cast<int>(folder->getMessageCount());
    const auto size = MailClampPageSize(page_size);

    EMailMessagePage page;
    if (total == 0) {
      emit SignalMessages(seq, page);
      return;
    }

    // The session cap is applied before anything is asked of the server, so a
    // picker that has reached it costs the server nothing more.
    auto room = kMailMaxRetainedRows - retained;
    if (room <= 0) {
      page.capped = true;
      emit SignalMessages(seq, page);
      return;
    }

    // Paged newest-first by sequence number, which is a server-side range.
    // The alternative -- asking for everything and keeping the tail -- is what
    // this design exists to avoid.
    int highest = total;
    if (before_uid != 0) highest = static_cast<int>(before_uid) - 1;
    if (highest <= 0) {
      emit SignalMessages(seq, page);
      return;
    }

    auto wanted = std::min(size, room);
    const auto first = std::max(1, highest - wanted + 1);

    auto messages =
        folder->getMessages(vmime::net::messageSet::byNumber(first, highest));
    folder->fetchMessages(messages, ListingAttributes());

    for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
      auto row = SummarizeMessage(*it);
      // Carry the sequence number as the cursor: UIDs are not contiguous, and
      // this is what the next page is asked to come before.
      row.uid = static_cast<quint64>((*it)->getNumber());
      page.rows.append(row);
    }

    page.remaining = std::max(0, first - 1);
    page.capped = room <= size && page.remaining > 0;
    emit SignalMessages(seq, page);
  } catch (const vmime::exception& e) {
    emit SignalFailed(
        seq, ClassifyVmimeException(
                 e, MailStage::kLISTING,
                 impl_->timeouts && impl_->timeouts->LastWasCancelled()));
  }
}

void EMailImapWorker::SearchMessages(quint64 seq, const QString& folder_path,
                                     const QString& query, int page_size) {
  try {
    auto folder = OpenFolderReadOnly(folder_path);
    if (!folder) {
      emit SignalFailed(seq, MailInternalError("no folder"));
      return;
    }

    auto imap = vmime::dynamicCast<vmime::net::imap::IMAPFolder>(folder);
    if (!imap) {
      emit SignalFailed(seq, MailInternalError("search needs IMAP"));
      return;
    }

    const auto text = query.trimmed().toStdString();
    vmime::net::imap::IMAPSearchAttributes attributes;
    attributes.add(vmime::net::imap::IMAPSearchTokenFactory::OR(
        vmime::net::imap::IMAPSearchTokenFactory::SUBJECT(text),
        vmime::net::imap::IMAPSearchTokenFactory::FROM(text)));

    // vmime hands back the whole match set: IMAP SEARCH has no LIMIT and this
    // call materializes every UID. They are cheap individually, but the count
    // is the server's to choose, so the result is capped here and the user is
    // told the view was narrowed rather than being quietly given a slice.
    auto uids = imap->getMessageUIDsMatchingSearchAttributes(attributes);

    EMailMessagePage page;
    const auto size = MailClampPageSize(page_size);
    if (uids.empty()) {
      emit SignalMessages(seq, page);
      return;
    }

    page.capped = static_cast<int>(uids.size()) > size;

    std::vector<vmime::net::message::uid> wanted;
    for (auto it = uids.rbegin();
         it != uids.rend() && static_cast<int>(wanted.size()) < size; ++it) {
      wanted.push_back(*it);
    }

    for (const auto& uid : wanted) {
      auto messages = folder->getMessages(vmime::net::messageSet::byUID(uid));
      if (messages.empty()) continue;
      folder->fetchMessages(messages, ListingAttributes());
      auto row = SummarizeMessage(messages.front());
      row.uid = static_cast<quint64>(messages.front()->getNumber());
      page.rows.append(row);
    }

    page.remaining = std::max(0, static_cast<int>(uids.size()) - size);
    emit SignalMessages(seq, page);
  } catch (const vmime::exception& e) {
    emit SignalFailed(
        seq, ClassifyVmimeException(
                 e, MailStage::kLISTING,
                 impl_->timeouts && impl_->timeouts->LastWasCancelled()));
  }
}

void EMailImapWorker::FetchMessage(quint64 seq, const QString& folder_path,
                                   quint64 number) {
  try {
    auto folder = OpenFolderReadOnly(folder_path);
    if (!folder) {
      emit SignalFailed(seq, MailInternalError("no folder"));
      return;
    }

    auto messages = folder->getMessages(
        vmime::net::messageSet::byNumber(static_cast<size_t>(number)));
    if (messages.empty()) {
      emit SignalFailed(seq, MailInternalError("message not found"));
      return;
    }

    auto message = messages.front();
    folder->fetchMessage(message, vmime::net::fetchAttributes(
                                      vmime::net::fetchAttributes::SIZE));

    // Refused on the reported size, before a byte of the body is asked for.
    if (static_cast<qint64>(message->getSize()) > kMailMaxMessageSize) {
      MailError error;
      error.category = MailErrorCategory::kFETCH;
      error.title = QCoreApplication::translate(
          "EMailTransport", "That message is too large to open");
      emit SignalFailed(seq, error);
      return;
    }

    std::ostringstream stream;
    vmime::utility::outputStreamAdapter out(stream);

    // peek = true is the whole point: opening a message here must not be a
    // change to the mailbox, and the folder being read-only means the server
    // could not honour a \Seen write even if one were attempted.
    message->extract(out, nullptr, 0, static_cast<size_t>(-1), true);

    const auto data = stream.str();
    emit SignalMessageFetched(
        seq, QByteArray(data.data(), static_cast<int>(data.size())));
  } catch (const vmime::exception& e) {
    emit SignalFailed(
        seq, ClassifyVmimeException(
                 e, MailStage::kFETCH,
                 impl_->timeouts && impl_->timeouts->LastWasCancelled()));
  }
}

void EMailImapWorker::FindInSentFolder(quint64 seq, const QString& message_id) {
  try {
    const auto path = ResolveSentFolder();
    if (path.isEmpty()) {
      // "We could not tell" is a different answer from "it is not there", and
      // collapsing the two would claim knowledge we do not have.
      emit SignalSentLookup(seq, false, false, {});
      return;
    }

    auto folder = OpenFolderReadOnly(path);
    if (!folder) {
      emit SignalSentLookup(seq, false, false, path);
      return;
    }

    auto imap = vmime::dynamicCast<vmime::net::imap::IMAPFolder>(folder);
    if (!imap) {
      emit SignalSentLookup(seq, false, false, path);
      return;
    }

    const auto needle = NormalizeMessageId(message_id);
    vmime::net::imap::IMAPSearchAttributes attributes;
    attributes.add(vmime::net::imap::IMAPSearchTokenFactory::HEADER(
        "Message-ID", needle.toStdString()));
    auto uids = imap->getMessageUIDsMatchingSearchAttributes(attributes);

    emit SignalSentLookup(seq, true, !uids.empty(), path);
  } catch (const vmime::exception& e) {
    emit SignalFailed(
        seq, ClassifyVmimeException(
                 e, MailStage::kLISTING,
                 impl_->timeouts && impl_->timeouts->LastWasCancelled()));
  }
}

void EMailImapWorker::Disconnect() { impl_->Close(); }

auto EMailImapWorker::OpenFolderReadOnly(const QString& path)
    -> vmime::shared_ptr<vmime::net::folder> {
  if (!impl_->store) return nullptr;

  if (impl_->folder && impl_->folder->isOpen() && impl_->folder_path == path) {
    return impl_->folder;
  }

  if (impl_->folder && impl_->folder->isOpen()) {
    try {
      impl_->folder->close(false);
    } catch (...) {
    }
  }

  auto folder = impl_->store->getFolder(vmime::utility::path::fromString(
      path.toStdString(), "/", vmime::charsets::UTF_8));

  folder->open(vmime::net::folder::MODE_READ_ONLY, true);

  impl_->folder = folder;
  impl_->folder_path = path;
  return folder;
}

auto EMailImapWorker::ResolveSentFolder() -> QString {
  const auto override_path = impl_->account.sent_folder_override.trimmed();
  if (!override_path.isEmpty()) return override_path;

  if (!impl_->store) return {};

  try {
    // Attributes only exist on the objects a LIST produced; a folder looked up
    // by path comes back with an empty attribute set, so SPECIAL-USE would be
    // invisible if we re-derived it here.
    if (impl_->listed.empty()) {
      impl_->listed = impl_->store->getRootFolder()->getFolders(true);
    }

    for (const auto& folder : impl_->listed) {
      const auto attributes = folder->getAttributes();
      if (attributes.getSpecialUse() !=
          vmime::net::folderAttributes::SPECIALUSE_SENT) {
        continue;
      }
      if ((attributes.getFlags() &
           vmime::net::folderAttributes::FLAG_NO_OPEN) != 0) {
        continue;
      }
      return QString::fromStdString(
          folder->getFullPath().toString("/", vmime::charsets::UTF_8));
    }
  } catch (...) {
    return {};
  }

  return {};
}
