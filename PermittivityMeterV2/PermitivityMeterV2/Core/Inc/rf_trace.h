#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RF_TRACE_MODE_NONE = 0,
    RF_TRACE_MODE_CALIBRATION,
    RF_TRACE_MODE_MEASUREMENT
} RF_TraceMode_t;

typedef struct {
    float voltage;
    float amplitude;
} RFTraceSample_t;

#define RF_TRACE_MAX_SAMPLES 128U

void RF_Trace_Begin(RF_TraceMode_t mode);
void RF_Trace_Add(float voltage, float amplitude);
void RF_Trace_End(void);
size_t RF_Trace_Copy(RFTraceSample_t *out_samples, size_t max_entries, RF_TraceMode_t *out_mode);
RF_TraceMode_t RF_Trace_GetMode(void);
size_t RF_Trace_GetCount(void);

#ifdef __cplusplus
}
#endif
