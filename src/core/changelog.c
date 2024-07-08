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

#define cl_entry_at(log, offset) ((LogEntry *)((uint8_t *)log->log + (offset)))

void cl_init(ChangeLog *log) {
  *log = (ChangeLog){0};
  log->capacity = 1024;
  log->log = malloc(log->capacity);
  if (log->log == NULL) {
    abort();
  }
  cl_clear(log);
}

void cl_free(ChangeLog *log) {
  free(log->log);
  arrfree(log->commits);
  arrfree(log->poppedCommits);
}

void cl_clear(ChangeLog *log) {
  log->nextEntry = 0;
  cl_entry_at(log, log->nextEntry)->psize = 0;
  log->lastEntry = NO_LOG_INDEX;

  arrsetlen(log->commits, 1);
  log->commits[0] = 0;

  arrsetlen(log->poppedCommits, 0);

  log_debug("<<<CLEAR>>>");
}

static void cl_advance(ChangeLog *log) {
  // advance the pointers
  log->lastEntry = log->nextEntry;
  log_debug(
    "Advancing log by %d bytes", cl_entry_at(log, log->nextEntry)->size);
  log->nextEntry += cl_entry_at(log, log->nextEntry)->size;
  cl_entry_at(log, log->nextEntry)->psize =
    cl_entry_at(log, log->lastEntry)->size;
}

static void cl_expand(ChangeLog *log, size_t size) {
  size_t newSize = log->nextEntry + size + sizeof(LogEntry);
  if (newSize < log->capacity) {
    return;
  }
  size_t newCapacity = log->capacity;
  while (newCapacity < newSize) {
    newCapacity *= 2;
  }

  log->log = realloc(log->log, newCapacity);
  if (log->log == NULL) {
    // TODO: handle OOM better
    abort();
  }
  log->capacity = newCapacity;
}

static inline void cl_truncate_redo(ChangeLog *log) {
  arrsetlen(log->poppedCommits, 0);
}

void cl_commit(ChangeLog *log) {
  cl_truncate_redo(log);

  log_debug("<<COMMIT>>");

  if (log->commits[arrlen(log->commits) - 1] == log->nextEntry) {
    log_debug("No changes to commit");
    return;
  }

  // mark the last entry as a commit
  if (log->lastEntry != NO_LOG_INDEX) {
    cl_entry_at(log, log->lastEntry)->verb |= LOG_COMMIT;
  }

  // add the commit to the list of commits
  arrput(log->commits, log->nextEntry);
}

void cl_create(ChangeLog *log, ID id, uint8_t table) {
  cl_truncate_redo(log);
  cl_expand(log, sizeof(LogEntry));
  LogEntry *entry = cl_entry_at(log, log->nextEntry);
  entry->verb = LOG_CREATE;
  entry->id = id;
  entry->table = table;
  entry->size = sizeof(LogEntry);
  cl_advance(log);
}

void cl_delete(ChangeLog *log, ID id, uint8_t table) {
  cl_truncate_redo(log);
  cl_expand(log, sizeof(LogEntry));
  LogEntry *entry = cl_entry_at(log, log->nextEntry);
  entry->verb = LOG_DELETE;
  entry->id = id;
  entry->table = table;
  entry->size = sizeof(LogEntry);
  cl_advance(log);
}

void cl_update(
  ChangeLog *log, ID id, uint8_t table, uint8_t column, void *newValue,
  size_t size) {
  cl_truncate_redo(log);

  // scan entries since the last commit for an existing update we can
  // overwrite. This ensures things like dragging a component around only
  // creates a single change log entry for a given commit.
  LogEntry *entry = cl_entry_at(log, log->commits[arrlen(log->commits) - 1]);
  LogEntry *nextEntry = cl_entry_at(log, log->nextEntry);
  while (entry != nextEntry) {
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

  cl_expand(log, entrySize);

  LogEntry *next = cl_entry_at(log, log->nextEntry);
  next->verb = LOG_UPDATE;
  next->id = id;
  next->table = table;
  next->size = entrySize;
  LogUpdate *update = (LogUpdate *)next;
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

  if (arrlen(log->commits) <= 1) {
    return;
  }

  // there should be no outstanding uncommitted changes
  LogIndex currentCommitStart = arrpop(log->commits);
  assert(currentCommitStart == log->nextEntry);

  LogIndex prevCommitStart = log->commits[arrlen(log->commits) - 1];
  arrput(log->poppedCommits, currentCommitStart);

  log_debug("Current commit: %d", currentCommitStart);
  log_debug("Prev commit: %d", prevCommitStart);

  assert(currentCommitStart != prevCommitStart);

  // restore the snapshot
  log->cl_revert_snapshot(log->user);

  log_debug("Restored snapshot");

  int count = 0;

  // replay all actions up to the previous commit point
  LogEntry *entry = (LogEntry *)log->log;
  LogEntry *last = cl_entry_at(log, prevCommitStart);
  while (entry < last) {
    cl_replay_entry(log, entry);
    entry = (LogEntry *)((uint8_t *)entry + entry->size);
    count++;
  }

  log_debug("Replayed %d actions", count);

  log->nextEntry = prevCommitStart;
  log->lastEntry = log->nextEntry - cl_entry_at(log, log->nextEntry)->psize;
}

void cl_redo(ChangeLog *log) {
  if (arrlen(log->poppedCommits) == 0) {
    return;
  }

  // there should be no outstanding uncommitted changes
  assert(log->commits[arrlen(log->commits) - 1] == log->nextEntry);

  LogIndex poppedCommitStart = arrpop(log->poppedCommits);
  arrput(log->commits, poppedCommitStart);
  log_debug("Popped commit: %d", poppedCommitStart);

  int count = 0;

  LogEntry *entry = cl_entry_at(log, log->nextEntry);
  LogEntry *lastEntry = cl_entry_at(log, poppedCommitStart);
  while (entry < lastEntry) {
    cl_replay_entry(log, entry);
    entry = (LogEntry *)((uint8_t *)entry + entry->size);
    count++;
  }

  log_debug("Redid %d actions", count);

  log->nextEntry = poppedCommitStart;
  log->lastEntry = log->nextEntry - cl_entry_at(log, log->nextEntry)->psize;
}