# verify_smoke_shots.py — pixel-level cross-check of the smoke screenshots.
# Independent oracle for the RENDERED geometry: recover the crosshair's staff reading
# from the PNG itself (digit band position + projection scale) and compare with the
# levelsim core truth logged by the smoke run. This closes the loop "what the player
# sees == what measure() predicts" without trusting either side.
#
# Method per shot:
#   1. crosshair row  = darkest row across a staff-area column window (2 px black hair)
#   2. dm digit band  = red-pixel rows in the digits column (7-seg numerals), clustered;
#      take the cluster whose centre is nearest the crosshair -> that dm line's centre
#      row corresponds exactly to reading = dm/10 m (digits are centred on the dm line)
#   3. px/cm          = 720 / (2*D*tan(vfov/2)*100), vfov from hfov 2.3 deg @ 16:9
#   4. reading_at_hair = dm_value + (digit_row - hair_row)/px_per_cm / 100
# PASS if |reading_at_hair - core_truth| <= 2 mm (eyeball-free, sub-px noise dominated).
#
# truth_m derivation (R-AUDIT C-10): the hardcoded truth values below are not magic
# numbers — they reproduce what `levelsim::measure()` returns for the SmokeAutoLevel
# scene state, computed from the published core constants in LevelSimPawn:
#   ScrewTravel = [1e-4, 1e-4, 1e-4]   (SmokeAutoLevel sets equal travels)
#   opticsHeightZ = kBaseOpticsM + (Σ ScrewTravel)/3.0 = 1.5 + 1e-4 = 1.50010 m
#   collimationRad = arcsec(10) ≈ 4.8481e-5 rad   (LevelSimPawn ctor)
#   residual = 0  (equal travels → tilt = 0 → residual clamped to 0 in smoke path)
#   BM  staff at D=10, baseZ=0     → reading = 1.50010 + 10·tan(c) − 0      ≈ 1.50059 ≈ 1.5006
#   P1  staff at D= 8, baseZ=0.37  → reading = 1.50010 +  8·tan(c) − 0.37   ≈ 1.13049 ≈ 1.1305
# If kBaseOpticsM, the smoke screw travel set, the collimation constant, or the P1
# staff baseZ change, the truth_m arguments below must be re-derived from the formula
# above (NOT eyeballed from a new screenshot).
#
# Resolution assumption (R-AUDIT C-11): smoke shots are taken at 1280×720 by
# `run_smoke.bat` (`-ResX=1280 -ResY=720`); px_per_cm and the cluster-gap heuristic
# (gap > 12 px starts a new digit band) both assume this size. analyze() asserts it.
import sys
from PIL import Image
import math

SHOT_DIR = r"E:\project\ArchSim\Saved\Screenshots\WindowsEditor"
HFOV = 2.3
W, H = 1280, 720

def vfov_rad():
    return 2.0 * math.atan(math.tan(math.radians(HFOV / 2)) * H / W)

def analyze(path, dist_m, digit_value_m, truth_m):
    im = Image.open(path).convert("RGB")
    assert im.size == (W, H), im.size
    px = im.load()

    # 1) crosshair row: darkest row averaged over staff columns, excluding vertical hair
    cols = [x for x in range(500, 760) if abs(x - W // 2) > 14]
    best_row, best_v = None, 1e9
    for y in range(H // 2 - 80, H // 2 + 80):
        v = sum(sum(px[x, y]) for x in cols) / len(cols)
        if v < best_v:
            best_v, best_row = v, y

    # 2) red digit rows in the digits column (right side of the staff face)
    # digits column only: local y +3.2..+6.6 cm -> x = 640 + y*px_per_cm; the E-spine
    # right edge ends at +2 cm (~x 700-720) and MUST be excluded or clusters merge.
    reds = []
    for y in range(0, H):
        cnt = 0
        for x in range(745, 910):
            r, g, b = px[x, y]
            # marks render dark maroon under scene lighting (~(48,13,21)): ratio test
            if r > 28 and r > 1.7 * g and r > 1.5 * b:
                cnt += 1
        if cnt >= 3:
            reds.append(y)
    # cluster consecutive rows (gap > 12 px starts a new digit band)
    clusters, cur = [], [reds[0]]
    for y in reds[1:]:
        if y - cur[-1] <= 12:
            cur.append(y)
        else:
            clusters.append(cur); cur = [y]
    clusters.append(cur)
    centers = [sum(c) / len(c) for c in clusters]
    digit_row = min(centers, key=lambda c: abs(c - best_row))

    # 3) projection scale
    px_per_cm = H / (2.0 * dist_m * math.tan(vfov_rad() / 2.0) * 100.0)

    # 4) recovered reading at the crosshair (image y grows downward; staff up = smaller y)
    reading = digit_value_m + (digit_row - best_row) / px_per_cm / 100.0
    err_mm = (reading - truth_m) * 1000.0
    ok = abs(err_mm) <= 2.0
    print(f"{path.split(chr(92))[-1]}: hair_row={best_row} digit_row={digit_row:.1f} "
          f"px/cm={px_per_cm:.2f} recovered={reading:.4f} m truth={truth_m:.4f} m "
          f"err={err_mm:+.2f} mm -> {'PASS' if ok else 'FAIL'}")
    return ok

ok1 = analyze(SHOT_DIR + r"\levelsim_smoke_03_scope_bm.png", 10.0, 1.50, 1.5006)
ok2 = analyze(SHOT_DIR + r"\levelsim_smoke_04_scope_p1.png",  8.0, 1.10, 1.1305)
sys.exit(0 if (ok1 and ok2) else 1)
