// ALevelSimHUD — canvas-drawn UI for the playable station. Pure presentation: every value
// shown comes from the pawn, which gets it from the levelsim core. English text for MVP
// (engine fonts have no CJK glyphs; a CJK font face is a later presentation task).
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "LevelSimHUD.generated.h"

class ALevelSimPawn;

UCLASS()
class ALevelSimHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;

private:
    void DrawOverview(ALevelSimPawn* P);
    void DrawLeveling(ALevelSimPawn* P);
    void DrawTelescope(ALevelSimPawn* P);
    void DrawBooking(ALevelSimPawn* P, bool bFinal);
    void DrawRouteSummary(ALevelSimPawn* P);
    void DrawTypingBox(ALevelSimPawn* P, const FString& Label);
    void DrawMessage(ALevelSimPawn* P);
    void DrawCircle(float CX, float CY, float R, int32 Segs, const FLinearColor& C, float Thick);
};
