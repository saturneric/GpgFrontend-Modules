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

#include <GFModule.h>

#include <QHash>
#include <QPointer>

#include "GFModuleIdentity.h"
#include "GFModuleWebEditor.h"
#include "WebEngineEditorView.h"
#include "WebEngineScheme.h"
#include "WebEngineView.h"

/**
 * @file WebEngineModule.cpp
 * @brief QtWebEngine as the Host's HTML renderer.
 *
 * Provides one thing: the engine behind read-only HTML views, for whichever
 * module embeds one. What a view may show, and which parts it may ask for,
 * is decided by the Host; this module only makes the engine keep to it.
 */

namespace {

class Renderer : public gf::ui::HtmlViewRenderer {
 public:
  auto Create(quint64 view, const GFHtmlViewPolicy& policy)
      -> QWidget* override {
    // A bit this engine cannot enforce means no view, not a weaker one.
    if ((policy.required & ~kWebEngineEnforced) != 0) return nullptr;

    WebEngineView::Host host;
    host.request_part =
        [view](const QString& session, const QString& cid,
               std::function<void(int, const QString&, const QByteArray&)> done)
        -> quint64 { return RequestPart(view, session, cid, std::move(done)); };
    host.cancel_part = [](quint64 request) { CancelPart(request); };
    host.link_activated = [view](const QString& url) {
      LinkActivated(view, url);
    };
    host.loaded = [view](bool ok) { Loaded(view, ok); };
    host.refused = [](const QString& what) { LOG_D() << "refused:" << what; };

    auto* widget = new WebEngineView(std::move(host));
    views_.insert(view, widget);
    return widget;
  }

  auto Load(quint64 view, const QString& session, const QByteArray& document)
      -> bool override {
    auto* widget = views_.value(view).data();
    if (widget == nullptr) return false;
    widget->Load(session, document);
    return true;
  }

  void Clear(quint64 view) override {
    if (auto* widget = views_.value(view).data()) widget->Clear();
  }

  void Destroyed(quint64 view) override { views_.remove(view); }

 private:
  /// Not owned: each widget is the Host's once returned.
  QHash<quint64, QPointer<WebEngineView>> views_;
};

class EditorEngine : public gf::ui::WebEditorEngine {
 public:
  auto Create(quint64 editor, const QString& session) -> QWidget* override {
    WebEngineEditorView::Host host;
    host.resolve = [editor](const QString& url, QByteArray* data,
                            QString* mime) {
      return Resolve(editor, url, data, mime);
    };
    host.message = [editor](const QByteArray& json) { Message(editor, json); };
    host.link_activated = [editor](const QString& url) {
      LinkActivated(editor, url);
    };
    host.ready = [editor]() { Ready(editor); };
    auto* widget = new WebEngineEditorView(session, std::move(host));
    editors_.insert(editor, widget);
    return widget;
  }

  auto Post(quint64 editor, const QByteArray& json) -> bool override {
    auto* widget = editors_.value(editor).data();
    if (widget == nullptr) return false;
    widget->Post(json);
    return true;
  }

  void Destroyed(quint64 editor) override { editors_.remove(editor); }

 private:
  /// Not owned: each widget is the Host's once returned.
  QHash<quint64, QPointer<WebEngineEditorView>> editors_;
};

auto OnActivate() -> GFResult {
  // Both before anything of WebEngine exists: the engine reads its switches
  // and locks its schemes when it starts, and ignores both afterwards.
  ApplyEngineSwitches();
  const bool scheme = RegisterPartScheme();
  const bool editor_schemes = RegisterEditorSchemes();
  if (!scheme) {
    LOG_WARN(
        "QtWebEngine had already started, so the part scheme could not be "
        "registered; HTML views wait for a restart");
  }
  if (!editor_schemes) {
    LOG_WARN(
        "QtWebEngine had already started, so the editor schemes could not be "
        "registered; web editors wait for a restart");
  }
  const bool ok = gf::ui::RegisterHtmlViewRenderer(
      "qtwebengine", kWebEngineEnforced,
      scheme ? 0U : GF_HTML_PROVIDER_FEATURE_RESTART_REQUIRED,
      std::make_unique<Renderer>());
  const bool editor_ok = gf::ui::RegisterWebEditorEngine(
      "qtwebengine",
      editor_schemes ? 0U : GF_HTML_PROVIDER_FEATURE_RESTART_REQUIRED,
      std::make_unique<EditorEngine>());
  if (!ok) return GFResult::Fail("the HTML renderer was not registered");
  if (!editor_ok) return GFResult::Fail("the web editor was not registered");
  return GFResult::Ok();
}

const GFModuleHooks kHooks = {
    sizeof(GFModuleHooks),
    GF_MODULE_ID,
    GF_MODULE_VERSION,
    GF_MODULE_TRANSLATION_CONTEXT,
    &OnActivate,
    nullptr,  // the Host drains the renderer itself, and keeps it mapped
    nullptr,
    nullptr,  // no events
    0,
    nullptr,  // no commands
    0,
};

}  // namespace

extern "C" GF_MODULE_EXPORT auto GFModuleGetApi(uint32_t abi)
    -> const GFModuleApi* {
  return GFModuleRuntimeGetApi(abi, &kHooks);
}
