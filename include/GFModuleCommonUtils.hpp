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
#include <qsharedpointer.h>

#include <QMap>
#include <QString>
#include <cstring>

#include "GFSDKBasic.h"
#include "GFSDKLog.h"
#include "GFSDKModule.h"

#define DUP(v) GFModuleStrDup(v)
#define UDUP(v) UnStrDup(v)
#define QDUP(v) QStrDup(v)

#define LISTEN(event) GFModuleListenEvent(GFGetModuleID(), DUP(event));

#define EXECUTE_MODULE()                                \
  auto GFExecuteModule(GFModuleEvent* p_event) -> int { \
    auto event = ConvertEventToMap(p_event);

#define END_EXECUTE_MODULE() }

#define CB_SUCC(event)                        \
  CB(event, GFGetModuleID(), {{"ret", "0"}}); \
  return 0;

#define CB_ERR(event, ret, err)                                  \
  CB(event, GFGetModuleID(),                                     \
     {{"ret", QString::number(ret)}, {"reason", QString(err)}}); \
  return ret;

inline void MLogDebug(const QString& s) { GFModuleLogDebug(s.toUtf8()); }
inline void MLogInfo(const QString& s) { GFModuleLogInfo(s.toUtf8()); }
inline void MLogWarn(const QString& s) { GFModuleLogWarn(s.toUtf8()); }
inline void MLogError(const QString& s) { GFModuleLogError(s.toUtf8()); }

#define LOG_DEBUG(format) MLogDebug(FormatString(QString(format)))
#define LOG_INFO(format) MLogDebug(FormatString(QString(format)))
#define LOG_WARN(format) MLogDebug(FormatString(QString(format)))
#define LOG_ERROR(format) MLogDebug(FormatString(QString(format)))

#define FLOG_DEBUG(format, ...) \
  MLogDebug(FormatString(QString(format), __VA_ARGS__))
#define FLOG_INFO(format, ...) \
  MLogInfo(FormatString(QString(format), __VA_ARGS__))
#define FLOG_WARN(format, ...) \
  MLogWarn(FormatString(QString(format), __VA_ARGS__))
#define FLOG_ERROR(format, ...) \
  MLogError(FormatString(QString(format), __VA_ARGS__))

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

    new_node->key = DUP(key);
    new_node->value = DUP(value);

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

inline auto ConvertEventParamsToMap(GFModuleEventParam* params)
    -> QMap<QString, QString> {
  QMap<QString, QString> param_map;
  GFModuleEventParam* current = params;
  GFModuleEventParam* last;

  while (current != nullptr) {
    param_map[current->name] = UDUP(current->value);

    last = current;
    current = current->next;
    GFFreeMemory(last);
  }

  return param_map;
}

inline auto ConvertMapToParams(const QMap<QString, QString>& param_map)
    -> GFModuleEventParam* {
  GFModuleEventParam* head = nullptr;
  GFModuleEventParam* prev = nullptr;

  for (const auto& [key, value] : param_map.asKeyValueRange()) {
    auto* param = static_cast<GFModuleEventParam*>(
        GFAllocateMemory(sizeof(GFModuleEventParam)));

    param->name = DUP(key.toUtf8());
    param->value = DUP(value.toUtf8());
    param->next = nullptr;

    if (prev == nullptr) {
      head = param;
    } else {
      prev->next = param;
    }
    prev = param;
  }

  return head;
}

inline auto ConvertEventToMap(GFModuleEvent* event) -> QMap<QString, QString> {
  QMap<QString, QString> event_map;

  event_map["event_id"] = UDUP(event->id);
  event_map["trigger_id"] = UDUP(event->trigger_id);
  event_map.insert(ConvertEventParamsToMap(event->params));

  GFFreeMemory(event);

  return event_map;
}

inline auto ConvertMapToEvent(QMap<QString, QString> event_map)
    -> GFModuleEvent* {
  auto* event =
      static_cast<GFModuleEvent*>(GFAllocateMemory(sizeof(GFModuleEvent)));

  event->id = DUP(event_map["event_id"].toUtf8());
  event->trigger_id = DUP(event_map["trigger_id"].toUtf8());
  event->params = nullptr;

  return event;
}

inline void CB(const QMap<QString, QString>& event, const char* module,
               const QMap<QString, QString>& params) {
  GFModuleTriggerModuleEventCallback(ConvertMapToEvent(event), module,
                                     ConvertMapToParams(params));
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

template <typename T>
class PointerConverter {
 public:
  explicit PointerConverter(void* ptr) : ptr_(ptr) {}

  auto AsType() const -> T* { return static_cast<T*>(ptr_); }

 private:
  void* ptr_;
};

/**
 * @brief
 *
 * @tparam T
 * @return T*
 */
template <typename T>
auto SecureMallocAsType(std::size_t size) -> T* {
  return PointerConverter<T>(GFAllocateMemory(size)).AsType();
}

/**
 * @brief
 *
 * @return void*
 */
template <typename T>
auto SecureReallocAsType(T* ptr, std::size_t size) -> T* {
  return PointerConverter<T>(GFReallocateMemory(ptr, size)).AsType();
}

template <typename T, typename... Args>
auto SecureCreateQSharedObject(Args&&... args) -> QSharedPointer<T> {
  void* mem = GFAllocateMemory(sizeof(T));
  if (!mem) throw std::bad_alloc();

  try {
    T* obj = new (mem) T(std::forward<Args>(args)...);
    return QSharedPointer<T>(obj, [](T* ptr) {
      ptr->~T();
      GFFreeMemory(ptr);
    });
  } catch (...) {
    GFFreeMemory(mem);
    throw;
  }
}

inline auto CharArrayToQStringList(char** pl_components,
                                   int size) -> QStringList {
  QStringList list;
  for (int i = 0; i < size; ++i) {
    list.append(QString::fromUtf8(pl_components[i]));
    GFFreeMemory(pl_components[i]);
  }
  GFFreeMemory(pl_components);
  return list;
}

template <typename... Args>
auto FormatString(const QString& format, Args... args) -> QString {
  return FormatStringHelper(format, args...);
}

template <typename T>
auto FormatStringHelper(const QString& format, T arg) -> QString {
  return format.arg(arg);
}

template <typename T, typename... Args>
auto FormatStringHelper(const QString& format, T arg, Args... args) -> QString {
  return FormatStringHelper(format.arg(arg), args...);
}