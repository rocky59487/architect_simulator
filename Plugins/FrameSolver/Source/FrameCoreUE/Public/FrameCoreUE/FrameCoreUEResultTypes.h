#pragma once

// v3.4 Phase 2 -- USTRUCT mirrors of frame::SolveResult and its sub-types, plus the
// post-process DemandSummary (worstUtilization) / ShellDemandSummary
// (worstShellUtilization) outputs. Pure consumer-side reflection -- no engine source
// touched. Precision is float32 (visualisation tolerance budget rel<1e-4).

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "FrameCoreUEResultTypes.generated.h"

// ---------------------------------------------------------------------------
// Failure mode enum -- mirrors frame::FailMode
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EFrameFailMode : uint8
{
    None          UMETA(DisplayName="None"),
    Crush         UMETA(DisplayName="Crush"),
    Tension       UMETA(DisplayName="Tension"),
    Shear         UMETA(DisplayName="Shear"),
    Bending       UMETA(DisplayName="Bending"),
    Torsion       UMETA(DisplayName="Torsion"),
    ShellVonMises UMETA(DisplayName="Shell Von Mises"),
};

// ---------------------------------------------------------------------------
// Per-node displacement / reaction
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameNodalDisplacement
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Displacement") int32 NodeIndex = -1;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Displacement") int32 NodeId    = 0;

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Displacement") float Ux = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Displacement") float Uy = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Displacement") float Uz = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Displacement") float Rx = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Displacement") float Ry = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Displacement") float Rz = 0.f;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameNodalReaction
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Reaction") int32 NodeIndex = -1;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Reaction") int32 NodeId    = 0;

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Reaction") float Fx = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Reaction") float Fy = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Reaction") float Fz = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Reaction") float Mx = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Reaction") float My = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Reaction") float Mz = 0.f;
    // True for any DOF that is constrained (Node::fixed[d] == true). At free DOFs the engine
    // writes NaN; the marshal layer collapses NaN to 0 here and clears the flag, so a BP
    // consumer can branch on bHasConstrainedDof without dealing with NaN.
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Reaction") bool bHasConstrainedDof = false;
};

// ---------------------------------------------------------------------------
// Per-member end forces + internal forces
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameMemberEndForces
{
    GENERATED_BODY()

    // Axial N is COMPRESSION-POSITIVE (matches engine FrameTypes.h convention).
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|MemberForce") float N  = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|MemberForce") float Vy = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|MemberForce") float Vz = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|MemberForce") float T  = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|MemberForce") float My = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|MemberForce") float Mz = 0.f;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameMemberInternalForces
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|MemberForce") int32 MemberIdx = -1;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|MemberForce") int32 MemberId  = 0;

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|MemberForce") FFrameMemberEndForces EndI;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|MemberForce") FFrameMemberEndForces EndJ;
};

// ---------------------------------------------------------------------------
// Member utilization (per-end and peak D/C)
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameDemandResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Utilization") float          Risk = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Utilization") EFrameFailMode Mode = EFrameFailMode::None;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Utilization") float SComp = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Utilization") float STens = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Utilization") float Tau   = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Utilization") float STor  = 0.f;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameMemberUtilization
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Utilization") int32 MemberIdx = -1;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Utilization") int32 MemberId  = 0;

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Utilization") FFrameDemandResult EndI;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Utilization") FFrameDemandResult EndJ;
    // Peak = max(EndI.Risk, EndJ.Risk) with the governing FailMode propagated.
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Utilization") FFrameDemandResult Peak;
};

// ---------------------------------------------------------------------------
// Shell internal forces + utilization
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameShellInternalForces
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|ShellForce") int32 ShellIdx = -1;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|ShellForce") int32 ShellId  = 0;

    // Centre values (averages for a linearly-varying field).
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|ShellForce") float Mxx = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|ShellForce") float Myy = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|ShellForce") float Mxy = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|ShellForce") float Qx  = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|ShellForce") float Qy  = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|ShellForce") float Nxx = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|ShellForce") float Nyy = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|ShellForce") float Nxy = 0.f;

    // Per-corner bending (length 4 each, matching ShellQuad::n CCW order). For a DKQ plate
    // these hold Gauss-point values; for MITC4 they're corner extrapolations.
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|ShellForce") TArray<float> MxxCorners;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|ShellForce") TArray<float> MyyCorners;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|ShellForce") TArray<float> MxyCorners;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameShellUtilization
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Utilization") int32 ShellIdx = -1;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Utilization") int32 ShellId  = 0;

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Utilization") float Risk    = 0.f;
    // -1 == centre; 0..3 == corner index (matches ShellQuad::n CCW).
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Utilization") int32 Corner  = -1;
    // true == top face (+bending side), false == bottom face.
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Utilization") bool  bTop    = true;
};

// ---------------------------------------------------------------------------
// DemandSummary -- worstUtilization + worstShellUtilization aggregate
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameDemandSummary
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Summary") float          MaxDC          = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Summary") float          SafetyFactor   = 0.f;
    // Internal index of the governing member (-1 == no governing member). The user-assigned
    // MemberId is GoverningMemberId; matches v3.3 stress-field U-07 sentinel pattern.
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Summary") int32          GoverningMemberIdx = -1;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Summary") int32          GoverningMemberId  = 0;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Summary") EFrameFailMode Mode               = EFrameFailMode::None;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Summary") bool           bValid             = false;

    // Shell side.
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Summary") float MaxShellDC          = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Summary") int32 GoverningShellIdx   = -1;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Summary") int32 GoverningShellId    = 0;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result|Summary") bool  bShellValid         = false;
};

// ---------------------------------------------------------------------------
// FFrameSolveResult -- top-level marshal of frame::SolveResult
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameSolveResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result") bool    bSingular   = false;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result") FString Diagnostic;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result") float   PivotMargin = 0.f;

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result") TArray<FFrameNodalDisplacement>     Displacements;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result") TArray<FFrameNodalReaction>         Reactions;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result") TArray<FFrameMemberInternalForces>  MemberForces;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result") TArray<FFrameMemberUtilization>     MemberUtilization;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result") TArray<FFrameShellInternalForces>   ShellForces;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result") TArray<FFrameShellUtilization>      ShellUtilization;

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Result") FFrameDemandSummary Utilization;
};

// ---------------------------------------------------------------------------
// Marshal helpers (impl in FrameCoreUEResultMarshal.cpp). Pure functions.
// ---------------------------------------------------------------------------

namespace frame {
    struct FrameModel;
    struct SolveResult;
    struct DemandSummary;
    struct ShellDemandSummary;
}

namespace FrameCoreUE
{
    // Full marshal: result + model needed because per-node disp/reaction unpack against
    // model.nodes (6N flat -> N x 6), and worstUtilization() / worstShellUtilization()
    // take the model too. Returns a populated FFrameSolveResult; on R.singular == true,
    // Displacements/Reactions/MemberForces/ShellForces may be empty (matches engine
    // semantics: some singular paths skip recover()).
    FRAMECOREUE_API FFrameSolveResult ToBlueprint(const frame::FrameModel& M, const frame::SolveResult& R);

    // Stand-alone marshal of just the demand summaries (for callers that already have a
    // SolveResult and only want the screen aggregate without re-walking everything).
    FRAMECOREUE_API FFrameDemandSummary ToBlueprint(const frame::DemandSummary& D);
    FRAMECOREUE_API FFrameDemandSummary ToBlueprint(const frame::DemandSummary& M, const frame::ShellDemandSummary& S);
}
