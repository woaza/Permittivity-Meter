#pragma once

#include "app_types.h"

CalibrationData_t RF_PerformAirCalibration(void);
MeasurementResult_t RF_PerformSnowMeasurement(CalibrationData_t calib);
