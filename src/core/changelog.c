/*
   Copyright 2024 Ryan "rj45" Sanche

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "core.h"
#include <stdint.h>

#define MAX_FIELD_SIZE 8
#define MAX_OBJECT_SIZE 40

typedef struct LogUpdate {
  uint32_t fieldOffset;
  uint32_t fieldSize;
  uint8_t newValue[MAX_FIELD_SIZE];
  uint8_t oldValue[MAX_FIELD_SIZE];
} LogUpdate;

typedef struct LogObject {
  uint32_t size;
  uint8_t data[MAX_OBJECT_SIZE];
} LogObject;

typedef struct LogEntry {
  enum {
    LOG_CREATE,
    LOG_DELETE,
    LOG_UPDATE,
  } verb;
  ID id;
  uint32_t dataIndex;
} LogEntry;

typedef struct ChangeLog {
  arr(LogEntry) log;
  arr(LogUpdate) updates;
  arr(LogObject) objects;
  arr(size_t) commitPoints;

  uint32_t redoIndex;

  void (*cl_create_object)(ID id, void *object, size_t size);
  void (*cl_delete_object)(ID id, void *object, size_t size);
  void (*cl_update_object)(ID id, size_t fieldOffset, void *data, size_t size);
} ChangeLog;

#define cl_record_update(log, id, ptr, field, value)                           \
  (cl_record_update_(                                                          \
     (log), (id), ((char *)(&((ptr)->(field))) - (char *)(ptr)),               \
     sizeof((ptr)->(field)), &(value), &((ptr)->(field))),                     \
   (ptr)->(field) = value)

void cl_commit(ChangeLog *log) {
  assert(log->redoIndex == arrlen(log->commitPoints));
  arrput(log->commitPoints, arrlen(log->log));
  log->redoIndex++;
}

void cl_truncate_redo(ChangeLog *log) {
  if (log->redoIndex < arrlen(log->commitPoints)) {
    arrsetlen(log->commitPoints, log->redoIndex);
  }
}

void cl_create(ChangeLog *log, ID id, void *object, size_t size) {
  cl_truncate_redo(log);

  LogEntry entry = (LogEntry){LOG_CREATE, id, arrlen(log->objects)};
  arrput(log->log, entry);

  LogObject obj = (LogObject){size};
  arrput(log->objects, obj);
  memcpy(log->objects[arrlen(log->objects) - 1].data, object, size);
}

void cl_delete(ChangeLog *log, ID id, void *object, size_t size) {
  cl_truncate_redo(log);

  LogEntry entry = (LogEntry){LOG_DELETE, id, arrlen(log->objects)};
  arrput(log->log, entry);

  LogObject obj = (LogObject){size};
  arrput(log->objects, obj);
  memcpy(log->objects[arrlen(log->objects) - 1].data, object, size);
}

void cl_record_update_(
  ChangeLog *log, ID id, size_t fieldOffset, size_t size, void *oldValue,
  void *newValue) {
  cl_truncate_redo(log);

  LogEntry entry = (LogEntry){LOG_UPDATE, id, arrlen(log->updates)};
  arrput(log->log, entry);
  memcpy(log->updates[arrlen(log->updates)].oldValue, oldValue, size);
  memcpy(log->updates[arrlen(log->updates)].newValue, newValue, size);
}

// undo all actions to the previous commit point
void cl_undo(ChangeLog *log) {
  if (log->redoIndex == 0) {
    return;
  }

  log->redoIndex--;
  size_t commitIndex = log->commitPoints[log->redoIndex];
  size_t goalIndex = 0;
  if (log->redoIndex > 0) {
    goalIndex = log->commitPoints[log->redoIndex - 1];
  }

  while (commitIndex >= goalIndex) {
    LogEntry entry = log->log[commitIndex];
    switch (entry.verb) {
    case LOG_CREATE:
      log->cl_delete_object(
        entry.id, log->objects[entry.dataIndex].data,
        log->objects[entry.dataIndex].size);
      break;
    case LOG_DELETE:
      log->cl_create_object(
        entry.id, log->objects[entry.dataIndex].data,
        log->objects[entry.dataIndex].size);
      break;
    case LOG_UPDATE: {
      LogUpdate *update = &log->updates[entry.dataIndex];
      log->cl_update_object(
        entry.id, update->fieldOffset, update->oldValue, update->fieldSize);
      break;
    }
    }
    commitIndex--;
  }
}
