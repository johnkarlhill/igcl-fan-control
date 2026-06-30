//===========================================================================
// IGCL Fan Control — Result/helper utilities
//===========================================================================
#include "result_utils.h"
#include <cstdio>

const char* ResultStr(ctl_result_t res) {
    switch (res) {
        case CTL_RESULT_SUCCESS:                       return "SUCCESS";
        case CTL_RESULT_ERROR_NOT_INITIALIZED:          return "NOT_INITIALIZED";
        case CTL_RESULT_ERROR_ALREADY_INITIALIZED:      return "ALREADY_INITIALIZED";
        case CTL_RESULT_ERROR_DEVICE_LOST:              return "DEVICE_LOST";
        case CTL_RESULT_ERROR_INSUFFICIENT_PERMISSIONS: return "INSUFFICIENT_PERMISSIONS (try Admin)";
        case CTL_RESULT_ERROR_NOT_AVAILABLE:            return "NOT_AVAILABLE";
        case CTL_RESULT_ERROR_UNINITIALIZED:            return "UNINITIALIZED";
        case CTL_RESULT_ERROR_UNSUPPORTED_FEATURE:      return "UNSUPPORTED_FEATURE";
        case CTL_RESULT_ERROR_INVALID_ARGUMENT:         return "INVALID_ARGUMENT";
        case CTL_RESULT_ERROR_INVALID_NULL_HANDLE:      return "INVALID_NULL_HANDLE";
        case CTL_RESULT_ERROR_INVALID_NULL_POINTER:     return "INVALID_NULL_POINTER";
        case CTL_RESULT_ERROR_NOT_IMPLEMENTED:          return "NOT_IMPLEMENTED";
        case CTL_RESULT_ERROR_DEVICE_UNAVAILABLE:       return "DEVICE_UNAVAILABLE";
        default:                                        return "UNKNOWN_ERROR";
    }
}

const char* FanModeStr(ctl_fan_speed_mode_t mode) {
    switch (mode) {
        case CTL_FAN_SPEED_MODE_DEFAULT: return "DEFAULT (hardware)";
        case CTL_FAN_SPEED_MODE_FIXED:   return "FIXED SPEED";
        case CTL_FAN_SPEED_MODE_TABLE:   return "CUSTOM TABLE (fan curve)";
        default:                         return "UNKNOWN";
    }
}

const char* FanUnitsStr(ctl_fan_speed_units_t units) {
    switch (units) {
        case CTL_FAN_SPEED_UNITS_RPM:     return "RPM";
        case CTL_FAN_SPEED_UNITS_PERCENT: return "%";
        default:                          return "?";
    }
}

void PrintSeparator(const char* title) {
    printf("\n==================================================================\n");
    if (title) printf("  %s\n", title);
    printf("==================================================================\n");
}
