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

#include <GFSDKBasic.h>

#include <QCoreApplication>
#include <QFile>
#include <QIODevice>
#include <QString>

#include "GFModuleLog.h"
#include "GFModuleMemory.h"

/**
 * @file GFModuleI18n.h
 * @brief Translating a module's own strings.
 *
 * A module compiles its `.qm` files into itself as Qt resources, and hands
 * them to the host on request. Two lines set that up:
 *
 *     DEFINE_TRANSLATIONS_STRUCTURE()   // once, at file scope
 *     REGISTER_TRANS_READER()           // once, during activation
 *
 * The context name is no longer passed in. It comes from
 * `GF_MODULE_TRANSLATION_CONTEXT`, generated from `module.json` and supplied by
 * `GFModuleIdentity.h` -- which this header does not include, because a macro
 * is expanded where it is used: the one translation unit that bootstraps has
 * that definition, and no other source needs it. It comes from one place
 * because it used to be written out three times -- here, in
 * `gpgfrontend_collect_ts_files` and in the `.ts` filenames -- and nothing
 * checked that the three agreed. A context that disagrees with its `.ts` files
 * does not fail to build; it silently produces a module that is never
 * translated.
 */

#define GF_CONCATENATE_DETAIL(x, y) x##y
#define GF_CONCATENATE(x, y) GF_CONCATENATE_DETAIL(x, y)
#define GF_STRINGIFY(x) #x
#define GF_TOSTRING(x) GF_STRINGIFY(x)

/// Mark a string for translation in the module's own context.
#define GC_TR(text) QT_TRANSLATE_NOOP("GTrC", text)

#define GTRC_TR(name, src) GF_CONCATENATE(GTrC_, name)::tr(src)
#define GTRC_AS_STRING(name) GF_TOSTRING(GTrC_##name)

/**
 * @brief Define the module's translation context and its `.qm` reader.
 *
 * Expands at file scope, once per module. The reader is what
 * REGISTER_TRANS_READER() hands to the host.
 */
#define DEFINE_TRANSLATIONS_STRUCTURE()                                  \
  class GTrC {                                                           \
    Q_DECLARE_TR_FUNCTIONS(GTrC)                                         \
  };                                                                     \
  auto TranslatorDataReader(const char* p_l, char** p_d) -> int {        \
    auto locale = QString::fromUtf8(p_l == nullptr ? "" : p_l);          \
    QFile f(QString(":/i18n/%2.%1.qm")                                   \
                .arg(locale)                                             \
                .arg(GF_MODULE_TRANSLATION_CONTEXT));                    \
    if (f.exists() && f.open(QIODevice::ReadOnly)) {                     \
      auto b = f.readAll();                                              \
      *p_d = AllocBufferAndCopy(b);                                      \
      return b.size();                                                   \
    }                                                                    \
    FLOG_WARN("%3 loading, locale: %1, not found", locale, f.fileName(), \
              QString::fromUtf8(GFGetModuleID()));                       \
    *p_d = nullptr;                                                      \
    return 0;                                                            \
  }

/// Tell the host where to get this module's translations.
#define REGISTER_TRANS_READER() \
  GFAppRegisterTranslatorReader(GFGetModuleID(), TranslatorDataReader)
