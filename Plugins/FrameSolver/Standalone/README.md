# FrameCore standalone tools

This directory builds the UE-free FrameCore executables used for validation and for
driving the engine from external clients.

## Tools

| Script | Output | Purpose |
|---|---|---|
| `build.bat` | `frametest.exe` | the standalone analytic/benchmark gate (fixtures F1–F54): beam-column closed forms, releases, Timoshenko, grillage, MITC4 shell plate/patch tests, Scordelis-Lo roof, pinched cylinder, the linear-analysis suite, collapse, reanalysis, P-Delta, tension-only, FSD, BESO, co-rotational, N–M hinges |
| `build_cli.bat` | `frame_cli.exe` | stdin/stdout solver driver (wire protocol in [docs/CLI_PROTOCOL.md](../../../docs/CLI_PROTOCOL.md)) used by the Python audits, OpenSees comparisons, and the Grasshopper text bridge; supports single-shot and daemon (multi-block `EOR`) modes |
| `build_capi.bat` | `frame_capi.dll` | C API DLL sharing the same protocol core (`frame_cli_core.{h,cpp}`) for in-process clients |
| `build_linear_audit.bat` | `linear_deep_audit.exe` | independent deep audit (104 checks): sympy/numpy-derived references, element-spectrum oracles, bit-identity no-op checks |
| `build_perf.bat` | `frame_perf.exe` | in-process performance benchmark (the anchor for [docs/PERFORMANCE_BASELINE.md](../../../docs/PERFORMANCE_BASELINE.md)) |

## Quick start

From the repository root:

```bat
Plugins\FrameSolver\Standalone\build.bat
```

Expected result:

```text
ALL PASS  (failures=0)
```

## Notes

- The scripts locate Visual Studio through `vswhere` (including prerelease installs) and
  compile against the UE-bundled Eigen headers (`EIGEN_MPL2_ONLY`). The public FrameCore
  API remains UE-free; only the build scripts know the local UE install path.
- The build scripts carry an **explicit source-file list**: a new `Private/*.cpp` must be
  added to `build.bat`, `build_cli.bat` *and* `build_linear_audit.bat` (header-only changes
  need nothing).
- Do not run multiple build scripts concurrently — they share `obj*` directories.
