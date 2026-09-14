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
#include <algorithm>
#include <optional>
#include <string>
#include <vmime/net/imap/IMAPFolder.hpp>
#include <vmime/net/imap/IMAPFolderStatus.hpp>
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
  SecureAuthenticator(QString username, EMailSecretPtr password,
                      bool allow_cleartext)
      : username_(std::move(username)),
        password_(std::move(password)),
        allow_cleartext_(allow_cleartext) {}

  // No wipe here: the bytes belong to EMailSecret and are erased when the
  // last holder releases them. Wiping a local copy was the old mistake -- it
  // detached and zeroed a fresh buffer, leaving the original intact.
  ~SecureAuthenticator() override = default;

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
    // A copy vmime owns and goes on to copy again while encoding AUTH.
    // Neither is ours to erase; see EMailSecret.
    return password_ ? password_->StdStringCopy() : std::string();
  }

 private:
  QString username_;
  EMailSecretPtr password_;
  bool allow_cleartext_;
};

/// A header field in its WIRE form: exactly the octets the server sent.
///
/// Right for Message-ID and Date, which are already plain text and must not be
/// reinterpreted. Wrong for anything a human reads -- see HeaderText below.
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

/**
 * @brief A header field as text a person can read.
 *
 * The distinction matters and is easy to miss: `generate()` re-emits a field
 * the way it travels on the wire, so a subject that arrived as an RFC 2047
 * encoded-word comes back as the literal string "=?utf-8?Q?Fam=2C_Can...?="
 * rather than as the words it encodes. Every non-ASCII subject in a mailbox
 * then reads as mojibake. Asking vmime for the TYPED value instead is what
 * decodes it, which is the same thing EMAilHelper.cpp does when it parses a
 * message from disk.
 *
 * Charset conversion is to UTF-8, and the result is still only ever shown as
 * plain text: a subject is server-controlled input, never markup.
 */
auto HeaderText(const vmime::shared_ptr<const vmime::header>& header,
                const char* field) -> QString {
  if (!header) return {};
  try {
    auto f = header->findField(field);
    if (!f) return {};

    if (auto value = f->getValue<vmime::text>()) {
      return QString::fromStdString(
                 value->getConvertedText(vmime::charsets::UTF_8))
          .trimmed();
    }
  } catch (...) {
  }
  return HeaderValue(header, field);
}

/// A sender as "Display Name <address>", with the name decoded.
auto HeaderMailbox(const vmime::shared_ptr<const vmime::header>& header,
                   const char* field) -> QString {
  if (!header) return {};
  try {
    auto f = header->findField(field);
    if (!f) return {};

    vmime::shared_ptr<const vmime::mailbox> box;
    if (auto one = f->getValue<vmime::mailbox>()) {
      box = one;
    } else if (auto list = f->getValue<vmime::addressList>()) {
      if (list->getAddressCount() > 0) {
        box = vmime::dynamicCast<const vmime::mailbox>(list->getAddressAt(0));
      }
    }

    if (box) {
      const auto name = QString::fromStdString(box->getName().getConvertedText(
                                                   vmime::charsets::UTF_8))
                            .trimmed();
      const auto address =
          QString::fromStdString(box->getEmail().toString()).trimmed();
      if (name.isEmpty()) return address;
      return QString("%1 <%2>").arg(name, address);
    }
  } catch (...) {
  }
  return HeaderText(header, field);
}

/**
 * @brief Reports body-download progress as it arrives.
 *
 * vmime drives this from the stream copy that pulls the message down, which
 * makes a download the one IMAP operation that can honestly show a fraction:
 * everything else reports completion and nothing in between. Cancellation is
 * NOT expressed here -- this interface has no say in it -- and continues to
 * ride on the timeout handler, which is polled inside the blocking socket
 * reads that this progress is measuring.
 */
class FetchProgress : public vmime::utility::progressListener {
 public:
  FetchProgress(EMailImapWorker* worker, quint64 seq)
      : worker_(worker), seq_(seq) {}

  void start(const size_t predicted_total) override {
    total_ = static_cast<qint64>(predicted_total);
    emit worker_->SignalFetchProgress(seq_, 0, total_);
  }

  void progress(const size_t current, const size_t current_total) override {
    total_ = static_cast<qint64>(current_total);
    emit worker_->SignalFetchProgress(seq_, static_cast<qint64>(current),
                                      total_);
  }

  void stop(const size_t current) override {
    emit worker_->SignalFetchProgress(seq_, static_cast<qint64>(current),
                                      total_);
  }

 private:
  EMailImapWorker* worker_;
  quint64 seq_;
  qint64 total_{0};
};

/// Message-ID with the angle brackets stripped, for comparison.
/**
 * @brief An output stream that refuses to grow past a ceiling.
 *
 * The size check before a fetch reads RFC822.SIZE, which is the SERVER's
 * claim about the message. A server that under-reports -- or simply one that
 * is wrong -- then streams as much as it likes into an ostringstream that has
 * no bound of its own, and the process grows until it is killed. The only
 * timeout in the way is reset by every read, so a slow drip never trips it.
 *
 * So the ceiling is enforced where the bytes actually arrive. Throwing is what
 * stops the transfer: vmime has no way to say "stop" to a listener.
 */
class BoundedOutputStream : public vmime::utility::outputStream {
 public:
  BoundedOutputStream(std::ostringstream& sink, size_t limit)
      : sink_(sink), limit_(limit) {}

  void flush() override {}

  [[nodiscard]] auto Written() const -> size_t { return written_; }

 protected:
  void writeImpl(const vmime::byte_t* data, const size_t count) override {
    written_ += count;
    if (written_ > limit_) {
      throw vmime::exceptions::invalid_response(
          "FETCH", "the message is larger than its reported size");
    }
    sink_.write(reinterpret_cast<const char*>(data), count);
  }

 private:
  std::ostringstream& sink_;
  size_t limit_;
  size_t written_{0};
};

auto NormalizeMessageId(const QString& raw) -> QString {
  auto value = raw.trimmed();
  if (value.startsWith('<')) value.remove(0, 1);
  if (value.endsWith('>')) value.chop(1);
  return value;
}

}  // namespace

auto ImapQuotable(const QString& text) -> std::optional<std::string> {
  std::string out;
  out.reserve(static_cast<size_t>(text.size()));

  for (const auto ch : text) {
    const auto code = ch.unicode();

    // CR and LF end the command; everything else below 0x20, and DEL, has no
    // business in a quoted string either.
    if (code < 0x20 || code == 0x7F) return std::nullopt;
    if (code > 0x7E) return std::nullopt;

    // The two characters a quoted string has to escape, per RFC 3501.
    if (ch == '"' || ch == '\\') out.push_back('\\');
    out.push_back(static_cast<char>(code));
  }

  return out;
}

struct EMailImapWorker::Impl {
  vmime::shared_ptr<vmime::net::session> session;
  vmime::shared_ptr<vmime::net::store> store;
  vmime::shared_ptr<vmime::net::folder> folder;
  QString folder_path;
  vmime::shared_ptr<EMailTimeoutHandlerFactory> timeouts;
  MailAccountConfig account;

  /// Folder metadata from the last recursive LIST, as plain data.
  ///
  /// Deliberately NOT the vmime folder objects. Every IMAPFolder registers
  /// itself with the store when constructed and only deregisters when
  /// destroyed, and open() refuses if *any other live object* shares its path
  /// -- so holding the listing alive makes opening any listed folder fail with
  /// "folder already open", even though nothing is open at all. Keeping only
  /// what we actually need lets those objects die immediately.
  QList<EMailFolderInfo> folder_cache;

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
    folder_cache.clear();
    folder = nullptr;
    store = nullptr;
    session = nullptr;
    timeouts = nullptr;
    folder_path.clear();
  }
};

/// Consumes a cancellation when the operation it was aimed at ends.
///
/// A stop applies to the request it was aimed at, not to the session.
///
/// This used to be done by clearing a shared flag on the way out of every
/// slot, which got both ends wrong: a stop pressed while a request was still
/// queued was wiped before the slot could see it, and a stop left over from a
/// finished request could still be sitting there for the next one. The token
/// is scoped by sequence number instead, so announcing the operation here is
/// all that is needed -- see EMailCancelToken.
class CancelScope {
 public:
  CancelScope(EMailCancelTokenPtr token, quint64 seq,
              vmime::shared_ptr<EMailTimeoutHandlerFactory> timeouts)
      : token_(std::move(token)), timeouts_(std::move(timeouts)) {
    if (token_) token_->Begin(seq);
  }

  /// For a slot that REPLACES the timeout factory while it runs -- Connect
  /// closes the old session, which drops it, and installs a new one. Taking
  /// the factory by value there would capture the one being thrown away and
  /// leave the new one's cancelled flag set for the next operation to trip
  /// over, so this form reads the member at destruction instead.
  CancelScope(EMailCancelTokenPtr token, quint64 seq,
              vmime::shared_ptr<EMailTimeoutHandlerFactory>* timeouts)
      : token_(std::move(token)), timeouts_slot_(timeouts) {
    if (token_) token_->Begin(seq);
  }

  /// Whether this operation has been stopped. Checked at slot entry, because a
  /// stop can arrive before the slot runs and there is no point opening a
  /// socket for work nobody wants any more.
  [[nodiscard]] auto Cancelled() const -> bool {
    return token_ && token_->IsCancelled();
  }

  ~CancelScope() {
    // The token is NOT cleared here: it clears itself when the next operation
    // announces a higher sequence number. Only the timeout factory's record of
    // "the last stop was deliberate" belongs to this operation.
    const auto& timeouts =
        timeouts_slot_ != nullptr ? *timeouts_slot_ : timeouts_;
    if (timeouts) timeouts->ClearLastCancelled();
  }

  CancelScope(const CancelScope&) = delete;
  CancelScope(CancelScope&&) = delete;
  auto operator=(const CancelScope&) -> CancelScope& = delete;
  auto operator=(CancelScope&&) -> CancelScope& = delete;

 private:
  EMailCancelTokenPtr token_;
  vmime::shared_ptr<EMailTimeoutHandlerFactory> timeouts_;
  vmime::shared_ptr<EMailTimeoutHandlerFactory>* timeouts_slot_{nullptr};
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
                              EMailSecretPtr password) {
  impl_->Close();

  // Reset on the way in AND cleared on the way out, like every other slot
  // here. Without the scope, a Stop pressed during a connect left the token
  // SET after this returned: the connect could still succeed, and the folder
  // listing that follows it would then fail immediately as cancelled, leaving
  // a connected session with an empty window and nothing said about why.
  //
  // By address, because the factory this slot ends up owning is not the one it
  // starts with.
  const CancelScope cancel_scope(token_, seq, &impl_->timeouts);

  // A stop that arrived while this request was still queued applies to it:
  // there is no point opening a socket for work nobody wants any more.
  if (cancel_scope.Cancelled()) {
    emit SignalFailed(seq, MailCancelledError());
    return;
  }

  impl_->account = account;

  const auto& config = account.imap;
  const auto cleartext = config.tls == MailTlsMode::kNONE;

  // Belt and braces over the settings page: even a hand-edited configuration
  // cannot produce a cleartext session to a remote host.
  if (cleartext && !MailHostAllowsCleartext(config.host)) {
    emit SignalFailed(seq, MailTlsRequiredError(config.host));
    return;
  }

  // Declared out here so the catch below can read it: what the verifier
  // records belongs to this connection attempt, and is the only honest source
  // for the fingerprint the user may be offered a pin for.
  EMailTlsSetup::SeenCertificatePtr seen;

  try {
    impl_->session = vmime::net::session::create();

    impl_->timeouts =
        vmime::make_shared<EMailTimeoutHandlerFactory>(token_, 30);

    const auto protocol = EMailTlsSetup::ProtocolName(true, config.tls);
    vmime::utility::url url(protocol.toStdString(), config.host.toStdString());
    url.setPort(config.EffectivePort(true));

    impl_->store = impl_->session->getStore(url);
    impl_->store->setTimeoutHandlerFactory(impl_->timeouts);

    seen =
        EMailTlsSetup::Apply(impl_->session, impl_->store,
                             QString("store.%1").arg(protocol), config, true);

    impl_->store->setAuthenticator(vmime::make_shared<SecureAuthenticator>(
        config.username, password, cleartext));

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
    const auto cancelled =
        impl_->timeouts && impl_->timeouts->LastWasCancelled();
    auto error = ClassifyVmimeException(e, MailStage::kCONNECT, cancelled);
    // From THIS connection's verifier, so the fingerprint the user is shown
    // and offered a pin for belongs to the server that actually refused.
    if (seen) MailAttachCertificate(error, seen->fingerprint, seen->summary);
    impl_->Close();
    emit SignalFailed(seq, error);
  } catch (const std::exception& e) {
    impl_->Close();
    emit SignalFailed(seq, MailInternalError(QString::fromUtf8(e.what())));
  }
}

namespace {}  // namespace

void EMailImapWorker::ListFolders(quint64 seq) {
  const CancelScope cancel_scope(token_, seq, impl_->timeouts);

  // A stop that arrived while this request was still queued applies to it:
  // there is no point opening a socket for work nobody wants any more.
  if (cancel_scope.Cancelled()) {
    emit SignalFailed(seq, MailCancelledError());
    return;
  }

  if (!impl_->store) {
    emit SignalFailed(seq, MailInternalError("not connected"));
    return;
  }

  try {
    // Scoped: the folder objects must not outlive this loop, or nothing in
    // the listing can be opened afterwards.
    auto listed = impl_->store->getRootFolder()->getFolders(true);

    QList<EMailFolderInfo> result;
    for (const auto& folder : listed) {
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

    listed.clear();
    impl_->folder_cache = result;

    emit SignalFolders(seq, result);
  } catch (const vmime::exception& e) {
    emit SignalFailed(
        seq, ClassifyVmimeException(
                 e, MailStage::kFOLDER,
                 impl_->timeouts && impl_->timeouts->LastWasCancelled()));
  } catch (const std::exception& e) {
    // Nothing may propagate out of a slot: this runs from the worker thread's
    // event loop, where an escaping exception is std::terminate. Answering the
    // sequence also matters -- an unanswered request leaves the controller
    // busy forever.
    emit SignalFailed(seq, MailInternalError(QString::fromUtf8(e.what())));
  } catch (...) {
    emit SignalFailed(seq, MailInternalError("unknown error"));
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
  row.subject = HeaderText(header, "Subject");
  row.from = HeaderMailbox(header, "From");
  row.message_id = NormalizeMessageId(HeaderValue(header, "Message-ID"));

  const auto date = HeaderValue(header, "Date");
  if (!date.isEmpty()) {
    row.date = QDateTime::fromString(date, Qt::RFC2822Date);
  }
  return row;
}

}  // namespace

namespace {

/// The comparable facts about an open or named folder, or an unknown-valued
/// EMailFolderValidators when the server will not say.
auto ReadFolderValidators(const vmime::shared_ptr<vmime::net::folder>& folder)
    -> EMailFolderValidators {
  if (!folder) return {};

  try {
    auto status = folder->getStatus();
    if (!status) return {};

    EMailFolderValidators validators;
    validators.known = true;
    validators.message_count = static_cast<quint64>(status->getMessageCount());

    // The UID fields are IMAP's, not the generic folder interface's.
    auto imap_status =
        vmime::dynamicCast<vmime::net::imap::IMAPFolderStatus>(status);
    if (imap_status) {
      validators.uid_validity =
          static_cast<quint32>(imap_status->getUIDValidity());
      validators.uid_next = static_cast<quint32>(imap_status->getUIDNext());
      validators.highest_mod_seq =
          static_cast<quint64>(imap_status->getHighestModSeq());
    }

    // A server that reports no UIDVALIDITY has given nothing a cache can be
    // keyed on, whatever else it said.
    if (validators.uid_validity == 0) return {};

    return validators;
  } catch (...) {
    return {};
  }
}

}  // namespace

void EMailImapWorker::FolderStatus(quint64 seq, const QString& folder_path) {
  const CancelScope cancel_scope(token_, seq, impl_->timeouts);

  // Nothing to report, and deliberately not an error: the caller's fallback is
  // to fetch the listing, which is exactly what it would have done anyway.
  if (cancel_scope.Cancelled() || !impl_->store) {
    emit SignalFolderStatus(seq, folder_path, {});
    return;
  }

  try {
    // STATUS does not need the folder selected, so this must not open one --
    // opening is what the round trip is being spent to avoid. An already-open
    // folder is reused rather than fetched again, because a second live object
    // on the same path is what OpenFolderReadOnly() exists to prevent.
    vmime::shared_ptr<vmime::net::folder> folder;
    if (impl_->folder && impl_->folder_path == folder_path) {
      folder = impl_->folder;
    } else {
      folder = impl_->store->getFolder(vmime::utility::path::fromString(
          folder_path.toStdString(), "/", vmime::charsets::UTF_8));
    }

    emit SignalFolderStatus(seq, folder_path, ReadFolderValidators(folder));
  } catch (...) {
    emit SignalFolderStatus(seq, folder_path, {});
  }
}

void EMailImapWorker::ListMessages(quint64 seq, const QString& folder_path,
                                   quint64 before_seq, int page_size,
                                   int retained) {
  const CancelScope cancel_scope(token_, seq, impl_->timeouts);

  // A stop that arrived while this request was still queued applies to it:
  // there is no point opening a socket for work nobody wants any more.
  if (cancel_scope.Cancelled()) {
    emit SignalFailed(seq, MailCancelledError());
    return;
  }

  try {
    auto folder = OpenFolderReadOnly(folder_path);
    if (!folder) {
      emit SignalFailed(seq, MailInternalError("no folder"));
      return;
    }

    const auto total = static_cast<int>(folder->getMessageCount());
    const auto size = MailClampPageSize(page_size);

    EMailMessagePage page;
    // Read once, here, and carried on every return path below: these rows and
    // the validators that describe them have to come from the same moment, or
    // a later visit would compare its STATUS against a folder state that was
    // never the one these rows were read from.
    page.validators = ReadFolderValidators(folder);
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
    if (before_seq != 0) highest = static_cast<int>(before_seq) - 1;
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
      // The cursor, ALONGSIDE the UID rather than instead of it. This used to
      // overwrite row.uid, which left every row identified by its position --
      // and a position is only true until something else is expunged.
      row.seq = static_cast<quint64>((*it)->getNumber());
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
  } catch (const std::exception& e) {
    // Nothing may propagate out of a slot: this runs from the worker thread's
    // event loop, where an escaping exception is std::terminate. Answering the
    // sequence also matters -- an unanswered request leaves the controller
    // busy forever.
    emit SignalFailed(seq, MailInternalError(QString::fromUtf8(e.what())));
  } catch (...) {
    emit SignalFailed(seq, MailInternalError("unknown error"));
  }
}

void EMailImapWorker::SearchMessages(quint64 seq, const QString& folder_path,
                                     const QString& query, int page_size) {
  const CancelScope cancel_scope(token_, seq, impl_->timeouts);

  // A stop that arrived while this request was still queued applies to it:
  // there is no point opening a socket for work nobody wants any more.
  if (cancel_scope.Cancelled()) {
    emit SignalFailed(seq, MailCancelledError());
    return;
  }

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

    const auto text = ImapQuotable(query.trimmed());
    if (!text) {
      MailError error;
      error.category = MailErrorCategory::kLISTING;
      error.title = QCoreApplication::translate(
          "EMailTransport", "That search text cannot be sent");
      error.detail = QCoreApplication::translate(
          "EMailTransport",
          "A search may only contain ordinary ASCII characters, with no line "
          "breaks. Try searching for a shorter part of the word.");
      emit SignalFailed(seq, error);
      return;
    }

    vmime::net::imap::IMAPSearchAttributes attributes;
    attributes.add(vmime::net::imap::IMAPSearchTokenFactory::OR(
        vmime::net::imap::IMAPSearchTokenFactory::SUBJECT(*text),
        vmime::net::imap::IMAPSearchTokenFactory::FROM(*text)));

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

    // One set, one fetch. Asking per UID meant a full server round trip for
    // every row -- fifty of them for a full page -- where the listing path
    // beside this one has always fetched its whole range at once. On anything
    // but a local server that was the difference between a search feeling
    // instant and feeling broken.
    auto set = vmime::net::messageSet::empty();
    for (const auto& uid : wanted)
      set.addRange(vmime::net::UIDMessageRange(uid));

    auto messages = folder->getMessages(set);
    if (!messages.empty()) {
      folder->fetchMessages(messages, ListingAttributes());

      // The server may return these in its own order; the search asked for
      // newest first, so the order is restored rather than assumed.
      QHash<QString, EMailMessageSummary> by_uid;
      for (const auto& message : messages) {
        auto row = SummarizeMessage(message);
        // SummarizeMessage already read the real UID; only the cursor is
        // taken from the position. Search results are not paged by position,
        // so this is recorded for completeness rather than used.
        row.seq = static_cast<quint64>(message->getNumber());
        by_uid.insert(QString::fromStdString(message->getUID()), row);
      }

      for (const auto& uid : wanted) {
        const auto key = QString::fromStdString(uid);
        if (by_uid.contains(key)) page.rows.append(by_uid.value(key));
      }
    }

    page.remaining = std::max(0, static_cast<int>(uids.size()) - size);
    emit SignalMessages(seq, page);
  } catch (const vmime::exception& e) {
    emit SignalFailed(
        seq, ClassifyVmimeException(
                 e, MailStage::kLISTING,
                 impl_->timeouts && impl_->timeouts->LastWasCancelled()));
  } catch (const std::exception& e) {
    // Nothing may propagate out of a slot: this runs from the worker thread's
    // event loop, where an escaping exception is std::terminate. Answering the
    // sequence also matters -- an unanswered request leaves the controller
    // busy forever.
    emit SignalFailed(seq, MailInternalError(QString::fromUtf8(e.what())));
  } catch (...) {
    emit SignalFailed(seq, MailInternalError("unknown error"));
  }
}

void EMailImapWorker::FetchMessage(quint64 seq, const QString& folder_path,
                                   quint64 uid) {
  const CancelScope cancel_scope(token_, seq, impl_->timeouts);

  // A stop that arrived while this request was still queued applies to it:
  // there is no point opening a socket for work nobody wants any more.
  if (cancel_scope.Cancelled()) {
    emit SignalFailed(seq, MailCancelledError());
    return;
  }

  try {
    auto folder = OpenFolderReadOnly(folder_path);
    if (!folder) {
      emit SignalFailed(seq, MailInternalError("no folder"));
      return;
    }

    // By UID, which is the message's identity. Addressing by sequence number
    // meant asking for a POSITION: an expunge by another client renumbers
    // everything after it, so the row the user clicked and the message that
    // came back could be two different messages, with nothing to say so.
    auto messages = folder->getMessages(vmime::net::messageSet::byUID(
        static_cast<vmime::net::message::uid>(std::to_string(uid))));
    if (messages.empty()) {
      // A UID that no longer resolves means the message is gone, which is a
      // different thing from a fetch failing and is worth saying so.
      MailError error;
      error.category = MailErrorCategory::kFETCH;
      error.title = QCoreApplication::translate(
          "EMailTransport", "That message is no longer in this folder");
      error.detail = QCoreApplication::translate(
          "EMailTransport",
          "It was moved or deleted after this list was loaded. Refresh the "
          "folder to see what is there now.");
      emit SignalFailed(seq, error);
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

    // Two ceilings, and the body has to respect both.
    //
    // The first is this application's own limit, which owes nothing to
    // anything the server said. The second is the server's own claim plus a
    // margin: it has just told us how big this message is, and a body that
    // runs well past that is either a server that is wrong about its own
    // mailbox or one that is trying something. Without either, the check above
    // was the ONLY bound and it was made of the server's own number -- so a
    // server that under-reported streamed as much as it liked into a stream
    // that would grow until the process was killed.
    //
    // The margin is generous because servers do miscount, usually over line
    // endings, and being refused a message you can see in another client would
    // be a worse failure than accepting a few kilobytes too many.
    constexpr size_t kFetchSizeSlack = 64U * 1024U;
    const auto claimed = static_cast<size_t>(message->getSize());
    auto limit = static_cast<size_t>(kMailMaxMessageSize);
    if (claimed > 0) {
      limit = std::min(limit, claimed + kFetchSizeSlack);
    }

    std::ostringstream stream;
    BoundedOutputStream out(stream, limit);

    // peek = true is the whole point: opening a message here must not be a
    // change to the mailbox, and the folder being read-only means the server
    // could not honour a \Seen write even if one were attempted.
    FetchProgress listener(this, seq);
    message->extract(out, &listener, 0, static_cast<size_t>(-1), true);

    const auto data = stream.str();
    emit SignalMessageFetched(
        seq, QByteArray(data.data(), static_cast<int>(data.size())));
  } catch (const vmime::exception& e) {
    emit SignalFailed(
        seq, ClassifyVmimeException(
                 e, MailStage::kFETCH,
                 impl_->timeouts && impl_->timeouts->LastWasCancelled()));
  } catch (const std::exception& e) {
    // Nothing may propagate out of a slot: this runs from the worker thread's
    // event loop, where an escaping exception is std::terminate. Answering the
    // sequence also matters -- an unanswered request leaves the controller
    // busy forever.
    emit SignalFailed(seq, MailInternalError(QString::fromUtf8(e.what())));
  } catch (...) {
    emit SignalFailed(seq, MailInternalError("unknown error"));
  }
}

void EMailImapWorker::FindInSentFolder(quint64 seq, const QString& message_id) {
  const CancelScope cancel_scope(token_, seq, impl_->timeouts);

  // A stop that arrived while this request was still queued applies to it:
  // there is no point opening a socket for work nobody wants any more.
  if (cancel_scope.Cancelled()) {
    emit SignalFailed(seq, MailCancelledError());
    return;
  }

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

    // Escaped, and refused outright if it cannot be. On the byte-preserving
    // send path this identifier is read out of a message the user loaded from
    // elsewhere, so it is chosen by whoever wrote that message -- a quote in
    // it would end the search string early and the rest would be read as more
    // of the command.
    const auto needle = ImapQuotable(NormalizeMessageId(message_id));
    if (!needle) {
      // Not an error worth stopping the user over: the copy in Sent simply
      // cannot be confirmed by searching for an identifier like this one.
      emit SignalSentLookup(seq, false, false, path);
      return;
    }

    vmime::net::imap::IMAPSearchAttributes attributes;
    attributes.add(vmime::net::imap::IMAPSearchTokenFactory::HEADER(
        "Message-ID", *needle));
    auto uids = imap->getMessageUIDsMatchingSearchAttributes(attributes);

    emit SignalSentLookup(seq, true, !uids.empty(), path);
  } catch (const vmime::exception& e) {
    emit SignalFailed(
        seq, ClassifyVmimeException(
                 e, MailStage::kLISTING,
                 impl_->timeouts && impl_->timeouts->LastWasCancelled()));
  } catch (const std::exception& e) {
    // Nothing may propagate out of a slot: this runs from the worker thread's
    // event loop, where an escaping exception is std::terminate. Answering the
    // sequence also matters -- an unanswered request leaves the controller
    // busy forever.
    emit SignalFailed(seq, MailInternalError(QString::fromUtf8(e.what())));
  } catch (...) {
    emit SignalFailed(seq, MailInternalError("unknown error"));
  }
}

void EMailImapWorker::Disconnect() { impl_->Close(); }

auto EMailImapWorker::OpenSentFolderForWrite(const QString& path)
    -> vmime::shared_ptr<vmime::net::folder> {
  if (!impl_->store) return nullptr;

  // The cached read-only folder goes first, whatever path it holds: vmime
  // refuses open() while any other live object shares the path, so a cached
  // Sent folder would make this fail every time.
  if (impl_->folder) {
    try {
      if (impl_->folder->isOpen()) impl_->folder->close(false);
    } catch (...) {
    }
    impl_->folder = nullptr;
    impl_->folder_path.clear();
  }

  auto folder = impl_->store->getFolder(vmime::utility::path::fromString(
      path.toStdString(), "/", vmime::charsets::UTF_8));

  // Deliberately NOT stored in impl_->folder. Nothing may inherit a writable
  // handle: the next reader opens its own, read-only, as it always did.
  folder->open(vmime::net::folder::MODE_READ_WRITE, true);
  return folder;
}

namespace {

/// Whether @p folder already holds a message carrying @p message_id.
auto SentFolderHolds(const vmime::shared_ptr<vmime::net::folder>& folder,
                     const QString& message_id) -> bool {
  auto imap = vmime::dynamicCast<vmime::net::imap::IMAPFolder>(folder);
  if (!imap || message_id.isEmpty()) return false;

  // See FindInSentFolder: this identifier comes out of message bytes, so it is
  // escaped, and a value that cannot be escaped is treated as "not found"
  // rather than sent. Filing a second copy is a far smaller problem than
  // letting a message dictate part of an IMAP command.
  const auto needle = ImapQuotable(message_id);
  if (!needle) return false;

  vmime::net::imap::IMAPSearchAttributes attributes;
  attributes.add(
      vmime::net::imap::IMAPSearchTokenFactory::HEADER("Message-ID", *needle));
  return !imap->getMessageUIDsMatchingSearchAttributes(attributes).empty();
}

}  // namespace

void EMailImapWorker::SaveToSentFolder(quint64 seq, const QString& message_id,
                                       const QByteArray& eml) {
  const CancelScope cancel_scope(token_, seq, impl_->timeouts);

  // A stop that arrived while this request was still queued applies to it:
  // there is no point opening a socket for work nobody wants any more.
  if (cancel_scope.Cancelled()) {
    emit SignalFailed(seq, MailCancelledError());
    return;
  }

  vmime::shared_ptr<vmime::net::folder> folder;
  // Declared out here so the catch blocks can say WHERE the copy was being
  // filed. They used to report an empty folder, which the dialog then wrote
  // over the resolved name -- losing the one fact the user needs to go and
  // look for the message themselves.
  QString path;

  try {
    path = ResolveSentFolder();
    if (path.isEmpty()) {
      emit SignalSentSaved(seq, MailSentSaveOutcome::kUNRESOLVED, {}, {});
      return;
    }

    folder = OpenSentFolderForWrite(path);
    if (!folder) {
      emit SignalSentSaved(seq, MailSentSaveOutcome::kFAILED, path,
                           MailInternalError("cannot open the Sent folder"));
      return;
    }

    const auto needle = NormalizeMessageId(message_id);

    // Asked before anything is written. A server that files its own copy --
    // Gmail does, most do not -- would otherwise end up with two, and a
    // duplicate in Sent is a thing the user then has to clean up by hand.
    if (SentFolderHolds(folder, needle)) {
      folder->close(false);
      emit SignalSentSaved(seq, MailSentSaveOutcome::kALREADY_THERE, path, {});
      return;
    }

    // The stream overload, for the same reason the SMTP worker uses it: these
    // octets may be exactly what a signature covers, and anything that parses
    // and re-renders the message would rewrite them.
    vmime::utility::inputStreamStringAdapter input(
        std::string(eml.constData(), static_cast<size_t>(eml.size())));

    // Seen, because a message the user just sent has been read by definition.
    // Leaving it unread would put a bold entry in Sent and a "1 unread" badge
    // on a folder nobody reads.
    folder->addMessage(input, static_cast<size_t>(eml.size()),
                       vmime::net::message::FLAG_SEEN, nullptr, nullptr);

    // Read back rather than trusted. The append reported success, but what
    // matters to the user is that the copy is findable in the folder, and a
    // message with no Message-ID cannot be looked up at all -- which is
    // reported as the gap it is rather than as a confirmation.
    const auto verified = SentFolderHolds(folder, needle);
    folder->close(false);

    emit SignalSentSaved(seq,
                         verified ? MailSentSaveOutcome::kSAVED
                                  : MailSentSaveOutcome::kSAVED_UNVERIFIED,
                         path, {});
  } catch (const vmime::exception& e) {
    if (folder) {
      try {
        if (folder->isOpen()) folder->close(false);
      } catch (...) {
      }
    }

    // Reported as a failure to FILE the message, never as a failure to send
    // it. The send already succeeded and nothing here can undo that.
    emit SignalSentSaved(
        seq, MailSentSaveOutcome::kFAILED, path,
        ClassifyVmimeException(
            e, MailStage::kLISTING,
            impl_->timeouts && impl_->timeouts->LastWasCancelled()));
  } catch (const std::exception& e) {
    // See the SignalFailed slots above: nothing may escape a slot, and the
    // sequence has to be answered either way.
    emit SignalSentSaved(seq, MailSentSaveOutcome::kFAILED, path,
                         MailInternalError(QString::fromUtf8(e.what())));
  } catch (...) {
    emit SignalSentSaved(seq, MailSentSaveOutcome::kFAILED, path,
                         MailInternalError("unknown error"));
  }
}

auto EMailImapWorker::OpenFolderReadOnly(const QString& path)
    -> vmime::shared_ptr<vmime::net::folder> {
  if (!impl_->store) return nullptr;

  if (impl_->folder && impl_->folder->isOpen() && impl_->folder_path == path) {
    return impl_->folder;
  }

  // Release the previous folder outright rather than merely closing it. A
  // closed-but-still-alive IMAPFolder stays in the store's registry, and
  // open() refuses when any other live object shares the path -- so keeping
  // one around makes reopening the very same folder fail.
  if (impl_->folder) {
    try {
      if (impl_->folder->isOpen()) impl_->folder->close(false);
    } catch (...) {
    }
    impl_->folder = nullptr;
    impl_->folder_path.clear();
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
    // Attributes only exist on the objects a LIST produced -- a folder looked
    // up by path comes back with an empty attribute set -- so the cache is
    // filled from a listing rather than re-derived per folder.
    if (impl_->folder_cache.isEmpty()) {
      auto listed = impl_->store->getRootFolder()->getFolders(true);
      for (const auto& folder : listed) {
        const auto attributes = folder->getAttributes();

        EMailFolderInfo info;
        info.path = QString::fromStdString(
            folder->getFullPath().toString("/", vmime::charsets::UTF_8));
        info.selectable = (attributes.getFlags() &
                           vmime::net::folderAttributes::FLAG_NO_OPEN) == 0;
        info.is_sent = attributes.getSpecialUse() ==
                       vmime::net::folderAttributes::SPECIALUSE_SENT;

        if (!info.path.isEmpty()) impl_->folder_cache.append(info);
      }
      // Released before anything is opened; see folder_cache's comment.
      listed.clear();
    }

    for (const auto& info : impl_->folder_cache) {
      if (info.is_sent && info.selectable) return info.path;
    }
  } catch (...) {
    return {};
  }

  return {};
}
