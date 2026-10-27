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

class QWebEnginePage;
class QWebEngineProfile;
class QWebEngineView;

/**
 * @file WebEngineView.h
 * @brief One HTML view on QtWebEngine, locked down by the engine itself.
 *
 * Knows nothing of the SDK: what it may ask of the Host arrives as four
 * functions, so the policy enforcement can be driven by a probe exactly as
 * the module drives it.
 *
 * What enforces what:
 *  - nothing leaves the process: a request interceptor blocks every URL
 *    that is not gfpart:, before Chromium's network stack sees it;
 *  - parts only: gfpart: is served by this view's own scheme handler, from
 *    the Host, for the current session alone;
 *  - no script, plugins, PDF viewer, WebGL, DNS prefetch, hyperlink pings,
 *    error pages, full screen, screen capture: off in the page settings;
 *  - no navigation: only the document's own URL and its srcdoc frame are
 *    accepted; a link the user activates goes to the Host instead;
 *  - no popups: createWindow() makes none;
 *  - no downloads: every download is cancelled as it is requested;
 *  - no permissions: every request is denied, in every form Qt offers it;
 *  - no persistence: an off-the-record profile per view, with a memory
 *    cache and no cookies on disk.
 */
class WebEngineView : public QWidget {
 public:
  /// What the view may ask of whoever made it.
  struct Host {
    /// Ask for a part of @p session; the request id, 0 when refused. The
    /// callback gets GF_HTML_PART_* status, the MIME type and the bytes.
    std::function<quint64(
        const QString& session, const QString& cid,
        std::function<void(int, const QString&, const QByteArray&)>)>
        request_part;
    std::function<void(quint64)> cancel_part;
    std::function<void(const QString& url)> link_activated;
    std::function<void(bool ok)> loaded;
    /// Optional: every navigation refused, said for the log.
    std::function<void(const QString& what)> refused;
  };

  explicit WebEngineView(Host host, QWidget* parent = nullptr);
  ~WebEngineView() override;

  /// Show @p document, whose own URL is gfpart://<session>/.
  void Load(const QString& session, const QByteArray& document);
  /// Show nothing, and forget the document.
  void Clear();

  /// For the scheme handler: the document for @p session, or nullptr.
  [[nodiscard]] auto Document(const QString& session) const
      -> const QByteArray*;
  [[nodiscard]] auto HostCalls() const -> const Host& { return host_; }
  /// For the page: whether @p url is the one main-frame load expected now,
  /// which it is at most once per Load().
  auto TakeExpectedMainLoad(const QUrl& url) -> bool;
  /// For the policy probe, which clicks the links in it.
  [[nodiscard]] auto Page() const -> QWebEnginePage* { return page_; }

 private:
  Host host_;
  QWebEngineProfile* profile_ = nullptr;
  QWebEnginePage* page_ = nullptr;
  QWebEngineView* view_ = nullptr;
  QString session_;
  QByteArray document_;
  QUrl expected_main_;
  bool awaiting_load_ = false;
};
