//===========================================================================
// IGCL Fan Control — GPU discovery and matching
//===========================================================================
#pragma once
#include "igcl_api.h"
#include <string>
#include <vector>

struct GpuInfo {
    ctl_device_adapter_handle_t handle;
    std::string name;
    uint32_t vendorId;
    uint32_t deviceId;
    uint32_t subSysId;
    uint32_t subSysVendorId;
    uint32_t revId;
    bool isIntel;
    int  fanCount;
    bool fanControlAvailable;  // best-effort: canControl && (TABLE or FIXED or maxPoints>0)
};

std::vector<GpuInfo> EnumerateGpus(ctl_api_handle_t hAPI);
void ShowFanInfo(ctl_fan_handle_t hFan, int fanIdx);
void ShowGpuDetails(const GpuInfo& gpu, int idx);

// Returns the index of the first GPU whose name contains 'match' (case-insensitive).
// If match is "*" or empty, returns 0 (first GPU).
int  FindMatchingGpu(const std::vector<GpuInfo>& gpus, const std::string& match);
