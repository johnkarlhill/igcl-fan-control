//===========================================================================
// IGCL Fan Control — Fan profile application
//
// Key insight from testing on Battlemage (Arc Pro B50/B70):
//   - Driver reports supportedUnits=RPM-only but PERCENT works. RPM is often REJECTED.
//   - Driver reports supportedModes=FIXED-only but TABLE (curve) works. FIXED is often REJECTED.
//   - The tool ALWAYS tries PERCENT first, RPM second.
//   - The tool ALWAYS tries TABLE first, FIXED second (for curve profiles).
//===========================================================================
#include "fan_control.h"
#include "result_utils.h"
#include <cstdio>

//===========================================================================
// Read current fan properties + config + state (for --info)
//===========================================================================
void ShowFanConfig(ctl_fan_handle_t hFan) {
    ctl_fan_config_t cfg = {};
    cfg.Size = sizeof(cfg);
    ctl_result_t res = ctlFanGetConfig(hFan, &cfg);
    if (res != CTL_RESULT_SUCCESS) {
        printf("  [WARN] ctlFanGetConfig: %s\n", ResultStr(res));
        return;
    }

    printf("  Current mode:    %s\n", FanModeStr(cfg.mode));

    if (cfg.mode == CTL_FAN_SPEED_MODE_FIXED) {
        printf("  Fixed speed:     %d %s\n",
               cfg.speedFixed.speed, FanUnitsStr(cfg.speedFixed.units));
    } else if (cfg.mode == CTL_FAN_SPEED_MODE_TABLE) {
        printf("  Curve points:    %d\n", cfg.speedTable.numPoints);
        printf("  %-12s %s\n", "Temperature", "Fan Speed");
        printf("  %-12s %s\n", "-----------", "---------");
        for (int32_t i = 0; i < cfg.speedTable.numPoints; i++) {
            printf("  %4u C         %3d %s\n",
                   cfg.speedTable.table[i].temperature,
                   cfg.speedTable.table[i].speed.speed,
                   FanUnitsStr(cfg.speedTable.table[i].speed.units));
        }
    }
}

void ShowFanState(ctl_fan_handle_t hFan) {
    int32_t speed = 0;
    ctl_result_t res;

    res = ctlFanGetState(hFan, CTL_FAN_SPEED_UNITS_RPM, &speed);
    if (res == CTL_RESULT_SUCCESS && speed >= 0)
        printf("  Current speed:   %d RPM\n", speed);

    res = ctlFanGetState(hFan, CTL_FAN_SPEED_UNITS_PERCENT, &speed);
    if (res == CTL_RESULT_SUCCESS && speed >= 0)
        printf("  Current speed:   %d %%\n", speed);
}

const char* ProfileModeStr(const FanProfile& profile) {
    switch (profile.mode) {
        case FanProfile::DEFAULT: return "HARDWARE DEFAULT";
        case FanProfile::FIXED:   return "FIXED SPEED";
        case FanProfile::TABLE:   return "CUSTOM CURVE";
        default:                  return "UNKNOWN";
    }
}

//===========================================================================
// Build and apply a temperature/speed table (curve)
// Returns: 0 = failed, 1 = succeeded with PERCENT, 2 = succeeded with RPM
//===========================================================================
static int ApplyTableMode(ctl_fan_handle_t hFan, const FanProfile& profile, bool dryRun) {
    ctl_fan_speed_table_t table = {};
    table.Size = sizeof(table);
    table.Version = 0;

    // Build the table in the configured units
    int n = (int)profile.points.size();
    if (n > CTL_FAN_TEMP_SPEED_PAIR_COUNT) n = CTL_FAN_TEMP_SPEED_PAIR_COUNT;
    table.numPoints = n;

    for (int i = 0; i < n; i++) {
        table.table[i].Size = sizeof(ctl_fan_temp_speed_t);
        table.table[i].Version = 0;
        table.table[i].temperature = profile.points[i].temp;
        table.table[i].speed.Size = sizeof(ctl_fan_speed_t);
        table.table[i].speed.Version = 0;
        table.table[i].speed.speed = profile.points[i].speed;
        table.table[i].speed.units = (profile.units == FanProfile::RPM)
            ? CTL_FAN_SPEED_UNITS_RPM : CTL_FAN_SPEED_UNITS_PERCENT;
    }

    if (dryRun) {
        printf("  [DRY-RUN] Would set TABLE curve (%s units):\n",
               profile.units == FanProfile::RPM ? "RPM" : "PERCENT");
        for (int i = 0; i < n; i++) {
            printf("    %3u C -> %3d %s\n",
                   table.table[i].temperature,
                   table.table[i].speed.speed,
                   profile.units == FanProfile::RPM ? "RPM" : "%");
        }
        return 1;
    }

    // --- Attempt 1: with the configured units ---
    ctl_result_t res = ctlFanSetSpeedTableMode(hFan, &table);
    if (res == CTL_RESULT_SUCCESS) return 1;

    printf("  TABLE (%s) failed: %s\n",
           profile.units == FanProfile::RPM ? "RPM" : "PERCENT", ResultStr(res));

    // --- Attempt 2: try the other unit ---
    ctl_fan_speed_units_t altUnits;
    const char* altLabel;
    if (profile.units == FanProfile::RPM) {
        altUnits = CTL_FAN_SPEED_UNITS_PERCENT;
        altLabel = "PERCENT";
        // Convert RPM back to percent (assume ~6000 max if unknown)
        for (int i = 0; i < n; i++)
            table.table[i].speed.speed = (profile.points[i].speed <= 6000)
                ? (profile.points[i].speed * 100) / 6000 : 100;
    } else {
        altUnits = CTL_FAN_SPEED_UNITS_RPM;
        altLabel = "RPM";
        // Convert percent to RPM (assume ~6000 max)
        for (int i = 0; i < n; i++)
            table.table[i].speed.speed = (profile.points[i].speed * 6000) / 100;
    }
    for (int i = 0; i < n; i++)
        table.table[i].speed.units = altUnits;

    printf("  Trying TABLE (%s)...\n", altLabel);
    res = ctlFanSetSpeedTableMode(hFan, &table);
    if (res == CTL_RESULT_SUCCESS) return 2;

    printf("  TABLE (%s) also failed: %s\n", altLabel, ResultStr(res));
    return 0;
}

//===========================================================================
// Apply a fixed speed
// Returns: 0 = failed, 1 = succeeded with PERCENT, 2 = succeeded with RPM
//===========================================================================
static int ApplyFixedMode(ctl_fan_handle_t hFan, const FanProfile& profile, bool dryRun) {
    ctl_fan_speed_t speed = {};
    speed.Size = sizeof(speed);
    speed.Version = 0;
    speed.speed = profile.fixedSpeed;

    ctl_fan_speed_units_t primaryUnit = (profile.units == FanProfile::RPM)
        ? CTL_FAN_SPEED_UNITS_RPM : CTL_FAN_SPEED_UNITS_PERCENT;
    speed.units = primaryUnit;

    if (dryRun) {
        printf("  [DRY-RUN] Would set FIXED speed: %d %s\n",
               profile.fixedSpeed,
               profile.units == FanProfile::RPM ? "RPM" : "%");
        return 1;
    }

    ctl_result_t res = ctlFanSetFixedSpeedMode(hFan, &speed);
    if (res == CTL_RESULT_SUCCESS) return 1;

    printf("  FIXED (%s): %s\n",
           profile.units == FanProfile::RPM ? "RPM" : "PERCENT", ResultStr(res));

    // Try other unit
    speed.units = (primaryUnit == CTL_FAN_SPEED_UNITS_RPM)
        ? CTL_FAN_SPEED_UNITS_PERCENT : CTL_FAN_SPEED_UNITS_RPM;
    if (speed.units == CTL_FAN_SPEED_UNITS_RPM)
        speed.speed = (profile.fixedSpeed * 6000) / 100;
    else
        speed.speed = (profile.fixedSpeed > 6000) ? 100 : (profile.fixedSpeed * 100) / 6000;

    const char* altLabel = (speed.units == CTL_FAN_SPEED_UNITS_RPM) ? "RPM" : "PERCENT";
    printf("  Trying FIXED (%s)...\n", altLabel);
    res = ctlFanSetFixedSpeedMode(hFan, &speed);
    if (res == CTL_RESULT_SUCCESS) return 2;

    printf("  FIXED (%s) also failed: %s\n", altLabel, ResultStr(res));
    return 0;
}

//===========================================================================
// Public API
//===========================================================================
bool ApplyProfile(ctl_fan_handle_t hFan, const FanProfile& profile, bool dryRun) {
    if (profile.mode == FanProfile::DEFAULT) {
        if (dryRun) {
            printf("  [DRY-RUN] Would reset fan to hardware defaults.\n");
            return true;
        }
        return ResetFanToDefault(hFan, false);
    }

    if (profile.mode == FanProfile::TABLE) {
        // Try TABLE first. If it fails, fall back to FIXED at 100%.
        int result = ApplyTableMode(hFan, profile, dryRun);
        if (result > 0) {
            printf("  OK: Fan curve applied (%s units).\n",
                   result == 1 ? "PERCENT" : "RPM");
            return true;
        }
        // Fallback: FIXED at max
        printf("  TABLE mode failed; falling back to FIXED at max speed...\n");
        FanProfile fixedProfile = profile;
        fixedProfile.mode = FanProfile::FIXED;
        fixedProfile.fixedSpeed = 100;
        fixedProfile.units = FanProfile::PERCENT;
        int fr = ApplyFixedMode(hFan, fixedProfile, dryRun);
        if (fr > 0) {
            printf("  OK: FIXED max speed applied (%s units).\n",
                   fr == 1 ? "PERCENT" : "RPM");
            return true;
        }
        return false;
    }

    if (profile.mode == FanProfile::FIXED) {
        int result = ApplyFixedMode(hFan, profile, dryRun);
        if (result > 0) {
            printf("  OK: Fixed speed applied (%s units).\n",
                   result == 1 ? "PERCENT" : "RPM");
            return true;
        }
        return false;
    }

    return false;
}

bool ResetFanToDefault(ctl_fan_handle_t hFan, bool dryRun) {
    if (dryRun) {
        printf("  [DRY-RUN] Would reset fan to hardware defaults.\n");
        return true;
    }
    ctl_result_t res = ctlFanSetDefaultMode(hFan);
    if (res != CTL_RESULT_SUCCESS) {
        printf("  Reset failed: %s\n", ResultStr(res));
        return false;
    }
    printf("  OK: Fan reset to hardware defaults.\n");
    return true;
}
