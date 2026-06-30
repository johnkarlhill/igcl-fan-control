//===========================================================================
// IGCL Fan Control — Lightweight INI parser
//
// Uses GetPrivateProfileStringW (kernel32) for section/key reading.
// Custom tokenizer for the "points = temp:speed, temp:speed, ..." syntax.
//===========================================================================
#include "ini_reader.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <map>
#include <algorithm>

//===========================================================================
// Helpers
//===========================================================================
static std::string WstrToUtf8(const wchar_t* wstr) {
    if (!wstr || !*wstr) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], len, nullptr, nullptr);
    return result;
}

static std::wstring Utf8ToWstr(const std::string& str) {
    if (str.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring result(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], len);
    return result;
}

static std::string ReadIniString(const std::wstring& path, const wchar_t* section,
                                  const wchar_t* key, const wchar_t* defaultVal = L"") {
    wchar_t buf[4096] = {};
    GetPrivateProfileStringW(section, key, defaultVal, buf, 4096, path.c_str());
    return WstrToUtf8(buf);
}

static void EnumerateSection(const std::wstring& path, const wchar_t* section,
                              std::vector<std::string>& outKeys) {
    wchar_t buf[16384] = {};
    GetPrivateProfileStringW(section, nullptr, L"", buf, 16384, path.c_str());
    const wchar_t* p = buf;
    while (*p) {
        std::string key = WstrToUtf8(p);
        if (!key.empty()) outKeys.push_back(key);
        p += wcslen(p) + 1;
    }
}

// Trim whitespace from both ends
static void Trim(std::string& s) {
    while (!s.empty() && (unsigned char)s.front() <= ' ') s.erase(s.begin());
    while (!s.empty() && (unsigned char)s.back() <= ' ') s.pop_back();
}

// Remove inline comment from a line (anything after # or ;)
static void StripComment(std::string& s) {
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '#' || s[i] == ';') {
            s.resize(i);
            break;
        }
    }
}

//===========================================================================
// Parse "temp:speed, temp:speed, ..." point list
//===========================================================================
static bool ParsePoints(const std::string& raw, std::vector<FanProfile::Point>& out,
                         int& errorLine, std::string& errorMsg) {
    out.clear();
    std::string s = raw;
    StripComment(s);

    // Split on commas
    size_t pos = 0;
    while (pos < s.size()) {
        // Find next comma
        size_t comma = s.find(',', pos);
        std::string pair = s.substr(pos, comma - pos);
        Trim(pair);
        pos = (comma == std::string::npos) ? s.size() : comma + 1;
        if (pair.empty()) continue;

        // Split on colon
        size_t colon = pair.find(':');
        if (colon == std::string::npos) {
            errorMsg = "expected 'temp:speed' but got '" + pair + "'";
            return false;
        }

        std::string tempStr = pair.substr(0, colon);
        std::string speedStr = pair.substr(colon + 1);
        Trim(tempStr);
        Trim(speedStr);

        char* end = nullptr;
        long temp = strtol(tempStr.c_str(), &end, 10);
        if (*end != '\0' || temp < 0 || temp > 125) {
            errorMsg = "invalid temperature '" + tempStr + "' (expected 0-125 C)";
            return false;
        }
        long speed = strtol(speedStr.c_str(), &end, 10);
        if (*end != '\0' || speed < 0 || speed > 20000) {
            errorMsg = "invalid speed '" + speedStr + "' (expected 0-20000)";
            return false;
        }

        out.push_back({(uint32_t)temp, (int32_t)speed});
    }

    // Validate ascending temperatures
    for (size_t i = 1; i < out.size(); i++) {
        if (out[i].temp <= out[i-1].temp) {
            errorMsg = "temperature " + std::to_string(out[i].temp) +
                       " is not strictly above previous " + std::to_string(out[i-1].temp) +
                       " (must be ascending)";
            return false;
        }
    }

    return true;
}

//===========================================================================
// Parse a single profile section
//===========================================================================
static bool ParseProfile(const std::wstring& path, const std::string& name,
                          FanProfile& out, int& errorLine, std::string& errorMsg) {
    out.name = name;
    std::wstring section = L"profile." + Utf8ToWstr(name);

    std::string mode = ReadIniString(path, section.c_str(), L"mode");
    Trim(mode);

    if (mode.empty()) {
        errorMsg = "missing 'mode' key";
        return false;
    }

    if (mode == "table") {
        out.mode = FanProfile::TABLE;
    } else if (mode == "fixed") {
        out.mode = FanProfile::FIXED;
    } else if (mode == "default") {
        out.mode = FanProfile::DEFAULT;
        return true;  // no further parsing needed
    } else {
        errorMsg = "unknown mode '" + mode + "' (expected table, fixed, or default)";
        return false;
    }

    // Units
    std::string units = ReadIniString(path, section.c_str(), L"units");
    Trim(units);
    if (units.empty()) units = "percent";

    if (units == "percent") {
        out.units = FanProfile::PERCENT;
    } else if (units == "rpm") {
        out.units = FanProfile::RPM;
    } else {
        errorMsg = "unknown units '" + units + "' (expected percent or rpm)";
        return false;
    }

    if (out.mode == FanProfile::TABLE) {
        std::string pointsRaw = ReadIniString(path, section.c_str(), L"points");
        Trim(pointsRaw);
        if (pointsRaw.empty()) {
            errorMsg = "mode=table requires 'points' key (e.g. points = 30:35, 50:75, 70:100)";
            return false;
        }
        if (!ParsePoints(pointsRaw, out.points, errorLine, errorMsg))
            return false;

        // Validate point speeds against units
        int maxSpeed = (out.units == FanProfile::RPM) ? 20000 : 100;
        for (auto& pt : out.points) {
            if (pt.speed > maxSpeed) {
                errorMsg = "speed " + std::to_string(pt.speed) + " exceeds max " +
                           std::to_string(maxSpeed) + " for " +
                           (out.units == FanProfile::RPM ? "RPM" : "percent") + " units";
                return false;
            }
        }
    } else if (out.mode == FanProfile::FIXED) {
        std::string speedStr = ReadIniString(path, section.c_str(), L"speed");
        Trim(speedStr);
        if (speedStr.empty()) {
            errorMsg = "mode=fixed requires 'speed' key (e.g. speed = 80)";
            return false;
        }
        char* end = nullptr;
        long speed = strtol(speedStr.c_str(), &end, 10);
        int maxSpeed = (out.units == FanProfile::RPM) ? 20000 : 100;
        if (*end != '\0' || speed < 0 || speed > maxSpeed) {
            errorMsg = "invalid speed '" + speedStr + "' (expected 0-" + std::to_string(maxSpeed) + " for " +
                       (out.units == FanProfile::RPM ? "RPM" : "percent") + " units)";
            return false;
        }
        out.fixedSpeed = (int32_t)speed;
    }

    return true;
}

//===========================================================================
// Public: Parse entire INI file
//===========================================================================
IniResult ParseIniFile(const std::string& filePath, IniConfig& out) {
    out = IniConfig{};

    // Resolve to absolute path for GetPrivateProfileStringW
    wchar_t fullPath[MAX_PATH] = {};
    GetFullPathNameW(Utf8ToWstr(filePath).c_str(), MAX_PATH, fullPath, nullptr);

    std::wstring wpath = fullPath;

    // Check file exists
    if (GetFileAttributesW(wpath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        IniResult r;
        r.ok = false;
        r.error = "file not found: " + filePath;
        return r;
    }

    // --- [global] section ---
    out.defaultProfile = ReadIniString(wpath, L"global", L"default_profile");
    Trim(out.defaultProfile);

    // --- Enumerate profile.* sections ---
    // We need to list all section names. GetPrivateProfileString with nullptr key
    // returns all keys in the section, but we need all *section names*.
    // Strategy: use a small workaround — enumerate keys in a sentinel section,
    // or use GetPrivateProfileSectionNames.
    wchar_t sectionBuf[32768] = {};
    GetPrivateProfileSectionNamesW(sectionBuf, 32768, wpath.c_str());

    const wchar_t* p = sectionBuf;
    while (*p) {
        std::string sectionName = WstrToUtf8(p);
        p += wcslen(p) + 1;

        // Check for profile.* sections
        if (sectionName.size() > 8 && sectionName.substr(0, 8) == "profile.") {
            std::string profileName = sectionName.substr(8);
            FanProfile fp;
            int errorLine = 0;
            std::string errorMsg;
            if (!ParseProfile(wpath, profileName, fp, errorLine, errorMsg)) {
                IniResult r;
                r.ok = false;
                r.error = "[" + sectionName + "] " + errorMsg;
                return r;
            }
            out.profiles[profileName] = fp;
        }

        // Check for gpu.* sections
        if (sectionName.size() > 4 && sectionName.substr(0, 4) == "gpu.") {
            std::string gpuLabel = sectionName.substr(4);
            GpuEntry entry;
            entry.label = gpuLabel;

            std::wstring wsection = p - (wcslen(Utf8ToWstr(sectionName).c_str()) + 1);
            // Actually, just reconstruct the section name:
            wsection = L"gpu." + Utf8ToWstr(gpuLabel);

            entry.match = ReadIniString(wpath, wsection.c_str(), L"match");
            Trim(entry.match);

            entry.profile = ReadIniString(wpath, wsection.c_str(), L"profile");
            Trim(entry.profile);

            if (entry.match.empty()) {
                IniResult r;
                r.ok = false;
                r.error = "[" + sectionName + "] missing 'match' key";
                return r;
            }

            out.gpuEntries.push_back(entry);
        }
    }

    // If no gpu sections, create a catch-all
    if (out.gpuEntries.empty()) {
        GpuEntry catchAll;
        catchAll.label = "all";
        catchAll.match = "*";
        catchAll.profile = out.defaultProfile;
        out.gpuEntries.push_back(catchAll);
    }

    IniResult r;
    r.ok = true;
    return r;
}
