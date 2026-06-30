//===========================================================================
// IGCL Fan Control — Fan profile application
//===========================================================================
#pragma once
#include "igcl_api.h"
#include "ini_reader.h"

// Apply a fan profile to a single fan handle.
// Returns true if the profile was applied (or dry-run was successful).
bool ApplyProfile(ctl_fan_handle_t hFan, const FanProfile& profile, bool dryRun);

// Reset fan to hardware default mode.
bool ResetFanToDefault(ctl_fan_handle_t hFan, bool dryRun);

// Return a short description string for the profile (for dry-run output).
const char* ProfileModeStr(const FanProfile& profile);
