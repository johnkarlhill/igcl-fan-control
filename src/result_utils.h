//===========================================================================
// IGCL Fan Control — Result/helper utilities
//===========================================================================
#pragma once
#include "igcl_api.h"

const char* ResultStr(ctl_result_t res);
const char* FanModeStr(ctl_fan_speed_mode_t mode);
const char* FanUnitsStr(ctl_fan_speed_units_t units);
void PrintSeparator(const char* title = nullptr);
