/**
 * @file    driver_board.c
 * @brief   Driver Layer - Hardware Abstraction Bridge
 * @author  Majdedin Al Rashid
 * @version 1.0
 * @date    2025-12-09
 * 
 * @details This module bridges BSP calls to real HAL hardware drivers.
 *          UI functions now control real GPIO pins via hal_gpio.
 *          RF simulation math is retained until ADC+DFT processing is implemented.
 */

#include "drv/driver_board.h"
#include "hl/hal_gpio.h"

#include <math.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                              Configuration                                 */
/* -------------------------------------------------------------------------- */

#ifndef DRIVER_BT_QUEUE_DEPTH
#define DRIVER_BT_QUEUE_DEPTH 4
#endif

#ifndef DRIVER_BT_HISTORY_DEPTH
#define DRIVER_BT_HISTORY_DEPTH 16
#endif

#ifndef DRIVER_DEBUG_DEPTH
#define DRIVER_DEBUG_DEPTH 32
#endif

/* -------------------------------------------------------------------------- */
/*                              Private Types                                 */
/* -------------------------------------------------------------------------- */

typedef struct 
{
    float resonance_voltage;
    float base_amplitude;
    float noise_level;
    float last_freq_voltage;
    float last_q_voltage;
    uint8_t gain_index;
    uint8_t force_failure;
} DriverRFState_t;

typedef struct 
{
    uint8_t leds[LED_COUNT];
    uint8_t button_pressed;  /* For simulation/testing only */
} DriverUIState_t;

typedef struct 
{
    char queue[DRIVER_BT_QUEUE_DEPTH][32];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
    char last_tx[96];
    char history[DRIVER_BT_HISTORY_DEPTH][96];
    uint8_t history_head;
    uint8_t history_count;
} DriverBTState_t;

typedef struct 
{
    DebugLogEntry_t entries[DRIVER_DEBUG_DEPTH];
    uint8_t head;
    uint8_t count;
} DriverDebugState_t;

/* -------------------------------------------------------------------------- */
/*                              Private Data                                  */
/* -------------------------------------------------------------------------- */

static DriverRFState_t s_rf_state;
static DriverUIState_t s_ui_state;
static DriverBTState_t s_bt_state;
static DriverDebugState_t s_debug_state;
static uint8_t s_initialized;

/* -------------------------------------------------------------------------- */
/*                              Private Helpers                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief Map LED ID to HAL GPIO pin.
 */
static HL_GPIO_Pin_t led_id_to_gpio(uint8_t led_id)
{
    switch (led_id)
    {
        case 0: return HL_GPIO_LED_INIT;
        case 1: return HL_GPIO_LED_MEAS;
        case 2: return HL_GPIO_LED_EXCITE;
        case 3: return HL_GPIO_LED_ERR;
        default: return HL_GPIO_LED_INIT;
    }
}

static void reset_state(void)
{
    memset(&s_rf_state, 0, sizeof(s_rf_state));
    memset(&s_ui_state, 0, sizeof(s_ui_state));
    memset(&s_bt_state, 0, sizeof(s_bt_state));
    memset(&s_debug_state, 0, sizeof(s_debug_state));

    /* Default RF simulation parameters */
    s_rf_state.resonance_voltage = 1.2f;
    s_rf_state.base_amplitude = 1.0f;
    s_rf_state.noise_level = 0.01f;
    s_initialized = 1U;
}

/* -------------------------------------------------------------------------- */
/*                              Initialization                                */
/* -------------------------------------------------------------------------- */

void Driver_Init(void)
{
    if (!s_initialized) 
    {
        reset_state();
    }
    
    /* Initialize HAL GPIO (already done in main.c, but safe to call again) */
    HL_GPIO_Init();
}

void Driver_Reset(void)
{
    reset_state();
}

/* -------------------------------------------------------------------------- */
/*                              RF Functions                                  */
/* -------------------------------------------------------------------------- */

float Driver_RF_ComputeAmplitude(float freq_voltage, float q_voltage, uint8_t gain_idx)
{
    s_rf_state.last_freq_voltage = freq_voltage;
    s_rf_state.last_q_voltage = q_voltage;
    s_rf_state.gain_index = gain_idx;

    if (s_rf_state.force_failure) 
    {
        return NAN;
    }

    /* Simulation: parabolic dip at resonance voltage */
    const float delta = freq_voltage - s_rf_state.resonance_voltage;
    const float curvature = 0.3f + (float)gain_idx * 0.05f;
    float amplitude = s_rf_state.base_amplitude + curvature * delta * delta;
    amplitude -= q_voltage * 0.02f;
    amplitude += s_rf_state.noise_level;
    
    /* TODO: Replace with real ADC + DFT processing when ready */
    
    return amplitude;
}

void Driver_RF_SetResonanceVoltage(float voltage_v)
{
    s_rf_state.resonance_voltage = voltage_v;
}

void Driver_RF_SetNoise(float noise_v)
{
    s_rf_state.noise_level = noise_v;
}

void Driver_RF_SetBaseAmplitude(float amplitude_v)
{
    s_rf_state.base_amplitude = amplitude_v;
}

void Driver_RF_SetForceFailure(uint8_t enable)
{
    s_rf_state.force_failure = enable ? 1U : 0U;
}

/* -------------------------------------------------------------------------- */
/*                              UI Functions                                  */
/* -------------------------------------------------------------------------- */

void Driver_UI_SetLED(uint8_t led_id, uint8_t state)
{
    if (led_id < LED_COUNT) 
    {
        s_ui_state.leds[led_id] = state;
        
        /* Write to real hardware via HAL GPIO */
        HL_GPIO_Pin_t pin = led_id_to_gpio(led_id);
        HL_GPIO_State_t gpio_state = state ? HL_GPIO_HIGH : HL_GPIO_LOW;
        HL_GPIO_Write(pin, gpio_state);
    }
}

uint8_t Driver_UI_GetLED(uint8_t led_id)
{
    if (led_id < LED_COUNT)
    {
        /* Read from real hardware via HAL GPIO */
        HL_GPIO_Pin_t pin = led_id_to_gpio(led_id);
        HL_GPIO_State_t state = HL_GPIO_LOW;
        HL_GPIO_Read(pin, &state);
        return (state == HL_GPIO_HIGH) ? 1U : 0U;
    }
    return 0U;
}

void Driver_UI_SetButton(uint8_t pressed)
{
    /* For simulation/testing - stores virtual button state */
    s_ui_state.button_pressed = pressed;
}

uint8_t Driver_UI_GetButton(void)
{
    /* Read from real hardware via HAL GPIO */
    HL_GPIO_State_t state = HL_GPIO_LOW;
    HL_GPIO_Read(HL_GPIO_BTN_USER, &state);
    
    /* Button is active-low on Nucleo boards (pressed = LOW) */
    return (state == HL_GPIO_LOW) ? 1U : 0U;
}

/* -------------------------------------------------------------------------- */
/*                           Bluetooth Functions                              */
/* -------------------------------------------------------------------------- */

void Driver_BT_QueueCommand(const char *cmd)
{
    if (s_bt_state.count >= DRIVER_BT_QUEUE_DEPTH) 
    {
        return;
    }
    strncpy(s_bt_state.queue[s_bt_state.tail], cmd, sizeof(s_bt_state.queue[0]) - 1U);
    s_bt_state.queue[s_bt_state.tail][sizeof(s_bt_state.queue[0]) - 1U] = '\0';
    s_bt_state.tail = (uint8_t)((s_bt_state.tail + 1U) % DRIVER_BT_QUEUE_DEPTH);
    ++s_bt_state.count;
}

const char *Driver_BT_DequeueCommand(void)
{
    if (s_bt_state.count == 0U) 
    {
        return NULL;
    }
    const char *cmd = s_bt_state.queue[s_bt_state.head];
    s_bt_state.head = (uint8_t)((s_bt_state.head + 1U) % DRIVER_BT_QUEUE_DEPTH);
    --s_bt_state.count;
    return cmd;
}

void Driver_BT_SetLastTx(const char *text)
{
    if (text == NULL) 
    {
        s_bt_state.last_tx[0] = '\0';
        return;
    }

    strncpy(s_bt_state.last_tx, text, sizeof(s_bt_state.last_tx) - 1U);
    s_bt_state.last_tx[sizeof(s_bt_state.last_tx) - 1U] = '\0';

    strncpy(s_bt_state.history[s_bt_state.history_head], s_bt_state.last_tx,
            sizeof(s_bt_state.history[0]) - 1U);
    s_bt_state.history[s_bt_state.history_head][sizeof(s_bt_state.history[0]) - 1U] = '\0';
    s_bt_state.history_head = (uint8_t)((s_bt_state.history_head + 1U) % DRIVER_BT_HISTORY_DEPTH);
    if (s_bt_state.history_count < DRIVER_BT_HISTORY_DEPTH) 
    {
        ++s_bt_state.history_count;
    }
}

const char *Driver_BT_GetLastTx(void)
{
    return s_bt_state.last_tx;
}

uint8_t Driver_BT_GetHistoryCount(void)
{
    return s_bt_state.history_count;
}

const char *Driver_BT_GetHistoryEntry(uint8_t offset_from_newest)
{
    if (offset_from_newest >= s_bt_state.history_count) 
    {
        return NULL;
    }

    const uint8_t newest_index = (uint8_t)((DRIVER_BT_HISTORY_DEPTH + s_bt_state.history_head - 1U) % DRIVER_BT_HISTORY_DEPTH);
    const uint8_t index = (uint8_t)((DRIVER_BT_HISTORY_DEPTH + newest_index - offset_from_newest) % DRIVER_BT_HISTORY_DEPTH);
    return s_bt_state.history[index];
}

void Driver_BT_ClearHistory(void)
{
    memset(s_bt_state.history, 0, sizeof(s_bt_state.history));
    s_bt_state.history_head = 0U;
    s_bt_state.history_count = 0U;
    s_bt_state.last_tx[0] = '\0';
}

/* -------------------------------------------------------------------------- */
/*                            Debug Functions                                 */
/* -------------------------------------------------------------------------- */

void Driver_DebugPush(const DebugLogEntry_t *entry)
{
    if (entry == NULL) 
    {
        return;
    }

    const uint8_t write_index = (uint8_t)((s_debug_state.head + s_debug_state.count) % DRIVER_DEBUG_DEPTH);
    s_debug_state.entries[write_index] = *entry;

    if (s_debug_state.count == DRIVER_DEBUG_DEPTH) 
    {
        s_debug_state.head = (uint8_t)((s_debug_state.head + 1U) % DRIVER_DEBUG_DEPTH);
    } 
    else 
    {
        ++s_debug_state.count;
    }
}

size_t Driver_DebugCopy(DebugLogEntry_t *out_entries, size_t max_entries)
{
    if (out_entries == NULL || max_entries == 0U) 
    {
        return 0U;
    }

    size_t to_copy = s_debug_state.count;
    if (to_copy > max_entries) 
    {
        to_copy = max_entries;
    }

    for (size_t i = 0; i < to_copy; ++i) 
    {
        const uint8_t index = (uint8_t)((s_debug_state.head + i) % DRIVER_DEBUG_DEPTH);
        out_entries[i] = s_debug_state.entries[index];
    }

    return to_copy;
}

const DebugLogEntry_t *Driver_DebugGetLast(void)
{
    if (s_debug_state.count == 0U) 
    {
        return NULL;
    }

    const uint8_t index = (uint8_t)((s_debug_state.head + s_debug_state.count - 1U) % DRIVER_DEBUG_DEPTH);
    return &s_debug_state.entries[index];
}

const DebugLogEntry_t *Driver_DebugPeek(uint8_t offset_from_newest)
{
    if (offset_from_newest >= s_debug_state.count) 
    {
        return NULL;
    }

    const uint8_t newest_index = (uint8_t)((s_debug_state.head + s_debug_state.count - 1U) % DRIVER_DEBUG_DEPTH);
    const uint8_t index = (uint8_t)((DRIVER_DEBUG_DEPTH + newest_index - offset_from_newest) % DRIVER_DEBUG_DEPTH);
    return &s_debug_state.entries[index];
}

void Driver_DebugClear(void)
{
    memset(&s_debug_state, 0, sizeof(s_debug_state));
}
