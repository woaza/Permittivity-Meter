#include <stdint.h>
#include <stddef.h>

float Math_Varicap_VtoC(float voltage_v);
void Math_CalculateEpsilon(float v_air, float v_snow, float *epsilon_r, float *epsilon_i);

/**
 * @brief Calculates the magnitude of a specific frequency component using the Goertzel algorithm.
 * 
 * @param samples       Pointer to the ADC sample buffer.
 * @param num_samples   Number of samples in the buffer (N).
 * @param target_freq   The frequency component to extract (in Hz).
 * @param sampling_rate The ADC sampling rate (in Hz).
 * @return float        The magnitude of the target frequency.
 */
float Math_Goertzel_Magnitude(const uint16_t *samples, size_t num_samples, float target_freq, float sampling_rate);
