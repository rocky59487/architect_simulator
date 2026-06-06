# FrameSolver — UE5.7 beam-column structural mechanics core

Core physics engine for the architect-simulator graduation project. **Milestone 1**:
a linear-elastic 3D Euler–Bernoulli direct-stiffness solver, validated standalone
against closed-form analytic solutions.

## Layout
- `Source/FrameCore/` — engine-agnostic pure C++ core (Eigen + std only).
  - `Public/FrameCore/*.h` — Eigen-free public API (POD data model, `solve()`, `checkSection`).
  - `Private/*.cpp` + `Private/FrameEigen.h` — Eigen confined here (dual-build include shim).
  - `Private/Tests/*.cpp` — UE automation mirror (authored, compiled only with a host project).
- `Standalone/` — console **gate** (the milestone-1 success criterion). No UE build needed.
- `FrameSolver.uplugin` + `Source/FrameCore/FrameCore.Build.cs` — UE plugin descriptor +
  module rules (Eigen via `AddEngineThirdPartyPrivateStaticDependencies`, `FRAMECORE_UE=1`).
  Drop the whole `FrameSolver/` folder into `<YourProject>/Plugins/` when a `.uproject` exists.

## Build & run the gate
```bat
Standalone\build.bat
```
Expected: `ALL PASS  (failures=0)`. Fixtures: cantilever (δ=PL³/3EI, M=PL),
simply-supported UDL (δ=5wL⁴/384EI, M=wL²/8), mechanism detection, vertical column
(axial sign / refVec degeneracy).

## Design notes
- Two layers: real-time **elastic** screen (`checkSection`, this milestone) and a future
  **fiber-section nonlinear** precision layer — kept apart by `ISectionStrength`.
- `checkSection` is an **elastic / allowable-stress** screen, **not** RC ultimate strength.
- `StaticCondensation` (Schur) is a deferred **static-only** optimization (dynamics → CMS).
- Full math + provenance: `../PFSFv2-to-UE5-transferable-math.md` (see §0 "三個適用邊界").

## Excluded from milestone 1 (later)
Nonlinear (P-Δ / corotational), fiber sections / RC ultimate, modal / dynamics,
Schur substructuring impl, plate/wall equivalent folding, truss-release activation,
GPU, host `.uproject` + in-editor integration.
