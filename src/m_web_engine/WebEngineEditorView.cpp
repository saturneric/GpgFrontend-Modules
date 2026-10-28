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

#include "WebEngineEditorView.h"

#include <QFile>
#include <QVBoxLayout>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlSchemeHandler>
#include <QWebEngineView>

#include "GFSDKTypes.h"
#include "WebEngineLockdown.h"

namespace {

/// The two schemes pass, nothing else does.
auto IsEditorScheme(const QString& scheme) -> bool {
  return scheme == QLatin1StringView(GF_WEB_EDITOR_ASSET_SCHEME) ||
         scheme == QLatin1StringView(GF_WEB_EDITOR_RESOURCE_SCHEME);
}

/// Answered by the Host, synchronously, on the GUI thread.
class Handler : public QWebEngineUrlSchemeHandler {
 public:
  explicit Handler(WebEngineEditorView* view)
      : QWebEngineUrlSchemeHandler(view), view_(view) {}

  void requestStarted(QWebEngineUrlRequestJob* job) override {
    if (job->requestMethod() != "GET") {
      job->fail(QWebEngineUrlRequestJob::RequestDenied);
      return;
    }
    QByteArray data;
    QString mime;
    if (!view_->HostCalls().resolve(
            job->requestUrl().toString(QUrl::FullyEncoded), &data, &mime)) {
      job->fail(QWebEngineUrlRequestJob::UrlNotFound);
      return;
    }
    web_engine::ReplyBytes(job, mime.toUtf8(), data);
  }

 private:
  WebEngineEditorView* view_;
};

class EditorPage : public QWebEnginePage {
 public:
  EditorPage(QWebEngineProfile* profile, WebEngineEditorView* owner)
      : QWebEnginePage(profile, owner), owner_(owner) {}

 protected:
  auto acceptNavigationRequest(const QUrl& url, NavigationType type,
                               bool is_main_frame) -> bool override {
    if (is_main_frame && owner_->TakeExpectedMainLoad(url)) return true;
    if (type == NavigationTypeLinkClicked) {
      owner_->HostCalls().link_activated(url.toString(QUrl::FullyEncoded));
    }
    return false;
  }

  auto createWindow(WebWindowType /*type*/) -> QWebEnginePage* override {
    return nullptr;
  }

  // Never logged: a console line may quote the document.
  void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel /*level*/,
                                const QString& /*message*/, int /*line*/,
                                const QString& /*source*/) override {}

 private:
  WebEngineEditorView* owner_;
};

/// qwebchannel.js, then `window.gfBridge` over it. Injected by the engine,
/// not served: the page's policy does not apply to it, and the page cannot
/// replace it.
auto BridgeScript() -> QString {
  QFile channel(QStringLiteral(":/qtwebchannel/qwebchannel.js"));
  channel.open(QIODevice::ReadOnly);
  return QString::fromUtf8(channel.readAll()) + QStringLiteral(R"JS(
(function () {
  "use strict";
  var host = null, outbox = [], listeners = [];
  function deliver(json) {
    var obj;
    try { obj = JSON.parse(json); } catch (e) { return; }
    listeners.forEach(function (fn) { fn(obj); });
  }
  Object.defineProperty(window, "gfBridge", {
    configurable: false, writable: false,
    value: Object.freeze({
      post: function (obj) {
        var json = JSON.stringify(obj);
        if (host) host.post(json); else outbox.push(json);
      },
      onMessage: function (fn) { if (typeof fn === "function") listeners.push(fn); }
    })
  });
  new QWebChannel(qt.webChannelTransport, function (channel) {
    host = channel.objects.gfHost;
    host.SignalToPage.connect(deliver);
    outbox.splice(0).forEach(function (json) { host.post(json); });
  });
})();
)JS");
}

}  // namespace

WebEngineEditorView::WebEngineEditorView(const QString& session, Host host,
                                         QWidget* parent)
    : QWidget(parent), host_(std::move(host)) {
  index_ =
      QUrl(QString("%1://%2/index.html")
               .arg(QLatin1StringView(GF_WEB_EDITOR_ASSET_SCHEME), session));

  profile_ = web_engine::NewLockedProfile(this, &IsEditorScheme);
  auto* handler = new Handler(this);
  profile_->installUrlSchemeHandler(GF_WEB_EDITOR_ASSET_SCHEME, handler);
  profile_->installUrlSchemeHandler(GF_WEB_EDITOR_RESOURCE_SCHEME, handler);

  page_ = new EditorPage(profile_, this);
  web_engine::LockDown(page_->settings());
  for (auto* settings : {profile_->settings(), page_->settings()}) {
    // The one thing an editor needs that a mail view must not have.
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
  }
  web_engine::RefuseUserRequests(page_);

  auto* channel = new QWebChannel(this);
  bridge_ = new EditorBridge(
      [this](const QByteArray& json) {
        if (host_.message) host_.message(json);
      },
      this);
  channel->registerObject(QStringLiteral("gfHost"), bridge_);
  page_->setWebChannel(channel, QWebEngineScript::MainWorld);
  QWebEngineScript bridge;
  bridge.setName(QStringLiteral("gf-bridge"));
  bridge.setSourceCode(BridgeScript());
  bridge.setInjectionPoint(QWebEngineScript::DocumentCreation);
  bridge.setWorldId(QWebEngineScript::MainWorld);
  bridge.setRunsOnSubFrames(false);
  page_->scripts().insert(bridge);

  connect(page_, &QWebEnginePage::loadFinished, this, [this](bool ok) {
    if (!awaiting_load_) return;
    awaiting_load_ = false;
    if (ok && host_.ready) host_.ready();
  });
  // A crashed renderer took the document with it: say so, then start again.
  connect(page_, &QWebEnginePage::renderProcessTerminated, this,
          [this](QWebEnginePage::RenderProcessTerminationStatus status, int) {
            if (status == QWebEnginePage::NormalTerminationStatus) return;
            if (host_.message) host_.message(R"({"type":"engine.crashed"})");
            load();
          });

  view_ = new QWebEngineView(this);
  view_->setPage(page_);
  view_->setContextMenuPolicy(Qt::NoContextMenu);
  view_->setAcceptDrops(false);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(view_);
  load();
}

WebEngineEditorView::~WebEngineEditorView() {
  // A page goes before its profile, and the view before the page.
  delete view_;
  delete page_;
  delete profile_;
}

void WebEngineEditorView::load() {
  expected_main_ = index_;
  awaiting_load_ = true;
  page_->setUrl(index_);
}

void WebEngineEditorView::Post(const QByteArray& json) {
  emit bridge_->SignalToPage(QString::fromUtf8(json));
}

auto WebEngineEditorView::TakeExpectedMainLoad(const QUrl& url) -> bool {
  if (expected_main_.isEmpty() || url != expected_main_) return false;
  expected_main_.clear();
  return true;
}
