/**
 * @file    driver_board.h
 * @brief   Driver Layer - Hardware Abstraction Bridge
 * @author  Majdedin Al Rashid
 * @version 1.0
 * @date    2025-12-09
 * 
 * @details This module provides the interface between BSP and HAL layers.
 *          It handles both real hardware control and simulation/testing support.
 *          
 *          Architecture:
 *              Application (FSM) → BSP → Driver (this) → HAL → Hardware
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "app_types.h"
#include "bsp_ui.h"
#include "debug_log_types.h"

/* -------------------------------------------------------------------------- */
/*                              Initialization                                */
/* -------------------------------------------------------------------------- */

void Driver_Init(void);
void Driver_Reset(void);

/* -------------------------------------------------------------------------- */
/*                              RF Functions                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Compute RF amplitude from varicap settings.
 * @note  Currently uses simulation math. Will integrate ADC+DFT later.
 */
float Driver_RF_ComputeAmplitude(float freq_voltage, float q_voltage, uint8_t gain_idx);

void Driver_RF_SetResonanceVoltage(float voltage_v);
void Driver_RF_SetNoise(float noise_v);
void Driver_RF_SetBaseAmplitude(float amplitude_v);
void Driver_RF_SetForceFailure(uint8_t enable);

/* -------------------------------------------------------------------------- */
/*                              UI Functions                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Set LED state via HAL GPIO.
 * @param led_id LED identifier (0-3)
 * @param state  0=OFF, 1=ON
 */
void Driver_UI_SetLED(uint8_t led_id, uint8_t state);

/**
 * @brief Get LED state.
 * @param led_id LED identifier (0-3)
 * @return Current state (0=OFF, 1=ON)
 */
uint8_t Driver_UI_GetLED(uint8_t led_id);

/**
 * @brief Set simulated button state (for testing).
 * @param pressed 1=pressed, 0=released
 */
void Driver_UI_SetButton(uint8_t pressed);

/**
 * @brief Get button state from real hardware.
 * @return 1=pressed, 0=not pressed
 */
uint8_t Driver_UI_GetButton(void);

/* -------------------------------------------------------------------------- */
/*                           Bluetooth Functions                              */
/* -------------------------------------------------------------------------- */

void Driver_BT_QueueCommand(const char *cmd);
const char *Driver_BT_DequeueCommand(void);
void Driver_BT_SetLastTx(const char *text);
const char *Driver_BT_GetLastTx(void);
uint8_t Driver_BT_GetHistoryCount(void);
const char *Driver_BT_GetHistoryEntry(uint8_t offset_from_newest);
void Driver_BT_ClearHistory(void);

/* -------------------------------------------------------------------------- */
/*                            Debug Functions                                 */
/* -------------------------------------------------------------------------- */

void Driver_DebugPush(const DebugLogEntry_t *entry);
size_t Driver_DebugCopy(DebugLogEntry_t *out_entries, size_t max_entries);
const DebugLogEntry_t *Driver_DebugGetLast(void);
const DebugLogEntry_t *Driver_DebugPeek(uint8_t offset_from_newest);
void Driver_DebugClear(void);
