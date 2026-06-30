# Hardware Notes

## Known IGCL Driver Quirks

Battlemage GPUs (Arc Pro B50, B70, and likely B60) have several discrepancies
between what `ctlFanGetProperties` reports and what actually works:

### 1. Supported Units
- **Reported**: `supportedUnits = RPM` (only)
- **Reality**: PERCENT works. RPM is often rejected with `INVALID_ARGUMENT`.
- **Tool behavior**: Tries PERCENT first, falls back to RPM.

### 2. Supported Modes
- **Reported**: `supportedModes = FIXED` (only)
- **Reality**: TABLE (custom fan curve) works. FIXED is often rejected with `UNSUPPORTED_FEATURE`.
- **Tool behavior**: Tries TABLE first, falls back to FIXED.

### 3. Software Control Flag
- **Reported**: `canControl = false` (when not elevated)
- **Reality**: Works fine when running as Administrator.
- **Tool behavior**: Reports clear error when permissions are insufficient (exit code 5).

### 4. Max RPM
- **Reported**: `maxRPM = -1` (unknown)
- **Reality**: Arc Pro B50 SFF max is approximately 6000-6500 RPM.
- **Tool behavior**: Uses 6000 as estimate for percentage↔RPM conversion.

### 5. Persistence
- **Reality**: Fan curves are volatile — stored in the kernel driver's memory, lost on reboot.
- **Solution**: Use Windows Task Scheduler to re-apply at logon. See [SCHEDULED-TASK.md](SCHEDULED-TASK.md).

## Tested Configurations

| GPU | Driver Version | OS | TABLE (PERCENT) | TABLE (RPM) | FIXED (PERCENT) | FIXED (RPM) |
|-----|---------------|----|-----------------|-------------|-----------------|-------------|
| Arc Pro B50 | 32.0.101.x | Win 11 24H2 | ✅ Works | ❌ Rejected | ❌ Rejected | ❌ Rejected |
| Arc Pro B70 | 32.0.101.x | Win 11 24H2 | ✅ Works | ❌ Rejected | ❌ Rejected | ❌ Rejected |

## Contributing Test Results

If you test this tool on other Intel GPUs, please open an issue or PR with
your results. Include:

- GPU model and PCI device ID (visible with `igcl-fan-control.exe --info`)
- Driver version
- Which modes/units worked, which were rejected
- Any unexpected behavior
