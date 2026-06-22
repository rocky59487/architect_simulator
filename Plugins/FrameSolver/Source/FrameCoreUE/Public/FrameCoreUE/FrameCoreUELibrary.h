#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCoreUELibrary.generated.h"

// Blueprint entry points for the FrameCore stress-field post-process. Pure consumer-side:
// engine source remains untouched. Each entry point either (a) builds an in-memory
// fixture, calls frame::computeStressField, and marshals the result, or (b) operates on
// an already-marshalled FFrameStressField.
//
// Phase 1 ships `ComputeCantileverFixture` as the working demonstration. Phase 2 +
// Phase 3 add panel / model-load helpers. Production load-from-JSON path is deferred to
// v3.3 (see PLAN_v3.2_ue_interface.md §1 "不做什麼").
UCLASS()
class FRAMECOREUE_API UFrameCoreStressFieldLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Build a 2 m cantilever (rectangular 100x100 mm, S235-like material) with a tip
    // load -P (LOCAL +y), solve, compute the stress field, and return the BP-friendly
    // result. The defaults mirror the F68 standalone fixture so a BP smoke test can
    // assert against the same analytic |P|*(L-x)/Wz envelope.
    UFUNCTION(BlueprintCallable, Category="FrameCore|StressField",
              meta=(DisplayName="Compute Cantilever Fixture (Demo)"))
    static FFrameStressField ComputeCantileverFixture(
        float P = 1000.f,
        float L = 2000.f,
        int32 SamplesPerSpan = 11);

    // v3.3 (U-01): load a model from disk in the dispatcher's `model.set` JSON schema,
    // solve it, and return the marshalled FFrameStressField. Supports the subset
    // {materials, sections, nodes, members, nodalLoads, memberUDLs} -- enough for any
    // beam-only fixture. Shells, member releases / tensionOnly / prescribed displacements,
    // and per-fixture solver options are out of scope for v3.3 (use ComputeCantileverFixture
    // or call the engine directly until v3.4 widens the schema). On parse / solve failure
    // the function logs a warning and returns a default-constructed FFrameStressField
    // (governing indices == -1).
    UFUNCTION(BlueprintCallable, Category="FrameCore|StressField",
              meta=(DisplayName="Compute Stress Field From JSON"))
    static FFrameStressField ComputeFromJsonModel(
        const FString& JsonPath,
        int32 SamplesPerSpan = 11);

    // Convenience accessors so BP graphs don't have to break struct + index.
    // v3.3 BREAKING (U-07): renamed from GetGoverningMemberId / GetGoverningShellId.
    // Return value is now an INDEX into the field's Members / ShellsTop+ShellsBot
    // lists, with INDEX_NONE (-1) for "no governing element". To get the element's
    // user-assigned id, use Field.Members[Idx].MemberId (or .ShellId for shells).
    UFUNCTION(BlueprintPure, Category="FrameCore|StressField")
    static int32 GetGoverningMemberIdx(const FFrameStressField& Field);

    UFUNCTION(BlueprintPure, Category="FrameCore|StressField")
    static int32 GetGoverningShellIdx(const FFrameStressField& Field);

    UFUNCTION(BlueprintPure, Category="FrameCore|StressField")
    static float GetGlobalMaxFiberSigma(const FFrameStressField& Field);

    UFUNCTION(BlueprintPure, Category="FrameCore|StressField")
    static float GetGlobalMaxVonMises(const FFrameStressField& Field);

    // Returns Members[MemberIdx].Samples; empty if out of range.
    UFUNCTION(BlueprintPure, Category="FrameCore|StressField")
    static TArray<FFrameStressFieldSample> GetMemberSamples(
        const FFrameStressField& Field, int32 MemberIdx);
};
