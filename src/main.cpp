//===========================================================================
// IGCL Fan Control — Main entry point
//
// INI-driven Intel GPU fan control tool. Reads profiles and GPU assignments
// from an INI file, applies fan curves via the Intel Graphics Control Library.
//
// Requires Administrator privileges for fan control operations.
//===========================================================================
#include <windows.h>

#include "igcl_api.h"
#include "ini_reader.h"
#include "gpu_discovery.h"
#include "fan_control.h"
#include "result_utils.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

//===========================================================================
// CLI
//===========================================================================
struct Options {
    std::string configPath = "igcl-fan-control.ini";
    bool dryRun   = false;
    bool validate = false;
    bool showInfo = false;
    bool resetAll = false;
    bool help     = false;
};

void PrintUsage() {
    printf("IGCL Fan Control — Intel GPU fan curve management\n");
    printf("https://github.com/your-org/igcl-fan-control\n\n");
    printf("Usage: igcl-fan-control.exe [options]\n\n");
    printf("Options:\n");
    printf("  --config <path>  Path to INI file (default: igcl-fan-control.ini)\n");
    printf("  --dry-run        Show what would be applied without making changes\n");
    printf("  --validate       Validate INI file syntax and exit\n");
    printf("  --info           Show all Intel GPUs and fan capabilities\n");
    printf("  --reset          Reset ALL fans to hardware defaults\n");
    printf("  --help, -h       Show this help\n\n");
    printf("Examples:\n");
    printf("  igcl-fan-control.exe --validate\n");
    printf("  igcl-fan-control.exe --dry-run\n");
    printf("  igcl-fan-control.exe\n");
    printf("  igcl-fan-control.exe --config my-fans.ini\n");
    printf("  igcl-fan-control.exe --info\n");
    printf("  igcl-fan-control.exe --reset\n\n");
    printf("Run as Administrator to control fan speeds.\n");
}

bool ParseArgs(int argc, char* argv[], Options& opts) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            opts.help = true;
            PrintUsage();
            return true;
        } else if (arg == "--dry-run") {
            opts.dryRun = true;
        } else if (arg == "--validate") {
            opts.validate = true;
        } else if (arg == "--info") {
            opts.showInfo = true;
        } else if (arg == "--reset") {
            opts.resetAll = true;
        } else if (arg == "--config") {
            if (i + 1 < argc) {
                opts.configPath = argv[++i];
            } else {
                printf("ERROR: --config requires a path argument.\n");
                return false;
            }
        } else {
            printf("Unknown option: %s (use --help)\n", arg.c_str());
            return false;
        }
    }
    return true;
}

//===========================================================================
// Profile resolution: gpu-specific profile > global default
//===========================================================================
const FanProfile* ResolveProfile(const IniConfig& ini, const GpuEntry& entry) {
    // Use entry's profile if set, otherwise global default
    std::string profileName = entry.profile;
    if (profileName.empty()) profileName = ini.defaultProfile;

    auto it = ini.profiles.find(profileName);
    if (it == ini.profiles.end()) {
        printf("  WARNING: Profile '%s' not found.", profileName.c_str());
        if (&entry != &ini.gpuEntries[0]) {
            // Not the implicit catch-all — try default
            it = ini.profiles.find(ini.defaultProfile);
            if (it != ini.profiles.end()) {
                printf(" Falling back to default '%s'.", ini.defaultProfile.c_str());
                return &it->second;
            }
        }
        printf(" No usable profile.\n");
        return nullptr;
    }
    return &it->second;
}

//===========================================================================
// Main
//===========================================================================
int main(int argc, char* argv[]) {
    Options opts;
    if (!ParseArgs(argc, argv, opts)) return 1;
    if (opts.help) return 0;

    PrintSeparator("IGCL Fan Control");

    // --- Parse INI ---
    IniConfig ini;
    IniResult iniRes = ParseIniFile(opts.configPath, ini);

    if (!iniRes.ok) {
        printf("\nERROR parsing config file: %s\n", opts.configPath.c_str());
        printf("  %s\n", iniRes.error.c_str());
        return 2;
    }

    printf("\nConfig: %s\n", opts.configPath.c_str());
    printf("Default profile: %s\n", ini.defaultProfile.empty() ? "(none)" : ini.defaultProfile.c_str());
    printf("Profiles defined: %zu\n", ini.profiles.size());
    for (const auto& kv : ini.profiles) {
        printf("  [profile.%s]  mode=%s\n", kv.first.c_str(), ProfileModeStr(kv.second));
    }

    if (opts.validate) {
        printf("\nConfig validation: PASSED\n");
        return 0;
    }

    // --- Init IGCL ---
    printf("\nInitializing IGCL...\n");

    ctl_init_args_t initArgs = {};
    initArgs.Size = sizeof(initArgs);
    initArgs.Version = 0;
    initArgs.AppVersion = CTL_MAKE_VERSION(1, 1);
    initArgs.flags = CTL_INIT_FLAG_USE_LEVEL_ZERO | CTL_INIT_FLAG_IGSC_FUL;
    ZeroMemory(&initArgs.ApplicationUID, sizeof(ctl_application_id_t));

    ctl_api_handle_t hAPI = nullptr;
    ctl_result_t res = ctlInit(&initArgs, &hAPI);

    if (res != CTL_RESULT_SUCCESS) {
        printf("ERROR: ctlInit failed: %s (0x%X)\n", ResultStr(res), res);
        printf("\nPossible causes:\n");
        printf("  - Intel Graphics driver not installed\n");
        printf("  - IGCL DLL (ControlLib.dll) not found\n");
        printf("  - Try running as Administrator\n");
        return 4;
    }

    // --- Enumerate GPUs ---
    std::vector<GpuInfo> gpus = EnumerateGpus(hAPI);

    if (gpus.empty()) {
        printf("No Intel GPUs found.\n");
        ctlClose(hAPI);
        return 3;
    }

    if (opts.showInfo) {
        for (int i = 0; i < (int)gpus.size(); i++)
            ShowGpuDetails(gpus[i], i);
        ctlClose(hAPI);
        return 0;
    }

    // --- Reset all ---
    if (opts.resetAll) {
        PrintSeparator("Resetting All Fans");
        for (int i = 0; i < (int)gpus.size(); i++) {
            printf("\nGPU[%d] %s:\n", i, gpus[i].name.c_str());
            uint32_t fanCount = 0;
            res = ctlEnumFans(gpus[i].handle, &fanCount, nullptr);
            if (res != CTL_RESULT_SUCCESS || fanCount == 0) {
                printf("  No fans.\n");
                continue;
            }
            ctl_fan_handle_t* fans = new ctl_fan_handle_t[fanCount];
            ctlEnumFans(gpus[i].handle, &fanCount, fans);
            for (uint32_t f = 0; f < fanCount; f++)
                ResetFanToDefault(fans[f], opts.dryRun);
            delete[] fans;
        }
        PrintSeparator();
        printf("All fans reset to hardware defaults.\n");
        ctlClose(hAPI);
        return 0;
    }

    // --- Apply profiles ---
    PrintSeparator("Applying Fan Profiles");

    bool anyFailed = false;
    bool anyApplied = false;

    for (const auto& entry : ini.gpuEntries) {
        printf("\n[gpu.%s]  match=\"%s\"\n", entry.label.c_str(), entry.match.c_str());

        int gpuIdx = FindMatchingGpu(gpus, entry.match);
        if (gpuIdx < 0) {
            printf("  SKIP: No GPU matching \"%s\" found.\n", entry.match.c_str());
            continue;
        }

        const FanProfile* profile = ResolveProfile(ini, entry);
        if (!profile) {
            anyFailed = true;
            continue;
        }

        printf("  Matched GPU[%d]: %s\n", gpuIdx, gpus[gpuIdx].name.c_str());
        printf("  Profile: %s (%s)\n", profile->name.c_str(), ProfileModeStr(*profile));

        // Get fan handles
        uint32_t fanCount = 0;
        res = ctlEnumFans(gpus[gpuIdx].handle, &fanCount, nullptr);
        if (res != CTL_RESULT_SUCCESS || fanCount == 0) {
            printf("  SKIP: No fans on this GPU.\n");
            continue;
        }

        ctl_fan_handle_t* fans = new ctl_fan_handle_t[fanCount];
        ctlEnumFans(gpus[gpuIdx].handle, &fanCount, fans);

        for (uint32_t f = 0; f < fanCount; f++) {
            printf("  Fan %u: ", f);
            if (!ApplyProfile(fans[f], *profile, opts.dryRun)) {
                printf("  FAILED\n");
                anyFailed = true;
            } else {
                anyApplied = true;
            }
        }

        delete[] fans;
    }

    // --- Done ---
    ctlClose(hAPI);

    PrintSeparator();
    if (opts.dryRun) {
        printf("DRY RUN complete. No changes were made.\n");
        printf("Run without --dry-run (as Administrator) to apply settings.\n");
        return 0;
    }

    if (!anyApplied) {
        printf("WARNING: No profiles were applied. Check GPU matching and permissions.\n");
        return 5;
    }

    if (anyFailed) {
        printf("COMPLETED WITH ERRORS. Some profiles failed to apply.\n");
        return 6;
    }

    printf("All profiles applied successfully.\n");
    printf("Settings will persist until reboot or driver restart.\n");
    return 0;
}
