// LevelCore.cpp — 水準儀模擬器測量核心實作。
// 真值與誤差模型見 設計文件 §12；每函式對應 §7 oracle（驗證在 Standalone/level_gate.cpp）。
// 2026-06-08 對抗式審核強化版（見 LevelCore.h 頂註）。
#include "LevelCore.h"

namespace levelsim {

// ---------------------------------------------------------------- 驗證
bool validate(const InstrumentParams& P) {
    if (!(fin(P.screwRadius) && P.screwRadius > 0)) return false;
    if (!(fin(P.bubbleSensRad) && P.bubbleSensRad > 0)) return false;
    if (!(fin(P.compRangeRad) && P.compRangeRad > 0)) return false;
    if (!(fin(P.settingAccRad) && P.settingAccRad >= 0 && P.settingAccRad <= P.compRangeRad)) return false;
    if (!fin(P.collimationRad)) return false;
    if (!(fin(P.stadiaK) && P.stadiaK > 0)) return false;
    if (!(fin(P.parallaxCoef) && P.parallaxCoef >= 0)) return false;
    if (!(fin(P.readTol) && P.readTol >= 0)) return false;
    if (!(fin(P.readPartial) && P.readPartial > P.readTol)) return false;
    if (!(fin(P.bubbleRoughDiv) && P.bubbleRoughDiv > 0)) return false;
    if (!(fin(P.bubbleFineDiv) && P.bubbleFineDiv > 0 && P.bubbleFineDiv <= P.bubbleRoughDiv)) return false;
    // R-AUDIT C-01: refractionK is the atmospheric refraction coefficient k of the
    // curvature+refraction correction c = (1−k)·D²/(2R). Physical k is always > 0
    // (typical 0.13); k ≤ 0 would invert/over-amplify the correction. Reject up front.
    if (!(fin(P.refractionK) && P.refractionK > 0.0 && P.refractionK < 1.0 && fin(P.earthRadius) && P.earthRadius > 0)) return false;
    return true;
}

bool validate(const LevelSetup& S) {
    if (!fin(S.opticsHeightZ) || !fin(S.focusObjective)) return false;
    for (int i = 0; i < 3; ++i)
        if (!fin(S.screwTravel[i]) || !fin(S.screwPhiDeg[i])) return false;
    for (int i = 0; i < 3; ++i)                       // 拒絕重複方位（退化幾何）
        for (int j = i + 1; j < 3; ++j) {
            real d = std::fmod(std::fabs(S.screwPhiDeg[i] - S.screwPhiDeg[j]), 360.0);
            if (d < 1e-6 || d > 360.0 - 1e-6) return false;
        }
    return true;
}

bool validate(const Staff& st) {
    return fin(st.baseX) && fin(st.baseY) && fin(st.baseZ) && fin(st.length) && st.length > 0;
}

// ---------------------------------------------------------------- 三腳螺旋 → 傾斜
TiltState tiltFromScrews(const InstrumentParams& P, const LevelSetup& S) {
    TiltState t;
    if (!validate(P) || !validate(S)) { t.valid = false; return t; }

    LVec3 pts[3];
    for (int i = 0; i < 3; ++i) {
        const real phi = degToRad(S.screwPhiDeg[i]);
        pts[i] = LVec3{ P.screwRadius * std::cos(phi),
                        P.screwRadius * std::sin(phi),
                        S.screwTravel[i] };
    }
    const LVec3 raw  = cross(pts[1] - pts[0], pts[2] - pts[0]); // 未正規化法線
    const real  area = length(raw);                            // ∝ 三支點三角形面積
    const real  e1   = length(pts[1] - pts[0]);
    const real  e2   = length(pts[2] - pts[0]);
    const real  scale = std::max(e1, e2);
    // 退化幾何（共線/重合/零半徑）：面積遠小於邊長平方 → 不可用平面，不偽造 magRad=0。
    if (!(area > 1e-12 * std::max(scale * scale, (real)1e-30))) { t.valid = false; return t; }

    LVec3 n = raw * (1.0 / area);
    if (n.z < 0.0) n = n * -1.0;                  // M1：法線朝上守衛
    t.rollRad  = std::atan2(n.x, n.z);            // 對 X 軸
    t.pitchRad = std::atan2(n.y, n.z);            // 對 Y 軸
    // 傾角量用 atan2(水平分量, 鉛直分量)（近水平穩定，避免 acos 的 ulp 放大）。
    t.magRad   = std::atan2(std::sqrt(n.x * n.x + n.y * n.y), n.z);
    t.valid    = true;
    return t;
}

// ---------------------------------------------------------------- 圓水準器氣泡
BubbleState bubbleFromTilt(const InstrumentParams& P, const TiltState& t) {
    BubbleState b;
    if (!t.valid || P.bubbleSensRad <= 0) return b; // 退化 → 不視為置中（roughLevel/fineLevel 保持 false）
    // R-AUDIT C-02: tiltFromScrews validates roll/pitch, but a caller may construct a
    // hand-rolled TiltState directly; guard against non-finite roll/pitch so we never
    // emit ±∞ in offX/offY/magDiv.
    if (!fin(t.rollRad) || !fin(t.pitchRad) || !fin(t.magRad)) return b;

    const real denom = std::tan(P.bubbleSensRad);
    b.magDiv = std::tan(t.magRad) / denom;          // 偏移量（格）；magRad>π/2 → tan<0 → magDiv<0
    // 方向＝向上法線水平分量的反向（指向高側；修正方向反置 critical）。用 (tan roll, tan pitch)
    // ∝ (n.x, n.y) 取**精確梯度方位**——roll/pitch 角向量本身的方位有 O(tilt³) 非線性偏差。
    const real hx = std::tan(t.rollRad), hy = std::tan(t.pitchRad); // = n.x/n.z, n.y/n.z
    const real horiz = std::sqrt(hx * hx + hy * hy);                // = tan(magRad)
    if (horiz > 0.0) {
        b.offX = -(hx / horiz) * b.magDiv;
        b.offY = -(hy / horiz) * b.magDiv;
    }
    // magDiv<0（magRad>π/2）不得判為置中。
    b.roughLevel = (b.magDiv >= 0.0 && b.magDiv <= P.bubbleRoughDiv);
    b.fineLevel  = (b.magDiv >= 0.0 && b.magDiv <= P.bubbleFineDiv);
    return b;
}

// ---------------------------------------------------------------- 視線殘餘傾角
SightState sightTilt(const InstrumentParams& P, const TiltState& t, real residualSignedRad) {
    SightState s;
    if (!t.valid || !fin(residualSignedRad)) { s.inRange = false; s.readable = false; s.losTiltRad = qnan(); return s; }
    s.inRange = (t.magRad <= P.compRangeRad);
    if (!s.inRange) { s.readable = false; s.losTiltRad = qnan(); return s; } // 出範圍：不可讀，無偽造傾角
    const real residual = std::clamp(residualSignedRad, -P.settingAccRad, P.settingAccRad);
    s.readable   = true;
    s.losTiltRad = P.collimationRad + residual;     // i 角在 clamp 之外（i 角可大、殘餘有界）
    return s;
}

real horizontalDistance(const LevelSetup& /*S*/, const Staff& st) {
    return std::sqrt(st.baseX * st.baseX + st.baseY * st.baseY); // 儀器在原點
}

real curvatureRefraction(const InstrumentParams& P, real horizDist) {
    if (!(fin(P.refractionK) && P.refractionK < 1.0 && fin(P.earthRadius) && P.earthRadius > 0 && fin(horizDist)))
        return qnan();                              // 直接呼叫也要守衛（不靠 validate）
    return (1.0 - P.refractionK) * horizDist * horizDist / (2.0 * P.earthRadius);
}

// ---------------------------------------------------------------- 完整量測
ReadingResult measure(const InstrumentParams& P, const LevelSetup& S, const Staff& st, real residualSignedRad) {
    ReadingResult r;
    if (!validate(P) || !validate(S) || !validate(st) || !fin(residualSignedRad)) {
        r.valid = false; r.readable = false; r.onStaff = false; r.reading = qnan(); r.distance = qnan();
        return r;
    }
    const TiltState  t = tiltFromScrews(P, S);
    const SightState s = sightTilt(P, t, residualSignedRad);
    r.valid    = t.valid;
    r.readable = t.valid && s.readable;
    r.distance = horizontalDistance(S, st);
    if (!r.readable) { r.reading = qnan(); r.onStaff = false; return r; } // sentinel：不回傳偽造值

    real reading = (S.opticsHeightZ + r.distance * std::tan(s.losTiltRad)) - st.baseZ;
    if (P.applyCurvatureRefraction) reading += curvatureRefraction(P, r.distance);
    r.reading = reading;
    r.onStaff = (reading >= 0.0 && reading <= st.length); // 視線須落在標尺面 [0, 尺長]
    return r;
}

real trueReading(const InstrumentParams& P, const LevelSetup& S, const Staff& st,
                 real residualSignedRad, bool& readableOut) {
    const ReadingResult r = measure(P, S, st, residualSignedRad);
    readableOut = r.readable;
    return r.reading;
}

// ---------------------------------------------------------------- 三絲幾何視距
ThreeHairReading trueThreeHair(const InstrumentParams& P, const LevelSetup& S, const Staff& st, real residualSignedRad) {
    ThreeHairReading h;
    const ReadingResult mid = measure(P, S, st, residualSignedRad);
    if (!mid.readable) { h.valid = false; return h; }

    const TiltState  t = tiltFromScrews(P, S);
    const SightState s = sightTilt(P, t, residualSignedRad);
    const real D     = mid.distance;
    const real alpha = std::atan(1.0 / (2.0 * P.stadiaK)); // 上/下絲對中絲的半夾角
    const real cr    = P.applyCurvatureRefraction ? curvatureRefraction(P, D) : 0.0;
    const real base  = S.opticsHeightZ - st.baseZ + cr;
    h.upper = base + D * std::tan(s.losTiltRad + alpha); // 上絲射線：losTilt + α
    h.mid   = mid.reading;
    h.lower = base + D * std::tan(s.losTiltRad - alpha); // 下絲射線：losTilt − α
    h.valid = true;
    return h;
}

real stadiaDistance(const InstrumentParams& P, real upperHair, real lowerHair) {
    return P.stadiaK * (upperHair - lowerHair);
}

real parallaxJitter(const InstrumentParams& P, const LevelSetup& S, real horizDist) {
    return P.parallaxCoef * std::fabs(S.focusObjective - horizDist);
}

real heightOfInstrument(real knownElevation, real backsight) { return knownElevation + backsight; }
real pointElevation(real hi, real foresight)                 { return hi - foresight; }

// ---------------------------------------------------------------- 多站閉合 + 平差
bool validate(const LevelLeg& leg) {
    return fin(leg.bs) && fin(leg.fs) && fin(leg.distanceKm) && leg.distanceKm >= 0 && leg.stationCount >= 1;
}

LoopResult closeLoop(const std::vector<LevelLeg>& legs, real closureC, bool byDistance) {
    LoopResult r;
    if (legs.empty()) { r.valid = false; r.withinTolerance = true; return r; } // 空路線：無從閉合

    bool allLegsValid = true;
    real sumBs = 0, sumFs = 0, sumKm = 0;
    int  sumStations = 0;
    r.legDh.reserve(legs.size());
    for (const auto& leg : legs) {
        if (!validate(leg)) allLegsValid = false;   // 非有限/負距離/stationCount<1
        r.legDh.push_back(leg.bs - leg.fs);
        sumBs += leg.bs; sumFs += leg.fs;
        sumKm += leg.distanceKm; sumStations += leg.stationCount;
    }
    r.misclosureM = sumBs - sumFs;                  // 封閉路線理論應為 0

    const real totalW = byDistance ? sumKm : (real)sumStations;
    const bool degenerateW = !(totalW > 0);         // 權重總和非正（如 byDistance 但未填距離）
    r.valid = allLegsValid && !degenerateW;         // 任一問題（非物理權重/非有限/零權重）⇒ 不可信
    const bool useEqual = !r.valid;                 // 退回等權 1/n：有界、且仍確保 Σ adjusted = 0
    const real invN = 1.0 / (real)legs.size();
    r.adjustedDh.resize(legs.size());
    for (size_t i = 0; i < legs.size(); ++i) {
        const real w = byDistance ? legs[i].distanceKm : (real)legs[i].stationCount;
        const real share = useEqual ? invN : (w / totalW);
        r.adjustedDh[i] = r.legDh[i] - r.misclosureM * share;
    }
    const real L = byDistance ? sumKm : (real)sumStations;
    r.allowableMm     = closureC * std::sqrt(std::max(L, (real)0.0)); // ±C·√L 或 ±C·√n
    r.withinTolerance = std::fabs(r.misclosureM) * 1000.0 <= r.allowableMm;
    return r;
}

real recoverIAngleTwoPeg(real dhEqual, real dhUnequal, real Dbs, real Dfs) {
    if (!(fin(dhEqual) && fin(dhUnequal) && fin(Dbs) && fin(Dfs))) return qnan(); // 非有限守衛
    const real denom = Dbs - Dfs;
    if (std::fabs(denom) < 1e-12) return qnan();     // 等距無法解 i 角
    return std::atan((dhUnequal - dhEqual) / denom);
}

real scoreReading(const InstrumentParams& P, real trueReadingM, real playerReadingM) {
    if (!fin(trueReadingM) || !fin(playerReadingM)) return 0.0; // 非有限 ⇒ 零分（不傳染 NaN）
    const real err = std::fabs(playerReadingM - trueReadingM);
    if (err <= P.readTol)     return 1.0;
    if (err >= P.readPartial) return 0.0;
    return 1.0 - (err - P.readTol) / (P.readPartial - P.readTol); // 線性遞減
}

} // namespace levelsim
