#pragma once

#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BT_EVENT_NONE = 0,
    BT_EVENT_CONN,
    BT_EVENT_CAL,
    BT_EVENT_MEAS,
    BT_EVENT_MANUAL_ON,
    BT_EVENT_MANUAL_OFF
} BT_Event_t;

void BT_Manager_Init(void);
void BT_ProcessIncoming(const char *buffer);
BT_Event_t BT_PopEvent(void);
void BT_SendStatus(const char *status_tag);
void BT_SendResult(MeasurementResult_t result);

void BT_SetManualMode(uint8_t enabled);
uint8_t BT_IsManualMode(void);

// Mock helpers for unit tests and simulations.
void BT_MockEnqueueCommand(const char *cmd);
void BT_MockPump(void);
const char *BT_MockGetLastTx(void);

#ifdef __cplusplus
}
#endif
