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

#include "WebEngineView.h"

#include <QBuffer>
#include <QPointer>
#include <QVBoxLayout>
#include <QWebEngineDesktopMediaRequest>
#include <QWebEngineDownloadRequest>
#include <QWebEngineFileSystemAccessRequest>
#include <QWebEngineFullScreenRequest>
#include <QWebEnginePage>
#include <QWebEnginePermission>
#include <QWebEngineProfile>
#include <QWebEngineRegisterProtocolHandlerRequest>
#include <QWebEngineSettings>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlSchemeHandler>
#include <QWebEngineView>
#include <QWebEngineWebAuthUxRequest>
#include <memory>

#include "GFSDKTypes.h"

namespace {

auto PartScheme() -> QLatin1StringView {
  return QLatin1StringView(GF_HTML_PART_SCHEME);
}

/// Every request, before Chromium's network stack: the part scheme passes,
/// nothing else does. On the engine's IO thread, so it decides from the URL
/// alone and holds no state.
class Interceptor : public QWebEngineUrlRequestInterceptor {
 public:
  using QWebEngineUrlRequestInterceptor::QWebEngineUrlRequestInterceptor;

  void interceptRequest(QWebEngineUrlRequestInfo& info) override {
    if (info.requestUrl().scheme() != PartScheme()) info.block(true);
  }
};

/// gfpart://<session>/ is the document; gfpart://<session>/<cid> a part,
/// which comes from the Host and only for the session now shown.
class PartHandler : public QWebEngineUrlSchemeHandler {
 public:
  explicit PartHandler(WebEngineView* view)
      : QWebEngineUrlSchemeHandler(view), view_(view) {}

  void requestStarted(QWebEngineUrlRequestJob* job) override {
    if (job->requestMethod() != "GET") {
      job->fail(QWebEngineUrlRequestJob::RequestDenied);
      return;
    }
    const auto url = job->requestUrl();
    const auto session = url.host();
    const auto path = url.path(QUrl::FullyEncoded);

    if (path.isEmpty() || path == u"/") {
      const auto* document = view_->Document(session);
      if (document == nullptr) {
        job->fail(QWebEngineUrlRequestJob::RequestDenied);
        return;
      }
      auto* buffer = new QBuffer(job);
      buffer->setData(*document);
      buffer->open(QIODevice::ReadOnly);
      job->reply("text/html;charset=utf-8", buffer);
      return;
    }

    const auto cid = QUrl::fromPercentEncoding(path.mid(1).toUtf8());
    QPointer<QWebEngineUrlRequestJob> alive(job);
    auto gone = std::make_shared<QMetaObject::Connection>();
    const auto request = view_->HostCalls().request_part(
        session, cid,
        [alive, gone](int status, const QString& mime, const QByteArray& data) {
          QObject::disconnect(*gone);
          if (alive.isNull()) return;
          if (status != GF_HTML_PART_OK) {
            alive->fail(QWebEngineUrlRequestJob::UrlNotFound);
            return;
          }
          auto* buffer = new QBuffer(alive.data());
          buffer->setData(data);
          buffer->open(QIODevice::ReadOnly);
          alive->reply(mime.toUtf8(), buffer);
        });
    if (request == 0) {
      job->fail(QWebEngineUrlRequestJob::RequestDenied);
      return;
    }
    // The engine gave up on it (the frame went, the view was cleared): so
    // does the Host.
    const auto cancel = view_->HostCalls().cancel_part;
    *gone = QObject::connect(job, &QObject::destroyed, view_,
                             [cancel, request]() { cancel(request); });
  }

 private:
  WebEngineView* view_;
};

void LockDown(QWebEngineSettings* s) {
  using A = QWebEngineSettings;
  // Images are what parts are; everything else that could act, fetch or
  // reach outside the page is off.
  s->setAttribute(A::AutoLoadImages, true);
  for (const auto off : {
           A::JavascriptEnabled,
           A::JavascriptCanOpenWindows,
           A::JavascriptCanAccessClipboard,
           A::JavascriptCanPaste,
           A::LocalStorageEnabled,
           A::LocalContentCanAccessRemoteUrls,
           A::LocalContentCanAccessFileUrls,
           A::HyperlinkAuditingEnabled,
           A::ErrorPageEnabled,
           A::PluginsEnabled,
           A::PdfViewerEnabled,
           A::FullScreenSupportEnabled,
           A::ScreenCaptureEnabled,
           A::WebGLEnabled,
           A::Accelerated2dCanvasEnabled,
           A::AutoLoadIconsForPage,
           A::TouchIconsEnabled,
           A::FocusOnNavigationEnabled,
           A::AllowRunningInsecureContent,
           A::AllowGeolocationOnInsecureOrigins,
           A::AllowWindowActivationFromJavaScript,
           A::DnsPrefetchEnabled,
           A::NavigateOnDropEnabled,
           A::ReadingFromCanvasEnabled,
       }) {
    s->setAttribute(off, false);
  }
  s->setAttribute(A::PlaybackRequiresUserGesture, true);
  s->setAttribute(A::WebRTCPublicInterfacesOnly, true);
}

/**
 * @brief What a target=_blank link opens: a page never shown, that never
 *        loads.
 *
 * Its one navigation is the link the user clicked, which it hands to the
 * Host and refuses. No window exists at any point; a popup is only ever a
 * way for the click to reach the Host.
 */
class LinkCatcher : public QWebEnginePage {
 public:
  LinkCatcher(QWebEngineProfile* profile, WebEngineView* owner, QObject* parent)
      : QWebEnginePage(profile, parent), owner_(owner) {
    LockDown(settings());
  }

 protected:
  auto acceptNavigationRequest(const QUrl& url, NavigationType /*type*/,
                               bool /*is_main_frame*/) -> bool override {
    if (!reported_) {
      reported_ = true;
      owner_->HostCalls().link_activated(url.toString(QUrl::FullyEncoded));
    }
    deleteLater();
    return false;
  }

  auto createWindow(WebWindowType /*type*/) -> QWebEnginePage* override {
    return nullptr;
  }

 private:
  WebEngineView* owner_;
  bool reported_ = false;
};

/// The one document, its srcdoc frame, and nothing else ever loads.
class LockedPage : public QWebEnginePage {
 public:
  LockedPage(QWebEngineProfile* profile, WebEngineView* owner)
      : QWebEnginePage(profile, owner), owner_(owner) {}

 protected:
  auto acceptNavigationRequest(const QUrl& url, NavigationType type,
                               bool is_main_frame) -> bool override {
    if (is_main_frame && owner_->TakeExpectedMainLoad(url)) return true;
    // The sandboxed frame the body lives in: about:srcdoc, never a request.
    if (!is_main_frame && url.toString() == u"about:srcdoc") return true;
    // A link the user activated is the Host's to open, outside this view;
    // everything else -- a refresh, a form, a frame the body asks for -- is
    // simply refused.
    if (type == NavigationTypeLinkClicked) {
      owner_->HostCalls().link_activated(url.toString(QUrl::FullyEncoded));
      return false;
    }
    if (const auto& refused = owner_->HostCalls().refused) {
      refused(QString("navigation type %1, %2 frame: %3")
                  .arg(static_cast<int>(type))
                  .arg(is_main_frame ? "main" : "sub")
                  .arg(url.toString(QUrl::FullyEncoded)));
    }
    return false;
  }

  // The engine reports each load the policy refused as a console message.
  // Those are the policy working, one line per blocked image, so they go to
  // the refusal log rather than to the application's at warning level.
  void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel /*level*/,
                                const QString& message, int /*line*/,
                                const QString& /*source*/) override {
    if (const auto& refused = owner_->HostCalls().refused) refused(message);
  }

  // Only a click opens one -- there is no script to, and the engine's popup
  // blocker wants a user gesture -- and it opens nothing that is shown. A
  // child of this page, so it goes before the profile does.
  auto createWindow(WebWindowType /*type*/) -> QWebEnginePage* override {
    return new LinkCatcher(profile(), owner_, this);
  }

 private:
  WebEngineView* owner_;
};

}  // namespace

WebEngineView::WebEngineView(Host host, QWidget* parent)
    : QWidget(parent), host_(std::move(host)) {
  // No storage name: off the record, nothing written to disk, and nothing
  // shared with any other view.
  profile_ = new QWebEngineProfile(this);
  profile_->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
  profile_->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
  profile_->setPersistentPermissionsPolicy(
      QWebEngineProfile::PersistentPermissionsPolicy::AskEveryTime);
  profile_->setSpellCheckEnabled(false);
  profile_->setUrlRequestInterceptor(new Interceptor(profile_));
  profile_->installUrlSchemeHandler(GF_HTML_PART_SCHEME, new PartHandler(this));
  connect(profile_, &QWebEngineProfile::downloadRequested, this,
          [](QWebEngineDownloadRequest* download) { download->cancel(); });
  LockDown(profile_->settings());

  page_ = new LockedPage(profile_, this);
  LockDown(page_->settings());
  connect(page_, &QWebEnginePage::permissionRequested, this,
          [](QWebEnginePermission permission) { permission.deny(); });
  connect(page_, &QWebEnginePage::fullScreenRequested, this,
          [](QWebEngineFullScreenRequest request) { request.reject(); });
  connect(page_, &QWebEnginePage::fileSystemAccessRequested, this,
          [](QWebEngineFileSystemAccessRequest request) { request.reject(); });
  connect(page_, &QWebEnginePage::registerProtocolHandlerRequested, this,
          [](QWebEngineRegisterProtocolHandlerRequest request) {
            request.reject();
          });
  connect(
      page_, &QWebEnginePage::desktopMediaRequested, this,
      [](const QWebEngineDesktopMediaRequest& request) { request.cancel(); });
  connect(page_, &QWebEnginePage::webAuthUxRequested, this,
          [](QWebEngineWebAuthUxRequest* request) { request->cancel(); });
  // The document's own load, reported once. Every refused navigation also
  // ends in loadFinished(false), and that is not the document failing.
  connect(page_, &QWebEnginePage::loadFinished, this, [this](bool ok) {
    if (!awaiting_load_) return;
    awaiting_load_ = false;
    if (!session_.isEmpty() && host_.loaded) host_.loaded(ok);
  });

  view_ = new QWebEngineView(this);
  view_->setPage(page_);
  view_->setContextMenuPolicy(Qt::NoContextMenu);
  view_->setAcceptDrops(false);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(view_);
}

WebEngineView::~WebEngineView() {
  // A page must go before its profile, and the view before the page it
  // shows; child order would release them the other way round.
  delete view_;
  delete page_;
  delete profile_;
  if (!document_.isEmpty()) document_.fill('\0');
}

void WebEngineView::Load(const QString& session, const QByteArray& document) {
  if (!document_.isEmpty()) document_.fill('\0');
  session_ = session;
  document_ = document;
  expected_main_ = QUrl(QString("%1://%2/").arg(PartScheme(), session));
  awaiting_load_ = true;
  view_->setUrl(expected_main_);
}

void WebEngineView::Clear() {
  session_.clear();
  if (!document_.isEmpty()) document_.fill('\0');
  document_.clear();
  expected_main_ = QUrl(QStringLiteral("about:blank"));
  awaiting_load_ = false;
  view_->setUrl(expected_main_);
}

auto WebEngineView::Document(const QString& session) const
    -> const QByteArray* {
  if (session_.isEmpty() || session != session_) return nullptr;
  return &document_;
}

auto WebEngineView::TakeExpectedMainLoad(const QUrl& url) -> bool {
  if (expected_main_.isEmpty() || url != expected_main_) return false;
  expected_main_.clear();
  return true;
}
