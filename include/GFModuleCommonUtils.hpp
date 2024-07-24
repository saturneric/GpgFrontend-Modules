/**
 * Copyright (C) 2021 Saturneric <eric@bktus.com>
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

#include <GFSDKUI.h>

#include <QMap>
#include <QString>
#include <cstring>

#include "GFSDKBasic.h"
#include "GFSDKLog.h"
#include "GFSDKModule.h"

#define DUP(v) GFModuleStrDup(v)
#define UDUP(v) UnStrDup(v)
#define QDUP(v) QStrDup(v)

inline void MLogDebug(const QString& s) { GFModuleLogDebug(s.toUtf8()); }
inline void MLogInfo(const QString& s) { GFModuleLogInfo(s.toUtf8()); }
inline void MLogWarn(const QString& s) { GFModuleLogWarn(s.toUtf8()); }
inline void MLogError(const QString& s) { GFModuleLogError(s.toUtf8()); }

#define MLogDebugS(format, ...) \
  MLogDebug(QString::asprintf(format, __VA_ARGS__))
#define MLogInfoS(format, ...) MLogInfo(QString::asprintf(format, __VA_ARGS__))
#define MLogWarnS(format, ...) MLogWarn(QString::asprintf(format, __VA_ARGS__))
#define MLogErrorS(format, ...) \
  MLogError(QString::asprintf(format, __VA_ARGS__))

inline auto QStrDup(QString str) -> char* { return DUP(str.toUtf8()); }

inline auto UnStrDup(const char* src) -> QString {
  auto qt_str = QString::fromUtf8(src);
  GFFreeMemory(static_cast<void*>(const_cast<char*>(src)));
  return qt_str;
}

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

inline auto QMapToGFModuleMetaDataList(const QMap<QString, QString>& map)
    -> GFModuleMetaData* {
  GFModuleMetaData* head = nullptr;
  GFModuleMetaData* tail = nullptr;

  for (auto it = map.begin(); it != map.end(); ++it) {
    auto* new_node = static_cast<GFModuleMetaData*>(
        GFAllocateMemory(sizeof(GFModuleMetaData)));

    QByteArray const key = it.key().toUtf8();
    QByteArray const value = it.value().toUtf8();

    new_node->key = new char[key.size() + 1];
    std::strcpy(const_cast<char*>(new_node->key), key.constData());

    new_node->value = new char[value.size() + 1];
    std::strcpy(const_cast<char*>(new_node->value), value.constData());

    new_node->next = nullptr;

    if (tail != nullptr) {
      tail->next = new_node;
    } else {
      head = new_node;
    }
    tail = new_node;
  }

  return head;
}

inline auto AllocBufferAndCopy(const QByteArray& b) -> char* {
  auto* p = static_cast<char*>(GFAllocateMemory(sizeof(char) * b.size()));
  memcpy(p, b.constData(), b.size());
  return p;
}

template <typename T, typename... Args>
auto SecureCreateSharedObject(Args&&... args) -> std::shared_ptr<T> {
  void* mem = GFAllocateMemory(sizeof(T));
  if (!mem) throw std::bad_alloc();

  try {
    T* obj = new (mem) T(std::forward<Args>(args)...);
    return std::shared_ptr<T>(obj, [](T* ptr) {
      ptr->~T();
      GFFreeMemory(ptr);
    });
  } catch (...) {
    GFFreeMemory(mem);
    throw;
  }
}

inline auto CharArrayToQStringList(char** pl_components, int size)
    -> QStringList {
  QStringList list;
  for (int i = 0; i < size; ++i) {
    list.append(QString::fromUtf8(pl_components[i]));
    GFFreeMemory(pl_components[i]);
  }
  GFFreeMemory(pl_components);
  return list;
}