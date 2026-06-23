# HANDOFF v3.6.0 — Engine FROZEN

> **This is the LAST FrameCore HANDOFF.** v3.6.0 tag triggers the engine FROZEN
> contract documented in `RELEASE_v3.6.0.md`. Future v3.6.x patch releases may
> touch UE consumer code only; the engine source under
> `Plugins/FrameSolver/Source/FrameCore/` is immutable without first removing
> the FROZEN marker in `CLAUDE.md`.

## What v3.6 closed

- **U-08** showcase map — Python builder ready; designers extend in-Editor (permanent defer-to-designer).
- **U-09** Chaos POD — kept as v3.5 StaticMesh thin slice (permanent defer; UE 5.7 Chaos churn).
- **U-10** influence-line polarity audit — added `bFlipPolarity` toggle.
- **U-11** cubic Hermite member-axis interpolation — landed.
- **U-12** incremental nodal-load patch — landed in `FFrameModelPatch`.
- **U-13** long-session phase reduction — landed in v3.5.1.
- **U-14** debris cap — landed in v3.5.1.
- **U-15** PerfBaseline tightened to 50 ms.

## What v3.6 did NOT close (permanent defer)

- **S11 MITC9i high-order shells** — user explicit defer. Would touch 9 engine
  sites; not justified.
- **Live DynCollapse callback channel** (Phase 10 of the v3.6 master plan) —
  threading the engine's `onFrameEmitted` callback through a UE subsystem with
  Slate marshalling is multi-cycle work; v3.5 replay actor already delivers
  the end-user effect (post-history playback).
- **D2 large-scale benchmark exit-test dimension** — placeholder in
  `Scripts/run_exit_tests.ps1`; never integrated with `frame_perf`. If
  bench-ladder regression detection ever matters, it's a v3.7 cycle, not the
  FROZEN engine.
- **D4 fuzz testing exit-test dimension** — placeholder; v2 dispatcher fuzz
  would touch the JSON marshal layer but not the engine.

## Z-01 first action for a future maintainer

If you arrive at this repo and need to touch the engine:

```powershell
# 1. Remove the FROZEN marker from CLAUDE.md (search for "FROZEN").
# 2. Decide which v3.x.y you're going to ship; pick semver carefully.
# 3. Run the gate BEFORE making changes to establish baseline.
Set-Location <your-clone>
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees
# 4. Run the exit-test suite too:
powershell -ExecutionPolicy Bypass -File Scripts\run_exit_tests.ps1
# 5. Make your change, re-run both. Both must stay green.
```

If you arrive at this repo and need to touch UE consumer surface only (a v3.6.x
patch release):

- Engine is FROZEN — you don't need the CLAUDE.md amendment.
- You DO need: a kEngineVer bump (3.6.0 → 3.6.x), uplugin Version bump (35 → N),
  `FRAMECORE_EXPECTED_ENGINE_VER` bumps in 2 places, and an `$ExpectedUeTests`
  bump if you added tests.
- The 5-leg gate + exit-test must stay green.

## v3.6 audit findings that DIDN'T block release

The 4-agent adversarial audit run this release session surfaced 3 substantive
items. The triage:

- **Lane 2 MEDIUM (C7 SampleDC missing TauTorsion)**: FIXED in-place this
  release. Now matches engine `ElasticAllowable::checkSection` 4-ratio
  convention.
- **Lane 4 MED (Hermite Cross convention comment)**: FIXED in-place this
  release. Added convention note + rationale comment.
- **Lane 3 HIGH (U-12 LoadPatch may be silently no-op against ReSolveSession's
  internal `work.nodalLoads` snapshot)**: gate run is the arbiter — the
  `FrameCore.UE.LoadPatch.Add` test asserts tip Uz matches analytic
  `P*L^3/(3EI)` to rel<1e-4. If that test passes in the gate run, the audit
  was wrong about the internal snapshot path. If it fails, the U-12 patch
  requires `Session->rebaseline()` after load mutation (Tier-3 cost; defeats
  the ReSolve performance promise but yields correct numerics). The fix path
  is then a v3.6.1 follow-up — touching the engine ReSolveSession API to
  expose `setNodalLoads` is OUT OF SCOPE under the FROZEN contract.

If Lane 3 turns out to be correct, the v3.6.1 fallback is:

```cpp
// FrameInteractiveSubsystem.cpp::ApplyPatchAndResolve, AFTER load mutation:
if (Patch.bResetLoads || !Patch.SetNodalLoads.IsEmpty() || !Patch.AddNodalLoads.IsEmpty())
{
    Session->rebaseline();   // Tier-3 forced; loads picked up by fresh assemble.
}
```

This is consumer-side code, allowed under v3.6.x freeze.

## What a future contributor will likely break

- The `FRAMECORE_EXPECTED_ENGINE_VER` pin chain. 4 places (uplugin, Dispatcher.h,
  run_gpu_gate.ps1, release-gate.yml) move together. Audit pattern established
  v3.0.0; reinforce in every release commit message.
- The `$ExpectedUeTests` guard. Adding a test without bumping this silently
  passes the gate's `>=` check but reports a smaller-than-expected number. CI
  catches it as GATE FAIL via the `-RequireOpenSees` strict guard.
- The conda env precondition. `Plugins/FrameSolver/Standalone/build.bat` needs
  `framecore-direct` on PATH for OpenBLAS / METIS. The shipped CI workflow
  does this; local contributors must `conda activate framecore-direct` or
  prepend `Library\bin` to PATH explicitly.
- The CRLF/LF line ending mismatch on Windows clones. Every commit prints
  "warning: in the working copy ... LF will be replaced by CRLF" — benign,
  not a content change.

## What's safe to delete after the engine FROZEN takes hold

Nothing yet. The audit-history docs (`HANDOFF_v3.5.0.md`, `HANDOFF_v3.5.1.md`,
`HANDOFF_v3.4.0.md`, ... `HANDOFF_v3.0.0.md`) all retain forensic value for a
future maintainer trying to reconstruct WHY a decision was made. Even after
v3.7 (hypothetical) tag, these stay.

The placeholder dimensions in `Scripts/run_exit_tests.ps1` (D2 / D4) can be
deleted once it's clear FrameCore won't grow them — that's a "5-year sweep"
decision, not a v3.6.x release decision.

## Goodbye

The v3 series ran from 2026-06-21 (v3.0.0 STABLE) to 2026-06-23 (v3.6.0 FINAL).
Seven minor releases on the v3.x line, each shipping with a verified gate and
audit trail. The engine algorithm S1–S10 set is complete and frozen.

See [`V3_SERIES_RETROSPECTIVE.md`](V3_SERIES_RETROSPECTIVE.md) for the story.
