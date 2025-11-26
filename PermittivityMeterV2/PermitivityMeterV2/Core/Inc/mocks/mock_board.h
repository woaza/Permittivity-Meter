#pragma once

#include <stddef.h>
#include <stdint.h>

#include "app_types.h"
#include "bsp_ui.h"
#include "debug_log_types.h"

void MockBoard_Init(void);
void MockBoard_Reset(void);

float MockBoard_RF_ComputeAmplitude(float freq_voltage, float q_voltage, uint8_t gain_idx);
void MockBoard_RF_SetResonanceVoltage(float voltage_v);
void MockBoard_RF_SetNoise(float noise_v);
void MockBoard_RF_SetBaseAmplitude(float amplitude_v);
void MockBoard_RF_SetForceFailure(uint8_t enable);

void MockBoard_UI_SetLED(uint8_t led_id, uint8_t state);
uint8_t MockBoard_UI_GetLED(uint8_t led_id);
void MockBoard_UI_SetButton(uint8_t pressed);
uint8_t MockBoard_UI_GetButton(void);

void MockBoard_BT_QueueCommand(const char *cmd);
const char *MockBoard_BT_DequeueCommand(void);
void MockBoard_BT_SetLastTx(const char *text);
const char *MockBoard_BT_GetLastTx(void);
uint8_t MockBoard_BT_GetHistoryCount(void);
const char *MockBoard_BT_GetHistoryEntry(uint8_t offset_from_newest);
void MockBoard_BT_ClearHistory(void);

void MockBoard_DebugPush(const DebugLogEntry_t *entry);
size_t MockBoard_DebugCopy(DebugLogEntry_t *out_entries, size_t max_entries);
const DebugLogEntry_t *MockBoard_DebugGetLast(void);
const DebugLogEntry_t *MockBoard_DebugPeek(uint8_t offset_from_newest);
void MockBoard_DebugClear(void);
