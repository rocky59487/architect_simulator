#include "LevelSimGameMode.h"
#include "LevelSimPawn.h"
#include "LevelSimHUD.h"
#include "LevelStaffActor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"

namespace
{
    // Multi-station closed-loop route (metres). World datum: z=0 <-> BM elevation.
    // RoutePoints visit BM twice (start and close); the GameMode deduplicates the
    // physical staff actors. For backward compatibility with the single-station
    // smoke + pixel oracle, station 0 places the instrument at world origin and
    // the first two points (BM, TP1) reproduce the previous kBMX/kP1* geometry.
    struct RoutePointDef { double xm, ym, zm; const TCHAR* name; };
    constexpr RoutePointDef kRoute[] = {
        { 10.0,   0.0,   0.00, TEXT("BM")  },   // start (= previous kBMX/Y/Z)
        {  6.553, 4.589, 0.37, TEXT("TP1") },   // az=35 deg, D=8 m, dz +0.37 (= previous kP1*)
        { -3.0,   7.0,   0.15, TEXT("TP2") },   // second turning point
        { 10.0,   0.0,   0.00, TEXT("BM")  },   // close back to BM
    };
    constexpr int32 kRouteNumPoints = sizeof(kRoute) / sizeof(kRoute[0]);
    constexpr double kBMElev        = 100.0;
    constexpr double kClosureC_Dist = 12.0;     // mm/sqrt(km)
    constexpr double kClosureC_Sta  = 10.0;     // mm/sqrt(n)
    constexpr bool   kByDistance    = true;

    AStaticMeshActor* SpawnBox(UWorld* W, const TCHAR* MeshPath, const FVector& Loc, const FVector& Scale,
                               const FLinearColor& Color, const FRotator& Rot = FRotator::ZeroRotator)
    {
        UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
        if (!Mesh) return nullptr;
        AStaticMeshActor* A = W->SpawnActor<AStaticMeshActor>(Loc, Rot);
        A->SetMobility(EComponentMobility::Movable);
        UStaticMeshComponent* C = A->GetStaticMeshComponent();
        C->SetStaticMesh(Mesh);
        C->SetRelativeScale3D(Scale);
        if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
                TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
        {
            UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, A);
            MID->SetVectorParameterValue(TEXT("Color"), Color);
            C->SetMaterial(0, MID);
        }
        return A;
    }
}

ALevelSimGameMode::ALevelSimGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass      = ALevelSimPawn::StaticClass();
    HUDClass              = ALevelSimHUD::StaticClass();
}

void ALevelSimGameMode::BeginPlay()
{
    Super::BeginPlay();
    SpawnScene();
    bSmoke = FParse::Param(FCommandLine::Get(), TEXT("levelsim.smoke"));
    if (bSmoke)
    {
        UE_LOG(LogTemp, Display, TEXT("[LevelSimSmoke] smoke mode armed"));
        SmokeNextTime = GetWorld()->GetTimeSeconds() + 2.0;
    }
}

ALevelSimPawn* ALevelSimGameMode::PlayerPawn() const
{
    const APlayerController* PC = GetWorld()->GetFirstPlayerController();
    return PC ? Cast<ALevelSimPawn>(PC->GetPawn()) : nullptr;
}

void ALevelSimGameMode::SpawnScene()
{
    UWorld* W = GetWorld();

    // ---- lights & sky (template-style: sun + atmosphere + realtime-capture skylight) ----
    if (ADirectionalLight* Sun = W->SpawnActor<ADirectionalLight>(FVector(0, 0, 800), FRotator(-50, 35, 0)))
    {
        if (UDirectionalLightComponent* DC = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
        {
            DC->SetMobility(EComponentMobility::Movable);
            DC->SetIntensity(8.f);
        }
    }
    if (UClass* AtmoCls = LoadClass<AActor>(nullptr, TEXT("/Script/Engine.SkyAtmosphere")))
        W->SpawnActor<AActor>(AtmoCls, FVector::ZeroVector, FRotator::ZeroRotator);
    if (ASkyLight* Sky = W->SpawnActor<ASkyLight>(FVector(0, 0, 500), FRotator::ZeroRotator))
    {
        if (USkyLightComponent* SC = Sky->GetLightComponent())
        {
            SC->SetMobility(EComponentMobility::Movable);
            SC->SourceType = ESkyLightSourceType::SLS_CapturedScene;
            SC->SetRealTimeCaptureEnabled(true);
        }
    }

    // ---- ground & site dressing (1 uu = 1 cm) ----
    SpawnBox(W, TEXT("/Engine/BasicShapes/Cube.Cube"), FVector(0, 0, -5),
             FVector(100.f, 100.f, 0.1f), FLinearColor(0.22f, 0.28f, 0.18f));      // 100 m field, top z=0

    // BM brass marker (beside the BM staff which is at kRoute[0]).
    SpawnBox(W, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
             FVector(kRoute[0].xm * 100, kRoute[0].ym * 100 + 30, 1),
             FVector(0.28f, 0.28f, 0.02f), FLinearColor(0.75f, 0.7f, 0.1f));

    // One platform box per turning point that has a non-zero z (so it sits above ground).
    for (int32 i = 1; i < kRouteNumPoints; ++i)
    {
        const RoutePointDef& P = kRoute[i];
        if (FMath::Abs(P.zm) < 1e-6) continue;
        const FVector Base(P.xm * 100, P.ym * 100, 0);
        SpawnBox(W, TEXT("/Engine/BasicShapes/Cube.Cube"),
                 Base + FVector(0, 0, P.zm * 100 * 0.5),
                 FVector(2.6f, 2.6f, (float)P.zm),
                 FLinearColor(0.45f, 0.43f, 0.40f));
    }

    // ---- staffs (one actor per unique route name; revisits share the actor) ----
    auto SpawnStaffActor = [&](double Xm, double Ym, double Zm) -> ALevelStaffActor*
    {
        const FVector Loc(Xm * 100, Ym * 100, Zm * 100);
        const FRotator Yaw(0, FMath::RadiansToDegrees(FMath::Atan2(Ym, Xm)), 0); // face the instrument origin
        ALevelStaffActor* A = W->SpawnActor<ALevelStaffActor>(Loc, Yaw);
        A->BuildStaff(4.0f);
        return A;
    };

    TArray<FRoutePoint>       RoutePoints;
    TArray<ALevelStaffActor*> RouteActors;
    TArray<ALevelStaffActor*> UniqueActors;
    TMap<FString, int32>      NameToUniqueIdx;
    for (int32 i = 0; i < kRouteNumPoints; ++i)
    {
        const RoutePointDef& P = kRoute[i];
        const FString Name = FString(P.name);
        int32 UniqueIdx;
        if (const int32* Hit = NameToUniqueIdx.Find(Name))
        {
            UniqueIdx = *Hit;
        }
        else
        {
            UniqueActors.Add(SpawnStaffActor(P.xm, P.ym, P.zm));
            UniqueIdx = UniqueActors.Num() - 1;
            NameToUniqueIdx.Add(Name, UniqueIdx);
        }
        FRoutePoint Pt;
        Pt.WorldXm = P.xm; Pt.WorldYm = P.ym; Pt.WorldZm = P.zm; Pt.Name = Name;
        RoutePoints.Add(Pt);
        RouteActors.Add(UniqueActors[UniqueIdx]);
    }

    auto InitWithRoute = [this, RoutePoints, RouteActors]()
    {
        if (ALevelSimPawn* Pawn = PlayerPawn())
        {
            Pawn->InitRoute(RoutePoints, RouteActors, kBMElev,
                            kClosureC_Dist, kClosureC_Sta, kByDistance);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[LevelSim] no pawn to init"));
        }
    };

    if (PlayerPawn())
    {
        InitWithRoute();
    }
    else
    {
        // pawn possession can land after BeginPlay; retry next tick
        GetWorldTimerManager().SetTimerForNextTick(
            FTimerDelegate::CreateWeakLambda(this, [InitWithRoute]() { InitWithRoute(); }));
    }

    UE_LOG(LogTemp, Display, TEXT("[LevelSim] route spawned: %d points (%d legs), unique staffs=%d, BM elev=%.2f m"),
           RoutePoints.Num(), FMath::Max(0, RoutePoints.Num() - 1), UniqueActors.Num(), kBMElev);
    for (int32 i = 0; i < RoutePoints.Num(); ++i)
    {
        const FRoutePoint& P = RoutePoints[i];
        UE_LOG(LogTemp, Display, TEXT("[LevelSim]   pt %d: %s @ (%.3f, %.3f, %.3f) m"),
               i, *P.Name, P.WorldXm, P.WorldYm, P.WorldZm);
    }
}

void ALevelSimGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bSmoke && GetWorld()->GetTimeSeconds() >= SmokeNextTime)
        SmokeAdvance();
}

void ALevelSimGameMode::SmokeAdvance()
{
    ALevelSimPawn* Pawn = PlayerPawn();
    if (!Pawn) { SmokeNextTime += 0.5; return; }
    switch (SmokeStep++)
    {
    case 0:
        FScreenshotRequest::RequestScreenshot(TEXT("levelsim_smoke_01_overview"), true, false);
        SmokeNextTime += 1.5; break;
    case 1:
        Pawn->SmokePerturbScrews();
        SmokeNextTime += 1.0; break;
    case 2:
        FScreenshotRequest::RequestScreenshot(TEXT("levelsim_smoke_02_bubble"), true, false);
        SmokeNextTime += 1.5; break;
    case 3:
        Pawn->SmokeAutoLevel();
        SmokeNextTime += 1.0; break;
    case 4:
        FScreenshotRequest::RequestScreenshot(TEXT("levelsim_smoke_03_scope_bm"), true, false);
        SmokeNextTime += 1.5; break;
    case 5:
        Pawn->SmokeAimSecond();
        SmokeNextTime += 1.0; break;
    case 6:
        FScreenshotRequest::RequestScreenshot(TEXT("levelsim_smoke_04_scope_p1"), true, false);
        SmokeNextTime += 1.5; break;
    case 7:
        Pawn->SmokeLogScoringPath();
        SmokeNextTime += 1.0; break;
    // Steps 8-11: multi-station drive (skipped for single-leg routes / N=2 RoutePoints).
    case 8:
        if (Pawn->NumLegs() > 1) Pawn->SmokePerfectSubmitAndAdvance();
        SmokeNextTime += 1.0; break;
    case 9:
        if (Pawn->NumLegs() > 1)
            FScreenshotRequest::RequestScreenshot(TEXT("levelsim_smoke_05_route_stn1_scope"), true, false);
        SmokeNextTime += 1.5; break;
    case 10:
        // Synthetically finish the remaining legs so closure is computed and logged.
        if (Pawn->NumLegs() > 1)
        {
            while (Pawn->CurrentStationIdx < Pawn->NumLegs())
                Pawn->SmokePerfectSubmitAndAdvance();
            Pawn->SmokeLogClosureStatus();
        }
        SmokeNextTime += 1.0; break;
    case 11:
        UE_LOG(LogTemp, Display, TEXT("[LevelSimSmoke] sequence complete, quitting"));
        SmokeNextTime += 0.5; break;
    default:
        if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
            PC->ConsoleCommand(TEXT("quit"));
        bSmoke = false; break;
    }
}
