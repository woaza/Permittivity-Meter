#include "rf_trace.h"

#include <string.h>

#ifndef RF_TRACE_MAX_SAMPLES
#define RF_TRACE_MAX_SAMPLES 128U
#endif

typedef struct {
    RF_TraceMode_t mode;
    RFTraceSample_t samples[RF_TRACE_MAX_SAMPLES];
    size_t count;
    uint8_t active;
} RFTraceState_t;

static RFTraceState_t s_state;

void RF_Trace_Begin(RF_TraceMode_t mode)
{
    s_state.mode = mode;
    s_state.count = 0U;
    s_state.active = 1U;
}

void RF_Trace_Add(float voltage, float amplitude)
{
    if (!s_state.active || s_state.count >= RF_TRACE_MAX_SAMPLES) {
        return;
    }

    s_state.samples[s_state.count].voltage = voltage;
    s_state.samples[s_state.count].amplitude = amplitude;
    ++s_state.count;
}

void RF_Trace_End(void)
{
    s_state.active = 0U;
}

size_t RF_Trace_Copy(RFTraceSample_t *out_samples, size_t max_entries, RF_TraceMode_t *out_mode)
{
    if (out_samples == NULL || max_entries == 0U) {
        return 0U;
    }

    if (out_mode != NULL) {
        *out_mode = s_state.mode;
    }

    const size_t count = (s_state.count < max_entries) ? s_state.count : max_entries;
    memcpy(out_samples, s_state.samples, count * sizeof(RFTraceSample_t));
    return count;
}

RF_TraceMode_t RF_Trace_GetMode(void)
{
    return s_state.mode;
}

size_t RF_Trace_GetCount(void)
{
    return s_state.count;
}
