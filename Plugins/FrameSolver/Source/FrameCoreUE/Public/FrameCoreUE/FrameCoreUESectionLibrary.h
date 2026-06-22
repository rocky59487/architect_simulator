#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FrameCoreUE/FrameCoreUEModelTypes.h"
#include "FrameCoreUESectionLibrary.generated.h"

// v3.4 Phase 1 -- BP-callable section factories.
//
// Currently exposes the two shapes the engine has analytic factories for:
//   * MakeRectangular(width, depth)  -> frame::Section::Rectangular(b, d)
//   * MakeCircular(diameter)         -> frame::Section::Circular(r = d/2)
//
// MakeIBeam / MakeHSS / MakeCircularHollow are deferred to v3.5 or a later v3.4 hardening:
// the engine has no IBeam/HSS shape (Section::Shape only has Rectangular and Circular), so
// honest exposure would require either (a) computing equivalent A/I/J/Z analytically and
// reporting Shape=Rectangular (correct elastic, conservative biaxial), or (b) extending the
// engine Section::Shape enum (violates v3.4 0-engine-delta target). Phase 1 ships option
// (a) only for the two cases the engine already covers analytically.
UCLASS()
class FRAMECOREUE_API UFrameSectionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // b = width (along member-local z), d = depth (along member-local y). Matches engine
    // frame::Section::Rectangular(b, d): Zz = b*d^2/4, Zy = d*b^2/4, A = b*d, Asy = Asz =
    // 5/6 * A (Timoshenko shear correction for a solid rectangle).
    UFUNCTION(BlueprintPure, Category="FrameCore|Section|Presets",
              meta=(DisplayName="Make Rectangular Section"))
    static FFrameSection MakeRectangular(float Width, float Depth);

    // Solid circular section. Iy = Iz = pi*r^4 / 4, J = pi*r^4 / 2, A = pi*r^2,
    // Zy = Zz = 4r^3 / 3 (plastic), Asy = Asz = 0.9 * A (textbook shear correction).
    UFUNCTION(BlueprintPure, Category="FrameCore|Section|Presets",
              meta=(DisplayName="Make Circular Section"))
    static FFrameSection MakeCircular(float Diameter);
};
