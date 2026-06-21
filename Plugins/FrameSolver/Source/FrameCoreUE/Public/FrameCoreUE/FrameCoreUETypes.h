#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "FrameCoreUETypes.generated.h"

// USTRUCT mirrors of frame::StressField POD types. Pure consumer-side reflection layer:
// every USTRUCT here is BlueprintType + BlueprintReadOnly. Marshal is one-way (engine POD
// -> USTRUCT). Floats are float32 — the underlying engine `frame::real` is double; the
// lossy cast is intentional for visualisation tolerance budget rel<1e-4 (Phase 2 smoke
// test asserts this). A future v3.x can add double-precision USTRUCT variants if BP
// designers ever need finer than 1e-4 — current renderers paint at 8-bit colour anyway.

namespace frame {
    struct StressField;        // forward-decl, engine POD types
}

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameStressFieldSample
{
    GENERATED_BODY()

    // Arc length from end-i, 0 <= X <= L (member-local coordinate).
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float X = 0.f;

    // Fiber sigmas — compression-positive (matches ElasticAllowable convention).
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float SigmaCompMax = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float SigmaTensMax = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float TauShear     = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float TauTorsion   = 0.f;

    // Raw internal forces at X (LOCAL, for downstream BMD/SFD visualisation).
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float N  = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float Vy = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float Vz = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float T  = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float My = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float Mz = 0.f;

    // Per-fiber raw sigma values (LOCAL, at sample x).
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float SigmaTopY   = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float SigmaBotY   = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float SigmaPlusZ  = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float SigmaMinusZ = 0.f;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameMemberStressTrace
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") int32 MemberIdx = -1;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") int32 MemberId  = 0;

    // Length == samplesPerSpan (default 11).
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") TArray<FFrameStressFieldSample> Samples;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameShellStressPoint
{
    GENERATED_BODY()

    // -1 == centre; 0..3 == corner (matches ShellQuad::n order).
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") int32 CornerIdx = -1;

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float SigmaXX  = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float SigmaYY  = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float TauXY    = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float Sigma1   = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float Sigma2   = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float VonMises = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float ThetaRad = 0.f;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameShellStressLayer
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") int32 ShellIdx     = -1;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") int32 ShellId      = 0;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") bool  bIsTopLayer  = true;

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") FFrameShellStressPoint Center;

    // Length == 4 (cornerIdx 0..3, matching ShellQuad::n traversal).
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") TArray<FFrameShellStressPoint> Corners;
};

USTRUCT(BlueprintType)
struct FRAMECOREUE_API FFrameStressField
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") TArray<FFrameMemberStressTrace> Members;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") TArray<FFrameShellStressLayer>  ShellsTop;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") TArray<FFrameShellStressLayer>  ShellsBot;

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float GlobalMaxFiberSigma   = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") float GlobalMaxVonMises     = 0.f;

    // -1 sentinel when no governing element (v3.1.0 C-07/C-08 audit pattern).
    //
    // v3.2 audit A-1 NOTE / deferred v3.3 U-07: the engine POD frame::StressField uses
    // 0 as its "no governing" sentinel (StressField.h L78-79: `int governingMemberId = 0`).
    // ToBlueprint() in FrameCoreUETypes.cpp passes the engine value through verbatim,
    // so this USTRUCT field carries the engine 0 sentinel when nobody governs --
    // ambiguous with real member id 0. For a model where member id 0 is non-existent
    // or non-governing the engine writes 0 here and the USTRUCT shows 0 (interpreted
    // by a careful BP consumer as "real id 0 governs" -- incorrect). The proper fix
    // (engine-side default -1 + writer setting actual id) is deferred to v3.3 to keep
    // v3.2 honouring engine rule #1 (zero edits under FrameCore/). For now, consumers
    // should rely on Field.GlobalMaxFiberSigma > 0 to confirm that any governing
    // member actually contributes, before trusting GoverningMemberId.
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") int32 GoverningMemberId     = -1;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") int32 GoverningShellId      = -1;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") int32 GoverningShellCorner  = -1;
    UPROPERTY(BlueprintReadOnly, Category="FrameCore|StressField") bool  bGoverningShellLayerIsTop = true;
};

// Engine POD -> USTRUCT marshal. Pure function; allocates the returned TArrays. Lives in
// the FrameCoreUE namespace (not BP-exposed) — BP code calls UFrameCoreStressFieldLibrary
// which wraps both compute + marshal.
namespace FrameCoreUE
{
    FRAMECOREUE_API FFrameStressField ToBlueprint(const frame::StressField& Field);
}
