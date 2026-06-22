# S11 -- Stress Field (visualisation numerical layer)

> Status: **landed v3.1.0** (engine), default-ON post-process. Pure numerical layer;
> no Eigen leak; no UE coupling. Visualisation / colour band rendering is consumer-side.

> ⚠ **BREAKING (v3.3.0)**: the governing-element pointer fields were renamed
> `governingMemberId / governingShellId` → `governingMemberIdx / governingShellIdx`,
> and the value semantics changed from "user id with sentinel 0" to "internal index
> with sentinel -1". The struct field names and JSON key examples below already
> reflect the v3.3 schema. **A v3.2 consumer reading these names assuming the old
> semantics will silently mis-resolve every governing pointer** — read
> [`S11_v3.3_schema_migration.md`](S11_v3.3_schema_migration.md) before consuming the
> new wire on a pre-v3.3 client base.

## What this is

The visualisation stress field is the numerical post-process that a renderer
(UE5 spline mesh, Rhino/Grasshopper, a CLI dump) reads to paint a continuous
fiber-stress band along every member and a per-corner von Mises field on every
shell facet. It is the data layer behind "stress cloud" visuals; the rendering
itself is out of scope for FrameCore.

The kernel formulas (member fiber sigma, shell layer sigma, principal stress,
von Mises) are factored into a single source-of-truth header so that the
elastic D/C screen (`ElasticAllowable`) and the field post-process
(`computeStressField`) cannot drift.

## Headers and types

- `Public/FrameCore/StressKernel.h` -- pure inline math helpers.
  - `memberFiberSigma(N,A,My,Mz,Iy,Iz,cy,cz,fiber)` -- compression-positive
    fiber sigma at one of 4 face mid-lines (`MemberFiber::TopY/BotY/PlusZ/MinusZ`).
  - `memberCornerSigmaMax(N,A,My,Mz,Wy,Wz,shape)` -- returns
    `{sComp, sTens, sBend, sAxial}` for the worst corner (rectangular sum-of-abs;
    circular resultant). This is exactly what `ElasticAllowable::checkSection`
    now calls; the previous inline formula is gone.
  - `memberShearPeak(Vy,Vz,A,shape)` -- peak transverse shear (k * V/A; k = 1.5
    rectangle, 4/3 circle).
  - `memberTorsionTau(T,J,cy,cz,shape)` -- T * c / J at the extreme fibre.
  - `shellLayerSigma(Nxx,Nyy,Nxy,Mxx,Myy,Mxy,t,layer)` -> (sx,sy,tau).
  - `principalStress(sx,sy,txy)` -> {s1, s2, vonMises, theta}.
- `Public/FrameCore/StressField.h` -- POD result types + entry point.
  - `MemberStressSample {x, sigmaFiber{TopY,BotY,PlusZ,MinusZ},
                          sigmaCompMax, sigmaTensMax, tauShear, tauTorsion,
                          N, Vy, Vz, T, My, Mz}`.
  - `MemberStressTrace {memberIdx, memberId, samples}` (samples = `samplesPerSpan`, default 11).
  - `ShellStressPoint {cornerIdx, sigmaXX, sigmaYY, tauXY,
                       sigma1, sigma2, vonMises, thetaRad}`.
  - `ShellStressLayer {shellIdx, shellId, layer, center, corners[4]}`.
  - `StressField {members, shellsTop, shellsBot, globalMaxFiberSigma,
                  globalMaxVonMises, governingMemberIdx, governingShellIdx,
                  governingShellLayer, governingShellCorner}` (v3.3+; see migration
                  doc for the pre-v3.3 `...Id` form and the value-semantics change).
- `Private/StressField.cpp` -- implementation. No allocations beyond the
  returned vectors; UDL lookup is a linear scan over `model.memberUDLs`.

## Sampling

- **Members**: `samplesPerSpan` along-axis points (default 11) at `x = k * L / (n - 1)`.
  Internal forces at `x` are reconstructed from the model's end-i forces + UDL by:
  ```
  N(x)  = N_i  - w_x * x
  Vy(x) = Vy_i - w_y * x                Mz(x) = Mz_i - Vy_i * x + 0.5 * w_y * x^2
  Vz(x) = Vz_i - w_z * x                My(x) = My_i + Vz_i * x - 0.5 * w_z * x^2
  T(x)  = T_i
  ```
  The sign convention is calibrated against F68: end-i is the "fixed" end for the
  cantilever fixture, so `Mz(0)` must equal `endI.Mz` and `Mz(L)` must vanish at
  the free end. F70's D/C interlock then asserts equivalence against
  `ElasticAllowable` at the governing element bit-exact.
- **Shells**: every facet yields 5 sample points per fiber face (centre + 4 corners),
  on each of `ShellLayer::Top` and `ShellLayer::Bot`. The traversal mirrors
  `ElasticAllowable::checkShellSurface`'s `kc=-1..3 / face=0..1` loop so the two
  cannot drift.

## Public C ABI

- `inspect.stress_field` (v3.1.0 capability) -- dispatcher reads the cached
  `SolveResult`, runs `computeStressField`, packs the field as JSON:
  ```
  {
    "stressField": {
      "samplesPerSpan": 11,
      "globalMaxFiberSigma": <real>,
      "globalMaxVonMises":   <real>,
      "governingMemberIdx":  <int>,   // v3.3+; pre-v3.3 was "governingMemberId" (user id, 0 = no governing)
      "governingShellIdx":   <int>,   // v3.3+; pre-v3.3 was "governingShellId"  (user id, 0 = no governing)
      "governingShellLayer": "top"|"bot",
      "governingShellCorner": -1|0|1|2|3,
      "members": [ { "memberId", "memberIdx",
                     "samples": [ { "x", "sigmaFiber*", "sigmaCompMax", ... } * 11 ] } * ],
      "shellsTop": [ { "shellId", "shellIdx", "layer": "top",
                       "center": { ... }, "corners": [ { ... } * 4 ] } * ],
      "shellsBot": [ ... mirror ... ]
    }
  }
  ```
  Optional request body field: `samplesPerSpan` (int in `[2, 1024]`, default 11).
- Capability advertised in `Capabilities()` so clients can gate on it (lesson
  from v2.5: unadvertised capability = invisible to the SDK = effectively absent).

## Oracle ladder

| Fixture | What it verifies | Tolerance |
|---|---|---|
| **F1..F66** | Engine unchanged after the `StressKernel`-refactor of `ElasticAllowable`. | unchanged (existing rel<1e-12) |
| **F68** | Cantilever member stress field; sample[0]/sample[L] bit-exact vs `ElasticAllowable(endI/endJ)`; analytic `|P|(L-x)/Wz` at 11 samples; tip sigma vanishes; root TopY/BotY fiber equals sComp. | rel<1e-12 (interlock), rel<1e-9 (analytic) |
| **F69** | Shell layer recovery `(top+bot)/2 == Nxx/t` AND `(top-bot)/2 == 6Mxx/t^2` over every sample point of every shell; vonMises invariant under a 30 deg rigid z-rotation. | rel<1e-12 (recovery), rel<1e-9 (invariance) |
| **F70** | D/C interlock: governing member/shell ids match between `worstUtilization`/`worstShellUtilization` and `StressField`; max fiber sigma equals max ElasticAllowable end sigma (no UDL); `globalMaxVonMises` equals `worstShellUtilization.maxDC * cap.vm`. | rel<1e-12 (bit-exact, shared kernel) |
| **UE** `FFrameCoreStressFieldTest` | UE-build mirror of F68 (member side). | rel<1e-12 + rel<1e-9 |

### OpenSees note

An OpenSees direct sigma comparison was deliberately not added in v3.1.0. The
StressField output for a shell sample is `Nxx/t +/- 6Mxx/t^2` -- a textbook
identity over `ShellElementForces`. `Nxx`/`Mxx` themselves are covered by the
existing OpenSees gate (Tools/opensees_compare.py over the shell milestone
fixtures). Combined with F70's bit-exact interlock against `ElasticAllowable`
(whose vM was always Nxx/t +/- 6Mxx/t^2 internally), the StressField path is
*transitively* OpenSees-verified. Adding a direct sigma_xx sample on the OSPy
side is future work if a real divergence is ever suspected.

> **F71 reassigned in v3.3**: the v3.1.0 spec reserved `F71` as a placeholder
> for the OpenSees direct sigma fixture above. v3.3 reassigns `F71` to a
> sentinel edge fixture (no-governing-member / all-inactive) for the
> `governingMemberIdx` schema migration; see
> [`S11_v3.3_schema_migration.md`](S11_v3.3_schema_migration.md). An
> OpenSees direct sigma fixture, if ever added, will take a later slot.

## Honest boundaries

- **Sampling is a post-process**, not a refined element. The 11-sample-per-member
  reconstruction is *exact under EB / Timoshenko-equivalent* (transverse shear
  flexibility does not change the bending moment distribution). For Timoshenko
  elements `tauShear = k * V/A` is the peak; the actual through-thickness
  parabolic distribution is not exposed.
- **Shell sampling is at centre + 4 corners only** (matching `ElasticAllowable`).
  Sub-element interpolation (e.g. Gauss-point smoothing) is out of scope here;
  it belongs to a renderer-side resampling pass.
- **Membrane is held at the centre** (`mx = Nxx/t`) and reused at the corners,
  same `element-constant approximation` `checkShellSurface` documents. A finer
  per-corner membrane field would require new MITC4 output, not a post-process.
- **No NM-interaction at the screen level** -- the fiber sigma here is pure
  elastic axial + bending (compression-positive). S10's NM interaction lives
  in the *plastic-hinge formation* path, not the visualisation field.
- **No DKQ tangent-stress refinement** -- when `SolveOptions::useDKQPlate` is
  on, `MxxC[k]` carries Gauss-point values rather than corner extrapolations
  (per `SolveResult.h`); the field then reflects those Gauss-point values at
  the four "corner" slots, which is correct for the underlying element but
  loses the peak-vs-corner distinction. The centre value is the design peak in
  that mode.
