# RELEASE v0.1.2 — Architect Simulator game body (patch: 5-leg gate covers ArchSim namespace)

> **Tag:** `v0.1.2` · **Date:** 2026-06-25 · **Repository:** rocky59487/architect_simulator
> **Engine baseline:** FrameCore `v4.0.0` (FROZEN) + LevelSim `v1` (untouched)
> **This release adds:** AS-01 closure — `Scripts/run_gate.ps1` filter now accepts
> `ArchSim.*` namespace, so the v0.1.1 `ArchSim.Persistence.SaveLoadRoundTrip` test
> is now formally part of the 5-leg gate. **No source-code behaviour change; no
> engine touch; no docs other than this release + handoff + README.**

---

## 1. What v0.1.2 is

A micro-patch that closes the AS-01 deferred item from v0.1.1. v0.1.1 added the
first `ArchSim.*` automation oracle but the gate driver still filtered by
`FrameCore.*` only, so the new test was technically green but **not formally
under gate enforcement**. v0.1.2 fixes that: now the next time anyone breaks
the registry SaveGame contract, `run_gate.ps1 -RequireOpenSees` exits non-zero.

**Two surgical edits, one file:**

| File | Line | Change | Why |
|---|---|---|---|
| `Scripts/run_gate.ps1` | 70 | `'Automation RunTests FrameCore; Quit'` → `'Automation RunTests FrameCore+ArchSim; Quit'` | UE Automation native `+`-separated filter chain — matches tests with namespace beginning `FrameCore` OR `ArchSim` |
| `Scripts/run_gate.ps1` | 29 | `[int]$ExpectedUeTests = 135` → `[int]$ExpectedUeTests = 136`; cumulative-release comment appended with `"v0.1.1 game body: +1 ArchSim.Persistence.SaveLoadRoundTrip (total 136; non-cuDSS fallback 134)."` | Count-guard now includes the new ArchSim test |

**Source delta vs v0.1.1:**

| Path | Lines changed | Notes |
|---|---|---|
| `Plugins/FrameSolver/Source/FrameCore/` | **0** | 鐵則 #1 FROZEN honoured |
| `Plugins/FrameSolver/Source/FrameCoreUE/` | **0** | |
| `Plugins/LevelSim/` | **0** | 鐵則 #5 honoured |
| `ArchSim.uproject` / `.gitignore` | **0** | 鐵則 #5 honoured |
| `Source/ArchSim/` | **0** | Test source unchanged from v0.1.1 |
| `Scripts/run_gate.ps1` | **2 line edits** | Surgical filter + count bump; FRAMECORE_EXPECTED_ENGINE_VER='4.0.0' and the entire historical breakdown comment preserved |

**Why `Scripts/run_gate.ps1` is NOT in the FROZEN zone:** the FROZEN marker on
鐵則 #1 applies specifically to `Plugins/FrameSolver/Source/FrameCore/`. The gate
driver script lives at repo root and has always evolved per release (the
`$ExpectedUeTests` count has grown 57 → 72 → 96 → 120 → 135 → 136 across the
v2.x and v3.x series). Modifying it for an additive test does not violate the
engine-source freeze.

---

## 2. Verification matrix

| Leg | Command | Result | Notes |
|---|---|---|---|
| **5-leg gate (this release)** | `Scripts\run_gate.ps1 -RequireOpenSees` | ✅ `GATE: PASS` (`GATE_EXIT=0`) | 136 tests run (135 FrameCore + 1 ArchSim), filter chain `FrameCore+ArchSim` verified working |
| UE Editor build | `%UE_ENGINE_ROOT%\Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development` | ✅ PASS (incremental 7.46s, relink only — test source unchanged from v0.1.1) | |
| UE Editor → Plugins panel visual confirm | (Open UE Editor → Edit → Plugins) | ⚠️ **NOT RUN (human-action item)** | Carried over from v0.1 / v0.1.1; tracked as AS-04 |

**Gate breakdown (`Saved/Logs/ArchSim.log`):**
- `FrameCore.*` namespace: 135 tests, all PASS (v4.0.0 engine baseline)
- `ArchSim.*` namespace: 1 test (`ArchSim.Persistence.SaveLoadRoundTrip`), `Result={成功}`
- Total: **136 tests run**, hits `$ExpectedUeTests = 136` exactly

**Honest scope:**
- v0.1.2 verifies the gate driver change. It does **not** add any new test
  coverage — A1-07 was already verified in v0.1.1; v0.1.2 just formalises its
  enforcement.
- The non-cuDSS fallback (134) has been **noted** in the comment but **not
  re-verified on a non-cuDSS host** this cycle. Last verified on cuDSS host
  2026-06-25; a non-cuDSS host should pass `-ExpectedUeTests 134` per the
  pattern documented in the line-29 comment.

---

## 3. Reproducibility

```powershell
# Prereqs: UE 5.7 installed at %UE_ENGINE_ROOT%; framecore-direct conda env present.
git clone https://github.com/rocky59487/architect_simulator.git
cd architect_simulator
git checkout v0.1.2

# Restore the 4 plugin clones
git clone --branch 4.17 https://github.com/Sixze/ALS-Refactored           Plugins/ALS
git clone --depth 1   https://github.com/unknownworlds/prefabricator-ue5  Plugins/Prefabricator
git clone --depth 1   https://github.com/sinbad/SPUD                      Plugins/SPUD
git clone --depth 1   https://github.com/sinbad/SUQS                      Plugins/SUQS
# Patch the 3 plugins missing EngineVersion (see SPRINT_NOTES Spike 1)

# Build the editor
& "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat" ArchSimEditor Win64 Development `
    -project="$PWD\ArchSim.uproject" -waitmutex

# Single command — now covers ArchSim.* in addition to FrameCore.*
.\Scripts\run_gate.ps1 -RequireOpenSees
# Expect:  GATE: PASS  (standalone OK, UE 136 tests green, OpenSees PASS, deep audit OK, CLI round-trip OK)
# Expect:  GATE_EXIT=0

# On a non-cuDSS host, the two GPU tests (F67/F67s) compile out; pass:
# .\Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 134
```

---

## 4. Breaking changes

**None.** v0.1.2 is purely an additive gate-driver fix. All v0.1 / v0.1.1 contracts
hold; clients calling either the engine or the game-body code are not affected.

---

## 5. Deferred items (audit IDs carried forward)

AS-01 is now **closed** by this release. The remaining backlog from `HANDOFF_v0.1.1.md`
moves forward unchanged:

| ID | Item | Deferred to |
|---|---|---|
| ~~AS-01~~ | ~~`run_gate.ps1` ArchSim namespace coverage~~ | **✅ closed in v0.1.2** |
| AS-02 | A1-06 full integration test | S-02 (v0.2) |
| AS-03 | A2-01 ALS pawn integration | S-02 (v0.2) |
| AS-04 | Gate 0 UE Editor → Plugins panel visual confirm | Human action, any time |
| AS-05 | K1-T2 / K4 art assets | S-02 / S-03 |
| AS-06 | SPUD UE5.5 `StructUtils` deprecation | Before any UE5.7 → UE5.8 upgrade |
| AS-07 | A1-07 MaxRank=96 stress test | S-02+ (v0.2+) |
| AS-08 | SPUD orchestration `RF_Transient` audit | When wiring SPUD orchestration |
| AS-09 (new) | Re-verify gate on a non-cuDSS host with `-ExpectedUeTests 134` | Next opportunity on a non-cuDSS box |

---

## 6. Tag plan

```bash
git add Scripts/run_gate.ps1 docs/RELEASE_v0.1.2.md docs/HANDOFF_v0.1.2.md README.md
git commit -m "release: v0.1.2 -- 5-leg gate covers ArchSim namespace (AS-01 closed)"
git tag -a v0.1.2 -m "Architect Simulator game body v0.1.2 -- AS-01 gate-coverage closure"

git push origin main
git push origin v0.1.2
gh release create v0.1.2 \
    --title "Architect Simulator game body v0.1.2" \
    --notes-file docs/RELEASE_v0.1.2.md
```

---

*v0.1.2 is the third game-body release on top of FrameCore v4.0.0 + LevelSim v1.*
*Three releases in a single day (v0.1 → v0.1.1 → v0.1.2) reflect the new tight*
*release cadence — every release-hardening cycle ships.*
