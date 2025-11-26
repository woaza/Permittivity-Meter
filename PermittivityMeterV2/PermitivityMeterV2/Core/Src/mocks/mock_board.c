#include "mocks/mock_board.h"

#include <math.h>
#include <string.h>

#ifndef MOCK_BT_QUEUE_DEPTH
#define MOCK_BT_QUEUE_DEPTH 4
#endif

#ifndef MOCK_BT_HISTORY_DEPTH
#define MOCK_BT_HISTORY_DEPTH 16
#endif

#ifndef MOCK_DEBUG_DEPTH
#define MOCK_DEBUG_DEPTH 32
#endif

typedef struct {
    float resonance_voltage;
    float base_amplitude;
    float noise_level;
    float last_freq_voltage;
    float last_q_voltage;
    uint8_t gain_index;
    uint8_t force_failure;
} MockRFState_t;

typedef struct {
    uint8_t leds[LED_COUNT];
    uint8_t button_pressed;
} MockUIState_t;

typedef struct {
    char queue[MOCK_BT_QUEUE_DEPTH][32];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
    char last_tx[96];
    char history[MOCK_BT_HISTORY_DEPTH][96];
    uint8_t history_head;
    uint8_t history_count;
} MockBTState_t;

typedef struct {
    DebugLogEntry_t entries[MOCK_DEBUG_DEPTH];
    uint8_t head;
    uint8_t count;
} MockDebugState_t;

static MockRFState_t s_rf_state;
static MockUIState_t s_ui_state;
static MockBTState_t s_bt_state;
static MockDebugState_t s_debug_state;
static uint8_t s_initialized;

static void reset_state(void)
{
    memset(&s_rf_state, 0, sizeof(s_rf_state));
    memset(&s_ui_state, 0, sizeof(s_ui_state));
    memset(&s_bt_state, 0, sizeof(s_bt_state));
    memset(&s_debug_state, 0, sizeof(s_debug_state));

    s_rf_state.resonance_voltage = 1.2f;
    s_rf_state.base_amplitude = 1.0f;
    s_rf_state.noise_level = 0.01f;
    s_initialized = 1U;
}

void MockBoard_Init(void)
{
    if (!s_initialized) {
        reset_state();
    }
}

void MockBoard_Reset(void)
{
    reset_state();
}

float MockBoard_RF_ComputeAmplitude(float freq_voltage, float q_voltage, uint8_t gain_idx)
{
    s_rf_state.last_freq_voltage = freq_voltage;
    s_rf_state.last_q_voltage = q_voltage;
    s_rf_state.gain_index = gain_idx;

    if (s_rf_state.force_failure) {
        return NAN;
    }

    const float delta = freq_voltage - s_rf_state.resonance_voltage;
    const float curvature = 0.3f + (float)gain_idx * 0.05f;
    float amplitude = s_rf_state.base_amplitude + curvature * delta * delta;
    amplitude -= q_voltage * 0.02f;
    amplitude += s_rf_state.noise_level;
    return amplitude;
}

void MockBoard_RF_SetResonanceVoltage(float voltage_v)
{
    s_rf_state.resonance_voltage = voltage_v;
}

void MockBoard_RF_SetNoise(float noise_v)
{
    s_rf_state.noise_level = noise_v;
}

void MockBoard_RF_SetBaseAmplitude(float amplitude_v)
{
    s_rf_state.base_amplitude = amplitude_v;
}

void MockBoard_RF_SetForceFailure(uint8_t enable)
{
    s_rf_state.force_failure = enable ? 1U : 0U;
}

void MockBoard_UI_SetLED(uint8_t led_id, uint8_t state)
{
    if (led_id < LED_COUNT) {
        s_ui_state.leds[led_id] = state;
    }
}

uint8_t MockBoard_UI_GetLED(uint8_t led_id)
{
    return (led_id < LED_COUNT) ? s_ui_state.leds[led_id] : 0U;
}

void MockBoard_UI_SetButton(uint8_t pressed)
{
    s_ui_state.button_pressed = pressed;
}

uint8_t MockBoard_UI_GetButton(void)
{
    return s_ui_state.button_pressed;
}

void MockBoard_BT_QueueCommand(const char *cmd)
{
    if (s_bt_state.count >= MOCK_BT_QUEUE_DEPTH) {
        return;
    }
    strncpy(s_bt_state.queue[s_bt_state.tail], cmd, sizeof(s_bt_state.queue[0]) - 1U);
    s_bt_state.queue[s_bt_state.tail][sizeof(s_bt_state.queue[0]) - 1U] = '\0';
    s_bt_state.tail = (uint8_t)((s_bt_state.tail + 1U) % MOCK_BT_QUEUE_DEPTH);
    ++s_bt_state.count;
}

const char *MockBoard_BT_DequeueCommand(void)
{
    if (s_bt_state.count == 0U) {
        return NULL;
    }
    const char *cmd = s_bt_state.queue[s_bt_state.head];
    s_bt_state.head = (uint8_t)((s_bt_state.head + 1U) % MOCK_BT_QUEUE_DEPTH);
    --s_bt_state.count;
    return cmd;
}

void MockBoard_BT_SetLastTx(const char *text)
{
    if (text == NULL) {
        s_bt_state.last_tx[0] = '\0';
        return;
    }

    strncpy(s_bt_state.last_tx, text, sizeof(s_bt_state.last_tx) - 1U);
    s_bt_state.last_tx[sizeof(s_bt_state.last_tx) - 1U] = '\0';

    strncpy(s_bt_state.history[s_bt_state.history_head], s_bt_state.last_tx,
            sizeof(s_bt_state.history[0]) - 1U);
    s_bt_state.history[s_bt_state.history_head][sizeof(s_bt_state.history[0]) - 1U] = '\0';
    s_bt_state.history_head = (uint8_t)((s_bt_state.history_head + 1U) % MOCK_BT_HISTORY_DEPTH);
    if (s_bt_state.history_count < MOCK_BT_HISTORY_DEPTH) {
        ++s_bt_state.history_count;
    }
}

const char *MockBoard_BT_GetLastTx(void)
{
    return s_bt_state.last_tx;
}

uint8_t MockBoard_BT_GetHistoryCount(void)
{
    return s_bt_state.history_count;
}

const char *MockBoard_BT_GetHistoryEntry(uint8_t offset_from_newest)
{
    if (offset_from_newest >= s_bt_state.history_count) {
        return NULL;
    }

    const uint8_t newest_index = (uint8_t)((MOCK_BT_HISTORY_DEPTH + s_bt_state.history_head - 1U) % MOCK_BT_HISTORY_DEPTH);
    const uint8_t index = (uint8_t)((MOCK_BT_HISTORY_DEPTH + newest_index - offset_from_newest) % MOCK_BT_HISTORY_DEPTH);
    return s_bt_state.history[index];
}

void MockBoard_BT_ClearHistory(void)
{
    memset(s_bt_state.history, 0, sizeof(s_bt_state.history));
    s_bt_state.history_head = 0U;
    s_bt_state.history_count = 0U;
    s_bt_state.last_tx[0] = '\0';
}

void MockBoard_DebugPush(const DebugLogEntry_t *entry)
{
    if (entry == NULL) {
        return;
    }

    const uint8_t write_index = (uint8_t)((s_debug_state.head + s_debug_state.count) % MOCK_DEBUG_DEPTH);
    s_debug_state.entries[write_index] = *entry;

    if (s_debug_state.count == MOCK_DEBUG_DEPTH) {
        s_debug_state.head = (uint8_t)((s_debug_state.head + 1U) % MOCK_DEBUG_DEPTH);
    } else {
        ++s_debug_state.count;
    }
    
    // Output to console
    printf("[%s] %s\n", 
           (entry->domain == DEBUG_LOG_DOMAIN_STATE) ? "STATE" :
           (entry->domain == DEBUG_LOG_DOMAIN_EVENT) ? "EVENT" : "DRV",
           entry->text);
}

size_t MockBoard_DebugCopy(DebugLogEntry_t *out_entries, size_t max_entries)
{
    if (out_entries == NULL || max_entries == 0U) {
        return 0U;
    }

    size_t to_copy = s_debug_state.count;
    if (to_copy > max_entries) {
        to_copy = max_entries;
    }

    for (size_t i = 0; i < to_copy; ++i) {
        const uint8_t index = (uint8_t)((s_debug_state.head + i) % MOCK_DEBUG_DEPTH);
        out_entries[i] = s_debug_state.entries[index];
    }

    return to_copy;
}

const DebugLogEntry_t *MockBoard_DebugGetLast(void)
{
    if (s_debug_state.count == 0U) {
        return NULL;
    }

    const uint8_t index = (uint8_t)((s_debug_state.head + s_debug_state.count - 1U) % MOCK_DEBUG_DEPTH);
    return &s_debug_state.entries[index];
}

const DebugLogEntry_t *MockBoard_DebugPeek(uint8_t offset_from_newest)
{
    if (offset_from_newest >= s_debug_state.count) {
        return NULL;
    }

    const uint8_t newest_index = (uint8_t)((s_debug_state.head + s_debug_state.count - 1U) % MOCK_DEBUG_DEPTH);
    const uint8_t index = (uint8_t)((MOCK_DEBUG_DEPTH + newest_index - offset_from_newest) % MOCK_DEBUG_DEPTH);
    return &s_debug_state.entries[index];
}

void MockBoard_DebugClear(void)
{
    memset(&s_debug_state, 0, sizeof(s_debug_state));
}
