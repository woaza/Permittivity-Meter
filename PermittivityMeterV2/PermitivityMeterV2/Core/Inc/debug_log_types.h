#pragma once

#include <stdint.h>

#include "app_types.h"

#define DEBUG_LOG_TEXT_LEN 48U

typedef enum {
    DEBUG_LOG_DOMAIN_STATE = 0,
    DEBUG_LOG_DOMAIN_EVENT,
    DEBUG_LOG_DOMAIN_DRIVER
} DebugLogDomain_t;

typedef struct {
    DebugLogDomain_t domain;
    AppState_t state;
    char text[DEBUG_LOG_TEXT_LEN];
} DebugLogEntry_t;
