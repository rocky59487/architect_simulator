#pragma once
#include "FrameCore/FrameTypes.h"
#include "FrameCore/SolveResult.h"
#include "FrameCore/StressKernel.h"
#include <vector>

// Visualisation stress-field post-process. Pure function of (FrameModel, SolveResult);
// no Eigen, no allocations beyond the returned vectors, no thread state. The numerical
// kernel is StressKernel.h so the values are bit-identical to ElasticAllowable's D/C
// screen (F70 interlock).
//
// Member sampling: N(x), V(x), M(x) reconstructed analytically from MemberEndForces
// and MemberUDL (axial linear, transverse shear linear, bending quadratic — EB / Timoshenko-
// equivalent at the centreline; transverse shear flexibility does NOT change the bending
// moment distribution). Each member yields `samplesPerSpan` (default 11) samples along x = 0..L.
//
// Shell sampling: every shell yields exactly 5 sample points (centre + 4 corners) on
// EACH of the Top / Bottom fibers, matching ElasticAllowable::checkShellSurface's
// kc = -1..3 / face = 0..1 traversal so the two cannot drift.

namespace frame {

struct FrameModel;

// One sample along a member's axis (LOCAL coordinates, fiber sigmas compression-positive).
struct MemberStressSample {
    real x;                // arc length from end-i, 0 <= x <= L
    real sigmaFiberTopY;   // sigma at local +y face mid (TopY fiber)
    real sigmaFiberBotY;   // sigma at local -y face mid (BotY fiber)
    real sigmaFiberPlusZ;  // sigma at local +z face mid (PlusZ fiber)
    real sigmaFiberMinusZ; // sigma at local -z face mid (MinusZ fiber)
    real sigmaCompMax;     // worst-corner compressive demand magnitude (matches ElasticAllowable sComp)
    real sigmaTensMax;     // worst-corner tensile  demand magnitude    (matches ElasticAllowable sTens)
    real tauShear;         // peak transverse shear stress (k * V/A)
    real tauTorsion;       // torsional shear at extreme fibre (T * c / J)
    // Raw internal forces at x (LOCAL, for downstream BMD/SFD visualisation).
    real N, Vy, Vz, T, My, Mz;
};

struct MemberStressTrace {
    int memberIdx = -1;                       // index into FrameModel::members
    int memberId  = 0;                        // mirrors model.members[memberIdx].id
    std::vector<MemberStressSample> samples;  // length = samplesPerSpan (default 11)
};

// One shell sample point (centre or one of 4 corners), on ONE fiber face (Top or Bot).
struct ShellStressPoint {
    int  cornerIdx;   // -1 = centre; 0..3 = corner (matches ShellQuad::n order)
    real sigmaXX;
    real sigmaYY;
    real tauXY;
    real sigma1;      // major principal stress
    real sigma2;      // minor principal stress
    real vonMises;
    real thetaRad;    // angle of sigma1 axis from local +x
};

// All sample points for ONE shell on ONE fiber face.
struct ShellStressLayer {
    int        shellIdx = -1;                 // index into FrameModel::shells
    int        shellId  = 0;
    ShellLayer layer    = ShellLayer::Top;
    ShellStressPoint center;                  // cornerIdx = -1
    ShellStressPoint corners[4];              // cornerIdx = 0..3
};

// Aggregate field over the whole model. Members and shells are paired with the
// model's element lists in index order (skipping inactive elements -- those leave
// no entry in the field, just like ElasticAllowable's worstUtilization).
struct StressField {
    std::vector<MemberStressTrace> members;
    std::vector<ShellStressLayer>  shellsTop;
    std::vector<ShellStressLayer>  shellsBot;

    // Worst values across the whole field (>= 0). 0 if no screenable element.
    real globalMaxFiberSigma  = 0;    // max(sigmaCompMax, sigmaTensMax) over members
    real globalMaxVonMises    = 0;    // max vM across all shell sample points
    // v3.3 BREAKING (U-07): pointers below identify the governing element by its
    // INTERNAL INDEX into FrameModel::members / ::shells, not by its user-assigned
    // .id. Pre-v3.3 these fields stored the user id and used 0 for "no governing",
    // which silently collided with a legitimate element whose id == 0. See
    // docs/specs/S11_v3.3_schema_migration.md for the migration rationale.
    int  governingMemberIdx   = -1;   // -1 if no member governs; else index into FrameModel::members
    int  governingShellIdx    = -1;   // -1 if no shell  governs; else index into FrameModel::shells
    ShellLayer governingShellLayer = ShellLayer::Top;
    int  governingShellCorner = -1;
};

// Compute the stress field for a solved model. `samplesPerSpan` controls the number of
// along-axis samples per member (must be >= 2; default 11; sampled at x = k*L/(n-1)).
// Inactive members / shells are skipped (mirroring worstUtilization's contract).
[[nodiscard]] FRAMECORE_API StressField computeStressField(const FrameModel& model,
                                                           const SolveResult& sr,
                                                           int samplesPerSpan = 11);

} // namespace frame
