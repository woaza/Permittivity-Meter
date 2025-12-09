#include "bsp_rf.h"

#include <stdio.h>

#include "debug_log.h"
#include "drv/driver_board.h"

static float s_freq_voltage;
static float s_q_voltage;
static uint8_t s_gain_idx;
static uint8_t s_opamp_enabled;
static uint8_t s_excitation_enabled;

static void log_rf_value(const char *tag, float value)
{
    char buffer[24];
    (void)snprintf(buffer, sizeof(buffer), "%s=%.3f", tag, value);
    Debug_LogDriver("RF", buffer);
}

void BSP_RF_Init(void)
{
    Driver_Init();
    s_freq_voltage = 0.0f;
    s_q_voltage = 0.0f;
    s_gain_idx = 0U;
    s_opamp_enabled = 0U;
    s_excitation_enabled = 0U;
    Debug_LogDriver("RF", "init");
}

void BSP_RF_SetFreqVaricap(float voltage_v)
{
    s_freq_voltage = voltage_v;
    log_rf_value("freq", voltage_v);
}

void BSP_RF_SetQVaricap(float voltage_v)
{
    s_q_voltage = voltage_v;
    log_rf_value("q", voltage_v);
}

void BSP_RF_SetGain(uint8_t gain_idx)
{
    s_gain_idx = gain_idx & 0x03U;
    char buffer[16];
    (void)snprintf(buffer, sizeof(buffer), "gain=%u", (unsigned)s_gain_idx);
    Debug_LogDriver("RF", buffer);
}

void BSP_RF_SetOpAmpEnable(uint8_t enable)
{
    s_opamp_enabled = enable ? 1U : 0U;
    Debug_LogDriver("RF", s_opamp_enabled ? "opamp:on" : "opamp:off");
}

void BSP_RF_EnableExcitation(uint8_t enable)
{
    s_excitation_enabled = enable ? 1U : 0U;
    Debug_LogDriver("RF", s_excitation_enabled ? "excite:on" : "excite:off");
}

float BSP_RF_ReadAmplitude(void)
{
    return Driver_RF_ComputeAmplitude(s_freq_voltage, s_q_voltage, s_gain_idx);
}

void BSP_RF_MockSetResonanceVoltage(float voltage_v)
{
    Driver_RF_SetResonanceVoltage(voltage_v);
}

void BSP_RF_MockSetNoiseLevel(float noise_v)
{
    Driver_RF_SetNoise(noise_v);
}

void BSP_RF_MockSetBaseAmplitude(float amplitude_v)
{
    Driver_RF_SetBaseAmplitude(amplitude_v);
}

void BSP_RF_MockSetForceFailure(uint8_t enable)
{
    Driver_RF_SetForceFailure(enable);
}

