#pragma once

#include "app_types.h"

typedef enum {
    FSM_EVENT_NONE = 0,
    FSM_EVENT_INIT_DONE,
    FSM_EVENT_BUTTON_PRESS,
    FSM_EVENT_BT_CONN,
    FSM_EVENT_BT_CAL,
    FSM_EVENT_BT_MEAS,
    FSM_EVENT_BT_MANUAL_ON,
    FSM_EVENT_BT_MANUAL_OFF,
    FSM_EVENT_CAL_DONE,
    FSM_EVENT_MEAS_DONE,
    FSM_EVENT_ERROR_FLAG
} FSM_Event_t;

void FSM_Init(void);
void FSM_PostEvent(FSM_Event_t event);
void FSM_RunOnce(void);
AppState_t FSM_GetState(void);
