// v3.5.1 PMC-DUP-01 — shared PMC-extrusion helpers. v3.5.0 duplicated MemberLocalAxes
// + Corner offsets + ring count across 6 actor TUs; this header consolidates them so
// a single behavioural change (e.g., bumping the near-vertical threshold from 0.95f)
// propagates to every renderer without silent divergence.
//
// HEADER-ONLY. Private/ scope (no FRAMECOREUE_API export). Inline so each TU still
// inlines the helper at the call site — no DLL boundary, no link-time cost.

#pragma once

#include "CoreMinimal.h"

namespace FrameCorePMC
{
    // v3.5 contract: 11 rings per member extrusion -> 44 vertices, 252 indices (10 side
    // segments * 4 faces * 2 tris + 2 end caps * 2 tris). Test fixtures hard-code these
    // counts (FFrameCoreUEActorStressMeshTest + FFrameCoreUEDeformedShapeSectionCountTest
    // assert vertex == 44 / index == 252). Bumping NRings requires syncing those tests.
    inline constexpr int32 kRings = 11;

    // Build an orthonormal {axis, refY, refZ} frame from a member axis. refY is the
    // projection of global up onto the plane perpendicular to axis; falls back to
    // global X when the member is within ~18 degrees of vertical (dot >= 0.95).
    inline void MemberLocalAxes(const FVector& InAxis,
                                FVector& OutAxis,
                                FVector& OutRefY,
                                FVector& OutRefZ)
    {
        OutAxis = InAxis.GetSafeNormal();
        if (OutAxis.IsNearlyZero())
        {
            OutAxis = FVector::ForwardVector;
            OutRefY = FVector::RightVector;
            OutRefZ = FVector::UpVector;
            return;
        }
        const FVector GlobalUp = FVector::UpVector;
        const float dotUp = FMath::Abs(FVector::DotProduct(OutAxis, GlobalUp));
        const FVector RefSeed = (dotUp > 0.95f) ? FVector::ForwardVector : GlobalUp;
        OutRefZ = FVector::CrossProduct(OutAxis, RefSeed).GetSafeNormal();
        OutRefY = FVector::CrossProduct(OutRefZ, OutAxis).GetSafeNormal();
    }

    // Corner offset for a ring vertex (CornerIdx 0..3). Convention:
    //   0 = +RefY +RefZ, 1 = +RefY -RefZ, 2 = -RefY -RefZ, 3 = -RefY +RefZ
    // Matches v3.3 FrameCoreStressFieldActor; tests rely on this ordering.
    inline FVector CornerOffset(int32 CornerIdx,
                                const FVector& RefY, const FVector& RefZ,
                                float HalfW, float HalfD)
    {
        switch (CornerIdx)
        {
            case 0: return  RefY * HalfW + RefZ * HalfD;
            case 1: return  RefY * HalfW - RefZ * HalfD;
            case 2: return -RefY * HalfW - RefZ * HalfD;
            case 3: return -RefY * HalfW + RefZ * HalfD;
        }
        return FVector::ZeroVector;
    }

    // v3.6 U-11 — cubic Hermite interpolation between two endpoints with tangents.
    //   h(t) = (2t^3 - 3t^2 + 1) P0  +  (t^3 - 2t^2 + t) M0
    //        + (-2t^3 + 3t^2)   P1  +  (t^3 - t^2)       M1
    // t in [0,1]. Tangents M0 / M1 are displacement-rate at each end (typically
    // member-axis-aligned scaled by length and the rotation amplitude).
    inline FVector HermitePoint(float t,
                                const FVector& P0, const FVector& M0,
                                const FVector& P1, const FVector& M1)
    {
        const float t2 = t * t;
        const float t3 = t2 * t;
        const float h00 =  2.f * t3 - 3.f * t2 + 1.f;
        const float h10 =        t3 - 2.f * t2 + t;
        const float h01 = -2.f * t3 + 3.f * t2;
        const float h11 =        t3 -       t2;
        return h00 * P0 + h10 * M0 + h01 * P1 + h11 * M1;
    }
}
