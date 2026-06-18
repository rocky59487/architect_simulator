// level_gate.cpp — 水準儀模擬器測量核心 standalone oracle gate。
// 對應 設計文件 §7（修正後）+ §12 真值模型。沿用 FrameCore 文化：每能力一條獨立數值斷言。
// 2026-06-08 對抗式審核（27 確認發現）後強化：修正圓氣泡方向、真正獨立 oracle、
//   三絲幾何視距、曲率折光、多站閉合+平差、兩樁法、驗證/退化守衛、onStaff。
// 編譯/執行見 build.bat（期望 "ALL PASS  (failures=0)"，exit 0）。
#include "LevelCore.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace levelsim;

static int g_fail = 0;

static void checkAbs(const char* tag, real got, real expect, real atol) {
    const real e  = std::fabs(got - expect);
    const bool ok = std::isfinite(got) && e <= atol;
    if (!ok) ++g_fail;
    std::printf("  %s %-42s got=%-13.6g exp=%-13.6g abs=%.2e (atol=%.0e)\n",
                ok ? "[PASS]" : "[FAIL]", tag, got, expect, e, atol);
}
static void checkClose(const char* tag, real got, real expect, real tol) {
    const real e  = std::fabs(got - expect) / std::max(std::fabs(expect), (real)1e-30);
    const bool ok = std::isfinite(got) && e <= tol;
    if (!ok) ++g_fail;
    std::printf("  %s %-42s got=%-13.6g exp=%-13.6g rel=%.2e (tol=%.0e)\n",
                ok ? "[PASS]" : "[FAIL]", tag, got, expect, e, tol);
}
static void checkTrue(const char* tag, bool cond, const std::string& detail = "") {
    if (!cond) ++g_fail;
    std::printf("  %s %-42s %s\n", cond ? "[PASS]" : "[FAIL]", tag, detail.c_str());
}

// 真正獨立的 2 直線（x-z 平面）求交，用 Cramer 行列式 —— 結構上不同於 D*tan 的路徑。
// 視線 a0 + s*da，標尺鉛直線 b0 + u*db；回傳交點 world Z。
static real lineLineZ(real a0x, real a0z, real dax, real daz, real b0x, real b0z, real dbx, real dbz) {
    const real det = dax * (-dbz) - (-dbx) * daz;
    const real s   = ((b0x - a0x) * (-dbz) - (-dbx) * (b0z - a0z)) / det;
    return a0z + s * daz;
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("# LevelSim measurement-core gate | compiled %s %s\n", __DATE__, __TIME__);
    std::printf("water-level simulator oracle  (units: m, rad)\n\n");

    const InstrumentParams P;   // 預設參數

    // ---------- L1 整平水平：eps=0 => 讀數不隨距離變化 ----------
    std::printf("[L1] level-true: tilt=0,i=0,residual=0 => reading invariant in D\n");
    {
        LevelSetup S; bool readable = false;
        const real Dlist[4] = { 5, 10, 20, 50 };
        real base = 0;
        for (int i = 0; i < 4; ++i) {
            Staff st; st.baseX = Dlist[i]; st.baseZ = 0;
            const real r = trueReading(P, S, st, 0.0, readable);
            if (i == 0) base = r;
            checkAbs((std::string("reading@D=") + std::to_string((int)Dlist[i])).c_str(), r, base, 1e-12);
        }
        checkAbs("reading == opticsZ - baseZ", base, 1.5, 1e-12);
        checkTrue("readable", readable);
    }

    // ---------- L2 讀數定義：vs 獨立 line-line 行列式 + 小角級數（非恆等式）----------
    std::printf("[L2] reading-def: vs line-line determinant AND small-angle series\n");
    {
        InstrumentParams P2 = P; P2.collimationRad = degToRad(0.05); // i != 0 -> eps != 0
        const real eps = P2.collimationRad;
        LevelSetup S; bool rd = false;
        for (real D : { 7.0, 23.0, 41.0 }) {
            Staff st; st.baseX = D; st.baseZ = 0.2;
            const real r = trueReading(P2, S, st, 0.0, rd);
            // 獨立行列式求交：視線 (0,1.5)+s(cos,sin)，標尺 x=D 鉛直線。
            const real zHit = lineLineZ(0.0, 1.5, std::cos(eps), std::sin(eps), D, 0.0, 0.0, 1.0);
            checkClose((std::string("vs determinant @D=") + std::to_string((int)D)).c_str(), r, zHit - 0.2, 1e-12);
            // 小角級數：tan(eps) == eps + eps^3/3 + ...，殘差 O(eps^5)。證明是 tan（sin 會在 eps^3 偏離）。
            const real tanPart = r - (1.5 - 0.2);            // == D*tan(eps)
            const real series  = D * eps + D * eps * eps * eps / 3.0;
            checkAbs((std::string("vs series @D=") + std::to_string((int)D)).c_str(), tanPart, series, 1e-9);
        }
    }

    // ---------- L3 補償器：殘餘有界(修C1) + i角加法性(非恆等) + 工作範圍邊界 + 出範圍NaN ----------
    std::printf("[L3] compensator: bounded residual, additive c+r inside tan, boundary, out=>NaN\n");
    {
        LevelSetup S; Staff st; st.baseX = 30; st.baseZ = 0;
        bool rd = false;
        const real ideal   = trueReading(P, S, st, 0.0, rd);
        const real withRes = trueReading(P, S, st, P.settingAccRad, rd);
        checkTrue("in-range residual reading != 0", std::fabs(withRes - ideal) > 0.0);
        const real over = trueReading(P, S, st, 10.0 * P.settingAccRad, rd);   // 超量被夾
        checkAbs("residual clamped to settingAcc", std::fabs(over - ideal), 30.0 * std::tan(P.settingAccRad), 1e-12);
        // 加法性：losTilt = collimation + clamp(residual)，須是 tan(c+r) 而非 tan c + tan r。
        InstrumentParams Pc = P; Pc.collimationRad = arcminToRad(30.0);
        const real c = Pc.collimationRad, r = P.settingAccRad;
        InstrumentParams P0 = Pc; P0.collimationRad = 0.0;
        const real delta = trueReading(Pc, S, st, r, rd) - trueReading(P0, S, st, 0.0, rd);
        checkClose("delta == D*tan(c+r)", delta, 30.0 * std::tan(c + r), 1e-12);
        checkTrue("delta != D*(tan c + tan r) (additive INSIDE tan)",
                  std::fabs(delta - 30.0 * (std::tan(c) + std::tan(r))) > 1e-9);
        // 工作範圍 <= 邊界（用手建 TiltState 精確命中）。
        auto readableAt = [&](real mag) { TiltState t; t.magRad = mag; t.valid = true; return sightTilt(P, t, 0.0).readable; };
        checkTrue("readable at compRange*(1-1e-6)", readableAt(P.compRangeRad * (1 - 1e-6)));
        checkTrue("readable at compRange (<=)",     readableAt(P.compRangeRad));
        checkTrue("NOT readable at compRange*(1+1e-6)", !readableAt(P.compRangeRad * (1 + 1e-6)));
        // 出工作範圍 => 不可讀 + 讀數為 NaN sentinel（修 1e17 假值）。
        LevelSetup Sout = S; Sout.screwTravel[0] = 0.02;
        const ReadingResult ro = measure(P, Sout, st, 0.0);
        checkTrue("out-of-range => !readable && reading is NaN", !ro.readable && !std::isfinite(ro.reading));
    }

    // ---------- L4 i角/中間法：等距相消 + 對「線性律 i*(Dbs-Dfs)」獨立交叉驗證 ----------
    std::printf("[L4] i-angle: equal-D cancels; vs LINEARIZED i*(Dbs-Dfs) (independent of tan path)\n");
    {
        InstrumentParams P4 = P; P4.collimationRad = degToRad(0.1);
        InstrumentParams P0 = P4; P0.collimationRad = 0.0;
        LevelSetup S; const real elevBM = 100.0;
        auto elevError = [&](real Dbs, real Dfs) -> real {
            Staff bs; bs.baseX = Dbs; Staff fs; fs.baseX = Dfs;
            const real fore  = pointElevation(heightOfInstrument(elevBM, measure(P4, S, bs, 0.0).reading), measure(P4, S, fs, 0.0).reading);
            const real fore0 = pointElevation(heightOfInstrument(elevBM, measure(P0, S, bs, 0.0).reading), measure(P0, S, fs, 0.0).reading);
            return fore - fore0;
        };
        checkAbs("equal D (20/20) => elev error 0", elevError(20, 20), 0.0, 1e-12);
        const real i = P4.collimationRad, e = elevError(40, 10);
        checkAbs("unequal D ~ linear law i*(Dbs-Dfs)", e, i * 30.0, 1e-7);     // 線性律（獨立於 tan）
        checkTrue("nonlinearity present (genuine tan, not linear)", std::fabs(e - i * 30.0) > 1e-8);
    }

    // ---------- L5 視差 ----------
    std::printf("[L5] parallax: focus==D => 0; defocus => >0\n");
    {
        LevelSetup S; Staff st; st.baseX = 25;
        const real D = horizontalDistance(S, st);
        S.focusObjective = D;
        checkAbs("focus==D => 0", parallaxJitter(P, S, D), 0.0, 1e-15);
        S.focusObjective = D + 3.0;
        checkAbs("defocus 3m => coef*3", parallaxJitter(P, S, D), P.parallaxCoef * 3.0, 1e-15);
    }

    // ---------- L6 圓水準器：大小/門檻/單調 + 方向(浮向高側,修critical) + 等變 + 大傾角守衛 ----------
    std::printf("[L6] bubble: magnitude/threshold/monotone + DIRECTION to high side + equivariance\n");
    {
        const BubbleState b0 = bubbleFromTilt(P, TiltState{});
        checkAbs("tilt 0 => offset 0", b0.magDiv, 0.0, 1e-15);
        checkTrue("tilt 0 => fineLevel", b0.fineLevel);
        real prev = -1.0; bool mono = true;
        for (real a : { 1e-4, 5e-4, 1e-3, 2e-3 }) {
            TiltState t; t.magRad = a; t.rollRad = a; const real m = bubbleFromTilt(P, t).magDiv;
            if (m <= prev) mono = false; prev = m;
        }
        checkTrue("offset monotone in tilt", mono);
        // 方向：螺旋 phi=0/120/240，升 +x 螺旋 => +x 為高側 => 氣泡 offX>0、offY~0。
        LevelSetup Sx; Sx.screwPhiDeg[0] = 0; Sx.screwPhiDeg[1] = 120; Sx.screwPhiDeg[2] = 240; Sx.screwTravel[0] = 5e-4;
        const BubbleState bx = bubbleFromTilt(P, tiltFromScrews(P, Sx));
        checkTrue("raise +x screw => bubble offX>0 (high side)", bx.offX > 0.0, std::string("offX=") + std::to_string(bx.offX));
        checkAbs("raise +x => offY ~ 0", bx.offY, 0.0, 1e-9);
        checkAbs("hypot(offX,offY) == magDiv", std::sqrt(bx.offX * bx.offX + bx.offY * bx.offY), bx.magDiv, 1e-12);
        // 等變 + 旋轉對稱：升各 phi 螺旋 => offset 方位追隨該 phi、三者 magDiv 相等。
        real mags[3];
        for (int k = 0; k < 3; ++k) {
            LevelSetup Sk; Sk.screwPhiDeg[0] = 0; Sk.screwPhiDeg[1] = 120; Sk.screwPhiDeg[2] = 240; Sk.screwTravel[k] = 5e-4;
            const BubbleState bk = bubbleFromTilt(P, tiltFromScrews(P, Sk));
            mags[k] = std::sqrt(bk.offX * bk.offX + bk.offY * bk.offY);
            real daz = std::atan2(bk.offY, bk.offX) - degToRad(120.0 * k);   // 環繞角差（避免 k=0 的 ~2π 假象）
            while (daz >  kPi) daz -= 2 * kPi; while (daz < -kPi) daz += 2 * kPi;
            checkAbs((std::string("offset azimuth tracks screw ") + std::to_string(k)).c_str(), daz, 0.0, 1e-6);
        }
        checkAbs("equivariant magnitudes equal (0 vs 1)", mags[0], mags[1], 1e-12);
        checkAbs("equivariant magnitudes equal (0 vs 2)", mags[0], mags[2], 1e-12);
        // 大傾角守衛：magRad>pi/2 => tan<0 => 不得判置中。
        TiltState big; big.magRad = 1.6; big.rollRad = 1.6; big.valid = true;
        const BubbleState bb = bubbleFromTilt(P, big);
        checkTrue("magRad>pi/2 => NOT centered", !bb.roughLevel && !bb.fineLevel);
    }

    // ---------- L7 三腳螺旋：全等=>0; 升一=>>0; 方位旋轉不變; 退化/NaN => valid=false ----------
    std::printf("[L7] screws->tilt: zero, positive, azimuth-rotation invariance, degeneracy guards\n");
    {
        checkAbs("equal screws => magRad 0", tiltFromScrews(P, LevelSetup{}).magRad, 0.0, 1e-12);
        LevelSetup Sa; Sa.screwTravel[0] = 5e-4;
        const TiltState ta = tiltFromScrews(P, Sa);
        checkTrue("raise one => magRad > 0", ta.magRad > 0.0);
        // 方位旋轉 +37deg：magRad 不變，水平傾斜向量剛性旋轉、模長守恆。
        LevelSetup Sb = Sa; for (int i = 0; i < 3; ++i) Sb.screwPhiDeg[i] += 37.0;
        const TiltState tb = tiltFromScrews(P, Sb);
        checkAbs("azimuth-rot invariant magRad", tb.magRad, ta.magRad, 1e-12);
        // 用 (tan roll, tan pitch) ∝ (n.x,n.y) 的水平法線方位（剛性旋轉）；roll/pitch 角本身有非線性。
        const real ax = std::tan(ta.rollRad), ay = std::tan(ta.pitchRad);
        const real bx = std::tan(tb.rollRad), by = std::tan(tb.pitchRad);
        real dAng = std::atan2(by, bx) - std::atan2(ay, ax);
        while (dAng >  kPi) dAng -= 2 * kPi; while (dAng < -kPi) dAng += 2 * kPi;
        checkAbs("tilt azimuth rotates by +37deg", dAng, degToRad(37.0), 1e-9);
        checkAbs("tilt magnitude (tan magRad) preserved", std::hypot(bx, by), std::hypot(ax, ay), 1e-12);
        // 退化 / 非有限 => valid=false（不偽造 magRad=0），且 measure 不可讀 NaN。
        LevelSetup Sdup; Sdup.screwPhiDeg[1] = Sdup.screwPhiDeg[0];   // 重複方位
        checkTrue("duplicate phi => invalid", !tiltFromScrews(P, Sdup).valid);
        InstrumentParams Pr0 = P; Pr0.screwRadius = 0.0;
        checkTrue("zero radius => invalid", !tiltFromScrews(Pr0, Sa).valid);
        LevelSetup Snan; Snan.screwTravel[0] = qnan();
        checkTrue("NaN screwTravel => invalid", !tiltFromScrews(P, Snan).valid);
        Staff st; st.baseX = 10;
        const ReadingResult rn = measure(P, Snan, st, 0.0);
        checkTrue("NaN input => measure not readable + NaN", !rn.readable && !std::isfinite(rn.reading));
    }

    // ---------- L8 視線高法 ----------
    std::printf("[L8] height-of-instrument: H.I.=known+BS; elev=H.I.-FS\n");
    {
        const real HI = heightOfInstrument(100.0, 1.234);
        checkAbs("H.I.", HI, 101.234, 1e-12);
        checkAbs("elev", pointElevation(HI, 0.789), 100.445, 1e-12);
    }

    // ---------- L9 評分：兩內點定線 + 端點連續 + 對稱 + 非有限=>0 ----------
    std::printf("[L9] scoring: two interior points, knee continuity, symmetry, non-finite=>0\n");
    {
        checkAbs("exact => 1.0",      scoreReading(P, 1.500, 1.500), 1.0, 1e-12);
        checkAbs("err=readTol => 1.0", scoreReading(P, 1.500, 1.500 + P.readTol), 1.0, 1e-12);
        checkAbs("err=1.5mm => 0.75", scoreReading(P, 1.500, 1.5015), 0.75, 1e-9);
        checkAbs("err=2mm => 0.5",    scoreReading(P, 1.500, 1.502), 0.5, 1e-9);
        checkAbs("err=2.5mm => 0.25", scoreReading(P, 1.500, 1.5025), 0.25, 1e-9);
        checkAbs("err=readPartial => 0.0", scoreReading(P, 1.500, 1.500 + P.readPartial), 0.0, 1e-12);
        checkAbs("symmetry +/- equal", scoreReading(P, 1.500, 1.5017), scoreReading(P, 1.500, 1.4983), 1e-15);
        checkAbs("NaN player => 0", scoreReading(P, 1.500, qnan()), 0.0, 1e-15);
        checkAbs("Inf player => 0", scoreReading(P, 1.500, INFINITY), 0.0, 1e-15);
    }

    // ---------- L10 horizontalDistance：3-4-5 + 同 D 同讀數（baseY 項真的被驗）----------
    std::printf("[L10] horizontalDistance: 3-4-5 quadrature; same D => same reading\n");
    {
        Staff a; a.baseX = 3; a.baseY = 4;
        checkAbs("D(3,4) == 5", horizontalDistance(LevelSetup{}, a), 5.0, 1e-12);
        InstrumentParams Pi = P; Pi.collimationRad = degToRad(0.1);
        LevelSetup S;
        Staff b; b.baseX = 5; b.baseY = 0;
        checkAbs("reading(3,4) == reading(5,0)  (D drives error)",
                 measure(Pi, S, a, 0.0).reading, measure(Pi, S, b, 0.0).reading, 1e-12);
    }

    // ---------- L11 validate()：預設 true；壞參數 false ----------
    std::printf("[L11] validate(): defaults true; bad params/inputs false\n");
    {
        checkTrue("default P valid",     validate(InstrumentParams{}));
        checkTrue("default S valid",     validate(LevelSetup{}));
        checkTrue("default Staff valid", validate(Staff{}));
        InstrumentParams Pr = P; Pr.screwRadius = -1;       checkTrue("screwRadius<0 invalid", !validate(Pr));
        InstrumentParams Ps = P; Ps.bubbleSensRad = 0;      checkTrue("bubbleSens=0 invalid", !validate(Ps));
        InstrumentParams Pa = P; Pa.settingAccRad = 2 * P.compRangeRad; checkTrue("settingAcc>compRange invalid", !validate(Pa));
        InstrumentParams Pt = P; Pt.readPartial = 0.0005;   checkTrue("readPartial<readTol invalid", !validate(Pt));
        InstrumentParams Pn = P; Pn.collimationRad = qnan();checkTrue("NaN param invalid", !validate(Pn));
        LevelSetup Sn; Sn.screwTravel[0] = qnan();          checkTrue("NaN screwTravel invalid", !validate(Sn));
        LevelSetup Sd; Sd.screwPhiDeg[1] = Sd.screwPhiDeg[0]; checkTrue("duplicate phi invalid", !validate(Sd));
        Staff sl; sl.length = 0;                            checkTrue("staff length 0 invalid", !validate(sl));
    }

    // ---------- L12 三絲幾何視距：D 為推導值（非公式）；mid=(u+l)/2 ----------
    std::printf("[L12] three-hair stadia: D=K*s DERIVED (not formula); mid==(u+l)/2 at eps=0\n");
    {
        LevelSetup S; Staff st; st.baseX = 30; st.baseZ = 0;
        const ThreeHairReading h = trueThreeHair(P, S, st, 0.0);
        checkTrue("three-hair valid", h.valid);
        checkAbs("mid == (upper+lower)/2", h.mid, 0.5 * (h.upper + h.lower), 1e-12);
        checkAbs("K*(upper-lower) == D (derived)", stadiaDistance(P, h.upper, h.lower), 30.0, 1e-9);
        checkAbs("K*s == horizontalDistance (indep path)", stadiaDistance(P, h.upper, h.lower), horizontalDistance(S, st), 1e-9);
        // 多距離一致。
        for (real D : { 7.0, 23.0, 41.0 }) {
            Staff s2; s2.baseX = D;
            const ThreeHairReading hh = trueThreeHair(P, S, s2, 0.0);
            checkAbs((std::string("K*s==D @") + std::to_string((int)D)).c_str(), stadiaDistance(P, hh.upper, hh.lower), D, 1e-9);
        }
        // L12b 傾斜視線：上下絲射線幾何精確(vs 獨立 tan(θ±α))，但 K*s 對鉛直標尺高估 D(cos²θ；非恆等於 D)。
        InstrumentParams Pi = P; Pi.collimationRad = arcminToRad(30.0); // θ=losTilt=30'
        const real th = Pi.collimationRad, al = std::atan(1.0 / (2.0 * Pi.stadiaK));
        const ThreeHairReading hi = trueThreeHair(Pi, S, st, 0.0);      // st 在 D=30
        const real sExp = 30.0 * (std::tan(th + al) - std::tan(th - al));
        checkAbs("inclined: (upper-lower)==D*(tan(th+a)-tan(th-a))", hi.upper - hi.lower, sExp, 1e-12);
        const real inflate = stadiaDistance(Pi, hi.upper, hi.lower) - 30.0;
        checkTrue("inclined: K*s over-estimates D (cos^2; NOT exact)", inflate > 1e-3 && inflate < 4e-3,
                  std::string("inflate=") + std::to_string(inflate));
    }

    // ---------- L13 曲率折光（opt-in）：c(100)~0.683mm；中間法相消；預設關不變 ----------
    std::printf("[L13] curvature+refraction (opt-in): c(100m), middle-method cancels, default-off identical\n");
    {
        const real expC = (1.0 - P.refractionK) * 100.0 * 100.0 / (2.0 * P.earthRadius); // 獨立內聯算術(非循環)
        checkAbs("curvatureRefraction == inline formula", curvatureRefraction(P, 100.0), expC, 1e-15);
        InstrumentParams Pc = P; Pc.applyCurvatureRefraction = true;
        LevelSetup S; Staff st; st.baseX = 100; st.baseZ = 0;
        const real on  = measure(Pc, S, st, 0.0).reading;
        const real off = measure(P,  S, st, 0.0).reading;
        checkAbs("apply adds c (vs inline)", on - off, expC, 1e-15);
        checkAbs("default off == ideal (byte-identical)", off, 1.5, 1e-12);
        // 中間法：前後視等距 => c 在高差中相消。
        InstrumentParams P0 = Pc; P0.applyCurvatureRefraction = false;
        Staff bs; bs.baseX = 50; Staff fs; fs.baseX = 50; fs.baseZ = 0.4;
        auto dh = [&](const InstrumentParams& q) {
            return measure(q, S, bs, 0.0).reading - measure(q, S, fs, 0.0).reading;
        };
        checkAbs("equal-D: curvature cancels in dh", dh(Pc), dh(P0), 1e-12);
    }

    // ---------- L14 多站閉合 + 平差 ----------
    std::printf("[L14] multi-station loop closure + adjustment (平差)\n");
    {
        std::vector<LevelLeg> legs = {
            { 1.2, 1.3, 0.5, 1 }, { 1.5, 1.4, 0.3, 1 }, { 1.1, 1.1, 0.2, 1 },
        };
        LoopResult ok = closeLoop(legs, 12.0, true);
        checkAbs("perfect loop misclosure 0", ok.misclosureM, 0.0, 1e-12);
        // 注入 +9mm 閉合差於最後一段 fs。
        std::vector<LevelLeg> bad = legs; bad[2].fs += 0.009;
        LoopResult br = closeLoop(bad, 12.0, true);
        checkAbs("injected misclosure -9mm", br.misclosureM, -0.009, 1e-12);
        real sumAdj = 0; for (real d : br.adjustedDh) sumAdj += d;
        checkAbs("adjusted dh sum to 0", sumAdj, 0.0, 1e-12);
        checkAbs("by-distance leg0 share +4.5mm", br.adjustedDh[0] - br.legDh[0], 0.0045, 1e-12);
        checkAbs("allowable = C*sqrt(L) = 12mm", br.allowableMm, 12.0, 1e-12);
        checkTrue("9mm within 12mm tolerance", br.withinTolerance);
        std::vector<LevelLeg> big = legs; big[2].fs += 0.015;     // 15mm > 12mm
        checkTrue("15mm NOT within tolerance", !closeLoop(big, 12.0, true).withinTolerance);
        // by-station 分支(依 stationCount 加權；與 by-distance 數字無關)。
        std::vector<LevelLeg> sl = { {1.2,1.3,0,2}, {1.5,1.4,0,1}, {1.1,1.1,0,3} };
        sl[2].fs += 0.009;                                       // 注入 -9mm
        LoopResult bsr = closeLoop(sl, 12.0, false);             // byDistance=false
        checkAbs("by-station leg0 share (2/6)", bsr.adjustedDh[0] - bsr.legDh[0], 0.009 * 2.0 / 6.0, 1e-12);
        checkAbs("by-station leg1 share (1/6)", bsr.adjustedDh[1] - bsr.legDh[1], 0.009 * 1.0 / 6.0, 1e-12);
        checkAbs("by-station leg2 share (3/6)", bsr.adjustedDh[2] - bsr.legDh[2], 0.009 * 3.0 / 6.0, 1e-12);
        checkAbs("by-station allowable = C*sqrt(6)", bsr.allowableMm, 12.0 * std::sqrt(6.0), 1e-12);
        checkTrue("by-station valid", bsr.valid);
        // 退化權重(byDistance 但距離全 0)→ 退回等權 1/n + valid=false，Σ 仍 0。
        std::vector<LevelLeg> zw = { {1.2,1.3,0,1}, {1.5,1.4,0,1}, {1.1,1.109,0,1} };
        LoopResult zr = closeLoop(zw, 12.0, true);
        real zsum = 0; for (real d : zr.adjustedDh) zsum += d;
        checkTrue("zero-weight => valid=false", !zr.valid);
        checkAbs("zero-weight => Sigma adjusted still 0 (1/n)", zsum, 0.0, 1e-12);
        // 空路線 => valid=false。
        LoopResult er = closeLoop({}, 12.0, true);
        checkTrue("empty loop => valid=false", !er.valid);
        checkTrue("empty loop => legDh empty", er.legDh.empty());
        // 非物理逐段權重 => valid=false（不得回傳 sign-inverted / 爆量分配）。
        std::vector<LevelLeg> neg = { {1.2,1.3,-1.0,1}, {1.5,1.4,3.0,1}, {1.1,1.109,1.0,1} };
        checkTrue("negative distanceKm => valid=false", !closeLoop(neg, 12.0, true).valid);
        std::vector<LevelLeg> negS = { {1.2,1.3,0,-1}, {1.5,1.4,0,1}, {1.1,1.109,0,1} };
        checkTrue("negative stationCount => valid=false", !closeLoop(negS, 12.0, false).valid);
        std::vector<LevelLeg> nanL = { {1.2,qnan(),0.5,1}, {1.5,1.4,0.3,1}, {1.1,1.1,0.2,1} };
        checkTrue("non-finite leg field => valid=false", !closeLoop(nanL, 12.0, true).valid);
        // 近抵銷混號（負距離）也被擋下，且退回等權 => 分配有界。
        std::vector<LevelLeg> cancel = { {1.2,1.3,1.0,1}, {1.5,1.4,-0.9999999,1}, {1.1,1.109,0.0,1} };
        LoopResult cr = closeLoop(cancel, 12.0, true);
        bool cbound = true; for (size_t i = 0; i < cr.adjustedDh.size(); ++i) if (std::fabs(cr.adjustedDh[i] - cr.legDh[i]) > 0.01) cbound = false;
        checkTrue("near-cancelling weights => valid=false + bounded share", !cr.valid && cbound);
        // valid=true 時所有 adjustedDh 必為有限。
        bool allFin = true; for (real d : br.adjustedDh) if (!std::isfinite(d)) allFin = false;
        checkTrue("valid=true => all adjustedDh finite", br.valid && allFin);
    }

    // ---------- L15 兩樁法回推 i 角（forward 的逆）----------
    std::printf("[L15] two-peg test: recover i-angle (inverse of forward D*tan i)\n");
    {
        InstrumentParams Pi = P; Pi.collimationRad = arcsecToRad(30.0);
        LevelSetup S;
        auto dh = [&](real Dbs, real Dfs) {
            Staff bs; bs.baseX = Dbs; bs.baseZ = 0.0; Staff fs; fs.baseX = Dfs; fs.baseZ = 0.3;
            return measure(Pi, S, bs, 0.0).reading - measure(Pi, S, fs, 0.0).reading;
        };
        const real recovered = recoverIAngleTwoPeg(dh(20, 20), dh(40, 10), 40, 10);
        // 物理交叉驗證：dh 由完整 measure() 幾何(i 角隨距離放大)產生 → 回推得儀器 i 角。
        checkAbs("recovered i == input i (via measure geometry)", recovered, Pi.collimationRad, 1e-12);
        // 代數往返(atan∘tan)一致性 + 守衛。
        const real ik = arcsecToRad(45.0);
        const real dhEq = 0.3, dhUneq = 0.3 + 30.0 * std::tan(ik);
        checkAbs("algebra round-trip recover==i_known", recoverIAngleTwoPeg(dhEq, dhUneq, 40, 10), ik, 1e-15);
        checkTrue("equal-D (Dbs==Dfs) => NaN guard", !std::isfinite(recoverIAngleTwoPeg(0.3, 0.3, 20, 20)));
        checkTrue("NaN input => non-finite (guarded)", !std::isfinite(recoverIAngleTwoPeg(qnan(), 0.3, 40, 10)));
        checkTrue("Inf distance => non-finite (guarded)", !std::isfinite(recoverIAngleTwoPeg(0.3, 0.3, INFINITY, 10)));
    }

    // ---------- L16 onStaff：落在 [0, 尺長] 內/外 ----------
    std::printf("[L16] onStaff: reading within [0, staff.length]\n");
    {
        LevelSetup S;
        Staff normal; normal.baseX = 10; normal.baseZ = 0; normal.length = 4.0;
        checkTrue("on-staff reading => onStaff true", measure(P, S, normal, 0.0).onStaff);
        Staff below; below.baseX = 10; below.baseZ = 2.0; below.length = 4.0; // 尺底高於視線 => 讀數<0
        const ReadingResult rb = measure(P, S, below, 0.0);
        checkTrue("below foot => onStaff false, still readable", !rb.onStaff && rb.readable);
        LevelSetup Shigh; Shigh.opticsHeightZ = 6.0;
        const ReadingResult ra = measure(P, Shigh, normal, 0.0);                // 讀數 6 > 尺長 4
        checkTrue("above top => onStaff false", !ra.onStaff);
        LevelSetup Sedge; Sedge.opticsHeightZ = 4.0;                            // 讀數剛好 4.0
        checkTrue("reading == length => onStaff true (inclusive)", measure(P, Sedge, normal, 0.0).onStaff);
        Staff zero; zero.baseX = 10; zero.baseZ = 1.5; zero.length = 4.0;       // opticsZ==baseZ => 讀數 0
        const ReadingResult rz = measure(P, S, zero, 0.0);
        checkAbs("reading == 0 (opticsZ==baseZ)", rz.reading, 0.0, 1e-12);
        checkTrue("reading == 0 => onStaff true (lower inclusive)", rz.onStaff);
    }

    std::printf("\n%s  (failures=%d)\n", g_fail == 0 ? "ALL PASS" : "FAILURES", g_fail);
    return g_fail == 0 ? 0 : 1;
}
