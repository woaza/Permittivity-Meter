#pragma once

#include <stdint.h>

void BSP_RF_Init(void);
void BSP_RF_SetFreqVaricap(float voltage_v);
void BSP_RF_SetQVaricap(float voltage_v);
void BSP_RF_SetGain(uint8_t gain_idx);
void BSP_RF_SetOpAmpEnable(uint8_t enable);
void BSP_RF_EnableExcitation(uint8_t enable);
float BSP_RF_ReadAmplitude(void);

// Mock control helpers to script scenarios from higher-level tests.
void BSP_RF_MockSetResonanceVoltage(float voltage_v);
void BSP_RF_MockSetNoiseLevel(float noise_v);
void BSP_RF_MockSetBaseAmplitude(float amplitude_v);
void BSP_RF_MockSetForceFailure(uint8_t enable);
