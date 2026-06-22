#pragma once

// v3.4 Phase 3 + Phase 4 -- USTRUCT mirrors of every analysis result struct under
// FrameCore Public. Pure consumer-side reflection -- no engine source touched (Phase 4
// gains some additional FRAMECORE_API facades for ::analyzeConnectivity, see commit
// log). Precision is float32 throughout (matches Phase 1 + Phase 2 convention).

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "FrameCoreUE/FrameCoreUEModelTypes.h"   // FFrameSection (used by FFrameSizeOptResult)
#include "FrameCoreUE/FrameCoreUEResultTypes.h"
#include "FrameCoreUEAnalysisTypes.generated.h"

// ---------------------------------------------------------------------------
// Phase 3 -- Modal
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameModalOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Modal") int32 NumModes        = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Modal") bool  bUseSparseSolver = false;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameModeShape
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Modal") float Omega  = 0.f;   // rad/s
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Modal") float FreqHz = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Modal") float Period = 0.f;   // s; 1/FreqHz, +inf-guarded
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Modal") TArray<FFrameNodalDisplacement> Shape;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameModalResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Modal") bool    bSingular = false;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Modal") FString Diagnostic;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Modal") TArray<FFrameModeShape> Modes;
};

// ---------------------------------------------------------------------------
// Phase 3 -- Buckling
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameBucklingOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Buckling") int32 DenseThreshold        = 500;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Buckling") int32 Nev                   = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Buckling") int32 MaxIter               = 300;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Buckling") float Tol                   = 1e-11f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Buckling") float ShellBucklingKnockdown = 0.f;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameBucklingResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Buckling") bool    bSingular = false;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Buckling") FString Diagnostic;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Buckling") float   CriticalFactor         = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Buckling") float   ReportedCriticalFactor = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Buckling") float   KnockdownFactor        = 1.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Buckling") TArray<FFrameNodalDisplacement> ModeShape;
};

// ---------------------------------------------------------------------------
// Phase 3 -- Response Spectrum
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EFrameSpectrumCombo : uint8
{
    SRSS UMETA(DisplayName="SRSS"),
    CQC  UMETA(DisplayName="CQC (Complete Quadratic Combination)"),
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameSpectrum
{
    GENERATED_BODY()

    // Periods (s), ascending.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|ResponseSpectrum") TArray<float> T;
    // Spectral pseudo-accelerations (mm/s^2) at each period.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|ResponseSpectrum") TArray<float> Sa;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameResponseSpectrumResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|ResponseSpectrum") bool    bSingular = false;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|ResponseSpectrum") FString Diagnostic;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|ResponseSpectrum") TArray<FFrameNodalDisplacement> PeakDisplacements;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|ResponseSpectrum") float BaseShear = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|ResponseSpectrum") TArray<float> EffMass;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|ResponseSpectrum") float TotalMass = 0.f;
};

// ---------------------------------------------------------------------------
// Phase 3 -- Modal Dynamics (RealTimeDynamic)
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameModalDynamicsOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|ModalDynamics") float Dt    = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|ModalDynamics") int32 NSteps = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|ModalDynamics") float Zeta  = 0.05f;
};

// A single Newmark step snapshot. BP-friendly compared to TArray<TArray<float>>.
USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameModalTimeStep
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|ModalDynamics") int32 StepIndex = 0;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|ModalDynamics") float Time      = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|ModalDynamics") TArray<FFrameNodalDisplacement> Displacements;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameModalTimeHistory
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|ModalDynamics") bool    bSingular = false;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|ModalDynamics") FString Diagnostic;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|ModalDynamics") float   Dt = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|ModalDynamics") TArray<FFrameModalTimeStep> Steps;
};

// ---------------------------------------------------------------------------
// Phase 3 -- Load combination envelope
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameLoadEnvelope
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Combination") bool    bSingular = false;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Combination") FString Diagnostic;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Combination") TArray<float> UMax;       // 6N (max over cases)
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Combination") TArray<float> UMin;       // 6N (min over cases)
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Combination") TArray<float> ReactMax;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Combination") TArray<float> ReactMin;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Combination") TArray<FFrameMemberEndForces> EndIMax;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Combination") TArray<FFrameMemberEndForces> EndIMin;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Combination") TArray<FFrameMemberEndForces> EndJMax;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Combination") TArray<FFrameMemberEndForces> EndJMin;
};

// ---------------------------------------------------------------------------
// Phase 3 -- Influence line
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameInfluenceLine
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|InfluenceLine") int32          ReactNode = 0;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|InfluenceLine") int32          ReactDof  = 0;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|InfluenceLine") TArray<int32>  LoadNodes;
    // ReactionAtPosition[k] = reaction at (ReactNode, ReactDof) when a unit -Z load is at LoadNodes[k].
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|InfluenceLine") TArray<float>  ReactionAtPosition;
};

// ---------------------------------------------------------------------------
// Phase 4 -- P-Delta
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFramePDeltaResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|PDelta") bool  bConverged    = false;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|PDelta") bool  bDiverged     = false;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|PDelta") int32 Iterations    = 0;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|PDelta") float LastIncrement = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|PDelta") FFrameSolveResult FinalState;
};

// ---------------------------------------------------------------------------
// Phase 4 -- Tension only
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameTensionOnlyResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|TensionOnly") bool  bConverged = false;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|TensionOnly") bool  bCycled    = false;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|TensionOnly") int32 Iterations = 0;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|TensionOnly") FFrameSolveResult FinalState;
    // Tension-only members left deactivated at convergence (user-assigned member ids).
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|TensionOnly") TArray<int32> Slack;
};

// ---------------------------------------------------------------------------
// Phase 4 -- Size optimisation
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameSizeOptResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|SizeOpt") TArray<float>         FinalAreas;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|SizeOpt") TArray<FFrameSection> FinalSections;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|SizeOpt") TArray<float>         FinalDC;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|SizeOpt") TArray<float>         DCHistory;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|SizeOpt") TArray<float>         WeightHistory;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|SizeOpt") bool  bConverged     = false;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|SizeOpt") bool  bCycled        = false;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|SizeOpt") bool  bSingular      = false;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|SizeOpt") bool  bInvalidDemand = false;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|SizeOpt") int32 Iterations     = 0;
};

// ---------------------------------------------------------------------------
// Phase 4 -- BESO topology
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EFrameBESOStop : uint8
{
    TargetReached  UMETA(DisplayName="Target Reached"),
    ComplianceJump UMETA(DisplayName="Compliance Jump"),
    Stalled        UMETA(DisplayName="Stalled"),
    Mechanism      UMETA(DisplayName="Mechanism"),
    MaxIter        UMETA(DisplayName="Max Iter"),
    Invalid        UMETA(DisplayName="Invalid"),
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameBESOResult
{
    GENERATED_BODY()

    // bool inside TArray<bool> is awkward in BP -- expose as TArray<int32> (1/0).
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|BESO") TArray<int32> FinalActive;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|BESO") TArray<int32> BestActive;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|BESO") TArray<float> VolFracHistory;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|BESO") TArray<float> ComplianceHistory;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|BESO") TArray<int32> ProtectedMembers;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|BESO") int32          BestIter   = -1;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|BESO") int32          Iterations = 0;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|BESO") EFrameBESOStop Reason     = EFrameBESOStop::Invalid;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|BESO") bool           bConverged = false;
};

// ---------------------------------------------------------------------------
// Phase 4 -- Corotational + Arc-length
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameCorotationalResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Corotational") bool  bConverged           = false;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Corotational") bool  bDiverged            = false;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Corotational") int32 LoadStepsCompleted   = 0;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Corotational") int32 TotalIterations      = 0;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Corotational") float LastResidual         = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Corotational") FFrameSolveResult FinalState;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Corotational") TArray<float> PathLambda;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|Corotational") TArray<float> PathDisp;
};

// ArcLength is just CorotationalResult with PathLambda/PathDisp populated. v3.4 ships a
// thin wrapper UFrameAnalysisLibrary::SolveArcLength that forces opts.bUseArcLength=true
// and returns FFrameCorotationalResult; no separate result struct.

// ---------------------------------------------------------------------------
// Phase 4 -- DynCollapse
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EFrameDynCollapseOutcome : uint8
{
    Stable    UMETA(DisplayName="Stable"),
    Collapsed UMETA(DisplayName="Collapsed"),
    MaxSteps  UMETA(DisplayName="MaxSteps"),
    Invalid   UMETA(DisplayName="Invalid"),
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameFragmentCluster
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") TArray<int32> Nodes;     // NodeIds, ascending
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") TArray<int32> Members;   // MemberIds, ascending
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") TArray<int32> Shells;    // shell ids, ascending
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") float   Mass = 0.f;       // tonnes
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") FVector COM  = FVector::ZeroVector;
    // Inertia tensor about COM, global axes, stored {Ixx, Iyy, Izz, Ixy, Ixz, Iyz}.
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") TArray<float> Inertia;   // length 6
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") FVector Vel    = FVector::ZeroVector;  // mm/s, global
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") FVector AngVel = FVector::ZeroVector;  // rad/s, global
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameCollapseHingeEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") int32 MemberId = 0;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") int32 Dof      = 0;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") float Mp       = 0.f;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameDynCollapseEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") float          Time = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") EFrameFailMode Mode = EFrameFailMode::None;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") TArray<int32>  RemovedMembers;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") TArray<int32>  RemovedShells;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") TArray<FFrameCollapseHingeEvent> FormedHinges;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") TArray<FFrameFragmentCluster>    Detached;

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") float TruncationResidual = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") float EnergyBefore       = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") float EnergyAfter        = 0.f;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameDynCollapseFrame
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") float         Time = 0.f;
    // Raw 6N flat displacement + velocity. v3.5 InteractiveSubsystem will unpack into per-node
    // FFrameNodalDisplacement for replay-friendly access; v3.4 ships the flat arrays so memory
    // footprint stays bounded (50K-DOF * 1000 frames = 300 MB if unpacked).
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") TArray<float> UFlat;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") TArray<float> VFlat;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameDynCollapseResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") EFrameDynCollapseOutcome Outcome = EFrameDynCollapseOutcome::Invalid;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") FString Diagnostic;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") TArray<FFrameDynCollapseEvent> Events;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse") TArray<FFrameDynCollapseFrame> Frames;
};

// ---------------------------------------------------------------------------
// Marshal helpers (impl in FrameCoreUEAnalysisMarshal.cpp). Forward decl of engine
// types so the header does not pull in every analysis header into FrameCoreUE consumers.
// ---------------------------------------------------------------------------

namespace frame {
    struct ModalResult;
    struct BucklingResult;
    struct ResponseSpectrumResult;
    struct ModalTimeHistory;
    struct ResultEnvelope;
    struct PDeltaResult;
    struct TensionOnlyResult;
    struct SizeOptResult;
    struct BESOResult;
    struct CorotationalResult;
    struct DynCollapseHistory;
    struct FrameModel;
}

namespace FrameCoreUE
{
    FRAMECOREUE_API FFrameModalResult            ToBlueprint(const frame::FrameModel& M, const frame::ModalResult& R);
    FRAMECOREUE_API FFrameBucklingResult         ToBlueprint(const frame::FrameModel& M, const frame::BucklingResult& R);
    FRAMECOREUE_API FFrameResponseSpectrumResult ToBlueprint(const frame::FrameModel& M, const frame::ResponseSpectrumResult& R);
    FRAMECOREUE_API FFrameModalTimeHistory       ToBlueprint(const frame::FrameModel& M, const frame::ModalTimeHistory& H);
    FRAMECOREUE_API FFrameLoadEnvelope           ToBlueprint(const frame::FrameModel& M, const frame::ResultEnvelope& E);
    FRAMECOREUE_API FFramePDeltaResult           ToBlueprint(const frame::FrameModel& M, const frame::PDeltaResult& R);
    FRAMECOREUE_API FFrameTensionOnlyResult      ToBlueprint(const frame::FrameModel& M, const frame::TensionOnlyResult& R);
    FRAMECOREUE_API FFrameSizeOptResult          ToBlueprint(const frame::FrameModel& M, const frame::SizeOptResult& R);
    FRAMECOREUE_API FFrameBESOResult             ToBlueprint(const frame::BESOResult& R);
    FRAMECOREUE_API FFrameCorotationalResult     ToBlueprint(const frame::FrameModel& M, const frame::CorotationalResult& R);
    FRAMECOREUE_API FFrameDynCollapseResult      ToBlueprint(const frame::FrameModel& M, const frame::DynCollapseHistory& H);
}
