# Release v2.2+1 — FrameCore v2.2 + LevelSim v1

**Date**: 2026-06-18 · **Branch**: `main` · **Tag**: `v2.2+1`
· **Repo**: https://github.com/rocky59487/architect-

Bundled release: this repository now ships **two independent engines**, packaged together
under a single tag. FrameCore is the structural-mechanics engine (graduation-project main
line); LevelSim is the surveying-level / high-elevation-station teaching simulator.
The two share no source — they can be built, tested, and consumed independently.

---

## FrameCore v2.2

**Engine code identical to `v2.1.0` (`4e660de`).** v2.2 is a version-alignment bump that
ships FrameCore together with LevelSim v1; no FrameCore behaviour changes.

For the FrameCore engine highlights, see:
- [`docs/PROGRESS_V21.md`](PROGRESS_V21.md) — full v2.1 release notes
- [`docs/VERIFICATION.md`](VERIFICATION.md) — capability → oracle → measured-agreement map
- [`docs/HANDOFF.md`](HANDOFF.md) — original v2.1 owner-handoff (unchanged, still authoritative
  for FrameCore)
- v2.1.0 GitHub release notes

What's verified for FrameCore (same as `v2.1.0`, repeated here so this notes file stands alone):
- S1–S10 main line: ReSolve ladder, dynamic collapse with momentum inheritance,
  P-Delta second order, tension-only members, FSD sizing, BESO topology, MITC4 shell
  upgrades (QM6 / DKQ), co-rotational large-displacement (planar / 3-D / arc-length),
  N-M interaction plastic hinges
- R-line supernodal direct lane (opt-in; LDLᵀ default + fallback): self-built BLAS3
  supernodal Cholesky with METIS + OpenBLAS; `useSupernodalPrimary` PERF-01
- R2 Neumaier-compensated iterative refinement (opt-in `irSteps`)
- AC-06 shell-buckling knockdown factor + AC-07 curved-shell mesh guard
- Shell K_σ geometric stiffness (F57), EICR shell co-rotational (F58/F59), warped quads (F61)
- MIT licence + third-party NOTICE.md

Small-fixes folded into v2.2 (audit-driven, **docs / scripts / uplugin metadata only —
zero FrameCore source change**, so byte-identity vs `v2.1.0` is preserved):
- `VERIFICATION.md` documents F60 as a numbering gap (was sole undocumented sibling to F41)
- `PROGRESS_R_supernodal.md` adds explicit `[THEORY: 外推]` label on 150k/390k reachable-edge
  numbers (was already worded as extrapolated; now matches the project's labelling discipline)
- `VERIFICATION.md` §3.9 adds `[VERIFIED: perf_sn first-hand]` tag on the supernodal speedup
  ratios (data-backed via `perf_sn.exe`)
- `Scripts/run_gate.ps1` header now documents two silent preconditions (UE not auto-rebuilt;
  conda `framecore-direct` env required for legs 1 + 4) — previously only in CLAUDE.md
- `FrameSolver.uplugin` `VersionName` bumped to `2.2.0` (was `0.1`)
- README status line updated to v2.2 + LevelSim v1; "two engines" note added at the top

---

## LevelSim v1  (new)

**Pure C++17 measurement core + PC playable single-station MVP, with multi-station
closed-loop adjustment already in the core.**

What's in:

- **Measurement engine** ([`Plugins/LevelSim/Core/LevelCore.{h,cpp}`](../Plugins/LevelSim/Core/)):
  POD/std-only public API; level-instrument tilt / bubble / line-of-sight / staff reading
  with compensator residual + collimation i-angle + optional curvature+refraction;
  three-wire stadia; two-peg `recoverIAngleTwoPeg()`; multi-station `closeLoop()` with
  by-distance or by-station Bowditch adjustment.
- **Standalone oracle gate** ([`Plugins/LevelSim/Standalone/level_gate.cpp`](../Plugins/LevelSim/Standalone/level_gate.cpp)):
  L1..L16, **115 runtime PASS asserts** (level_gate.exe self-reports). 4 rounds of
  adversarial review converged R1=27 → R2=14 → R3=5 → R4=0 findings.
- **Pixel oracle** ([`Plugins/LevelSim/Tools/verify_smoke_shots.py`](../Plugins/LevelSim/Tools/verify_smoke_shots.py)):
  recovers staff reading from the rendered PNG and compares against the core's truth;
  BM 0.04 mm / P1 0.24 mm vs ±2 mm tolerance. Closes the loop "what the player sees ==
  what `measure()` calculates" without trusting either side.
- **PC playable MVP** ([`Plugins/LevelSim/Source/LevelSimPlay/`](../Plugins/LevelSim/Source/LevelSimPlay/)):
  UE5 FSM (Overview / Leveling / Telescope / Booking / Done / RouteSummary), 3-camera rig
  (overview / bubble close-up / telescope), Canvas HUD (crosshair / stadia wires / bubble
  close-up / field-book / scoring). Telescope camera pitch is locked to the core's
  `sightTilt().losTiltRad` so on-screen readings match `measure()` predictions.
- **Multi-station closed loop (M5/M6)**: `closeLoop()` in the core, multi-station UE
  FSM in the Pawn, smoke harness covers `SmokePerfectSubmitAndAdvance` → final
  `SmokeLogClosureStatus`. Gate-verified (gate L14).
- **Glue-layer review (2026-06-10)**: Enter+Escape same-frame deadlock fix (`else-if`
  branch + `EnterPhase` clears `bTyping` outside Booking); Booking ESC feedback fix.

Small-fixes folded into v1.0 (audit-driven):
- `validate(InstrumentParams)` now rejects `refractionK ≤ 0` (was only `< 1.0`)
- `bubbleFromTilt()` guards against non-finite `rollRad`/`pitchRad`/`magRad` so a hand-
  constructed `TiltState` cannot drive `offX`/`offY` to ±∞
- `verify_smoke_shots.py` documents the closed-form derivation of the hardcoded
  `truth_m=1.5006/1.1305` from the published core constants (the resolution + screw-travel
  + collimation chain) and the 1280×720 resolution assumption used by `px_per_cm`
- `LevelSim.uplugin` `VersionName` bumped to `1.0.0`; `IsBetaVersion=true` kept honest
  ("v1 playable MVP")
- `Plugins/LevelSim/README.md` updates: M5/M6 status reflects "core + UE FSM done"
  (was ambiguous); "115 asserts" annotated as runtime PASS count; FrameCore gate
  reference de-numbered (LevelSim is zero-coupled, defers to FrameCore's own VERIFICATION.md)

---

## Verification matrix

| Gate | Status | Note |
|------|--------|------|
| LevelSim standalone gate (`level_gate.cpp`, L1..L16, 115 asserts) | **PASS** | Ran immediately before tag; `ALL PASS (failures=0)`. Reproduce: `Plugins\LevelSim\Standalone\build.bat` |
| FrameCore standalone gate (`frametest.exe`, F1–F64) | **NOT RUN this cycle** | Needs conda `framecore-direct` env (OpenBLAS + METIS) at compile time; not available on this machine. Last green = `v2.1.0` (`4e660de`); v2.1.0 → main = zero FrameCore source/docs/scripts changes, sole additions are LevelSim plugin + docs. Reproduce: `conda activate framecore-direct && Plugins\FrameSolver\Standalone\build.bat` |
| FrameCore five-leg gate (`run_gate.ps1 -RequireOpenSees`) | **NOT RUN this cycle** | Needs conda env + UE 5.7 build chain + OpenSeesPy. Last green = `v2.1.0`. Reproduce: `conda activate framecore-direct && powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees` |
| LevelSim smoke + pixel oracle (`run_smoke.bat` + `verify_smoke_shots.py`) | **NOT RUN this cycle** | Needs UE 5.7 editor build + Python PIL. Reproduce: rebuild `ArchSimEditor` target → `Plugins\LevelSim\run_smoke.bat` → `python Plugins\LevelSim\Tools\verify_smoke_shots.py` |
| UE automation (57 FrameCore tests + LevelSim UE coverage) | **NOT RUN this cycle** | Needs UE editor rebuild. `run_gate.ps1`'s `$ExpectedUeTests=57` guard catches a silently-missing test on next run |

The two run gates skipped this cycle have a transparent reason and a clear reproduction
command — pre-tag the integrator did NOT mark them PASS without running them. v2.1.0
already passed both legs; FrameCore is byte-identical to v2.1.0 in this release.

---

## Honest limitations

**LevelSim v1:**
- PC single-station vertical slice is the primary playable flow; multi-station core +
  FSM exists and is gate-verified, but the UI polish for "considered the main game" stays
  v1.x work
- HUD is English-only (UE engine font lacks CJK glyphs; the design doc is Chinese)
- Telescope mask is a square window (not round) — cosmetic only
- Coarse-leveling via tripod legs not implemented (footscrew fine-leveling only)
- Only the middle hair is scored; upper/lower stadia wires are HUD-rendered but
  not graded
- Player elevation arithmetic at station k feeds station k+1's basis (real-world
  surveying behaviour; the closure score is still computed against truth values so a
  per-station mistake doesn't snowball into the final closure judgement)

**FrameCore v2.2:** inherits all `v2.1.0` honest scope — see
[`docs/VERIFICATION.md`](VERIFICATION.md) §5 and the README's "Scope boundaries" section.
Highlights: D/C is an elastic screen (not RC ultimate); MITC4 is a flat 4-node facet
(curved shells converge under refinement); dynamics / buckling / response spectrum are
linear; the collapse driver is LSP-grade sequential linear (±30 % literature envelope);
plastic hinges are event-to-event with no unloading; no fibre sections / pushover by design.

---

## Breaking changes

**None.** All v2.2 small-fixes are additive (LevelSim Core) or comment-only / metadata
(everything else):
- LevelSim `validate(InstrumentParams)` rejecting `refractionK ≤ 0`: default is `+0.13`;
  any hand-crafted setup using a non-positive k was already producing physically inverted
  corrections — re-running the LevelSim gate after the change: still 115/115 PASS
- LevelSim `bubbleFromTilt()` non-finite roll/pitch guard: `tiltFromScrews()` (the
  canonical producer) already guards against non-finite; new branch only fires for
  hand-constructed `TiltState`s — gate still 115/115 PASS after the change
- uplugin `VersionName` bumps: read by tooling that hasn't been consuming the `0.1` value
  in any verified workflow

LevelSim's public C++ API is added in v1, not changed (this is its first release).
**FrameCore's source code is byte-identical to `v2.1.0`** — `git diff v2.1.0..HEAD --
Plugins/FrameSolver/Source/` is empty; only `FrameSolver.uplugin` (metadata) and
docs/scripts changed.

---

## Deferred items (next cycle)

1. **Deep audit "104 vs 109" reconciliation** — `linear_deep_audit.cpp` has 109 static
   `addRow(` calls, but README / VERIFICATION / PROGRESS_V21 all quote 104. Likely
   conditional `addRow` (the audit reports its own runtime check count via "checks=N");
   the integrator on this cycle could not run `linear_deep_audit.exe` (no conda env on
   hand). Reconcile in v2.2+2 by running the exe and updating all three documents.
2. **FrameCore `mat.rho < 0` validate guard** (audit A-06) — small additive guard
   ("negative density would silently invert self-weight"); intentionally NOT folded into
   v2.2 because this cycle could not run FrameCore's five-leg gate on this machine
   (conda `framecore-direct` env missing). Land in v2.2+2 after a fresh five-leg green run.
3. **LevelSim multi-station player-elev propagation** — `CurrentKnownElevM = LegRecords
   .Last().PlayerElev` at station k+1 propagates player arithmetic errors. Behaviour
   reflects real surveying (each station starts from the previous station's player-
   computed elevation; closure score is then computed against truth values, so the
   final misclosure judgement is not contaminated). Decision: keep current behaviour,
   document explicitly in `Plugins/LevelSim/README.md` "誠實邊界" section. (Done in this
   release notes file; the README update can wait for v2.2+2 if the wording needs more
   pedagogical thought.)

---

## Tag plan

```bash
git tag -l | grep v2.2          # confirm empty (no prior v2.2*)
git tag -a v2.2+1 -m "FrameCore v2.2 + LevelSim v1"
git push origin main
git push origin v2.2+1
gh release create v2.2+1 \
    --title "FrameCore v2.2 + LevelSim v1" \
    --notes-file docs/RELEASE_v2.2+1.md
```
