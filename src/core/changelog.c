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

#include "core/core.h"

void cl_init(ChangeLog *log) { *log = (ChangeLog){0}; }

void cl_free(ChangeLog *log) {
  arrfree(log->log);
  arrfree(log->updates);
  arrfree(log->commitPoints);
}

void cl_commit(ChangeLog *log) {
  assert(log->redoIndex == arrlen(log->commitPoints));
  arrput(log->commitPoints, arrlen(log->log));
  log->redoIndex++;
}

static void cl_truncate_redo(ChangeLog *log) {
  if (log->redoIndex >= arrlen(log->commitPoints)) {
    return;
  }

  ptrdiff_t commitPoint = (ptrdiff_t)log->commitPoints[log->redoIndex];
  for (ptrdiff_t i = arrlen(log->log) - 1; i >= commitPoint; i++) {
    if (log->log[i].verb == LOG_UPDATE) {
      arrpop(log->updates);
      assert(arrlen(log->updates) == log->log[i].dataIndex);
    }
  }
  arrsetlen(log->log, commitPoint);
  arrsetlen(log->commitPoints, log->redoIndex);
}

void cl_create(ChangeLog *log, ID id, uint16_t table) {
  cl_truncate_redo(log);

  LogEntry entry = (LogEntry){.verb = LOG_CREATE, .id = id, .table = table};
  arrput(log->log, entry);
}

void cl_delete(ChangeLog *log, ID id) {
  cl_truncate_redo(log);

  LogEntry entry = (LogEntry){.verb = LOG_DELETE, .id = id};
  arrput(log->log, entry);
}

void cl_update(
  ChangeLog *log, ID id, uint16_t table, uint16_t column, uint32_t row,
  void *newValue, size_t size) {
  cl_truncate_redo(log);
  assert(size <= MAX_COMPONENT_SIZE);

  // prevent bad things if asserts are turned off
  size = size > MAX_COMPONENT_SIZE ? MAX_COMPONENT_SIZE : size;

  LogEntry entry = (LogEntry){LOG_UPDATE, table, id};
  arrput(log->log, entry);
  LogUpdate update = (LogUpdate){
    .column = column, .size = (uint8_t)size, .row = row, .newValue = {0}};
  memcpy(update.newValue, newValue, size);
  arrput(log->updates, update);
}

static void cl_redo_range(ChangeLog *log, size_t start, size_t end) {
  for (size_t i = start; i < end; i++) {
    LogEntry *entry = &log->log[i];
    switch (entry->verb) {
    case LOG_CREATE:
      log->cl_replay_create(log->user, entry->id);
      break;
    case LOG_DELETE:
      log->cl_replay_delete(log->user, entry->id);
      break;
    case LOG_UPDATE: {
      LogUpdate *update = &log->updates[entry->dataIndex];
      log->cl_replay_update(
        log->user, entry->id, entry->table, update->column, update->row,
        update->newValue, update->size);
      break;
    }
    }
  }
}

// Undo all actions to the previous commit point.
// This is done by reverting to the snapshot and replaying all actions up to
// the previous commit point.
void cl_undo(ChangeLog *log) {
  if (log->redoIndex == 0) {
    return;
  }

  log->redoIndex--;
  size_t commitPoint = log->commitPoints[log->redoIndex];

  log->cl_revert_snapshot(log->user);

  cl_redo_range(log, 0, commitPoint);
}

void cl_redo(ChangeLog *log) {
  if ((log->redoIndex + 1) >= arrlen(log->commitPoints)) {
    return;
  }

  size_t commitPoint = log->commitPoints[log->redoIndex];
  size_t nextCommitPoint = log->commitPoints[log->redoIndex + 1];
  cl_redo_range(log, commitPoint, nextCommitPoint);

  log->redoIndex++;
}