//===========================================================================
// IGCL Fan Control — INI data structures
//===========================================================================
#pragma once
#include <string>
#include <vector>
#include <map>

struct FanProfile {
    enum Mode   { DEFAULT, FIXED, TABLE };
    enum Units  { PERCENT, RPM };

    struct Point {
        uint32_t temp;     // Celsius
        int32_t  speed;    // % (0-100) or RPM depending on units
    };

    std::string name;
    Mode        mode  = DEFAULT;
    Units       units = PERCENT;
    std::vector<Point> points;     // for TABLE mode
    int32_t     fixedSpeed = 100;  // for FIXED mode
};

struct GpuEntry {
    std::string label;    // arbitrary display name (from [gpu.<label>])
    std::string match;    // substring to match against GPU name
    std::string profile;  // profile name to apply (empty = use global default)
};

struct IniConfig {
    std::string defaultProfile;
    std::map<std::string, FanProfile> profiles;
    std::vector<GpuEntry> gpuEntries;
};

struct IniResult {
    bool ok = false;
    std::string error;  // error message if !ok
};

// Parse an INI file. On success, IniResult::ok is true.
IniResult ParseIniFile(const std::string& filePath, IniConfig& out);
