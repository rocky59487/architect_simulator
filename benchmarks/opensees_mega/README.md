# OpenSees Mega Benchmark

This directory contains the FrameCore x OpenSees mega benchmark requested in
`docs/AGENT_PROMPT_OPENSEES_MEGA_BENCHMARK.md`.

Units are N, mm, MPa throughout. The default run keeps the matrix small enough
for local iteration while covering all requested A/B/C/D families:

- A1-A8: building frames, trusses, mixed shell/frame floors, and removal states.
- B1-B4: beam, continuous beam, truss bridge, and segmented arch bridge.
- C1-C5: curved or warped shell cases at coarse, medium, and fine meshes.
- D1-D4: P-Delta, release/hinge surrogate, prescribed settlement, and combined loads.

The harness writes deterministic artifacts under `results/<run_id>/`:

- `matrix.csv`: one row per case/load/mesh comparison.
- `findings.json`: classified issues, unsupported mappings, and known gaps.
- `report.md`: executive report with family summaries and shell convergence notes.
- `plots/`: CSV convergence curves for the C-family shell cases.

## Run

From the repo root:

```powershell
.\benchmarks\opensees_mega\rerun.ps1
```

Or directly:

```powershell
python .\benchmarks\opensees_mega\harness\compare.py
```

The harness builds `frame_cli.exe` only if it is missing. It does not change
engine defaults and does not require any dependency beyond Python, NumPy/SciPy
as already used by OpenSeesPy, and `openseespy`.

## Notes

OpenSees is used as the primary independent solver for equivalent linear beam,
shell, P-Delta, modal, and displacement-state comparisons. Some requested
features do not map one-to-one through the current CLI or OpenSees:

- CLI text has no member-release token; D2 records the unsupported direct mapping
  and compares a hinge-state surrogate.
- Freeform NURBS shells are not a CLI primitive; C5 uses a faceted warped shell
  surrogate and records the smooth-surface oracle as a known gap.
- Shell resultants (`SF`) are kept from FrameCore output. OpenSees ShellMITC4
  does not expose the same engineering-resultant row in a stable portable form,
  so shell force comparison is recorded as not directly comparable.
