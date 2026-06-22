#pragma once

// v3.4 Phase 1 -- Input USTRUCT mirrors of frame::* POD types.
// Pure consumer-side reflection: engine source under Plugins/FrameSolver/Source/FrameCore/
// is NOT touched (CLAUDE.md 鐵則 #1). USTRUCT precision is float32, mirroring v3.3's
// FFrameStressField* convention -- the engine real is double, the cast is intentional for
// the visualisation tolerance budget (rel < 1e-4); a future v3.x may add double-precision
// variants if BP designers ever need finer.
//
// Note on options: every UPROPERTY default mirrors the corresponding C++ POD's default-
// initialised value, so a BP `Make Struct` with no overrides produces a model that round-
// trips identically through the marshal layer (Phase 1 LibraryPresets test asserts this).

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "FrameCoreUEModelTypes.generated.h"

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EFrameSectionShape : uint8
{
    Rectangular UMETA(DisplayName="Rectangular"),
    Circular    UMETA(DisplayName="Circular"),
};

// Member-end release preset, mirrors frame::ReleasePreset. Builder libraries use this
// to populate Member.Release[12]; the underlying engine helper is frame::makeRelease.
UENUM(BlueprintType)
enum class EFrameReleasePreset : uint8
{
    Rigid    UMETA(DisplayName="Rigid (no release)"),
    TrussPin UMETA(DisplayName="Truss Pin (axial only)"),
    HingeI   UMETA(DisplayName="Hinge I (end-i bending released)"),
    HingeJ   UMETA(DisplayName="Hinge J (end-j bending released)"),
};

// ---------------------------------------------------------------------------
// Material + Capacity
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameCapacity
{
    GENERATED_BODY()

    // Allowable / elastic capacities (MPa). NOT RC ultimate strength -- this is the
    // ElasticAllowable screen, an allowable-stress D/C ratio (engine boundary; honest).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Material") float Comp  = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Material") float Tens  = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Material") float Shear = 0.f;
    // Bend / Tors / VM are derived (min(Comp,Tens) / Shear / min(Comp,Tens)) by
    // FFrameCapacity::Make on the engine side. A hand-built FFrameCapacity{} leaves
    // them 0, which screens as D/C = infinity under demand (matches engine semantics).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Material") float Bend  = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Material") float Tors  = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Material") float VM    = 0.f;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameMaterial
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Material") float E   = 0.f;   // Young's modulus (MPa)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Material") float G   = 0.f;   // shear modulus (MPa)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Material") float Nu  = 0.f;   // Poisson ratio
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Material") float Rho = 0.f;   // density (kg/m^3)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Material") float Fy  = 0.f;   // yield strength (MPa) for Mp = fy*Z

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Material") FFrameCapacity Cap;
};

// ---------------------------------------------------------------------------
// Section
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameSection
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Section") float A   = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Section") float Iy  = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Section") float Iz  = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Section") float J   = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Section") float Cy  = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Section") float Cz  = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Section") float Asy = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Section") float Asz = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Section") float Zy  = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Section") float Zz  = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Section") EFrameSectionShape Shape = EFrameSectionShape::Rectangular;
};

// ---------------------------------------------------------------------------
// Node
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameNode
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Node") int32   Id = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Node") FVector Pos = FVector::ZeroVector;

    // 6-bool fixity flags [Ux, Uy, Uz, Rx, Ry, Rz]. Marshal layer rejects arrays of
    // length != 6 with a diagnostic. Defaults to all-free (length 6, all false).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Node") TArray<bool> Fixed;

    // 6-float prescribed displacement / rotation per DOF. Honored only at fixed DOFs.
    // Length-6 enforced by the marshal layer; default all zero.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Node") TArray<float> Prescribed;
};

// ---------------------------------------------------------------------------
// Member
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameMember
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Member") int32 Id = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Member") int32 I  = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Member") int32 J  = 0;

    // Index into FFrameModelDef::Materials / Sections. Range-checked at marshal time.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Member") int32 MatIdx = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Member") int32 SecIdx = -1;

    // Reference vector defining local x-y plane. Default global +Z, engine falls back to
    // +Y when parallel to member axis.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Member") FVector RefVec = FVector(0.f, 0.f, 1.f);

    // 12-bool per-DOF end release [node-i 6][node-j 6]. Honored only when SolveOptions.bEnableReleases.
    // Length-12 enforced by marshal layer; default all false.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Member") TArray<bool> Release;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Member") bool bActive       = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Member") bool bTensionOnly  = false;
};

// ---------------------------------------------------------------------------
// Shell
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameShellQuad
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Shell") int32 Id = 0;

    // 4 corner node ids, CCW about +normal. Length-4 enforced by marshal layer.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Shell") TArray<int32> N;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Shell") int32 MatIdx = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Shell") float T      = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Shell") bool  bActive = true;
};

// ---------------------------------------------------------------------------
// Loads
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameNodalLoad
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Load") int32 Node = 0;
    // 6-float per-DOF load [Fx, Fy, Fz, Mx, My, Mz]. Length-6 enforced.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Load") TArray<float> Comp;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameMemberUDL
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Load") int32   Member = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Load") FVector WLocal = FVector::ZeroVector;   // force / length, member-local
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameShellPressure
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Load") int32 Shell = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Load") float P     = 0.f;   // along facet +local-z
};

// ---------------------------------------------------------------------------
// Solver options
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameSolveOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Solve") float PivotTol         = 1e-12f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Solve") bool  bEnableReleases  = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Solve") bool  bUseTimoshenko   = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Solve") bool  bUseIncompatibleMembrane = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Solve") bool  bUseDKQPlate     = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Solve") bool  bShellGeometricStiffness = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Solve") bool  bUseWarpingCorrection = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Solve") float WarpTolerance    = 1.0e-6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Solve") float ShellCurvatureMaxAngleDeg = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Solve") bool  bUseSupernodalPrimary = false;
};

// ---------------------------------------------------------------------------
// Analysis-specific options (Phase 4 uses these)
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFramePDeltaOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|PDelta") int32 MaxIter      = 200;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|PDelta") float TolU         = 1e-12f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|PDelta") bool  bAccelerate  = true;
    // Frozen-reuse path (default) vs K_T refactor reference path.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|PDelta") bool  bRefactorPath = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|PDelta") FFrameSolveOptions Solve;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameTensionOnlyOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|TensionOnly") int32 MaxIter           = 32;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|TensionOnly") bool  bAllowReactivation = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|TensionOnly") float AxialTol           = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|TensionOnly") FFrameSolveOptions Solve;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameCollapseOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Collapse") float DLF              = 2.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Collapse") float RemoveThreshold  = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Collapse") int32 MaxSteps         = 256;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Collapse") TArray<int32> InitialRemovals;        // member ids
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Collapse") TArray<int32> InitialShellRemovals;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Collapse") bool  bPlasticHinges   = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Collapse") bool  bNMInteraction   = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Collapse") FFrameSolveOptions Solve;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameCorotationalOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Corotational") int32 LoadSteps   = 10;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Corotational") int32 MaxIter     = 50;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Corotational") float TolR        = 1e-9f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Corotational") float TolU        = 1e-12f;

    // Arc-length sub-options (in-engine `CorotationalOptions::useArcLength`-onwards block).
    // Spec table notes "frame::ArcLengthOptions" as a distinct struct, but the engine fold
    // arc-length INTO `CorotationalOptions`. Mirror the engine; v3.5 InteractiveSubsystem
    // may add a flat `FFrameArcLengthOptions` thin wrapper if BP ergonomics demand it.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Corotational") bool  bUseArcLength       = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Corotational") float ArcLength            = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Corotational") int32 ArcSteps             = 50;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Corotational") int32 MonitorDof           = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Corotational") bool  bConsistentTangent   = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Corotational") bool  bShellCorotational   = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Corotational") FFrameSolveOptions Solve;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameDynCollapseOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|DynCollapse") float Dt              = 1e-3f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|DynCollapse") float MaxTime         = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|DynCollapse") int32 BasisSize       = 30;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|DynCollapse") bool  bUseRitzVectors  = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|DynCollapse") float RayleighAlpha   = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|DynCollapse") float RayleighBeta    = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|DynCollapse") float RemoveThreshold = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|DynCollapse") int32 ScreenEvery     = 5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|DynCollapse") float QuietKineticRatio = 1e-6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|DynCollapse") int32 MaxEvents       = 64;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|DynCollapse") TArray<int32> InitialRemovals;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|DynCollapse") TArray<int32> InitialShellRemovals;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|DynCollapse") int32 FrameStride     = 10;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|DynCollapse") FFrameSolveOptions Solve;
    // onFrameEmitted / isCancelled / onEventEmitted callbacks deliberately NOT mirrored --
    // those bind to v3.5 InteractiveSubsystem delegates, not Phase 1 input USTRUCT.
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameSizeOptLoadCase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|SizeOpt") TArray<FFrameNodalLoad> NodalLoads;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|SizeOpt") TArray<FFrameMemberUDL> MemberUDLs;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameSizeOptOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|SizeOpt") int32 MaxIter = 40;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|SizeOpt") float DCTol   = 1e-8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|SizeOpt") float Amin    = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|SizeOpt") FFrameSolveOptions Solve;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|SizeOpt") TArray<float> SectionTable;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|SizeOpt") TArray<FFrameSizeOptLoadCase> Cases;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameBESOOptions
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|BESO") float TargetVolFrac           = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|BESO") float EvolRate                = 0.02f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|BESO") int32 MaxIter                 = 200;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|BESO") bool  bSensHistory            = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|BESO") float ComplianceJumpTol       = 2.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|BESO") bool  bComplianceBestRollback = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|BESO") float WAxial   = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|BESO") float WBending = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|BESO") float WShear   = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|BESO") float WTorsion = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|BESO") int32 RedundancyCheckEvery = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|BESO") int32 RedundancySamples    = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|BESO") FFrameCollapseOptions Redundancy;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|BESO") FFrameSolveOptions Solve;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameReanalysisOptions
{
    GENERATED_BODY()

    // S1 ReSolve ladder. Engine struct is `frame::ReanalysisOptions` (spec called this
    // `ReSolveOptions`; the file is `Reanalysis.h`).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Reanalysis") int32 MaxRank      = 96;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Reanalysis") float PcgTol       = 1e-10f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Reanalysis") int32 PcgMaxIter   = 500;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Reanalysis") bool  bAllowTier2  = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Reanalysis") float MechPivotTol = 1e-10f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Reanalysis") FFrameSolveOptions Solve;
};

// ---------------------------------------------------------------------------
// FFrameModelDef -- aggregate input model
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameModelDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Model") TArray<FFrameMaterial>      Materials;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Model") TArray<FFrameSection>       Sections;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Model") TArray<FFrameNode>          Nodes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Model") TArray<FFrameMember>        Members;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Model") TArray<FFrameShellQuad>     Shells;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Model") TArray<FFrameNodalLoad>     NodalLoads;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Model") TArray<FFrameMemberUDL>     MemberUDLs;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|Model") TArray<FFrameShellPressure> ShellPressures;
};

// ---------------------------------------------------------------------------
// Engine marshal helpers (impl in FrameCoreUEModelMarshal.cpp). Pure functions; not
// BP-exposed. Bool return + string out-param matches frame::FrameModel::validate.
// ---------------------------------------------------------------------------

namespace frame { struct FrameModel; struct SolveOptions; }

namespace FrameCoreUE
{
    FRAMECOREUE_API bool FromBlueprint(const FFrameModelDef& Def, frame::FrameModel& OutModel, FString& OutError);
    FRAMECOREUE_API bool FromBlueprint(const FFrameSolveOptions& Opts, frame::SolveOptions& OutOpts);
}
