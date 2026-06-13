# FrameSolver plugin

FrameSolver contains **FrameCore**, the engine-agnostic structural mechanics core used by
the ArchSim UE host project, the standalone validation gates, and the Grasshopper bridge.

## Current scope

**Linear core**

- 3-D linear-elastic direct-stiffness solver (sparse `SimplicialLDLT`, factorization-driven
  mechanism detection).
- Euler–Bernoulli beam-columns with optional Timoshenko shear flexibility and per-DOF end
  releases (static condensation of stiffness *and* fixed-end forces).
- MITC4 Reissner–Mindlin flat-shell facets (membrane + plate bending + drilling); opt-in
  QM6 incompatible membrane and DKQ thin-plate variants (S8).
- Elastic allowable-stress D/C screening; legacy grillage plate idealization.

**Analysis suite** — load combinations/envelopes, self-weight, factorize-once interactive
re-solves (`PreparedSystem`), prescribed settlement, influence lines, modal analysis
(dense + opt-in sparse), linear buckling, response spectrum (SRSS/CQC), modal-superposition
transients.

**Collapse line** — element removal, safety margins (`pivotMargin`), debris connectivity
(`FragmentCluster` for the physics-engine handoff), LSP progressive-collapse driver,
shell von Mises failure screen, event-to-event plastic hinges with opt-in N–M interaction
(S10), continuous dynamic collapse (modal-space Newmark with momentum-preserving debris
handoff, S2).

**Nonlinear / reanalysis line** — three-tier incremental reanalysis ladder
(Woodbury / stale-factorization PCG / rebaseline, S1), P-Delta second order (S3),
tension-only members (S4), co-rotational large displacement in 2-D and general 3-D with
arc-length snap-through continuation (S9/S9b/S9c).

**Optimization & ecosystem** — fully-stressed-design member sizing (S5), BESO topology
optimization with an optional collapse-robustness constraint (S7), and a Grasshopper /
external-client bridge (text CLI, daemon mode, and a C API DLL, S6).

Out of scope (deliberate): RC fiber-section ultimate strength, pushover, true
elasto-plasticity with unloading, code-specific design checks. The progressive-collapse
driver is LSP-grade sequential linear analysis — see the scope-boundary section of the
repository [README](../../README.md) for the honest accuracy envelope of every capability.

## Layout

- `Source/FrameCore/`: pure C++17 core. Public headers are POD/std-only; Eigen stays in
  private implementation files behind the single include site `Private/FrameEigen.h`.
- `Source/FrameCore/Private/Tests/`: UE automation tests for `FrameCore.*` (50 tests,
  UE-side oracle mirrors across the same subsystems).
- `Standalone/`: console gates and CLI drivers used by the Python audits — see
  [Standalone/README.md](Standalone/README.md).
- `Grasshopper/`: the C# client (`FrameCoreClient.cs`) that drives `frame_cli.exe` /
  `frame_capi.dll` from Grasshopper.

## Verification

Fast standalone gate (seconds):

```bat
Standalone\build.bat
```

Full five-leg gate from the repository root (standalone fixtures + UE headless automation
+ OpenSees cross-validation + the 104-check deep audit + the CLI round-trip):

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees
```

Use `-RequireOpenSees` in CI so a missing OpenSeesPy install cannot silently skip the
external comparison. The capability → oracle → gate-fixture evidence map lives in
[docs/VERIFICATION.md](../../docs/VERIFICATION.md).
