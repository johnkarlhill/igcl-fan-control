# Third-Party Notices

## Intel Graphics Control Library (IGCL)

This project redistributes the following files from the Intel Graphics Control Library:

| File | Source | License |
|------|--------|---------|
| `igcl/igcl_api.h` | Intel IGCL SDK | Intel Proprietary |
| `igcl/cApiWrapper.cpp` | Intel IGCL SDK | MIT |

The IGCL header and wrapper are provided by Intel to enable application
development against the IGCL runtime DLL (`ControlLib.dll`), which is
distributed as part of the Intel Graphics driver package.

See https://github.com/intel/drivers.gpu.control-library for the upstream
repository.

The remainder of this project (all files outside `igcl/`) is licensed under
the MIT license found in the `LICENSE` file.
