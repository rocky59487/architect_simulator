// ALevelSimPawn — the player IS the level instrument (set up over the station point).
// Owns the gameplay FSM, all input (polled, zero input assets), the three cameras
// (overview / bubble close-up / telescope), and the glue to the pure measurement core.
//
// Truth discipline: every number that matters (tilt, bubble, line-of-sight, reading,
// score) comes from levelsim:: (Core/LevelCore.h). This class only converts units
// (core metres <-> UE cm; 1 uu = 1 cm) and renders/binds input.
//
// Telescope correctness: an automatic level has NO player pitch — the compensator sets
// the line of sight. The telescope camera pitch is therefore locked to the core's
// sightTilt() losTiltRad (collimation + clamped residual), so what the player READS on
// the rendered staff is geometrically the same value measure() predicts (the smoke test
// verifies the crosshair lands on the predicted dm band).
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "LevelCore.h"
#include "LevelSimPawn.generated.h"

class UCameraComponent;
class ALevelStaffActor;
class APlayerController;

enum class ELevelPhase : uint8 { Overview, Leveling, Telescope, Booking, Done, RouteSummary };
enum class ELevelJob   : uint8 { ReadBS, ReadFS, FillHI, FillElev, Complete };

// One staff target in the scene. Positions in metres, expressed relative to the
// current instrument position (matches levelsim::Staff conventions, which assume
// the instrument at the origin). Multi-station: BuildRelativeTargets() rebuilds
// these per station so the core math stays single-station-relative.
struct FStaffTarget
{
    ALevelStaffActor* Actor = nullptr;
    double BaseXm = 0, BaseYm = 0, BaseZm = 0;
    FString Name;
    double AzimuthDeg() const { return FMath::RadiansToDegrees(FMath::Atan2(BaseYm, BaseXm)); }
    double DistM()      const { return FMath::Sqrt(BaseXm * BaseXm + BaseYm * BaseYm); }
};

// One physical route point (staff position in absolute world metres + display name).
// The route may revisit the same point (e.g., closed loop ends at BM); the GameMode
// deduplicates staff actors but RoutePoints lists every visit.
struct FRoutePoint
{
    double WorldXm = 0, WorldYm = 0, WorldZm = 0;
    FString Name;
};

// One completed instrument-setup leg (BS on RoutePoints[k] -> FS on RoutePoints[k+1]).
// Truth values feed closeLoop(); player values + score components feed the summary.
struct FLegRecord
{
    double TruthBS = 0, TruthFS = 0;
    double PlayerBS = 0, PlayerFS = 0;
    double ScoreBS = 0, ScoreFS = 0;
    bool   bHIok = false, bElevOk = false;
    double PlayerElev = 0;        // station(k+1) known elevation for the next leg
    double DistKm = 0;            // (BSdist + FSdist) / 2 / 1000, captured at submit time
    int32  StationCount = 1;      // always 1 per setup in this MVP
};

UCLASS()
class ALevelSimPawn : public APawn
{
    GENERATED_BODY()
public:
    ALevelSimPawn();
    virtual void Tick(float DeltaSeconds) override;
    virtual void BeginPlay() override;

    // ---- scene wiring (called by game mode) ----
    void InitTargets(const TArray<FStaffTarget>& InTargets, double InBMElevM);

    // ---- smoke-test hooks (deterministic, used by -levelsim.smoke) ----
    void SmokePerturbScrews();   // known tilt -> bubble visibly off
    void SmokeAutoLevel();       // equal travels -> tilt 0, telescope on BM
    void SmokeAimSecond();       // swing to the foresight staff
    void SmokeLogScoringPath();  // exercise measure()+scoreReading() and log

    // ---- state read by the HUD (single-writer: this pawn) ----
    ELevelPhase Phase = ELevelPhase::Overview;
    ELevelJob   Job   = ELevelJob::ReadBS;
    int32   SelectedScrew = 0;          // 0..2
    bool    bBrakeOn      = false;      // 制動
    bool    bTyping       = false;
    FString TypeBuf;
    FString Msg;                        // transient status line
    double  MsgUntil = 0;
    double  FocusM   = 2.0;             // objective focus distance (m); start blurry on purpose
    double  ScrewTravel[3] = { 0, 0, 0 };
    static constexpr float TelescopeHFovDeg = 2.3f;   // ~30x, matches design 1-1.5 deg vertical

    // booking record (current leg in progress)
    double PlayerBS = 0, PlayerFS = 0, TruthBS = 0, TruthFS = 0;
    double ScoreBS = 0, ScoreFS = 0;
    double PlayerHI = 0, PlayerElev = 0;
    bool   bHIok = false, bElevOk = false;
    double BMElevM = 100.0;                            // ANCHOR: BM (station 0) known elevation
    int32  TotalScore = 0;

    // multi-station route
    TArray<FRoutePoint>                 RoutePoints;   // [BM, TP1, ..., BM] for closed loop
    UPROPERTY() TArray<TObjectPtr<ALevelStaffActor>> RouteStaffActors; // GC-tracked; 1 entry per RoutePoint (deduped revisits share)
    TArray<FLegRecord>                  LegRecords;    // appended on each AdvanceStation()
    int32  CurrentStationIdx = 0;                      // 0..NumLegs()-1
    double InstXm = 0, InstYm = 0;                     // current instrument world position (m); 0,0 at station 0
    double CurrentKnownElevM = 0;                      // BM at stn 0; last leg's PlayerElev at stn k>0
    double PendingBSDistM = 0;                         // captured at ReadBS submit (before any rebase)
    double PendingDistKm  = 0;                         // (PendingBSDistM + FSdist) / 2 / 1000, set at ReadFS submit
    double ClosureC_Dist  = 12.0;                      // mm/sqrt(km)
    double ClosureC_Sta   = 10.0;                      // mm/sqrt(n)
    bool   bByDistance    = true;                      // which branch controls the route closure score
    levelsim::LoopResult CachedLoop;                   // bByDistance branch result
    levelsim::LoopResult CachedLoopBySta;              // !bByDistance branch (displayed alongside)
    bool   bClosureDone   = false;
    int32  RouteTotal     = 0;
    int32  NumLegs() const { return FMath::Max(0, RoutePoints.Num() - 1); }

    // ---- core glue (HUD also calls these read-only) ----
    levelsim::InstrumentParams CoreParams;
    levelsim::LevelSetup  MakeSetup() const;
    levelsim::TiltState   CurrentTilt() const;
    levelsim::BubbleState CurrentBubble() const;
    levelsim::SightState  CurrentSight() const;
    double Residual() const;                       // deterministic per screw configuration
    const FStaffTarget* SightedTarget() const;     // staff under the vertical hair, or null
    const FStaffTarget* ExpectedTarget() const;    // staff the current job wants
    levelsim::ReadingResult MeasureTarget(const FStaffTarget& T) const;
    double AimYawDeg = 0;
    const TArray<FStaffTarget>& Targets() const { return StaffTargets; }
    // True elevation of the FS point under the current sighting, accumulated through all completed legs.
    double TrueElevM() const
    {
        double Sum = BMElevM;
        for (const FLegRecord& LR : LegRecords) Sum += (LR.TruthBS - LR.TruthFS);
        return Sum + (TruthBS - TruthFS);
    }

private:
    UPROPERTY() TObjectPtr<USceneComponent>  Root;
    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
    UPROPERTY() TObjectPtr<USceneComponent>  TelescopeVisual; // yaws with aim (cosmetic)

    TArray<FStaffTarget> StaffTargets;
    bool   bSceneInit = false;
    double OverviewYawDeg = 0, OverviewPitchDeg = -8;

    void BuildInstrumentVisual();
    void EnterPhase(ELevelPhase NewPhase);
    void RestartStation(bool bRandomScrews);
    void PollInput(APlayerController* PC, float Dt);
    void PollTyping(APlayerController* PC);
    float WheelDelta(APlayerController* PC) const;
    void UpdateCamera();
    void TryBeginRead();
    void SubmitTyped();
    void SetMsg(const FString& M, double Sec = 3.0);
    static bool JustPressed(APlayerController* PC, const FKey& K);

public:
    // ---- multi-station route API (called by GameMode) ----
    void InitRoute(const TArray<FRoutePoint>& InPoints,
                   const TArray<ALevelStaffActor*>& InActors,
                   double InBMElev,
                   double InCDist, double InCSta, bool bInByDistance);

    // ---- smoke hooks added for multi-station ----
    void SmokePerfectSubmitAndAdvance();   // submit truth as player + AdvanceStation
    void SmokeLogClosureStatus();          // log CachedLoop / CachedLoopBySta

private:
    void AdvanceStation();              // commits current leg, sets up next station or enters RouteSummary
    void BuildRelativeTargets();        // (re)builds StaffTargets[0]/[1] from RoutePoints around InstXm/InstYm
    void BuildLoopResult();             // calls levelsim::closeLoop twice into CachedLoop / CachedLoopBySta
    void RouteRestart();                // R key: clears LegRecords, idx, InstX/Y, CachedLoop*, bClosureDone
};
