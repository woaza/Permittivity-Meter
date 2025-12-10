#include "debug_log.h"

#include <stdio.h>
#include <string.h>

#include "mocks/mock_board.h"

#ifndef DEBUG_LOG_DEFAULT_TAG
#define DEBUG_LOG_DEFAULT_TAG "STATE"
#endif

static AppState_t s_last_state = STATE_INIT;

static const char *state_to_string(AppState_t state)
{
    switch (state) {
    case STATE_INIT:
        return "INIT";
    case STATE_IDLE:
        return "IDLE";
    case STATE_CALIBRATION:
        return "CAL";
    case STATE_MEASURE_SEARCH:
        return "MEAS";
    case STATE_CALCULATION:
        return "CALC";
    case STATE_ERROR:
        return "ERR";
    default:
        return "UNK";
    }
}

static void push_entry(DebugLogDomain_t domain, AppState_t state, const char *text)
{
    if (text == NULL) {
        return;
    }

    DebugLogEntry_t entry;
    entry.domain = domain;
    entry.state = state;
    strncpy(entry.text, text, sizeof(entry.text) - 1U);
    entry.text[sizeof(entry.text) - 1U] = '\0';

    MockBoard_DebugPush(&entry);
}

void Debug_LogState(const char *tag, AppState_t from_state, AppState_t to_state)
{
    char buffer[DEBUG_LOG_TEXT_LEN];
    snprintf(buffer,
             sizeof(buffer),
             "%s:%s->%s",
             (tag != NULL) ? tag : DEBUG_LOG_DEFAULT_TAG,
             state_to_string(from_state),
             state_to_string(to_state));

    s_last_state = to_state;
    push_entry(DEBUG_LOG_DOMAIN_STATE, to_state, buffer);
}

void Debug_LogEvent(const char *source, const char *detail)
{
    char buffer[DEBUG_LOG_TEXT_LEN];
    snprintf(buffer,
             sizeof(buffer),
             "%s:%s",
             (source != NULL) ? source : "EVT",
             (detail != NULL) ? detail : "none");

    push_entry(DEBUG_LOG_DOMAIN_EVENT, s_last_state, buffer);
}

void Debug_LogDriver(const char *component, const char *detail)
{
    char buffer[DEBUG_LOG_TEXT_LEN];
    snprintf(buffer,
             sizeof(buffer),
             "%s:%s",
             (component != NULL) ? component : "DRV",
             (detail != NULL) ? detail : "update");

    push_entry(DEBUG_LOG_DOMAIN_DRIVER, s_last_state, buffer);
}

size_t Debug_LogCopy(DebugLogEntry_t *out_entries, size_t max_entries)
{
    return MockBoard_DebugCopy(out_entries, max_entries);
}

const char *Debug_LogGetLast(void)
{
    const DebugLogEntry_t *entry = MockBoard_DebugGetLast();
    return (entry != NULL) ? entry->text : NULL;
}

void Debug_LogClear(void)
{
    s_last_state = STATE_INIT;
    MockBoard_DebugClear();
}


