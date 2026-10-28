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

#include "WebEngineLockdown.h"

#include <QBuffer>
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
#include <QWebEngineWebAuthUxRequest>

namespace web_engine {

namespace {

class Interceptor : public QWebEngineUrlRequestInterceptor {
 public:
  Interceptor(SchemeFilter allowed, QObject* parent)
      : QWebEngineUrlRequestInterceptor(parent), allowed_(allowed) {}

  void interceptRequest(QWebEngineUrlRequestInfo& info) override {
    if (!allowed_(info.requestUrl().scheme())) info.block(true);
  }

 private:
  SchemeFilter allowed_;
};

}  // namespace

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

auto NewLockedProfile(QObject* owner, SchemeFilter allowed)
    -> QWebEngineProfile* {
  auto* profile = new QWebEngineProfile(owner);
  profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
  profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
  profile->setPersistentPermissionsPolicy(
      QWebEngineProfile::PersistentPermissionsPolicy::AskEveryTime);
  profile->setSpellCheckEnabled(false);
  profile->setUrlRequestInterceptor(new Interceptor(allowed, profile));
  QObject::connect(
      profile, &QWebEngineProfile::downloadRequested, profile,
      [](QWebEngineDownloadRequest* download) { download->cancel(); });
  LockDown(profile->settings());
  return profile;
}

void RefuseUserRequests(QWebEnginePage* page) {
  QObject::connect(page, &QWebEnginePage::permissionRequested, page,
                   [](QWebEnginePermission permission) { permission.deny(); });
  QObject::connect(
      page, &QWebEnginePage::fullScreenRequested, page,
      [](QWebEngineFullScreenRequest request) { request.reject(); });
  QObject::connect(
      page, &QWebEnginePage::fileSystemAccessRequested, page,
      [](QWebEngineFileSystemAccessRequest request) { request.reject(); });
  QObject::connect(page, &QWebEnginePage::registerProtocolHandlerRequested,
                   page, [](QWebEngineRegisterProtocolHandlerRequest request) {
                     request.reject();
                   });
  QObject::connect(
      page, &QWebEnginePage::desktopMediaRequested, page,
      [](const QWebEngineDesktopMediaRequest& request) { request.cancel(); });
  QObject::connect(
      page, &QWebEnginePage::webAuthUxRequested, page,
      [](QWebEngineWebAuthUxRequest* request) { request->cancel(); });
}

void ReplyBytes(QWebEngineUrlRequestJob* job, const QByteArray& mime,
                const QByteArray& data) {
  auto* buffer = new QBuffer(job);
  buffer->setData(data);
  buffer->open(QIODevice::ReadOnly);
  job->reply(mime, buffer);
}

}  // namespace web_engine
