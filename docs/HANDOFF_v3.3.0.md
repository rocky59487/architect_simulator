# 交接指南 — `v3.3.0` 後接手 owner

> `v3.3.0` 在 2026-06-22 發布 (closes the v3.2.2 audit's v3.3-candidate items U-07,
> U-01, U-03, V321-01a). 主交接文 chain:
> [`docs/HANDOFF.md`](HANDOFF.md) → [`docs/HANDOFF_v3.1.0.md`](HANDOFF_v3.1.0.md) →
> [`docs/HANDOFF_v3.2.0.md`](HANDOFF_v3.2.0.md) → [`docs/HANDOFF_v3.2.1.md`](HANDOFF_v3.2.1.md) →
> [`docs/HANDOFF_v3.2.2.md`](HANDOFF_v3.2.2.md) → 本檔.

## 1. `v3.3.0` = 什麼

**一句話 (BREAKING)**: 把 stress-field schema 從 user-id sentinel(0=「no governing」)
換成 internal-index sentinel(-1=「no governing」); 動 engine struct + Dispatcher v2 JSON
wire + USTRUCT + 5 個 UE marshal test 的 assertion 文字; 順手收 V321-01a SS-beam Vy
analytic oracle、加 U-03 UE renderer(`AFrameCoreStressFieldActor` + procedural mesh
sigma-band)+ U-01 BP load JSON entry(`UFrameCoreStressFieldLibrary::ComputeFromJsonModel`)。

**Why now**: v3.2.2 audit pinned U-07 as the only remaining audit-confirmed BLOCKER. The
"smoke test 碰巧對" pattern (cantilever fixture's member id=0 coincided with the engine's
no-governing sentinel 0) meant pre-v3.3 BP / GH clients could not tell "the model has no
governing element" from "the model's id-0 element governs". Now they can.

**Engine source delta vs v3.2.2:** 4 files / ~30 lines:
- `Plugins/FrameSolver/Source/FrameCore/Public/FrameCore/StressField.h` — rename
  `governingMemberId / governingShellId` → `governingMemberIdx / governingShellIdx`,
  default 0 → -1, comment block rewritten.
- `Plugins/FrameSolver/Source/FrameCore/Private/StressField.cpp` — rename local
  accumulators, switch writer from `mem.id` / `sh.id` to `(int)e` / `(int)s`.
- `Plugins/FrameSolver/Source/FrameCore/Private/Tests/StressFieldTest.cpp` — UE-side
  oracle assertion updated to read `.governingMemberIdx` + cross-check per-element
  user id via `Members[0].MemberId`.
- `Plugins/FrameSolver/Standalone/main.cpp` — F70 D/C interlock rewritten to
  `idx >= 0 && model.members[idx].id == dm.governingMember` lookup pattern (instead of
  the pre-v3.3 `fld.governingMemberId == dm.governingMember` direct compare that masked
  ID-0 ambiguity); new **F71** fixture with three sub-cases (empty model / all-inactive /
  id-0-governing).

CUDA-path engine source delta: 0 lines. GPU lane carries forward unchanged.

**Non-engine delta:**
- `Dispatcher.h` — `kEngineVer "3.2.0"` → `"3.3.0"`, comment block adds the U-07
  BREAKING note.
- `Dispatcher.cpp` — `packStressField` JSON key `governingMemberId/Id` →
  `governingMemberIdx/Idx`.
- `FrameCoreUE/` — `FFrameStressField` field rename + library accessor rename
  (`GetGoverningMemberId` → `GetGoverningMemberIdx`); 7 marshal tests in-place
  assertion update; new `AFrameCoreStressFieldActor` + new
  `FrameCoreUEActorStressMeshTest`; new `ComputeFromJsonModel` library entrypoint +
  new `FrameCoreUEMarshalJsonTest`.
- `FrameSolver.uplugin` — `Version 30 → 31`, `VersionName "3.2.0" → "3.3.0"`, new
  `Plugins.ProceduralMeshComponent` reference (PMC enabled transitively).
- `FrameCoreUE.Build.cs` — adds `ProceduralMeshComponent` + `Json` + `JsonUtilities`
  to `PublicDependencyModuleNames`.
- `Scripts/run_gate.ps1` — `$ExpectedUeTests 70 → 72`.
- `Scripts/run_gpu_gate.ps1`, `.github/workflows/release-gate.yml` —
  `FRAMECORE_EXPECTED_ENGINE_VER '3.2.0' → '3.3.0'`.
- `Tools/v2_roundtrip.py` — 4 new assertions on the renamed keys + 2 absence checks
  for the legacy keys.
- `docs/specs/S11_v3.3_schema_migration.md` — new spec doc, the migration source of
  truth.
- `docs/specs/S11_stress_field.md` — cross-link note that F71 is reassigned.
- `README.md`, `docs/VERIFICATION.md` — updated F1..F71, UE 72/72 counts, v3.3.0
  status section + migration callout.

## 2. 怎麼跑 (主要 reproduce paths)

```powershell
# Setup (one-time per shell):
$env:PATH = "$env:USERPROFILE\anaconda3\envs\framecore-direct\Library\bin;$env:PATH"
$env:UE_ENGINE_ROOT = "E:\project\UE_5.7"

# Rebuild + 5-leg gate (cuDSS host):
& 'E:\project\UE_5.7\Engine\Build\BatchFiles\Build.bat' ArchSimEditor Win64 Development -project='E:\project\ArchSim\ArchSim.uproject' -waitmutex
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees

# Non-cuDSS box:
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 70

# v2 dispatcher CPU round-trip:
Plugins\FrameSolver\Standalone\build_capi_v2.bat
$env:FRAMECORE_EXPECTED_ENGINE_VER = '3.3.0'
python Tools\v2_roundtrip.py
# Expected: "=== summary: ALL PASS ==="
# Includes 2 new assertions verifying the legacy `governingMember*Id` JSON keys are absent.

# GPU 6th leg (cuDSS host only):
powershell -ExecutionPolicy Bypass -File Scripts\run_gpu_gate.ps1 -Strict
```

**v3.3.0 verified on host (2026-06-22):**
- standalone F1..F71 **ALL PASS**
- UE 72/72 **ALL PASS** (incl. `FrameCore.UE.ActorStressMeshTest` and
  `FrameCore.UE.MarshalJsonTest`)
- OpenSees strict PASS
- linear deep audit 104 PASS
- CLI round-trip 13 PASS
- v2_roundtrip CPU **ALL PASS** with `FRAMECORE_EXPECTED_ENGINE_VER='3.3.0'`, both
  schema-break assertions green (`governingMemberId` / `governingShellId` keys
  **absent** from response; `governingMemberIdx == 0`, `governingShellIdx == -1`
  on the cantilever fixture).

**CUDA legs (F67s strict / v2_roundtrip CUDA / r2_bench 90k):** **NOT RUN** this
session — v3.3 engine source delta in the CUDA path is **0 lines** (Phase 3 +
Phase 4 are pure UE/Standalone-side, U-07 rename is in StressField path which is
not on the CUDA lane), so the v3.0.0 / v3.1.0 GPU evidence (`r2_bench --gpu 90k
margin +11.939 ms`, `F67s STRICT_EXECUTED` fingerprint) carries forward unchanged.
Honest: a cuDSS-equipped host should re-run `run_gpu_gate.ps1 -Strict` once before
GitHub release publish to refresh the evidence trail; standalone path was rebuilt
so the cuDSS `frametest_cuda.exe` is on the new source if rebuilt.

## 3. Migration guide for clients

### v3.2.x → v3.3 BREAKING wire-protocol change

`inspect.stress_field` request unchanged; response body renames two keys:

| Pre-v3.3 (v3.1.0 .. v3.2.2) | v3.3 |
|---|---|
| `body.stressField.governingMemberId` (int, 0 = no governing) | `body.stressField.governingMemberIdx` (int, -1 = no governing, else 0-based index into `model.members`) |
| `body.stressField.governingShellId` (int, 0 = no governing) | `body.stressField.governingShellIdx` (int, -1 = no governing, else 0-based index into `model.shells`) |

`solve.linear` `summary.governingMember` is **unchanged** (D/C screen path; no
ID-0 ambiguity on that path).

### C# bridge (GH / Rhino)

```csharp
// v3.2 — broken if a real element has id == 0:
int gid = body["stressField"]["governingMemberId"].AsInt();
var gov = model.Members.FirstOrDefault(m => m.Id == gid);  // returns null when id 0 == sentinel 0

// v3.3 — correct:
int gix = body["stressField"]["governingMemberIdx"].AsInt();
var gov = (gix >= 0) ? model.Members[gix] : null;
int govId = gov?.Id ?? -1;
```

### Blueprint designer (FrameCoreUE)

Old node: `Get Governing Member Id` → new node: `Get Governing Member Idx`. To
recover the user id, lookup the member array: `Field.Members[Idx].MemberId`.

### Server version pin

`FRAMECORE_EXPECTED_ENGINE_VER` env var bumps `'3.2.0' → '3.3.0'`. Without the
bump, `v2_roundtrip` fails the version check rather than (worse) silently passing
with a broken schema.

## 4. New surfaces — what BP designers / GH clients can now do

### `AFrameCoreStressFieldActor` (UE)

A runtime actor that draws the stress field as a sigma-band procedural mesh along
every member. Spawn it in a Map, set `Field` (from
`UFrameCoreStressFieldLibrary::ComputeCantileverFixture` / `ComputeFromJsonModel`)
and `MemberGeometry`, then call `BuildMesh()` (or leave `bAutoBuildOnBeginPlay`
true). The static helper
`AFrameCoreStressFieldActor::MakeCantileverDemoGeometry(L, Width, Depth)` wires a
single-member geometry list compatible with the cantilever fixture for a one-shot
BP demo. Thin slice — **member only, no shell heat-map / Niagara / collapse replay**;
those land in a later release.

### `UFrameCoreStressFieldLibrary::ComputeFromJsonModel(JsonPath, SamplesPerSpan)`

Load a model from disk in the dispatcher's `model.set` JSON schema subset (materials
/ sections / nodes / members / nodalLoads / memberUDLs), solve, and return the
marshalled FFrameStressField. Schema unsupported subset (logged but ignored):
shells, member release / tensionOnly / prescribed, solver options. On parse / solve
failure: logs a warning, returns a default-constructed FFrameStressField (governing
indices == -1) so the BP graph never crashes.

## 5. Deferred items (carry-forward to v3.4+)

- **U-02** — Slate fixture-selector dropdown for the developer panel
  (`SFrameCoreStressFieldPanel` still only shows the cantilever).
- **U-05** — float-only USTRUCT precision. v3.3 keeps `FFrameStressFieldSample`
  fields as `float`; downstream BP graphs that need double precision over a long
  member trace can still hit the rel<1e-5 float-lossy budget. A future release can
  add a double-precision variant or expose engine-side doubles via a parallel
  USTRUCT.
- **U-06** — UE 5.7 + VS 2026 "not preferred version" build warning. Cosmetic;
  build succeeds.
- **B-06** — `model.patch` schema. Open since v2.4; not implemented unless a GH
  client requests a transactional edit API.
- **v3.3 U-07 secondary** — `governingShellLayer` enum + `governingShellCorner`
  schema unchanged (these were not ambiguous pre-v3.3 — `-1` was already the
  sentinel for "no corner / centre"). Keep as is.

## 6. Lessons (v3.3-specific durable)

1. **Schema-break-in-place beats key reuse with new semantics**. v3.3 considered
   keeping `governingMemberId` and changing its value semantics from "user id" to
   "internal index"; rejected because a GH client reading the key would silently
   mis-resolve every governing pointer. Loud "key not found" >> silent
   mis-resolution every release.

2. **F70 "interlock" wasn't really an interlock pre-v3.3.** The pre-v3.3
   assertion `fld.governingMemberId == dm.governingMember` passed for the
   cantilever fixture because both sides returned 0 — but for completely
   different reasons (one was "user-id 0 governs", the other was "no governing"
   sentinel). The v3.3 lookup pattern
   `m.members[fld.governingMemberIdx].id == dm.governingMember` is what real
   D/C-vs-StressField cross-validation looks like. **Look for compares that
   pass for the wrong reason** — they often surface schema collisions.

3. **Sign-agnostic conservation oracles outlast directional analytic closed-forms.**
   V321-01a's v3.2.2 attempt `|Vy| at midspan member 0 == w*L/4` failed because
   "midspan of member 0" is the quarter-span, AND because the engine's
   `endI.Vy` sign convention is undocumented. v3.3's reopened oracle uses
   conservation (`Vy(endI) - Vy(endJ) = w_y * span`) + reaction-magnitude checks
   that hold regardless of sign. **First pass on internal-force oracles: write
   integrals, not point values.**

4. **PMC in headless UE Automation needs an existing world context, not a
   new one.** `UWorld::CreateWorld` in a `-ExecCmds=Automation` commandlet
   crashes because the editor's GameInstance template isn't loaded.
   `GEngine->GetWorldContexts()[k].World()` returns a usable world the
   commandlet provisions; spawn there. Filed under UE-test patterns —
   reuse for any future actor-spawning test (Niagara stress particles,
   collapse replay actor, etc.).

5. **`FrameSolver.uplugin` `Plugins` block is the cleanest way to add a UE
   built-in plugin dep without touching `.uproject`.** PMC is needed by
   `AFrameCoreStressFieldActor` but `.uproject` is on the "do not touch"
   list (CLAUDE.md 鐵則 #5). The `.uplugin` `Plugins: [{"Name":
   "ProceduralMeshComponent", "Enabled": true}]` block makes PMC a transitive
   dep of FrameSolver — auto-enabled in any host project. Adopt this pattern
   for future Niagara / GeometryFramework / Slate-Insights deps.

6. **`-RequireOpenSees -ExpectedUeTests N` is the rebuild + count guard.**
   When adding UE tests, bump `$ExpectedUeTests` in `run_gate.ps1` AND pass
   `-ExpectedUeTests N` for the verification run during development before
   committing the script change — otherwise the new test passes locally but
   `run_gate.ps1` quietly accepts a stale count guard.

## 7. v3.4 candidates (rough cut)

- **U-02 polish** — Slate fixture dropdown.
- **U-05 widen** — double-precision USTRUCT variant or engine-double passthrough.
- **Shell heat-map renderer** — extend `AFrameCoreStressFieldActor` to draw
  per-corner vonMises heat-map on `Field.ShellsTop / ShellsBot`. Shares colour
  ramp helper.
- **Schema widen for `ComputeFromJsonModel`** — add shells, member releases,
  prescribed displacements. Currently a thin subset.
- **Demo Map asset** — `Content/Maps/FrameCoreStressDemo.umap` with a spawned
  `AFrameCoreStressFieldActor` + camera + post-process volume so BP designers
  can Hit Play and see the stress band without writing any BP. Held back from
  v3.3 because Map assets are UE-editor binary, not textual; could be added
  out-of-band by a manual UE-editor save.
- **`solve.linear` `summary.governingMember` schema review** — left unchanged
  by v3.3 because the D/C path's `0 == no failure` semantics is genuinely
  unambiguous, but the precedent of "rename rather than re-use" suggests a
  future cleanup if any other path sprouts a similar collision.

## 8. CLAUDE.md anchor maintenance

`E:\project\CLAUDE.md` lives outside the repo's git tracking; its v3.2.2 line-13
HEAD anchor remains stale through every v3.x release that touches it. Manual
sync as part of v3.3.0 release-hardening:

> Line 13 (was): `## 現況(2026-06-22,HEAD `v3.2.2`,...)`
> Line 13 (now): `## 現況(2026-06-22,HEAD `v3.3.0`,...) ...` (with the v3.3
> narrative above).

This file is the operator's quick-reference; out-of-tree maintenance is the only
way to keep it accurate across releases.

---

接手有問題:
- 主交接鏈: `docs/HANDOFF.md` → `docs/HANDOFF_v3.2.x` → 本檔 → `docs/RELEASE_v3.3.0.md`
- 5-min 入手指南: [`docs/FrameCoreUE_QuickStart.md`](FrameCoreUE_QuickStart.md)
- U-07 schema migration spec: [`docs/specs/S11_v3.3_schema_migration.md`](specs/S11_v3.3_schema_migration.md)
- StressField 全圖: [`docs/specs/S11_stress_field.md`](specs/S11_stress_field.md)
- Engine 全圖: [`docs/ARCHITECTURE.md`](ARCHITECTURE.md)
- 驗證證據鏈: [`docs/VERIFICATION.md`](VERIFICATION.md)
