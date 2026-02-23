#include "rf_measure.h"
#include "main.h"

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "bsp_rf.h"
#include "debug_log.h"
#include "math_model.h"
#include "rf_trace.h"

#define COARSE_START_V      0.0f
#define COARSE_END_V        2.5f
#define COARSE_STEP_V       0.05f
#define FINE_STEP_V         0.005f
#define FINE_WINDOW_STEPS   5
#define DEFAULT_Q_VOLTAGE   1.0f
#define DEFAULT_GAIN_INDEX  1U
#define LOG_BUF_LEN         32U

static float clamp_voltage(float voltage)
{
    if (voltage < COARSE_START_V) {
        return COARSE_START_V;
    }
    if (voltage > COARSE_END_V) {
        return COARSE_END_V;
    }
    return voltage;
}

static void log_float_event(const char *domain, const char *label, float value)
{
    char buffer[LOG_BUF_LEN];
    (void)snprintf(buffer, sizeof(buffer), "%s=%.3f", label, value);
    Debug_LogEvent(domain, buffer);
}

static float sample_at(float freq_voltage, float q_voltage, uint8_t gain_idx, float *out_amp)
{
    BSP_RF_SetGain(gain_idx);
    BSP_RF_SetFreqVaricap(freq_voltage);
    BSP_RF_SetQVaricap(q_voltage);
    
    /* Task 2.3: Settling Delay */
    /* Ensure varactors and op-amps stabilize before sampling. */
    //HAL_Delay(1); 

    const float amplitude = BSP_RF_ReadAmplitude();
    RF_Trace_Add(freq_voltage, amplitude);
    if (out_amp != NULL) {
        *out_amp = amplitude;
    }
    return amplitude;
}

static bool coarse_sweep(float *best_voltage, float *best_amplitude)
{
    bool found = false;
    float local_best_v = COARSE_START_V;
    float local_best_amp = FLT_MAX;

    for (float v = COARSE_START_V; v <= COARSE_END_V + 0.0001f; v += COARSE_STEP_V) {
        float amp = 0.0f;
        sample_at(v, DEFAULT_Q_VOLTAGE, DEFAULT_GAIN_INDEX, &amp);
        if (!isfinite(amp)) {
            continue;
        }
        if (amp < local_best_amp) {
            local_best_amp = amp;
            local_best_v = v;
            found = true;
        }
    }

    if (best_voltage != NULL) {
        *best_voltage = clamp_voltage(local_best_v);
    }
    if (best_amplitude != NULL) {
        *best_amplitude = local_best_amp;
    }

    return found;
}

static bool fine_sweep(float coarse_best_v, float *best_voltage, float *best_amplitude)
{
    const float window = (float)FINE_WINDOW_STEPS * FINE_STEP_V;
    const float start = clamp_voltage(coarse_best_v - window);
    const float end = clamp_voltage(coarse_best_v + window);

    bool found = false;
    float best_v = clamp_voltage(coarse_best_v);
    float best_amp = FLT_MAX;

    for (float v = start; v <= end + 0.0001f; v += FINE_STEP_V) {
        float amp = 0.0f;
        sample_at(v, DEFAULT_Q_VOLTAGE, DEFAULT_GAIN_INDEX, &amp);
        if (!isfinite(amp)) {
            continue;
        }
        if (amp < best_amp) {
            best_amp = amp;
            best_v = v;
            found = true;
        }
    }

    if (best_voltage != NULL) {
        *best_voltage = best_v;
    }
    if (best_amplitude != NULL) {
        *best_amplitude = best_amp;
    }

    return found;
}

static float parabolic_vertex(float x1, float y1, float x2, float y2, float x3, float y3)
{
    const float denom = (x1 - x2) * (y1 - y3) - (x1 - x3) * (y1 - y2);
    if (!isfinite(denom) || denom == 0.0f) {
        return x2;
    }

    const float numerator =
        ((x1 * x1 - x2 * x2) * (y1 - y3) - (x1 * x1 - x3 * x3) * (y1 - y2));
    if (!isfinite(numerator)) {
        return x2;
    }

    return numerator / (2.0f * denom);
}

static bool refine_vertex(float center_voltage, float *out_vertex)
{
    const float center = clamp_voltage(center_voltage);
    const float left = clamp_voltage(center - FINE_STEP_V);
    const float right = clamp_voltage(center + FINE_STEP_V);

    float y_center = 0.0f;
    float y_left = 0.0f;
    float y_right = 0.0f;

    sample_at(center, DEFAULT_Q_VOLTAGE, DEFAULT_GAIN_INDEX, &y_center);
    sample_at(left, DEFAULT_Q_VOLTAGE, DEFAULT_GAIN_INDEX, &y_left);
    sample_at(right, DEFAULT_Q_VOLTAGE, DEFAULT_GAIN_INDEX, &y_right);

    if (!isfinite(y_center) || !isfinite(y_left) || !isfinite(y_right)) {
        if (out_vertex != NULL) {
            *out_vertex = center;
        }
        return false;
    }

    float vertex = parabolic_vertex(left, y_left, center, y_center, right, y_right);
    if (!isfinite(vertex) || vertex < COARSE_START_V || vertex > COARSE_END_V) {
        vertex = center;
        if (out_vertex != NULL) {
            *out_vertex = vertex;
        }
        return false;
    }

    if (out_vertex != NULL) {
        *out_vertex = vertex;
    }
    return true;
}

CalibrationData_t RF_PerformAirCalibration(void)
{
    CalibrationData_t data = {0};

    Debug_LogEvent("RF_CAL", "start");
    RF_Trace_Begin(RF_TRACE_MODE_CALIBRATION);

    BSP_RF_EnableExcitation(1U);
    BSP_RF_SetGain(DEFAULT_GAIN_INDEX);

    float coarse_best_v = COARSE_START_V;
    float coarse_best_amp = FLT_MAX;
    if (!coarse_sweep(&coarse_best_v, &coarse_best_amp)) {
        Debug_LogEvent("RF_CAL", "coarse_fail");
        goto fail;
    }
    log_float_event("RF_CAL", "coarse_v", coarse_best_v);

    float fine_best_v = coarse_best_v;
    float fine_best_amp = coarse_best_amp;
    if (!fine_sweep(coarse_best_v, &fine_best_v, &fine_best_amp)) {
        Debug_LogEvent("RF_CAL", "fine_fail");
        goto fail;
    }
    log_float_event("RF_CAL", "fine_v", fine_best_v);

    float vertex = fine_best_v;
    const bool vertex_ok = refine_vertex(fine_best_v, &vertex);
    if (!vertex_ok) {
        Debug_LogEvent("RF_CAL", "vertex_fallback");
    }

    if (!isfinite(fine_best_amp)) {
        Debug_LogEvent("RF_CAL", "amp_invalid");
        goto fail;
    }

    data.air_dac_freq_voltage = vertex;
    data.air_adc_min = fine_best_amp;
    data.timestamp = 0U; // Placeholder until RTC integration.
    data.is_valid = 1U;
    log_float_event("RF_CAL", "vertex", vertex);
    log_float_event("RF_CAL", "adc", fine_best_amp);
    Debug_LogEvent("RF_CAL", "ok");
    goto exit;

fail:
    data.air_dac_freq_voltage = coarse_best_v;
    data.air_adc_min = coarse_best_amp;
    data.timestamp = 0U;
    data.is_valid = 0U;
    Debug_LogEvent("RF_CAL", "fail");

exit:
    RF_Trace_End();
    BSP_RF_EnableExcitation(0U);
    return data;
}

MeasurementResult_t RF_PerformSnowMeasurement(CalibrationData_t calib)
{
    MeasurementResult_t result = {0};
    result.gain_index = DEFAULT_GAIN_INDEX;
    result.dac_q_voltage = DEFAULT_Q_VOLTAGE;

    if (calib.is_valid == 0U) {
        Debug_LogEvent("RF_MEAS", "no_cal");
        result.epsilon_real = 1.0f;
        result.epsilon_imag = 0.0f;
        return result;
    }

    Debug_LogEvent("RF_MEAS", "start");
    RF_Trace_Begin(RF_TRACE_MODE_MEASUREMENT);
    BSP_RF_EnableExcitation(1U);
    BSP_RF_SetGain(result.gain_index);

    const float window = 0.2f;
    const float start = clamp_voltage(calib.air_dac_freq_voltage - window);
    const float end = clamp_voltage(calib.air_dac_freq_voltage + window);

    bool found = false;
    float best_v = clamp_voltage(calib.air_dac_freq_voltage);
    float best_amp = FLT_MAX;

    for (float v = start; v <= end + 0.0001f; v += FINE_STEP_V) {
        float amp = 0.0f;
        sample_at(v, result.dac_q_voltage, result.gain_index, &amp);
        if (!isfinite(amp)) {
            continue;
        }
        if (amp < best_amp) {
            best_amp = amp;
            best_v = v;
            found = true;
        }
    }

    if (!found) {
        Debug_LogEvent("RF_MEAS", "sweep_fail");
        result.dac_freq_voltage = calib.air_dac_freq_voltage;
        result.adc_voltage_min = FLT_MAX;
        goto meas_exit;
    }

    float vertex = best_v;
    const bool vertex_ok = refine_vertex(best_v, &vertex);
    if (!vertex_ok) {
        Debug_LogEvent("RF_MEAS", "vertex_fallback");
    }

    result.dac_freq_voltage = vertex;
    result.adc_voltage_min = best_amp;
    result.frequency_shift = vertex - calib.air_dac_freq_voltage;
    Math_CalculateEpsilon(
        calib.air_dac_freq_voltage, vertex, &result.epsilon_real, &result.epsilon_imag);
    result.snow_density = 0.3f + result.frequency_shift * 0.1f;
    log_float_event("RF_MEAS", "vertex", vertex);
    log_float_event("RF_MEAS", "shift", result.frequency_shift);
    Debug_LogEvent("RF_MEAS", "ok");

meas_exit:
    RF_Trace_End();
    BSP_RF_EnableExcitation(0U);
    return result;
}
