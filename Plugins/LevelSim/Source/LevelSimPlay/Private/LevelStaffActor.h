// ALevelStaffActor — E-pattern levelling staff rendered from GEOMETRY (instanced boxes),
// not a texture: graduations stay pixel-crisp at telescope magnification (no mip blur),
// which is what makes mm estimation readable. Layout is a stylized E-pattern staff:
//   * white backboard, 4 m tall (matches levelsim::Staff::length)
//   * 5 cm "E" blocks (three 1 cm bars + spine), checkerboarded left/right column so
//     every cm boundary is visible somewhere across the width
//   * 7-segment dm numerals (<meter><dm>, e.g. "13" = 1.3 m) centred on each dm line
//   * black marks on even metres, red on odd metres (common rod convention)
// Local Z 0 = staff base = ground contact; world Z of a band == baseZ + reading. The
// measurement TRUTH never comes from this geometry — it comes from levelsim::measure();
// this actor only has to agree with the same world-space placement (verified by the
// smoke screenshots: crosshair must sit on the dm band the core predicts).
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelStaffActor.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

UCLASS()
class ALevelStaffActor : public AActor
{
    GENERATED_BODY()
public:
    ALevelStaffActor();

    // Build graduation geometry. Call once after spawn. lengthM = staff length in metres.
    void BuildStaff(float LengthM);

private:
    UPROPERTY() TObjectPtr<USceneComponent>                Root;
    UPROPERTY() TObjectPtr<UStaticMeshComponent>           Board;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent>  BlackMarks;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent>  RedMarks;

    // One axis-aligned box instance on the staff face. y/z in cm (local, z up from base),
    // w/h in cm. Face plane: local -X (toward the instrument); marks sit proud of the board.
    static void AddBox(UInstancedStaticMeshComponent* ISM, float YCenterCm, float ZCenterCm,
                       float WidthCm, float HeightCm);
    static void AddSevenSegDigit(UInstancedStaticMeshComponent* ISM, int32 Digit,
                                 float YCenterCm, float ZCenterCm, float HCm, float WCm);
};
