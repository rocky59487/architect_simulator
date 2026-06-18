#include "LevelSimPawn.h"
#include "LevelStaffActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Crc.h"

namespace
{
    constexpr double M2U = 100.0;          // metres -> uu (1 uu = 1 cm)
    constexpr double kBaseOpticsM = 1.50;  // optics height with screws at zero travel

    UStaticMesh* LoadShape(const TCHAR* Path) { return LoadObject<UStaticMesh>(nullptr, Path); }

    UStaticMeshComponent* AddShape(AActor* Owner, USceneComponent* Parent, UStaticMesh* Mesh,
                                   const FVector& Loc, const FRotator& Rot, const FVector& Scale,
                                   const FLinearColor& Color)
    {
        UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
        C->SetStaticMesh(Mesh);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetupAttachment(Parent);
        C->RegisterComponent();
        C->SetRelativeLocation(Loc);
        C->SetRelativeRotation(Rot);
        C->SetRelativeScale3D(Scale);
        if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
                TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
        {
            UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, Owner);
            MID->SetVectorParameterValue(TEXT("Color"), Color);
            C->SetMaterial(0, MID);
        }
        return C;
    }

    double NormDeg(double A)
    {
        while (A > 180.0)  A -= 360.0;
        while (A < -180.0) A += 360.0;
        return A;
    }
}

ALevelSimPawn::ALevelSimPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(Root);

    // Instrument constants for the playable station (design §2/§12). Honest values:
    // ±15' compensator range, ±0.4" setting accuracy, +10" collimation error
    // (0.48 mm at 10 m — inside the ±1 mm full-mark band; folded into truth).
    CoreParams.collimationRad = levelsim::arcsecToRad(10.0);
}

void ALevelSimPawn::BeginPlay()
{
    Super::BeginPlay();
    BuildInstrumentVisual();
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly());
    }
}

void ALevelSimPawn::BuildInstrumentVisual()
{
    UStaticMesh* Cyl  = LoadShape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* Cube = LoadShape(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!Cyl || !Cube) return;
    const FLinearColor Leg(0.35f, 0.25f, 0.12f), Body(0.85f, 0.65f, 0.05f), Tube(0.15f, 0.15f, 0.17f);

    // Tripod: three legs from a 60 cm ground ring up to the head at z = 140.
    for (int32 i = 0; i < 3; ++i)
    {
        const double Phi = FMath::DegreesToRadians(60.0 + 120.0 * i);
        const FVector Bottom(60 * FMath::Cos(Phi), 60 * FMath::Sin(Phi), 0);
        const FVector Top(0, 0, 140);
        const FVector Mid = (Bottom + Top) * 0.5;
        const FVector Dir = (Top - Bottom).GetSafeNormal();
        const float   Len = (Top - Bottom).Size();
        AddShape(this, Root, Cyl, Mid, FRotationMatrix::MakeFromZ(Dir).Rotator(),
                 FVector(0.04f, 0.04f, Len / 100.f), Leg);
    }
    // Tribrach + body.
    AddShape(this, Root, Cyl, FVector(0, 0, 143), FRotator::ZeroRotator, FVector(0.26f, 0.26f, 0.03f), Body);
    // Foot screws (visual markers at their LevelSetup azimuths 90/210/330).
    for (int32 i = 0; i < 3; ++i)
    {
        const double Phi = FMath::DegreesToRadians(90.0 + 120.0 * i);
        AddShape(this, Root, Cyl, FVector(11 * FMath::Cos(Phi), 11 * FMath::Sin(Phi), 145.5),
                 FRotator::ZeroRotator, FVector(0.035f, 0.035f, 0.025f), Tube);
    }
    // Telescope tube — yaws with the aim (cosmetic only; we look THROUGH the camera).
    TelescopeVisual = NewObject<USceneComponent>(this);
    TelescopeVisual->SetupAttachment(Root);
    TelescopeVisual->RegisterComponent();
    TelescopeVisual->SetRelativeLocation(FVector(0, 0, 150));
    AddShape(this, TelescopeVisual, Cube, FVector(0, 0, 0), FRotator::ZeroRotator,
             FVector(0.32f, 0.07f, 0.07f), Tube);
}

void ALevelSimPawn::InitTargets(const TArray<FStaffTarget>& InTargets, double InBMElevM)
{
    // Thin legacy wrapper: build a single-leg (N=2 RoutePoints) route from the supplied targets,
    // then defer to InitRoute. Preserves the byte-identical smoke/oracle path at station 0
    // (InstXm/InstYm stay 0; BuildRelativeTargets produces the same BaseXm/Ym as the input).
    TArray<FRoutePoint> Points;
    TArray<ALevelStaffActor*> Actors;
    for (const FStaffTarget& T : InTargets)
    {
        FRoutePoint P; P.WorldXm = T.BaseXm; P.WorldYm = T.BaseYm; P.WorldZm = T.BaseZm; P.Name = T.Name;
        Points.Add(P);
        Actors.Add(T.Actor);
    }
    InitRoute(Points, Actors, InBMElevM, ClosureC_Dist, ClosureC_Sta, /*bByDistance*/true);
}

void ALevelSimPawn::InitRoute(const TArray<FRoutePoint>& InPoints,
                              const TArray<ALevelStaffActor*>& InActors,
                              double InBMElev,
                              double InCDist, double InCSta, bool bInByDistance)
{
    RoutePoints = InPoints;
    RouteStaffActors.Empty();
    for (ALevelStaffActor* A : InActors) RouteStaffActors.Add(A);
    BMElevM        = InBMElev;
    ClosureC_Dist  = InCDist;
    ClosureC_Sta   = InCSta;
    bByDistance    = bInByDistance;

    LegRecords.Reset();
    CurrentStationIdx = 0;
    InstXm = 0; InstYm = 0;
    bClosureDone = false;
    CachedLoop = levelsim::LoopResult{};
    CachedLoopBySta = levelsim::LoopResult{};
    RouteTotal = 0;

    SetActorLocation(FVector(0, 0, 0));
    BuildRelativeTargets();
    bSceneInit = true;
    RestartStation(/*bRandomScrews=*/true);
}

void ALevelSimPawn::BuildRelativeTargets()
{
    StaffTargets.Reset();
    if (RoutePoints.Num() < 2) return;
    const int32 KBS = CurrentStationIdx;
    const int32 KFS = CurrentStationIdx + 1;
    if (!RoutePoints.IsValidIndex(KBS) || !RoutePoints.IsValidIndex(KFS)) return;

    auto MakeTarget = [&](int32 K) -> FStaffTarget
    {
        const FRoutePoint& P = RoutePoints[K];
        FStaffTarget T;
        T.Actor   = RouteStaffActors.IsValidIndex(K) ? RouteStaffActors[K].Get() : nullptr;
        T.BaseXm  = P.WorldXm - InstXm;
        T.BaseYm  = P.WorldYm - InstYm;
        T.BaseZm  = P.WorldZm;
        T.Name    = P.Name;
        return T;
    };
    StaffTargets.Add(MakeTarget(KBS));
    StaffTargets.Add(MakeTarget(KFS));
    UE_LOG(LogTemp, Display,
        TEXT("[LevelSim] BuildRelativeTargets stn=%d Inst=(%.4f, %.4f) BS=%s@(%.4f, %.4f, %.4f) FS=%s@(%.4f, %.4f, %.4f)"),
        CurrentStationIdx, InstXm, InstYm,
        *StaffTargets[0].Name, StaffTargets[0].BaseXm, StaffTargets[0].BaseYm, StaffTargets[0].BaseZm,
        *StaffTargets[1].Name, StaffTargets[1].BaseXm, StaffTargets[1].BaseYm, StaffTargets[1].BaseZm);
}

void ALevelSimPawn::RestartStation(bool bRandomScrews)
{
    if (bRandomScrews)
        for (double& T : ScrewTravel)
            T = FMath::FRandRange(-0.00035, 0.00035);   // up to ~16' tilt: must re-level
    Phase = ELevelPhase::Overview;
    Job = ELevelJob::ReadBS;
    bBrakeOn = false; bTyping = false; TypeBuf.Reset();
    FocusM = 2.0;
    PlayerBS = PlayerFS = TruthBS = TruthFS = ScoreBS = ScoreFS = 0;
    PlayerHI = PlayerElev = 0; bHIok = bElevOk = false; TotalScore = 0;
    PendingBSDistM = 0; PendingDistKm = 0;
    CurrentKnownElevM = (CurrentStationIdx == 0 || LegRecords.Num() == 0)
                      ? BMElevM
                      : LegRecords.Last().PlayerElev;
    AimYawDeg = StaffTargets.Num() ? StaffTargets[0].AzimuthDeg() - 6.0 : 0.0;
    const FString BSName = StaffTargets.Num() ? StaffTargets[0].Name : TEXT("BS staff");
    SetMsg(FString::Printf(TEXT("Station %d/%d: level [L], then sight %s [T]."),
                           CurrentStationIdx + 1, FMath::Max(1, NumLegs()), *BSName), 5.0);
}

void ALevelSimPawn::AdvanceStation()
{
    // 1. Commit the leg the player just finished into LegRecords.
    FLegRecord LR;
    LR.TruthBS = TruthBS; LR.TruthFS = TruthFS;
    LR.PlayerBS = PlayerBS; LR.PlayerFS = PlayerFS;
    LR.ScoreBS = ScoreBS; LR.ScoreFS = ScoreFS;
    LR.bHIok = bHIok; LR.bElevOk = bElevOk;
    LR.PlayerElev = PlayerElev;
    LR.DistKm = PendingDistKm;
    LR.StationCount = 1;
    LegRecords.Add(LR);

    // 2. Step the station cursor.
    CurrentStationIdx++;

    // 3. More legs remain -> reposition the pawn at the midpoint between THIS station's
    //    BS staff (RoutePoints[k]) and FS staff (RoutePoints[k+1]) — standard surveying
    //    practice: equal sight distances cancel the collimation/curvature/refraction
    //    error contribution. Station 0 is special-cased in InitRoute (instrument at
    //    world origin) so the existing pixel oracle and smoke screenshots are
    //    byte-identical to the single-station baseline.
    if (CurrentStationIdx < NumLegs())
    {
        const FRoutePoint& BS = RoutePoints[CurrentStationIdx];
        const FRoutePoint& FS = RoutePoints[CurrentStationIdx + 1];
        InstXm = (BS.WorldXm + FS.WorldXm) * 0.5;
        InstYm = (BS.WorldYm + FS.WorldYm) * 0.5;
        SetActorLocation(FVector(InstXm * M2U, InstYm * M2U, 0));
        BuildRelativeTargets();
        RestartStation(/*bRandomScrews=*/true);
    }
    else
    {
        // All legs committed -> compute closure and show the route summary.
        EnterPhase(ELevelPhase::RouteSummary);
    }
}

void ALevelSimPawn::BuildLoopResult()
{
    std::vector<levelsim::LevelLeg> Legs;
    Legs.reserve(LegRecords.Num());
    for (const FLegRecord& LR : LegRecords)
    {
        levelsim::LevelLeg L;
        L.bs = LR.TruthBS;          // truth values feed closeLoop; player values stay in the summary table
        L.fs = LR.TruthFS;
        L.distanceKm = LR.DistKm;
        L.stationCount = LR.StationCount;
        Legs.push_back(L);
    }
    CachedLoop      = levelsim::closeLoop(Legs, ClosureC_Dist, true);
    CachedLoopBySta = levelsim::closeLoop(Legs, ClosureC_Sta,  false);

    // Route score: reading/arithmetic average + binary closure bonus (20 or 0).
    int32 LegSum = 0;
    for (const FLegRecord& LR : LegRecords)
    {
        const int32 LegScore = FMath::RoundToInt32(
            30.0 * LR.ScoreBS + 30.0 * LR.ScoreFS
            + (LR.bHIok ? 20.0 : 0.0) + (LR.bElevOk ? 20.0 : 0.0));
        LegSum += LegScore;
    }
    const double LegAvg = LegRecords.Num() > 0 ? (double)LegSum / (double)LegRecords.Num() : 0.0;
    const levelsim::LoopResult& Controlling = bByDistance ? CachedLoop : CachedLoopBySta;
    const int32 ClosureBonus = (Controlling.valid && Controlling.withinTolerance) ? 20 : 0;
    RouteTotal = FMath::Clamp(FMath::RoundToInt32(LegAvg * 0.80 + (double)ClosureBonus), 0, 100);
}

void ALevelSimPawn::RouteRestart()
{
    LegRecords.Reset();
    CurrentStationIdx = 0;
    InstXm = 0; InstYm = 0;
    bClosureDone = false;
    CachedLoop = levelsim::LoopResult{};
    CachedLoopBySta = levelsim::LoopResult{};
    RouteTotal = 0;
    SetActorLocation(FVector(0, 0, 0));
    BuildRelativeTargets();
    RestartStation(/*bRandomScrews=*/true);
}

void ALevelSimPawn::SmokePerfectSubmitAndAdvance()
{
    // Synthesize a perfect single-leg submission and step to the next station.
    // Used by the multi-station smoke harness to exercise AdvanceStation/closeLoop
    // without needing a real player. Zero the screws first so AdvanceStation's
    // re-randomized residual cannot drop the compensator out of range (would NaN
    // the reading and the closure).
    if (StaffTargets.Num() < 2) return;
    for (double& T : ScrewTravel) T = 0.0;
    const levelsim::ReadingResult BS = MeasureTarget(StaffTargets[0]);
    const levelsim::ReadingResult FS = MeasureTarget(StaffTargets[1]);
    TruthBS = BS.reading; PlayerBS = BS.reading; ScoreBS = 1.0;
    TruthFS = FS.reading; PlayerFS = FS.reading; ScoreFS = 1.0;
    PendingBSDistM = BS.distance;
    PendingDistKm  = (BS.distance + FS.distance) / 2.0 / 1000.0;
    PlayerHI   = CurrentKnownElevM + PlayerBS; bHIok = true;
    PlayerElev = PlayerHI - PlayerFS;          bElevOk = true;
    TotalScore = 100;
    UE_LOG(LogTemp, Display,
        TEXT("[LevelSimSmoke] perfect submit stn=%d %s->%s dH=%.4f m"),
        CurrentStationIdx, *StaffTargets[0].Name, *StaffTargets[1].Name,
        TruthBS - TruthFS);
    AdvanceStation();
}

void ALevelSimPawn::SmokeLogClosureStatus()
{
    if (!bClosureDone)
    {
        UE_LOG(LogTemp, Display, TEXT("[LevelSimSmoke] closure: not computed (route incomplete)"));
        return;
    }
    UE_LOG(LogTemp, Display,
        TEXT("[LevelSimSmoke] closure: misclosure=%.4f m allowable_dist=%.2f mm allowable_sta=%.2f mm within(dist)=%d within(sta)=%d route_total=%d"),
        CachedLoop.misclosureM, CachedLoop.allowableMm, CachedLoopBySta.allowableMm,
        CachedLoop.withinTolerance ? 1 : 0, CachedLoopBySta.withinTolerance ? 1 : 0,
        RouteTotal);
}

// ---------------------------------------------------------------- core glue
levelsim::LevelSetup ALevelSimPawn::MakeSetup() const
{
    levelsim::LevelSetup S;
    for (int32 i = 0; i < 3; ++i) S.screwTravel[i] = ScrewTravel[i];
    S.opticsHeightZ  = kBaseOpticsM + (ScrewTravel[0] + ScrewTravel[1] + ScrewTravel[2]) / 3.0;
    S.focusObjective = FocusM;
    return S;
}
levelsim::TiltState   ALevelSimPawn::CurrentTilt()   const { return levelsim::tiltFromScrews(CoreParams, MakeSetup()); }
levelsim::BubbleState ALevelSimPawn::CurrentBubble() const { return levelsim::bubbleFromTilt(CoreParams, CurrentTilt()); }
levelsim::SightState  ALevelSimPawn::CurrentSight()  const { return levelsim::sightTilt(CoreParams, CurrentTilt(), Residual()); }

double ALevelSimPawn::Residual() const
{
    // Deterministic residual setting error: re-levelling re-rolls it, re-reading does not.
    int64 Q[3];
    for (int32 i = 0; i < 3; ++i) Q[i] = (int64)llround(ScrewTravel[i] * 1e9); // nm quantization
    const uint32 H = FCrc::MemCrc32(Q, sizeof(Q), 0x9E3779B9u);
    const double U = ((double)H / (double)MAX_uint32) * 2.0 - 1.0;
    return U * CoreParams.settingAccRad;
}

levelsim::ReadingResult ALevelSimPawn::MeasureTarget(const FStaffTarget& T) const
{
    levelsim::Staff St;
    St.baseX = T.BaseXm; St.baseY = T.BaseYm; St.baseZ = T.BaseZm; St.length = 4.0;
    return levelsim::measure(CoreParams, MakeSetup(), St, Residual());
}

const FStaffTarget* ALevelSimPawn::SightedTarget() const
{
    const FStaffTarget* Best = nullptr;
    double BestAbs = 1e9;
    for (const FStaffTarget& T : StaffTargets)
    {
        const double HalfWidthDeg = FMath::RadiansToDegrees(FMath::Atan2(0.07, T.DistM())) + 0.03;
        const double D = FMath::Abs(NormDeg(AimYawDeg - T.AzimuthDeg()));
        if (D <= HalfWidthDeg && D < BestAbs) { Best = &T; BestAbs = D; }
    }
    return Best;
}

const FStaffTarget* ALevelSimPawn::ExpectedTarget() const
{
    if (!StaffTargets.Num()) return nullptr;
    if (Job == ELevelJob::ReadBS) return &StaffTargets[0];
    if (Job == ELevelJob::ReadFS && StaffTargets.Num() > 1) return &StaffTargets[1];
    return nullptr;
}

// ---------------------------------------------------------------- input
bool ALevelSimPawn::JustPressed(APlayerController* PC, const FKey& K) { return PC->WasInputKeyJustPressed(K); }

float ALevelSimPawn::WheelDelta(APlayerController* PC) const
{
    const float Axis = PC->GetInputAnalogKeyState(EKeys::MouseWheelAxis);
    if (Axis != 0.f) return Axis;
    if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp))   return 1.f;
    if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown)) return -1.f;
    return 0.f;
}

void ALevelSimPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bSceneInit) return;
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
        PollInput(PC, DeltaSeconds);
    UpdateCamera();
    if (TelescopeVisual)
        TelescopeVisual->SetRelativeRotation(FRotator(0, (float)AimYawDeg, 0));
}

void ALevelSimPawn::PollInput(APlayerController* PC, float Dt)
{
    if (bTyping) { PollTyping(PC); return; }

    float MX = 0, MY = 0;
    PC->GetInputMouseDelta(MX, MY);
    const float Wheel = WheelDelta(PC);
    const bool  bShift = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);

    // global keys
    if (JustPressed(PC, EKeys::R) && (Phase == ELevelPhase::Overview
                                   || Phase == ELevelPhase::Done
                                   || Phase == ELevelPhase::RouteSummary))
    { RouteRestart(); return; }

    switch (Phase)
    {
    case ELevelPhase::Overview:
    {
        OverviewYawDeg   += MX * 0.15;
        OverviewPitchDeg  = FMath::Clamp(OverviewPitchDeg + MY * 0.15, -40.0, 25.0);
        if (JustPressed(PC, EKeys::L)) EnterPhase(ELevelPhase::Leveling);
        if (JustPressed(PC, EKeys::T)) EnterPhase(ELevelPhase::Telescope);
        break;
    }
    case ELevelPhase::Leveling:
    {
        if (JustPressed(PC, EKeys::One)   || JustPressed(PC, EKeys::NumPadOne))   SelectedScrew = 0;
        if (JustPressed(PC, EKeys::Two)   || JustPressed(PC, EKeys::NumPadTwo))   SelectedScrew = 1;
        if (JustPressed(PC, EKeys::Three) || JustPressed(PC, EKeys::NumPadThree)) SelectedScrew = 2;
        if (Wheel != 0.f)
        {
            const double Step = bShift ? 5e-6 : 1e-4;   // fine 5 um / coarse 0.1 mm per notch
            ScrewTravel[SelectedScrew] += Wheel * Step;
            ScrewTravel[SelectedScrew] = FMath::Clamp(ScrewTravel[SelectedScrew], -0.005, 0.005);
        }
        if (JustPressed(PC, EKeys::T))      EnterPhase(ELevelPhase::Telescope);
        if (JustPressed(PC, EKeys::Escape)) EnterPhase(ELevelPhase::Overview);
        break;
    }
    case ELevelPhase::Telescope:
    {
        if (!bBrakeOn) AimYawDeg = NormDeg(AimYawDeg + MX * 0.0025);
        if (JustPressed(PC, EKeys::C))
        {
            bBrakeOn = !bBrakeOn;
            SetMsg(bBrakeOn ? TEXT("Brake ON - wheel = slow-motion (tangent) screw")
                            : TEXT("Brake OFF - coarse aim with mouse"));
        }
        if (Wheel != 0.f)
        {
            if (PC->IsInputKeyDown(EKeys::F))
                FocusM = FMath::Clamp(FocusM + Wheel * 0.5, 1.0, 50.0);
            else if (bBrakeOn)
                AimYawDeg = NormDeg(AimYawDeg + Wheel * (bShift ? 0.002 : 0.01));
        }
        // else-if: if Enter+Escape land in the SAME tick (low-fps / injected input),
        // running both would set bTyping=true (TryBeginRead) AND drop to Overview,
        // stranding the pawn in a typing state no phase can service -> deadlock.
        if (JustPressed(PC, EKeys::Enter))       TryBeginRead();
        else if (JustPressed(PC, EKeys::Escape)) EnterPhase(ELevelPhase::Overview);
        break;
    }
    case ELevelPhase::Booking:
        // typing-driven; ESC does nothing (booking must be completed)
        break;
    case ELevelPhase::Done:
        break;
    }
}

void ALevelSimPawn::PollTyping(APlayerController* PC)
{
    static const FKey Digits[10][2] = {
        { EKeys::Zero, EKeys::NumPadZero }, { EKeys::One, EKeys::NumPadOne },
        { EKeys::Two, EKeys::NumPadTwo },   { EKeys::Three, EKeys::NumPadThree },
        { EKeys::Four, EKeys::NumPadFour }, { EKeys::Five, EKeys::NumPadFive },
        { EKeys::Six, EKeys::NumPadSix },   { EKeys::Seven, EKeys::NumPadSeven },
        { EKeys::Eight, EKeys::NumPadEight }, { EKeys::Nine, EKeys::NumPadNine } };
    for (int32 d = 0; d < 10; ++d)
        if (JustPressed(PC, Digits[d][0]) || JustPressed(PC, Digits[d][1]))
            if (TypeBuf.Len() < 8) TypeBuf.AppendChar(TCHAR('0' + d));
    if ((JustPressed(PC, EKeys::Period) || JustPressed(PC, EKeys::Decimal)) && !TypeBuf.Contains(TEXT(".")))
        if (TypeBuf.Len() < 8) TypeBuf.AppendChar(TEXT('.'));
    if (JustPressed(PC, EKeys::BackSpace) && TypeBuf.Len() > 0)
        TypeBuf.LeftChopInline(1);
    if (JustPressed(PC, EKeys::Escape))
    {
        if (Phase == ELevelPhase::Telescope)        // reading is optional -> cancellable
        { bTyping = false; TypeBuf.Reset(); SetMsg(TEXT("Reading cancelled.")); }
        else if (Phase == ELevelPhase::Booking)     // booking must be finished -> explain, don't swallow
        { SetMsg(TEXT("Field book must be completed: type the value and press Enter."), 3.0); }
    }
    if (JustPressed(PC, EKeys::Enter)) SubmitTyped();
}

void ALevelSimPawn::TryBeginRead()
{
    const levelsim::SightState S = CurrentSight();
    if (!S.readable) { SetMsg(TEXT("Image unstable: COMPENSATOR OUT OF RANGE - re-level [Esc]->[L].")); return; }
    if (!bBrakeOn)   { SetMsg(TEXT("Set the brake first [C], then fine-aim with the wheel.")); return; }
    const FStaffTarget* Sighted  = SightedTarget();
    const FStaffTarget* Expected = ExpectedTarget();
    if (!Expected)  { SetMsg(TEXT("Booking already complete.")); return; }
    if (!Sighted)   { SetMsg(TEXT("No staff under the vertical hair - aim first.")); return; }
    if (Sighted != Expected)
    { SetMsg(FString::Printf(TEXT("Wrong target: this is %s, the booking needs %s."), *Sighted->Name, *Expected->Name)); return; }
    const levelsim::ReadingResult R = MeasureTarget(*Expected);
    if (!R.readable || !R.onStaff) { SetMsg(TEXT("Line of sight is off the staff.")); return; }
    bTyping = true; TypeBuf.Reset();
}

void ALevelSimPawn::SubmitTyped()
{
    const double V = FCString::Atod(*TypeBuf);
    const bool bNumeric = TypeBuf.Len() > 0 && FMath::IsFinite(V);
    if (!bNumeric) { SetMsg(TEXT("Enter a number (metres), e.g. 1.482")); return; }

    if (Phase == ELevelPhase::Telescope)
    {
        const FStaffTarget* Expected = ExpectedTarget();
        if (!Expected) { bTyping = false; return; }
        const levelsim::ReadingResult R = MeasureTarget(*Expected);
        const double Score = levelsim::scoreReading(CoreParams, R.reading, V);
        if (Job == ELevelJob::ReadBS)
        {
            PlayerBS = V; TruthBS = R.reading; ScoreBS = Score;
            PendingBSDistM = R.distance;   // capture BS distance BEFORE any AdvanceStation rebase
            Job = ELevelJob::ReadFS;
            bTyping = false; TypeBuf.Reset();
            SetMsg(FString::Printf(TEXT("BS recorded: %.3f m (err %+.1f mm). Now sight %s and read FS."),
                                   V, (V - R.reading) * 1000.0,
                                   StaffTargets.Num() > 1 ? *StaffTargets[1].Name : TEXT("?")), 5.0);
        }
        else if (Job == ELevelJob::ReadFS)
        {
            PlayerFS = V; TruthFS = R.reading; ScoreFS = Score;
            // Average sight distance per the standard field-book convention; key the
            // by-distance closeLoop weight off this. Captured here so it cannot be
            // contaminated by AdvanceStation's coord rebase that comes later.
            PendingDistKm = (PendingBSDistM + R.distance) / 2.0 / 1000.0;
            Job = ELevelJob::FillHI;
            bTyping = true; TypeBuf.Reset();
            EnterPhase(ELevelPhase::Booking);
            SetMsg(FString::Printf(TEXT("FS recorded: %.3f m (err %+.1f mm). Fill the field book."),
                                   V, (V - R.reading) * 1000.0), 5.0);
        }
        return;
    }

    if (Phase == ELevelPhase::Booking)
    {
        if (Job == ELevelJob::FillHI)
        {
            PlayerHI = V;
            // CurrentKnownElevM = BM at stn 0 / previous leg's PlayerElev at stn k>0.
            bHIok = FMath::Abs(V - (CurrentKnownElevM + PlayerBS)) <= 5e-4;
            Job = ELevelJob::FillElev;
            TypeBuf.Reset();
        }
        else if (Job == ELevelJob::FillElev)
        {
            PlayerElev = V;
            bElevOk = FMath::Abs(V - (PlayerHI - PlayerFS)) <= 5e-4;
            Job = ELevelJob::Complete;
            bTyping = false; TypeBuf.Reset();
            TotalScore = FMath::RoundToInt32(30.0 * ScoreBS + 30.0 * ScoreFS
                                             + (bHIok ? 20.0 : 0.0) + (bElevOk ? 20.0 : 0.0));
            // Multi-station route: commit this leg and step to the next station,
            // or roll into the route closure summary. Single-station/MVP wrapper path
            // (RoutePoints.Num() <= 2 / NumLegs() == 1) keeps the original Done view.
            if (NumLegs() > 1)
                AdvanceStation();
            else
                EnterPhase(ELevelPhase::Done);
        }
    }
}

void ALevelSimPawn::EnterPhase(ELevelPhase NewPhase)
{
    Phase = NewPhase;
    // Booking is the only phase that legitimately carries an in-progress typing state
    // (SubmitTyped sets bTyping=true then transitions here). Every other transition must
    // arrive with typing cleared, so no phase can be entered mid-edit and strand bTyping.
    if (NewPhase != ELevelPhase::Booking)
    { bTyping = false; TypeBuf.Reset(); }
    if (NewPhase == ELevelPhase::Telescope)
    {
        const levelsim::SightState S = CurrentSight();
        if (!S.readable)
            SetMsg(TEXT("Compensator out of range - the image is unusable until you level."), 4.0);
    }
    if (NewPhase == ELevelPhase::RouteSummary)
    {
        // Compute closure once on arrival; HUD reads CachedLoop / CachedLoopBySta / RouteTotal.
        BuildLoopResult();
        bClosureDone = true;
        SetMsg(FString::Printf(TEXT("Route closed: %d / 100  %s   [R] new route."),
                               RouteTotal, RouteTotal >= 60 ? TEXT("PASS") : TEXT("FAIL")), 6.0);
    }
}

void ALevelSimPawn::SetMsg(const FString& M, double Sec)
{
    Msg = M;
    MsgUntil = GetWorld() ? GetWorld()->GetTimeSeconds() + Sec : 0.0;
}

// ---------------------------------------------------------------- camera
void ALevelSimPawn::UpdateCamera()
{
    if (!Camera) return;
    FPostProcessSettings& PP = Camera->PostProcessSettings;
    PP.bOverride_DepthOfFieldFocalDistance = false;
    PP.bOverride_DepthOfFieldFstop = false;

    switch (Phase)
    {
    // Camera positions are expressed relative to the Pawn actor so that when
    // AdvanceStation() moves the Pawn to (InstXm, InstYm) the camera follows. At
    // station 0 GetActorLocation() == (0,0,0) so the old fixed-world coordinates
    // are byte-identical.
    case ELevelPhase::Overview:
    case ELevelPhase::Booking:
    case ELevelPhase::Done:
    case ELevelPhase::RouteSummary:
        Camera->SetWorldLocation(GetActorLocation() + FVector(-260, 60, 175));
        Camera->SetWorldRotation(FRotator((float)OverviewPitchDeg, (float)OverviewYawDeg, 0));
        Camera->SetFieldOfView(70.f);
        break;
    case ELevelPhase::Leveling:
        Camera->SetWorldLocation(GetActorLocation() + FVector(-52, 0, 196));
        Camera->SetWorldRotation(FRotator(-52.f, 0.f, 0.f));
        Camera->SetFieldOfView(55.f);
        break;
    case ELevelPhase::Telescope:
    {
        const levelsim::LevelSetup  S  = MakeSetup();
        const levelsim::SightState  St = CurrentSight();
        const double PitchDeg = St.readable ? FMath::RadiansToDegrees(St.losTiltRad) : 0.0;
        Camera->SetWorldLocation(GetActorLocation() + FVector(0, 0, (float)(S.opticsHeightZ * M2U)));
        Camera->SetWorldRotation(FRotator((float)PitchDeg, (float)AimYawDeg, 0));
        Camera->SetFieldOfView(TelescopeHFovDeg);
        // 對光: real defocus blur — focus wheel must match the staff distance (S8 hook).
        PP.bOverride_DepthOfFieldFocalDistance = true;
        PP.DepthOfFieldFocalDistance = (float)(FocusM * M2U);
        PP.bOverride_DepthOfFieldFstop = true;
        PP.DepthOfFieldFstop = 4.0f;
        break;
    }
    }
}

// ---------------------------------------------------------------- smoke hooks
void ALevelSimPawn::SmokePerturbScrews()
{
    ScrewTravel[0] = 2e-4; ScrewTravel[1] = -1e-4; ScrewTravel[2] = 5e-5;
    EnterPhase(ELevelPhase::Leveling);
    const levelsim::BubbleState B = CurrentBubble();
    const levelsim::TiltState   T = CurrentTilt();
    UE_LOG(LogTemp, Display, TEXT("[LevelSimSmoke] perturbed: tilt=%.4f arcmin bubble=(%.2f, %.2f) div mag=%.2f rough=%d fine=%d"),
           FMath::RadiansToDegrees(T.magRad) * 60.0, B.offX, B.offY, B.magDiv, B.roughLevel ? 1 : 0, B.fineLevel ? 1 : 0);
}

void ALevelSimPawn::SmokeAutoLevel()
{
    ScrewTravel[0] = ScrewTravel[1] = ScrewTravel[2] = 1e-4;
    EnterPhase(ELevelPhase::Telescope);
    bBrakeOn = true;
    if (StaffTargets.Num()) { AimYawDeg = StaffTargets[0].AzimuthDeg(); FocusM = StaffTargets[0].DistM(); }
    const levelsim::ReadingResult R = StaffTargets.Num() ? MeasureTarget(StaffTargets[0]) : levelsim::ReadingResult{};
    UE_LOG(LogTemp, Display, TEXT("[LevelSimSmoke] BM sighted: readable=%d onStaff=%d truth=%.4f m D=%.2f m"),
           R.readable ? 1 : 0, R.onStaff ? 1 : 0, R.reading, R.distance);
}

void ALevelSimPawn::SmokeAimSecond()
{
    if (StaffTargets.Num() > 1) { AimYawDeg = StaffTargets[1].AzimuthDeg(); FocusM = StaffTargets[1].DistM(); }
    const levelsim::ReadingResult R = StaffTargets.Num() > 1 ? MeasureTarget(StaffTargets[1]) : levelsim::ReadingResult{};
    UE_LOG(LogTemp, Display, TEXT("[LevelSimSmoke] FS sighted: readable=%d onStaff=%d truth=%.4f m D=%.2f m"),
           R.readable ? 1 : 0, R.onStaff ? 1 : 0, R.reading, R.distance);
}

void ALevelSimPawn::SmokeLogScoringPath()
{
    if (StaffTargets.Num() < 2) return;
    const levelsim::ReadingResult BS = MeasureTarget(StaffTargets[0]);
    const levelsim::ReadingResult FS = MeasureTarget(StaffTargets[1]);
    const double SGood = levelsim::scoreReading(CoreParams, BS.reading, BS.reading + 0.0005);
    const double SEdge = levelsim::scoreReading(CoreParams, BS.reading, BS.reading + 0.002);
    const double SBad  = levelsim::scoreReading(CoreParams, BS.reading, BS.reading + 0.010);
    const double DH = BS.reading - FS.reading;
    UE_LOG(LogTemp, Display, TEXT("[LevelSimSmoke] scoring: +0.5mm=%.3f +2mm=%.3f +10mm=%.3f | dH=%.4f m -> elev=%.4f m (scene truth 100.3700)"),
           SGood, SEdge, SBad, DH, BMElevM + DH);
}
