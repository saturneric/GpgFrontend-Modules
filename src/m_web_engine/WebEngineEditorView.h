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

#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QWidget>
#include <functional>

class EditorBridge;
class QWebEnginePage;
class QWebEngineProfile;
class QWebEngineView;

/**
 * @file WebEngineEditorView.h
 * @brief An editable page of a module's own, in QtWebEngine.
 *
 * What the engine does to keep to the Host's rules:
 *  - off the record: no profile path, memory cache, no persistent cookies,
 *    LocalStorage off; nothing the page holds reaches the disk;
 *  - every request that is not gfasset: or gfres: is blocked before
 *    Chromium's network stack, and those two are answered by the Host;
 *  - one main-frame load, ever: gfasset://<session>/index.html; links go to
 *    the Host, every other navigation, pop-up, download and permission is
 *    refused;
 *  - script runs, under the Host's policy in index.html: only the module's
 *    own files, no eval, no inline script;
 *  - the bridge is a QWebChannel exposed to the page as `window.gfBridge`
 *    ({post(obj), onMessage(fn)}) by a script the engine injects itself.
 */
class WebEngineEditorView : public QWidget {
  Q_OBJECT

 public:
  struct Host {
    /// What the page asked for: false when the Host does not serve it.
    std::function<bool(const QString& url, QByteArray* data, QString* mime)>
        resolve;
    std::function<void(const QByteArray& json)> message;
    std::function<void(const QString& url)> link_activated;
    std::function<void()> ready;
  };

  WebEngineEditorView(const QString& session, Host host,
                      QWidget* parent = nullptr);
  ~WebEngineEditorView() override;

  /// Deliver one JSON object to the page.
  void Post(const QByteArray& json);

  [[nodiscard]] auto HostCalls() const -> const Host& { return host_; }
  auto TakeExpectedMainLoad(const QUrl& url) -> bool;

 private:
  void load();

  Host host_;
  QUrl index_;
  QUrl expected_main_;
  bool awaiting_load_ = false;
  QWebEngineProfile* profile_ = nullptr;
  QWebEnginePage* page_ = nullptr;
  QWebEngineView* view_ = nullptr;
  EditorBridge* bridge_ = nullptr;
};

/**
 * @brief The one object on the channel: a page can call post() and hear
 * SignalToPage, and nothing else. Never the view itself, whose every
 * property and slot the channel would otherwise publish.
 */
class EditorBridge : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(EditorBridge)

 public:
  using Sink = std::function<void(const QByteArray&)>;
  EditorBridge(Sink sink, QObject* parent)
      : QObject(parent), sink_(std::move(sink)) {}

 signals:
  void SignalToPage(const QString& json);

 public slots:
  void post(const QString& json) {
    if (sink_) sink_(json.toUtf8());
  }

 private:
  Sink sink_;
};
