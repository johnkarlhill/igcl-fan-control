# IGCL Fan Control

INI-driven fan curve management for Intel Arc GPUs. Uses the [Intel Graphics
Control Library (IGCL)](https://github.com/intel/drivers.gpu.control-library)
to set custom temperature/speed curves on GPU fans — including secondary GPUs
where Intel Graphics Software does not expose tuning options.

## Requirements

- **Windows 10/11** (64-bit)
- **Intel Arc graphics driver** (includes the IGCL `ControlLib.dll`)
- **Administrator privileges** for fan control operations

## Quick Start

1. Download the [latest release](https://github.com/your-org/igcl-fan-control/releases)
   or [build from source](#building).

2. Edit `igcl-fan-control.ini` to configure your fan curves and GPU assignments:
   ```ini
   [gpu.primary]
   match = B70
   profile = aggressive

   [gpu.secondary]
   match = B50
   profile = max_cooling
   ```

3. **Run as Administrator** (required for fan control):
   ```
   igcl-fan-control.exe
   ```

4. Settings are **volatile** — re-apply at boot via [Scheduled Task](docs/SCHEDULED-TASK.md).

## Usage

```
igcl-fan-control.exe [options]

  --config <path>  Path to INI file (default: igcl-fan-control.ini)
  --dry-run        Preview what would be applied without making changes
  --validate       Validate INI syntax only
  --info           Show all Intel GPUs and fan capabilities
  --reset          Reset ALL fans to hardware defaults
  --help, -h       Show this help
```

## INI Reference

### `[global]` section

| Key | Description |
|-----|-------------|
| `default_profile` | Profile name applied to GPUs without a specific `[gpu.*]` entry |

### `[profile.<name>]` sections

| Key | Values | Description |
|-----|--------|-------------|
| `mode` | `table`, `fixed`, `default` | Fan control mode |
| `units` | `percent`, `rpm` | Speed units for the profile |
| `points` | `TEMP:SPEED, TEMP:SPEED, ...` | Temperature/speed pairs (table mode only, max 32 points, must be sorted ascending by temp) |
| `speed` | `0-100` (percent) or RPM value | Fixed fan speed (fixed mode only) |

### `[gpu.<label>]` sections

| Key | Description |
|-----|-------------|
| `match` | Case-insensitive substring to match against GPU name (e.g. `"B50"`, `"Arc"`, `"Iris"`). Use `"*"` for catch-all. Also accepts hex PCI device ID (e.g. `"0xE212"`). |
| `profile` | Profile to apply. If omitted, the global `default_profile` is used. |

## Supported GPUs

Tested and confirmed working:

| GPU | Architecture | Notes |
|-----|-------------|-------|
| Intel Arc Pro B50 | Battlemage | Driver misreports capabilities; works with PERCENT TABLE mode |
| Intel Arc Pro B70 | Battlemage | Same quirks as B50 |

Expected to work with all Intel Arc GPUs (Alchemist, Battlemage) and recent
Intel integrated graphics (Iris Xe, Arc iGPU). The IGCL API is generic — if
`ctlEnumFans` returns a fan handle, this tool can attempt to control it.

Contributions of test results for additional GPU models are welcome.

## Building

```powershell
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The resulting binary is `build/Release/igcl-fan-control.exe`.

## Known Hardware Quirks

Battlemage GPUs (Arc Pro B50/B70 and likely others) have been observed to:

1. **Misreport supported units**: Claim RPM-only, but PERCENT works. RPM mode is often rejected.
2. **Misreport supported modes**: Claim FIXED-only, but TABLE (curve) works. FIXED mode is often rejected.
3. **Require Administrator**: `canControl` reports NO without elevation, YES with elevation.
4. **Report `maxRPM = -1`**: Unknown maximum RPM.
5. **Lose settings on reboot**: Fan curves are volatile, stored in driver memory only.

This tool works around all these quirks by always trying PERCENT first
(falling back to RPM), always trying TABLE first (falling back to FIXED), and
never trusting the capability bitfields.

## License

This project's code is licensed under the MIT License. See [LICENSE](LICENSE).

The IGCL header and wrapper in the `igcl/` directory are redistributed from
the Intel Graphics Control Library under their respective licenses. See
[NOTICE.md](NOTICE.md) for details.
