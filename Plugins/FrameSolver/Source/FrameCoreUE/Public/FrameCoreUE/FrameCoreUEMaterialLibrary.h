#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FrameCoreUE/FrameCoreUEModelTypes.h"
#include "FrameCoreUEMaterialLibrary.generated.h"

// v3.4 Phase 1 -- BP-callable material presets. Units = engine's N-mm-tonne-s system
// (consistent with the rest of FrameCore): E and Capacity in MPa, density in kg/m^3.
//
// Steel grades (S235/S275/S355/S460) follow EN 10025-2 nominal yield + EN 1993-1-1
// elastic properties. Concrete grades (C30/C40/C50) follow EN 1992-1-1 mean strengths.
// Aluminum 6061-T6 is a representative light-alloy demo at typical aerospace handbook
// values.
UCLASS()
class FRAMECOREUE_API UFrameMaterialLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="FrameCore|Material|Presets") static FFrameMaterial GetS235();
    UFUNCTION(BlueprintPure, Category="FrameCore|Material|Presets") static FFrameMaterial GetS275();
    UFUNCTION(BlueprintPure, Category="FrameCore|Material|Presets") static FFrameMaterial GetS355();
    UFUNCTION(BlueprintPure, Category="FrameCore|Material|Presets") static FFrameMaterial GetS460();

    UFUNCTION(BlueprintPure, Category="FrameCore|Material|Presets") static FFrameMaterial GetConcreteC30();
    UFUNCTION(BlueprintPure, Category="FrameCore|Material|Presets") static FFrameMaterial GetConcreteC40();
    UFUNCTION(BlueprintPure, Category="FrameCore|Material|Presets") static FFrameMaterial GetConcreteC50();

    UFUNCTION(BlueprintPure, Category="FrameCore|Material|Presets") static FFrameMaterial GetAluminum6061();

    // Custom material constructor; Bend/Tors/VM in the capacity are derived just like
    // frame::Capacity::make (Bend = min(Comp,Tens), Tors = Shear, VM = min(Comp,Tens)).
    UFUNCTION(BlueprintPure, Category="FrameCore|Material",
              meta=(DisplayName="Make Custom Material"))
    static FFrameMaterial MakeCustomMaterial(float E, float G, float Rho, float Nu, float Fy,
                                             float CapComp, float CapTens, float CapShear);
};
