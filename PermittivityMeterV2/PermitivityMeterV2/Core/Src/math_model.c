#include "math_model.h"

#include <stddef.h>

typedef struct {
    float voltage;
    float capacitance_pf;
} VaricapSample_t;

static const VaricapSample_t kVaricapLut[] = {
    {0.00f, 190.0f},
    {0.25f, 150.0f},
    {0.50f, 120.0f},
    {0.75f, 100.0f},
    {1.00f, 85.0f},
    {1.25f, 72.0f},
    {1.50f, 62.0f},
    {1.75f, 54.0f},
    {2.00f, 48.0f},
    {2.25f, 43.0f},
    {2.50f, 39.0f}
};

static float interpolate(const VaricapSample_t *a, const VaricapSample_t *b, float voltage)
{
    const float span = b->voltage - a->voltage;
    if (span <= 0.0f) {
        return a->capacitance_pf;
    }
    const float t = (voltage - a->voltage) / span;
    return a->capacitance_pf + t * (b->capacitance_pf - a->capacitance_pf);
}

float Math_Varicap_VtoC(float voltage_v)
{
    if (voltage_v <= kVaricapLut[0].voltage) {
        return kVaricapLut[0].capacitance_pf;
    }
    const size_t count = sizeof(kVaricapLut) / sizeof(kVaricapLut[0]);
    for (size_t i = 1; i < count; ++i) {
        if (voltage_v <= kVaricapLut[i].voltage) {
            return interpolate(&kVaricapLut[i - 1U], &kVaricapLut[i], voltage_v);
        }
    }
    return kVaricapLut[count - 1U].capacitance_pf;
}

void Math_CalculateEpsilon(float v_air, float v_snow, float *epsilon_r, float *epsilon_i)
{
    const float c_air = Math_Varicap_VtoC(v_air);
    const float c_snow = Math_Varicap_VtoC(v_snow);
    const float ratio = (c_snow > 0.0f) ? (c_air / c_snow) : 1.0f;

    if (epsilon_r != NULL) {
        *epsilon_r = 1.0f + ratio * 0.5f;
    }

    if (epsilon_i != NULL) {
        *epsilon_i = 0.01f + (v_air - v_snow) * 0.02f;
    }
}

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

float Math_Goertzel_Magnitude(const uint16_t *samples, size_t num_samples, float target_freq, float sampling_rate)
{
    if (samples == NULL || num_samples == 0) {
        return 0.0f;
    }

    /* 1. Calculate k (bin index) and angular frequency omega */
    /* k = (N * f_target) / f_sample */
    float k = (float)num_samples * target_freq / sampling_rate;
    
    /* omega = (2 * pi * k) / N = 2 * pi * f_target / f_sample */
    float omega = (2.0f * (float)M_PI * k) / (float)num_samples;
    
    float sine = sinf(omega);
    float cosine = cosf(omega);
    float coeff = 2.0f * cosine;

    /* 2. Run the Goertzel loop */
    float q0 = 0.0f;
    float q1 = 0.0f;
    float q2 = 0.0f;

    for (size_t i = 0; i < num_samples; i++) {
        float sample_val = (float)samples[i];
        
        q0 = coeff * q1 - q2 + sample_val;
        q2 = q1;
        q1 = q0;
    }

    /* 3. Calculate Squared Magnitude */
    /* mag^2 = q1^2 + q2^2 - q1 * q2 * coeff */
    float magnitude_squared = (q1 * q1) + (q2 * q2) - (q1 * q2 * coeff);
    
    if (magnitude_squared < 0.0f) {
        magnitude_squared = 0.0f;
    }

    return sqrtf(magnitude_squared);
}
