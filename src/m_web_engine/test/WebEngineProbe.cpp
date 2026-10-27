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

/**
 * @file WebEngineProbe.cpp
 * @brief Does QtWebEngine, as m_web_engine configures it, keep the mail
 *        policy? Everything that got past shows up at the probe logger.
 *
 *   gf_web_engine_probe <probe_mail.html> [seconds]   seconds 0: stay open
 *
 * Prints one "POLICY ..." line per decision, as the WKWebView probe does.
 *
 * With GF_PROBE_CONTROL set it shows the same mail in a stock QWebEngineView
 * instead, with none of the policy: the run that MUST reach the logger, so a
 * clean result from the real one means something.
 */

#include <QApplication>
#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QSet>
#include <QTimer>
#include <QWebEngineFrame>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineView>

#include "GFSDKTypes.h"
#include "HtmlViewDocument.h"
#include "WebEngineScheme.h"
#include "WebEngineView.h"

namespace HtmlView = GpgFrontend::UI::HtmlView;

namespace {

void Log(const QString& what) {
  printf("POLICY %s\n", what.toUtf8().constData());
  fflush(stdout);
}

auto PartImage() -> QByteArray {
  QImage image(16, 16, QImage::Format_RGB32);
  image.fill(Qt::darkGreen);
  QByteArray png;
  QBuffer buffer(&png);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "PNG");
  return png;
}

}  // namespace

auto main(int argc, char* argv[]) -> int {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <probe_mail.html> [seconds]\n", argv[0]);
    return 2;
  }
  // As the Host does, before the application exists.
  QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
  QApplication app(argc, argv);

  // As the module does at activation, before anything of WebEngine exists.
  ApplyEngineSwitches();
  if (!RegisterPartScheme()) {
    Log(u"scheme FAILED: the engine refused gfpart"_s);
    return 3;
  }
  Log(u"scheme registered: "_s + QString::fromUtf8(GF_HTML_PART_SCHEME));

  QFile file(QString::fromLocal8Bit(argv[1]));
  if (!file.open(QIODevice::ReadOnly)) {
    fprintf(stderr, "cannot read %s\n", argv[1]);
    return 2;
  }
  const auto body = QString::fromUtf8(file.readAll());
  const double seconds = argc > 2 ? QString(argv[2]).toDouble() : 8.0;

  if (qEnvironmentVariableIsSet("GF_PROBE_CONTROL")) {
    Log(u"control: a stock view, no policy"_s);
    QWebEngineView stock;
    stock.resize(900, 800);
    stock.show();
    stock.setHtml(body, QUrl(u"http://127.0.0.1:8765/control/"_s));
    QTimer::singleShot(static_cast<int>(seconds * 1000), &app, [&]() {
      Log(u"done"_s);
      app.quit();
    });
    return app.exec();
  }

  const auto session = HtmlView::NewSession();
  const auto document = HtmlView::BuildDocument(body, session);
  const QHash<QString, QByteArray> parts = {
      {HtmlView::NormalizeCid(u"probe-part@gpgfrontend"_s), PartImage()}};
  QSet<quint64> cancelled;
  quint64 next_request = 0;

  WebEngineView::Host host;
  // The Host's rules, in miniature: the current session, a known part, and
  // the answer never inside the request.
  host.request_part =
      [&](const QString& s, const QString& cid,
          std::function<void(int, const QString&, const QByteArray&)> done)
      -> quint64 {
    if (s != session) {
      Log("part refuse (session) " + cid);
      return 0;
    }
    const auto id = ++next_request;
    QTimer::singleShot(0, &app, [&, id, cid, done]() {
      if (cancelled.contains(id)) return;
      const auto it = parts.constFind(HtmlView::NormalizeCid(cid));
      if (it == parts.constEnd()) {
        Log("part refuse (unknown) " + cid);
        done(GF_HTML_PART_NOT_FOUND, {}, {});
        return;
      }
      Log("part serve " + cid);
      done(GF_HTML_PART_OK, u"image/png"_s, *it);
    });
    return id;
  };
  host.cancel_part = [&](quint64 id) {
    cancelled.insert(id);
    Log("part cancel " + QString::number(id));
  };
  host.refused = [](const QString& what) { Log("refuse " + what); };
  host.link_activated = [](const QString& url) {
    Log("link_activated " + url);
  };
  // Click each link in the mail's frame as a person would: a real mouse
  // press and release, because a script's click() carries no user
  // activation and the policy treats the two differently on purpose. Where
  // each link is comes from the probe's own script world; the page's world
  // has no script and must keep none. Every click must come back as
  // link_activated, or as nothing at all -- never as a request.
  WebEngineView* shown = nullptr;
  std::function<void(int)> click_next = [&](int i) {
    const auto frames = shown->Page()->mainFrame().children();
    if (frames.isEmpty()) {
      Log(u"clicks: the mail frame is missing"_s);
      return;
    }
    auto frame = frames.first();
    frame.runJavaScript(
        QString("(() => { const a = document.querySelectorAll('a, button')[%1];"
                " if (!a) return '';"
                " a.scrollIntoView({block: 'center'});"
                " const r = a.getBoundingClientRect();"
                " return JSON.stringify([r.x + r.width / 2, r.y + r.height / 2,"
                " a.getAttribute('href') || a.outerHTML]); })()")
            .arg(i),
        QWebEngineScript::ApplicationWorld, [&, i](const QVariant& result) {
          const auto json = result.toString();
          if (json.isEmpty()) {
            Log("clicks done: " + QString::number(i));
            return;
          }
          const auto where = QJsonDocument::fromJson(json.toUtf8()).array();
          const QPointF at(where.at(0).toDouble(), where.at(1).toDouble());
          Log("click " + where.at(2).toString());
          auto* engine = shown->findChild<QWebEngineView*>();
          auto* target = engine->focusProxy();
          const auto global = target->mapToGlobal(at);
          QMouseEvent press(QEvent::MouseButtonPress, at, global,
                            Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
          QMouseEvent release(QEvent::MouseButtonRelease, at, global,
                              Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
          QCoreApplication::sendEvent(target, &press);
          QCoreApplication::sendEvent(target, &release);
          QTimer::singleShot(500, &app, [&, i]() { click_next(i + 1); });
        });
  };
  bool clicked = false;
  host.loaded = [&](bool ok) {
    Log(ok ? u"document finished"_s : u"document failed"_s);
    if (!ok || clicked || qEnvironmentVariableIsSet("GF_PROBE_NO_CLICKS")) {
      return;
    }
    clicked = true;
    QTimer::singleShot(500, &app, [&]() { click_next(0); });
  };

  WebEngineView view(host);
  shown = &view;
  view.setWindowTitle(u"HTML View policy probe (QtWebEngine)"_s);
  view.resize(900, 800);
  view.show();
  view.Load(session, document);

  if (seconds > 0) {
    QTimer::singleShot(static_cast<int>(seconds * 1000), &app, [&]() {
      Log(u"done"_s);
      app.quit();
    });
  }
  return app.exec();
}
