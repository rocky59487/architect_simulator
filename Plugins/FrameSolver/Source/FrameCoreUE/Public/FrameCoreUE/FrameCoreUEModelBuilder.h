#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FrameCoreUE/FrameCoreUEModelTypes.h"
#include "FrameCoreUEModelBuilder.generated.h"

// v3.4 Phase 1 -- BP entry points for FrameModel input plumbing.
//
// FFrameModelDef itself is BP-friendly via Make Struct / TArray helpers (Phase 1 input
// USTRUCT), so a CreateModel() factory adds little value over Make/Break. The library
// exposes only the operations that genuinely need engine code: validate (range-check ids,
// check warped-shell tolerance via the same engine path FrameModel::validate uses) and
// LoadModelFromJson (parses the dispatcher's model.set JSON schema subset, mirroring v3.3
// UFrameCoreStressFieldLibrary::ComputeFromJsonModel but returning the structural data so
// any analysis -- not just stress field -- can consume it).
UCLASS()
class FRAMECOREUE_API UFrameModelBuilder : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Runs the engine's FrameModel::validate against a marshalled copy of the BP model.
    // Returns true when the model is structurally sane; otherwise OutError carries a
    // one-line diagnostic (id-not-found, mat/sec out of range, warped shell quad, etc.).
    UFUNCTION(BlueprintCallable, Category="FrameCore|Model",
              meta=(DisplayName="Validate Model"))
    static bool ValidateModel(const FFrameModelDef& Def, FString& OutError);

    // Parses dispatcher model.set JSON schema subset (materials/sections/nodes/members/
    // shells/nodalLoads/memberUDLs/shellPressures) into a populated FFrameModelDef. On
    // failure: returns default-constructed Def and fills OutError. SHELLS / member release
    // arrays / prescribed displacements / load combinations are part of the BP schema
    // (already in the USTRUCTs) but only the fields v3.3 already parsed are honored here;
    // v3.5 (or a later v3.4 hardening) can extend this as needed.
    UFUNCTION(BlueprintCallable, Category="FrameCore|Model",
              meta=(DisplayName="Load Model From JSON"))
    static FFrameModelDef LoadModelFromJson(const FString& JsonPath, FString& OutError);
};
