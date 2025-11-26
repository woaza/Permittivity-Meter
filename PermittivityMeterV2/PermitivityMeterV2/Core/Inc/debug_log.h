#pragma once

#include <stddef.h>

#include "debug_log_types.h"

void Debug_LogState(const char *tag, AppState_t from_state, AppState_t to_state);
void Debug_LogEvent(const char *source, const char *detail);
void Debug_LogDriver(const char *component, const char *detail);
size_t Debug_LogCopy(DebugLogEntry_t *out_entries, size_t max_entries);
const char *Debug_LogGetLast(void);
void Debug_LogClear(void);
