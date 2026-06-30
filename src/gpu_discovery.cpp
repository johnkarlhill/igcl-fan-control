//===========================================================================
// IGCL Fan Control — GPU discovery and fan info display
//===========================================================================
#include "gpu_discovery.h"
#include "result_utils.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>

// Forward declarations from fan_control.cpp
extern void ShowFanConfig(ctl_fan_handle_t hFan);
extern void ShowFanState(ctl_fan_handle_t hFan);

//===========================================================================
// Show fan capabilities for --info mode
//===========================================================================
void ShowFanInfo(ctl_fan_handle_t hFan, int fanIdx) {
    ctl_fan_properties_t props = {};
    props.Size = sizeof(props);
    ctl_result_t res = ctlFanGetProperties(hFan, &props);
    if (res != CTL_RESULT_SUCCESS) {
        printf("  FAN[%d]: ctlFanGetProperties failed: %s\n", fanIdx, ResultStr(res));
        return;
    }

    printf("\n  --- Fan %d ---\n", fanIdx);
    printf("  Software control:   %s\n", props.canControl ? "YES" : "NO");
    printf("  Max RPM:            %d\n", props.maxRPM);
    printf("  Max curve points:   %d\n", props.maxPoints);

    printf("  Supported modes:   ");
    if (props.supportedModes & (1 << CTL_FAN_SPEED_MODE_DEFAULT)) printf(" DEFAULT");
    if (props.supportedModes & (1 << CTL_FAN_SPEED_MODE_FIXED))   printf(" FIXED");
    if (props.supportedModes & (1 << CTL_FAN_SPEED_MODE_TABLE))   printf(" TABLE(curve)");
    if (props.supportedModes == 0) printf(" (none reported)");
    printf("\n");

    printf("  Supported units:   ");
    if (props.supportedUnits & (1 << CTL_FAN_SPEED_UNITS_RPM))     printf(" RPM");
    if (props.supportedUnits & (1 << CTL_FAN_SPEED_UNITS_PERCENT)) printf(" PERCENT");
    if (props.supportedUnits == 0) printf(" (none reported)");
    printf("\n");

    printf("  NOTE: Battlemage GPUs are known to misreport supportedModes and\n");
    printf("        supportedUnits. The tool attempts all modes/units regardless.\n");

    ShowFanConfig(hFan);
    ShowFanState(hFan);
}

//===========================================================================
// Show detailed info for a single GPU
//===========================================================================
void ShowGpuDetails(const GpuInfo& gpu, int idx) {
    PrintSeparator();
    printf("  GPU[%d]: %s\n", idx, gpu.name.c_str());
    printf("  Vendor=0x%04X  Device=0x%04X  Rev=0x%02X\n",
           gpu.vendorId, gpu.deviceId, gpu.revId);
    printf("  Subsys=0x%04X  SubsysVendor=0x%04X\n",
           gpu.subSysId, gpu.subSysVendorId);

    uint32_t fanCount = 0;
    ctl_result_t res = ctlEnumFans(gpu.handle, &fanCount, nullptr);
    if (res != CTL_RESULT_SUCCESS || fanCount == 0) {
        printf("  No fans found on this GPU.\n");
        return;
    }

    printf("  Number of fans: %u\n", fanCount);

    ctl_fan_handle_t* fans = new ctl_fan_handle_t[fanCount];
    res = ctlEnumFans(gpu.handle, &fanCount, fans);
    if (res != CTL_RESULT_SUCCESS) {
        printf("  Failed to enumerate fans: %s\n", ResultStr(res));
        delete[] fans;
        return;
    }

    for (uint32_t f = 0; f < fanCount; f++) {
        ShowFanInfo(fans[f], f);
    }

    delete[] fans;
}

//===========================================================================
// Enumerate all Intel GPUs
//===========================================================================
std::vector<GpuInfo> EnumerateGpus(ctl_api_handle_t hAPI) {
    std::vector<GpuInfo> result;

    uint32_t adapterCount = 0;
    ctl_result_t res = ctlEnumerateDevices(hAPI, &adapterCount, nullptr);
    if (res != CTL_RESULT_SUCCESS) {
        printf("ERROR: ctlEnumerateDevices count failed: %s\n", ResultStr(res));
        return result;
    }

    printf("Found %u Intel adapter(s)\n\n", adapterCount);

    ctl_device_adapter_handle_t* handles =
        (ctl_device_adapter_handle_t*)malloc(sizeof(ctl_device_adapter_handle_t) * adapterCount);
    if (!handles) return result;

    res = ctlEnumerateDevices(hAPI, &adapterCount, handles);
    if (res != CTL_RESULT_SUCCESS) {
        printf("ERROR: ctlEnumerateDevices failed: %s\n", ResultStr(res));
        free(handles);
        return result;
    }

    for (uint32_t i = 0; i < adapterCount; i++) {
        GpuInfo info = {};
        info.handle = handles[i];

        ctl_device_adapter_properties_t props = {};
        props.Size = sizeof(props);
        props.Version = 2;
        props.pDeviceID = malloc(sizeof(LUID));
        props.device_id_size = sizeof(LUID);

        res = ctlGetDeviceProperties(handles[i], &props);
        if (res != CTL_RESULT_SUCCESS) {
            printf("GPU[%u]: ctlGetDeviceProperties failed: %s\n", i, ResultStr(res));
            free(props.pDeviceID);
            continue;
        }

        info.name = props.name;
        info.vendorId = props.pci_vendor_id;
        info.deviceId = props.pci_device_id;
        info.subSysId = props.pci_subsys_id;
        info.subSysVendorId = props.pci_subsys_vendor_id;
        info.revId = props.rev_id;
        info.isIntel = (info.vendorId == 0x8086);

        // Check fan availability
        uint32_t fanCount = 0;
        res = ctlEnumFans(handles[i], &fanCount, nullptr);
        info.fanCount = (res == CTL_RESULT_SUCCESS) ? (int)fanCount : 0;
        info.fanControlAvailable = false;

        if (fanCount > 0) {
            ctl_fan_handle_t* fans = new ctl_fan_handle_t[fanCount];
            res = ctlEnumFans(handles[i], &fanCount, fans);
            if (res == CTL_RESULT_SUCCESS) {
                for (uint32_t f = 0; f < fanCount; f++) {
                    ctl_fan_properties_t fanProps = {};
                    fanProps.Size = sizeof(fanProps);
                    if (ctlFanGetProperties(fans[f], &fanProps) == CTL_RESULT_SUCCESS) {
                        if (fanProps.canControl &&
                            ((fanProps.supportedModes & (1 << CTL_FAN_SPEED_MODE_TABLE)) ||
                             (fanProps.supportedModes & (1 << CTL_FAN_SPEED_MODE_FIXED)) ||
                             fanProps.maxPoints > 0)) {
                            info.fanControlAvailable = true;
                        }
                    }
                }
            }
            delete[] fans;
        }

        // Print discovery summary
        printf("  GPU[%u]: %s\n", i, props.name);
        printf("          Vendor=0x%04X  Device=0x%04X  Rev=0x%02X\n",
               props.pci_vendor_id, props.pci_device_id, props.rev_id);
        printf("          Subsys=0x%04X  SubsysVendor=0x%04X\n",
               props.pci_subsys_id, props.pci_subsys_vendor_id);
        printf("          Fans: %d  Fan control: %s\n",
               info.fanCount,
               info.fanControlAvailable ? "AVAILABLE" : "NOT AVAILABLE (may need Admin)");

        result.push_back(info);
        free(props.pDeviceID);
    }

    free(handles);
    return result;
}

//===========================================================================
// Match GPU by name substring (case-insensitive)
//===========================================================================
int FindMatchingGpu(const std::vector<GpuInfo>& gpus, const std::string& match) {
    if (match.empty() || match == "*") {
        return gpus.empty() ? -1 : 0;
    }

    std::string filter = match;
    for (auto& c : filter) c = tolower((unsigned char)c);

    for (size_t i = 0; i < gpus.size(); i++) {
        std::string name = gpus[i].name;
        for (auto& c : name) c = tolower((unsigned char)c);
        if (name.find(filter) != std::string::npos) {
            return (int)i;
        }
    }

    // Try matching against PCI device ID
    if (filter.size() >= 6 && filter[0] == '0' && (filter[1] == 'x' || filter[1] == 'X')) {
        char* end = nullptr;
        long deviceId = strtol(filter.c_str(), &end, 16);
        if (*end == '\0') {
            for (size_t i = 0; i < gpus.size(); i++) {
                if ((long)gpus[i].deviceId == deviceId) return (int)i;
            }
        }
    }

    return -1;
}
