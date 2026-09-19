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
#include <GFSDKModule.h>
#include <GFSDKUI.h>
#include <GFSDKUIModel.h>

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <cstring>
#include <string>
#include <type_traits>

#include "GFModuleMemory.h"

/**
 * @file GFModuleConvert.h
 * @brief Moving values across the C boundary, and formatting strings.
 *
 * Everything here converts between what a module works in (QString, QMap,
 * QList) and what the SDK speaks (char*, null-terminated arrays, linked
 * lists). The ownership rules that apply are in GFModuleMemory.h and are not
 * repeated: as a rule of thumb, a `Convert...ToMap` consumes what it is given
 * and a `ConvertMapTo...` produces something the callee will own.
 */

// ---------------------------------------------------------------- formatting

inline auto FormatStringHelper(const QString& format, const std::string& arg)
    -> QString {
  return format.arg(QString::fromStdString(arg));
}

template <typename T>
auto FormatStringHelper(const QString& format, T arg) -> QString {
  return format.arg(arg);
}

template <typename... Args>
auto FormatStringHelper(const QString& format, const std::string& arg,
                        Args... args) -> QString {
  return FormatStringHelper(format.arg(QString::fromStdString(arg)), args...);
}

template <typename T, typename... Args>
auto FormatStringHelper(const QString& format, T arg, Args... args) -> QString {
  return FormatStringHelper(format.arg(arg), args...);
}

inline auto FormatStringHelper(const QString& format) -> QString {
  return format;
}

template <typename T>
inline auto FormatStringHelper(const T& format) ->
    typename std::enable_if<std::is_same<T, std::string>::value,
                            QString>::type {
  return FormatStringHelper(QString::fromStdString(format));
}

/// Qt-style `%1`/`%2` substitution. NOTE: `%1`, not printf's `%s`.
template <typename... Args>
auto FormatString(const QString& format, Args... args) -> QString {
  return FormatStringHelper(format, args...);
}

// ------------------------------------------------------------------ metadata

inline auto QMapToMetaDataArray(const QMap<QString, QString>& map)
    -> MetaData** {
  auto** meta_data_array =
      static_cast<MetaData**>(GFAllocateMemory(sizeof(MetaData*) * map.size()));

  int index = 0;
  for (auto it = map.begin(); it != map.end(); ++it) {
    meta_data_array[index] =
        static_cast<MetaData*>(GFAllocateMemory(sizeof(MetaData)));

    QByteArray const key = it.key().toUtf8();
    QByteArray const value = it.value().toUtf8();

    meta_data_array[index]->key = GFModuleStrDup(key);
    meta_data_array[index]->value = GFModuleStrDup(value);

    ++index;
  }

  return meta_data_array;
}

// -------------------------------------------------------------- arrays, lists

inline auto CharArrayToQStringList(char** pl_components, int size)
    -> QStringList {
  QStringList list;
  for (int i = 0; i < size; ++i) {
    list.append(UDUP(pl_components[i]));
  }
  GFFreeMemory(static_cast<void*>(pl_components));
  return list;
}

inline auto QStringListToCharArray(const QStringList& list) -> char** {
  char** char_array =
      static_cast<char**>(GFAllocateMemory(list.size() * sizeof(char*)));

  int index = 0;
  for (const QString& item : list) {
    QByteArray value = item.toUtf8();
    char_array[index] = static_cast<char*>(GFAllocateMemory(value.size() + 1));
    std::strcpy(char_array[index], value.constData());
    index++;
  }

  return char_array;
}

template <typename T>
inline auto ArrayToQList(T** pl_components, int size) -> QList<T> {
  if (pl_components == nullptr || size <= 0) {
    return QList<T>();
  }

  QList<T> list;
  for (int i = 0; i < size; ++i) {
    list.append(*pl_components[i]);
    GFFreeMemory(pl_components[i]);
  }
  GFFreeMemory(pl_components);
  return list;
}

template <typename T>
inline auto QListToArray(const QList<T>& list) -> T** {
  T** array = static_cast<T**>(GFAllocateMemory(list.size() * sizeof(T*)));
  int index = 0;
  for (const T& item : list) {
    auto mem = static_cast<T*>(GFAllocateMemory(sizeof(T)));
    array[index] = new (mem) T(item);
    index++;
  }

  return array;
}

// ------------------------------------------------------------------- QVariant

inline auto ConvertQVariantToVoidPtr(const QVariant& variant) -> void* {
  void* mem = GFAllocateMemory(sizeof(QVariant));
  auto* variant_ptr = new (mem) QVariant(variant);
  return static_cast<void*>(variant_ptr);
}

inline auto ConvertVoidPtrToQVariant(void* ptr) -> QVariant {
  if (ptr == nullptr) return {};

  auto* variant_ptr = static_cast<QVariant*>(ptr);
  QVariant variant = *variant_ptr;

  GFFreeMemory(variant_ptr);
  return variant;
}

// --------------------------------------------------------------- gui objects

template <typename T>
auto GFUIGetGUIObjectAs(const QString& handle) -> T* {
  auto* obj =
      static_cast<QObject*>(GFUIGetGUIObject(handle.toUtf8().constData()));
  if (!obj) {
    return nullptr;
  }
  return qobject_cast<T*>(obj);
}

#define Q_VARIANT_Q_OBJECT_FACTORY_DECLARE(name) \
  auto name(void* data_raw_ptr)->void*;

#define Q_VARIANT_Q_OBJECT_FACTORY_DEFINE(name, func)   \
  auto name(void* data_raw_ptr)->void* {                \
    auto data = ConvertVoidPtrToQVariant(data_raw_ptr); \
    return func(data);                                  \
  }

#define GUI_OBJECT(factory, data) \
  GFUICreateGUIObject(factory, ConvertQVariantToVoidPtr(data))
