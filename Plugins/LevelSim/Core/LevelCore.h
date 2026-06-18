// LevelCore.h — 水準儀模擬器測量核心（pure C++17，零 UE / 零 Eigen）
//
// 對應「水準儀模擬器_設計文件.md」：§2/§12 真值與誤差模型、§4 數學、§7 oracle。
// 設計紀律（沿用 FrameCore）：公開 API 只用 POD/std 型別；物理可獨立 standalone 測試；
// 每個能力都有一條獨立數值 oracle（見 Standalone/level_gate.cpp）；不過度宣稱。
//
// 誠實邊界（§0 原則 5）：進讀數真值的誤差 = 補償器殘餘安平誤差、視準軸 i 角、視差未消。
//   地球曲率/折光為 **opt-in**（預設關，關閉時與不做時逐位元一致）。標尺永遠鉛直。
//   殘餘安平誤差以「各向同性的 LoS 殘餘界」建模（非標尺方位投影；見 §12 註與 measure() 說明）。
//
// 2026-06-08 對抗式審核（27 確認發現）後強化：修正圓氣泡方向反置（critical）、
//   加 validate()/有限性·退化守衛、真正獨立 oracle、三絲幾何視距、曲率折光 opt-in、
//   多站閉合+平差、兩樁法回推 i 角、標尺長度/onStaff、出範圍讀數改 NaN sentinel。
#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace levelsim {

using real = double;

constexpr real kPi = 3.14159265358979323846;
inline real degToRad(real d)    { return d * kPi / 180.0; }
inline real arcminToRad(real m) { return degToRad(m / 60.0); }
inline real arcsecToRad(real s) { return degToRad(s / 3600.0); }
inline real qnan()              { return std::numeric_limits<real>::quiet_NaN(); }
inline bool fin(real x)         { return std::isfinite(x); }

// ---- 極小 3D 向量（零外部依賴）----
struct LVec3 { real x = 0, y = 0, z = 0; };
inline LVec3 operator-(const LVec3& a, const LVec3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline LVec3 operator+(const LVec3& a, const LVec3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline LVec3 operator*(const LVec3& a, real s)         { return { a.x * s, a.y * s, a.z * s }; }
inline real  dot(const LVec3& a, const LVec3& b)       { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline LVec3 cross(const LVec3& a, const LVec3& b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
inline real  length(const LVec3& a) { return std::sqrt(dot(a, a)); }

// ---- 儀器參數（§2 表 + §12）。單位：長度 m、角度 rad ----
struct InstrumentParams {
    real screwRadius    = 0.05;             // 腳螺旋分布半徑（>0）
    real bubbleSensRad  = arcminToRad(2.0); // 圓水準器每 1 格對應傾角（>0）
    real compRangeRad   = arcminToRad(15.0);// 補償器工作範圍 ±15′（外部驗證）
    real settingAccRad  = arcsecToRad(0.4); // 安平精度 ±0.4″（修 C1；須 <= compRangeRad）
    real collimationRad = 0.0;              // 視準軸 i 角（固有，signed）
    real stadiaK        = 100.0;            // 視距常數（>0）
    real parallaxCoef   = 0.002;            // 視差抖動係數（>=0；m 抖動 / m 失焦）
    real readTol        = 0.001;            // ±1 mm 滿分（>=0）
    real readPartial    = 0.003;            // ±3 mm 之外 0 分（> readTol）
    real bubbleRoughDiv = 1.0;              // 進內圈（S3 過關，>0）
    real bubbleFineDiv  = 0.1;              // 置中（S4 過關，0<.<=rough）
    // 地球曲率 + 大氣折光（opt-in；預設關 → 與不做逐位元一致）
    bool applyCurvatureRefraction = false;
    real refractionK    = 0.13;             // 大氣折光係數 k
    real earthRadius    = 6371000.0;        // 地球半徑 R（m）
};

// ---- 站況輸入。儀器（物鏡中心）在世界原點 (0,0,opticsHeightZ) ----
struct LevelSetup {
    real screwTravel[3] = { 0, 0, 0 };               // 三腳螺旋升降
    real screwPhiDeg[3] = { 90.0, 210.0, 330.0 };    // 三螺旋方位（度，約 120° 分布；不可重複）
    real opticsHeightZ  = 1.5;                        // 物鏡中心 world Z
    real focusObjective = 0.0;                        // 物鏡對焦距離設定（消視差用）
};

// 標尺：底部 world 座標，假設永遠鉛直（+Z）；length = E 字分劃尺長度（m）。
struct Staff { real baseX = 10.0, baseY = 0.0, baseZ = 0.0; real length = 4.0; };

// ---- 結果型別 ----
struct TiltState   { real pitchRad = 0, rollRad = 0, magRad = 0; bool valid = true; };
struct BubbleState { real offX = 0, offY = 0, magDiv = 0; bool roughLevel = false, fineLevel = false; };
struct SightState  { bool inRange = true; bool readable = true; real losTiltRad = 0; };
// 完整一次量測結果。reading 在 !readable 時為 NaN（sentinel；修出範圍 1e17 假值）。
struct ReadingResult { real reading = 0; real distance = 0; bool valid = true; bool readable = true; bool onStaff = true; };
struct ThreeHairReading { real upper = 0, mid = 0, lower = 0; bool valid = false; }; // 三絲（選配/考試模式）
struct LevelLeg   { real bs = 0, fs = 0; real distanceKm = 0; int stationCount = 1; };
struct LoopResult { real misclosureM = 0; std::vector<real> legDh; std::vector<real> adjustedDh; bool withinTolerance = true; real allowableMm = 0; bool valid = true; }; // valid=false：空路線或退化權重(已退回等權)

// ---- 參數/輸入驗證（沿用 FrameCore validate() 文化）----
bool validate(const InstrumentParams& P);
bool validate(const LevelSetup& S);
bool validate(const Staff& st);
bool validate(const LevelLeg& leg);   // 有限 bs/fs/distanceKm + distanceKm>=0 + stationCount>=1

// ---- API（每個對應 §7 一條 oracle）----

// §4.1：三支點平面法線 → 傾角。退化幾何（共線/重合/零半徑）或非有限 → valid=false（不偽造 magRad=0）。
TiltState   tiltFromScrews(const InstrumentParams& P, const LevelSetup& S);

// §4.2：圓水準器氣泡。offset **浮向高側**（修正方向反置 critical）；magRad∈[0,π/2] 才視為置中。
BubbleState bubbleFromTilt(const InstrumentParams& P, const TiltState& t);

// §4.3 + §12：視線殘餘傾角。範圍內 losTilt = i角 + clamp(殘餘, ±settingAcc)；超範圍 readable=false。
SightState  sightTilt(const InstrumentParams& P, const TiltState& t, real residualSignedRad);

// 儀器（原點）到標尺的水平距離。
real horizontalDistance(const LevelSetup& S, const Staff& st);

// §4.4 + §12：完整真值量測。reading = (opticsZ + D·tanε) − baseZ（+ 可選曲率折光）。
//   readable=false（補償器超範圍/退化/非有限）⇒ reading=NaN。onStaff = reading∈[0, staff.length]。
ReadingResult measure(const InstrumentParams& P, const LevelSetup& S, const Staff& st, real residualSignedRad);

// 薄包裝（向後相容）：回中絲讀數，readableOut = valid && readable。
real trueReading(const InstrumentParams& P, const LevelSetup& S, const Staff& st,
                 real residualSignedRad, bool& readableOut);

// §4.4：三絲幾何視距。上/下絲＝望遠鏡內 ±α 固定夾角射線（α=atan(1/(2K))）各自與標尺求交。
//   水平視線時 D = K·(upper−lower)（推導結果，非由距離反推，避免循環論證 C2）；傾斜視線對鉛直標尺
//   嚴格為 H = K·s·cos²θ（θ=losTilt），K·s 略高估 D，但 |θ|≤compRange(~15′) ⇒ <0.002%，可忽略。
ThreeHairReading trueThreeHair(const InstrumentParams& P, const LevelSetup& S, const Staff& st, real residualSignedRad);

// 視距（薄便利；本版玩家只中絲）：D = K·(上絲 − 下絲)。
real stadiaDistance(const InstrumentParams& P, real upperHair, real lowerHair);

// 地球曲率 + 大氣折光合併修正 c = (1−k)·D²/(2R)（m）。中間法（前後視等距）會相消，同 i 角。
real curvatureRefraction(const InstrumentParams& P, real horizDist);

// §4.5：視差抖動（focusObjective==D ⇒ 0）。
real parallaxJitter(const InstrumentParams& P, const LevelSetup& S, real horizDist);

// 視線高法：H.I. = 已知高程 + 後視（BS）；待測高程 = H.I. − 前視（FS）。
real heightOfInstrument(real knownElevation, real backsight);
real pointElevation(real hi, real foresight);

// 多站閉合路線：misclosure = Σbs − Σfs（封閉路線理論 0）；平差分配（依距離或測站數）；容許 ±C·√L 或 ±C·√n。
LoopResult closeLoop(const std::vector<LevelLeg>& legs, real closureC, bool byDistance);

// 兩樁法回推視準軸 i 角：i = atan((dh_unequal − dh_equal) / (Dbs − Dfs))（forward D·tan(i) 的逆）。
real recoverIAngleTwoPeg(real dhEqual, real dhUnequal, real Dbs, real Dfs);

// 評分：±readTol 滿分(1.0)、±readPartial 之外 0、之間線性；非有限輸入 ⇒ 0。
real scoreReading(const InstrumentParams& P, real trueReadingM, real playerReadingM);

} // namespace levelsim
