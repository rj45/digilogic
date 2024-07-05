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

#include <stdalign.h>

#define LOG_LEVEL LL_DEBUG
#include "log.h"

void cl_init(ChangeLog *log) {
  *log = (ChangeLog){0};
  log->capacity = 1024;
  log->log = malloc(log->capacity);
  if (log->log == NULL) {
    abort();
  }
  log->nextEntry = (LogEntry *)log->log;
  log->nextEntry->psize = 0;
  log->lastCommitStart = NULL;
}

void cl_free(ChangeLog *log) { free(log->log); }

void cl_clear(ChangeLog *log) {
  log->nextEntry = (LogEntry *)log->log;
  log->lastCommitStart = NULL;
  log->lastEntry = NULL;
  log->redoEnd = NULL;
}

static void cl_advance(ChangeLog *log) {
  // advance the pointers
  log->lastEntry = log->nextEntry;
  log->nextEntry =
    (LogEntry *)((uint8_t *)log->nextEntry + log->nextEntry->size);
  log->nextEntry->psize = log->lastEntry->size;
}

static void cl_expand(ChangeLog *log, size_t size) {
  size_t oldSize = (uint8_t *)log->nextEntry - (uint8_t *)log->log;
  if (log->redoEnd != NULL) {
    oldSize = (uint8_t *)log->redoEnd - (uint8_t *)log->log;
  }
  size_t newSize = oldSize + size;
  if (newSize < log->capacity) {
    return;
  }
  size_t newCapacity = log->capacity;
  while (newCapacity < newSize) {
    newCapacity *= 2;
  }

  // save offsets of pointers
  ptrdiff_t lastCommitOffset =
    log->lastCommitStart == NULL
      ? -1
      : (uint8_t *)log->lastCommitStart - (uint8_t *)log->log;
  ptrdiff_t lastOffset = log->lastEntry == NULL
                           ? -1
                           : (uint8_t *)log->lastEntry - (uint8_t *)log->log;
  size_t nextOffset = (uint8_t *)log->nextEntry - (uint8_t *)log->log;
  ptrdiff_t redoEndOffset =
    log->redoEnd == NULL ? -1 : (uint8_t *)log->redoEnd - (uint8_t *)log->log;

  log->log = realloc(log->log, newCapacity);
  if (log->log == NULL) {
    // TODO: handle OOM better
    abort();
  }
  log->capacity = newCapacity;

  // restore pointers
  log->lastCommitStart =
    lastCommitOffset == -1
      ? NULL
      : (LogEntry *)((uint8_t *)log->log + lastCommitOffset);
  log->lastEntry =
    lastOffset == -1 ? NULL : (LogEntry *)((uint8_t *)log->log + lastOffset);
  log->nextEntry = (LogEntry *)((uint8_t *)log->log + nextOffset);
  log->redoEnd = redoEndOffset == -1
                   ? NULL
                   : (LogEntry *)((uint8_t *)log->log + redoEndOffset);
}

static inline void cl_truncate_redo(ChangeLog *log) { log->redoEnd = NULL; }

void cl_commit(ChangeLog *log) {
  cl_truncate_redo(log);

  log_debug("<<COMMIT>>");

  log->lastCommitStart = log->nextEntry;
  if (log->lastEntry != NULL) {
    log->lastEntry->verb |= LOG_COMMIT;
  }
}

void cl_create(ChangeLog *log, ID id, uint8_t table) {
  cl_truncate_redo(log);
  cl_expand(log, sizeof(LogEntry));
  log->nextEntry->verb = LOG_CREATE;
  log->nextEntry->id = id;
  log->nextEntry->table = table;
  log->nextEntry->size = sizeof(LogEntry);
  cl_advance(log);
}

void cl_delete(ChangeLog *log, ID id, uint8_t table) {
  cl_truncate_redo(log);
  cl_expand(log, sizeof(LogEntry));
  log->nextEntry->verb = LOG_DELETE;
  log->nextEntry->id = id;
  log->nextEntry->table = table;
  log->nextEntry->size = sizeof(LogEntry);
  cl_advance(log);
}

void cl_update(
  ChangeLog *log, ID id, uint8_t table, uint8_t column, void *newValue,
  size_t size) {
  cl_truncate_redo(log);

  // scan entries since the last commit for an existing update we can
  // overwrite. This ensures things like dragging a component around only
  // creates a single change log entry for a given commit.
  LogEntry *entry = log->lastCommitStart;
  while (entry != log->nextEntry && entry != NULL) {
    if (
      (entry->verb & LOG_MASK) == LOG_UPDATE && entry->id == id &&
      entry->table == table) {
      LogUpdate *update = (LogUpdate *)entry;
      if (update->column == column) {
        assert(entry->size >= sizeof(LogUpdate) + size);
        memcpy(update->newValue, newValue, size);
        return;
      }
    }
    entry = (LogEntry *)((uint8_t *)entry + entry->size);
  }

  // ensure log entries stay properly aligned
  size_t entrySize = sizeof(LogUpdate) + size;
  if (entrySize % alignof(LogUpdate) != 0) {
    entrySize += alignof(LogUpdate) - (entrySize % alignof(LogUpdate));
  }

  cl_expand(log, sizeof(LogUpdate) + entrySize);
  log->nextEntry->verb = LOG_UPDATE;
  log->nextEntry->id = id;
  log->nextEntry->table = table;
  log->nextEntry->size = entrySize;
  LogUpdate *update = (LogUpdate *)log->nextEntry;
  update->column = column;
  update->valueSize = size;
  memcpy(update->newValue, newValue, size);
  cl_advance(log);
}

void cl_tag(ChangeLog *log, ID id, uint8_t table, uint16_t tag) {
  // TODO: implement
}

void cl_untag(ChangeLog *log, ID id, uint8_t table, uint16_t tag) {
  // TODO: implement
}

static void cl_replay_entry(ChangeLog *log, LogEntry *entry) {
  switch (entry->verb & LOG_MASK) {
  case LOG_CREATE:
    log->cl_replay_create(log->user, entry->id, entry->table);
    break;
  case LOG_DELETE:
    log->cl_replay_delete(log->user, entry->id, entry->table);
    break;
  case LOG_UPDATE: {
    LogUpdate *update = (LogUpdate *)entry;
    log->cl_replay_update(
      log->user, entry->id, entry->table, update->column, update->newValue,
      update->valueSize);
  } break;
    // case LOG_TAG:
    //   log->cl_tag(log->user, entry->id, entry->table, ((LogTag*)entry)->tag);
    //   break;
    // case LOG_UNTAG:
    //   log->cl_untag(log->user, entry->id, entry->table,
    //   ((LogTag*)entry)->tag); break;
  }
}

// Undo all actions to the previous commit point.
// This is done by reverting to the snapshot and replaying all actions up to
// the previous commit point.
void cl_undo(ChangeLog *log) {
  log_debug("Undoing changes");

  if (log->lastCommitStart == NULL) {
    return;
  }

  if (log->redoEnd == NULL) {
    log->redoEnd = log->nextEntry;
  }

  if (log->lastCommitStart == (LogEntry *)log->log) {
    log->cl_revert_snapshot(log->user);
    log->lastCommitStart = NULL;
    log->lastEntry = NULL;
    log->nextEntry = (LogEntry *)log->log;
    return;
  }

  // find the last commit point
  LogEntry *prevCommitEnd = log->lastCommitStart;

  while (prevCommitEnd > (LogEntry *)log->log) {
    assert(prevCommitEnd->psize > 0);
    prevCommitEnd =
      (LogEntry *)((uint8_t *)prevCommitEnd - prevCommitEnd->psize);
    if (prevCommitEnd->verb & LOG_COMMIT) {
      break;
    }
  }

  while (prevCommitEnd > (LogEntry *)log->log) {
    assert(prevCommitEnd->psize > 0);
    prevCommitEnd =
      (LogEntry *)((uint8_t *)prevCommitEnd - prevCommitEnd->psize);
    if (prevCommitEnd->verb & LOG_COMMIT) {
      break;
    }
  }

  // TODO: remove this verification code
  // LogEntry *testEntry = prevCommitEnd;
  // testEntry = (LogEntry *)((uint8_t *)testEntry + testEntry->size);
  // while (testEntry < log->lastCommitStart) {
  //   assert((prevCommitEnd->verb & LOG_COMMIT) == 0);
  //   testEntry = (LogEntry *)((uint8_t *)testEntry + testEntry->size);
  // }

  log_debug("Found last commit");

  // restore the snapshot
  log->cl_revert_snapshot(log->user);

  log_debug("Restored snapshot");

  int count = 0;

  // replay all actions up to the previous commit point
  LogEntry *entry = (LogEntry *)log->log;
  while (entry <= prevCommitEnd) {
    cl_replay_entry(log, entry);
    entry = (LogEntry *)((uint8_t *)entry + entry->size);
    count++;
  }

  log_debug("Replayed %d actions", count);

  log->lastEntry = prevCommitEnd;
  log->nextEntry = (LogEntry *)((uint8_t *)prevCommitEnd + prevCommitEnd->size);
  log->lastCommitStart = log->nextEntry;
}

void cl_redo(ChangeLog *log) {
  if (log->redoEnd == NULL) {
    return;
  }

  LogEntry *entry = log->nextEntry;
  LogEntry *lastEntry = log->lastEntry;
  while (entry != log->redoEnd) {
    cl_replay_entry(log, entry);
    if (entry->verb & LOG_COMMIT) {
      break;
    }
    lastEntry = entry;
    entry = (LogEntry *)((uint8_t *)entry + entry->size);
  }

  log->lastEntry = lastEntry;
  log->nextEntry = entry;
  log->lastCommitStart = entry;

  if (entry == log->redoEnd) {
    log->redoEnd = NULL;
  }
}