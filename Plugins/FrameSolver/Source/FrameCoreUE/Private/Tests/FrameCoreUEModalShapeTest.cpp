// v3.5 Phase 3 tests -- 2 sub-checks of AFrameModalShapeActor.
//
//   1. FirstModeShape -- with PhaseSec = 0, the displacement scales by Amplitude * cos(0)
//                        = Amplitude; tip position == Geom.End + Amplitude * shape[tipNode].
//   2. ModeSwitch     -- changing ModeIndex to a different mode visibly shifts tip position
//                        (different shape vector -> different visual displacement).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"

#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCoreUE/FrameCoreUEAnalysisTypes.h"
#include "FrameCoreUE/FrameModalShapeActor.h"
#include "ProceduralMeshComponent.h"
#include "FrameCoreUETestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    using FrameCoreUETestHelpers::GetSpawnWorld;
    using FrameCoreUETestHelpers::TipCenter;

    FFrameModalResult MakeStubModes()
    {
        // 2-node fixture: node 0 = root (no shape motion), node 1 = tip.
        // Mode 0: tip moves +Z by 1.0 unit per amplitude.
        // Mode 1: tip moves +Y by 1.0 unit per amplitude.
        FFrameModalResult R;
        FFrameModeShape M0;
        M0.FreqHz = 10.f; M0.Omega = 2.f * PI * 10.f; M0.Period = 0.1f;
        FFrameNodalDisplacement N0; N0.NodeIndex = 0; N0.NodeId = 0;
        FFrameNodalDisplacement N1; N1.NodeIndex = 1; N1.NodeId = 1; N1.Uz = 1.f;
        M0.Shape = { N0, N1 };

        FFrameModeShape M1;
        M1.FreqHz = 20.f; M1.Omega = 2.f * PI * 20.f; M1.Period = 0.05f;
        FFrameNodalDisplacement N1b; N1b.NodeIndex = 1; N1b.NodeId = 1; N1b.Uy = 1.f;
        M1.Shape = { N0, N1b };

        R.Modes = { M0, M1 };
        return R;
    }

}

// --- 1. FirstModeShape -------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEModalShapeFirstModeTest,
    "FrameCore.UE.ModalShape.FirstMode",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEModalShapeFirstModeTest::RunTest(const FString& /*Parameters*/)
{
    UWorld* World = GetSpawnWorld();
    if (!World) return false;

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFrameModalShapeActor* Actor =
        World->SpawnActor<AFrameModalShapeActor>(AFrameModalShapeActor::StaticClass(),
                                                 FVector::ZeroVector, FRotator::ZeroRotator, SP);
    TestNotNull(TEXT("Actor spawned"), Actor);
    if (!Actor) return false;

    Actor->Modes = MakeStubModes();
    FFrameMemberGeometry G;
    G.MemberIdx = 0;
    G.Start = FVector::ZeroVector; G.End = FVector(2000.f, 0.f, 0.f);
    G.Width = 100.f; G.Depth = 100.f;
    G.EndINodeIdx = 0; G.EndJNodeIdx = 1;
    Actor->MemberGeometry = { G };
    Actor->ModeIndex = 0;
    Actor->Amplitude = 250.f;

    TestTrue(TEXT("BuildAtPhase(0) succeeds"), Actor->BuildAtPhase(0.f));
    const FProcMeshSection* Sec = Actor->GetMeshComponent()->GetProcMeshSection(0);
    TestNotNull(TEXT("Section 0 retrievable"), Sec);
    if (!Sec) { Actor->Destroy(); return false; }

    const FVector T0 = TipCenter(Sec);
    // shape[1].Uz = 1, Amplitude = 250, cos(0) = 1, so tip Z += 250.
    TestTrue(FString::Printf(TEXT("Tip Z at phase 0 == 250 (got %.4f)"), T0.Z),
             FMath::IsNearlyEqual(T0.Z, 250.f, 1.f));

    // At PhaseSec = 1 / (2 * FreqHz) = 1/20 s, cos(2*pi*10*0.05) = cos(pi) = -1 -> tip Z = -250.
    TestTrue(TEXT("BuildAtPhase(0.05) succeeds"), Actor->BuildAtPhase(0.05f));
    Sec = Actor->GetMeshComponent()->GetProcMeshSection(0);
    if (!Sec) { Actor->Destroy(); return false; }
    const FVector T_half = TipCenter(Sec);
    TestTrue(FString::Printf(TEXT("Tip Z at half period == -250 (got %.4f)"), T_half.Z),
             FMath::IsNearlyEqual(T_half.Z, -250.f, 2.f));

    Actor->Destroy();
    return true;
}

// --- 2. ModeSwitch -----------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEModalShapeSwitchTest,
    "FrameCore.UE.ModalShape.ModeSwitch",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEModalShapeSwitchTest::RunTest(const FString& /*Parameters*/)
{
    UWorld* World = GetSpawnWorld();
    if (!World) return false;

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFrameModalShapeActor* Actor =
        World->SpawnActor<AFrameModalShapeActor>(AFrameModalShapeActor::StaticClass(),
                                                 FVector::ZeroVector, FRotator::ZeroRotator, SP);
    if (!Actor) return false;

    Actor->Modes = MakeStubModes();
    FFrameMemberGeometry G;
    G.MemberIdx = 0;
    G.Start = FVector::ZeroVector; G.End = FVector(2000.f, 0.f, 0.f);
    G.Width = 100.f; G.Depth = 100.f;
    G.EndINodeIdx = 0; G.EndJNodeIdx = 1;
    Actor->MemberGeometry = { G };
    Actor->Amplitude = 250.f;

    // Mode 0 -> tip moves +Z
    Actor->ModeIndex = 0;
    Actor->BuildAtPhase(0.f);
    const FProcMeshSection* S0 = Actor->GetMeshComponent()->GetProcMeshSection(0);
    const FVector T_mode0 = TipCenter(S0);
    TestTrue(TEXT("Mode 0: tip Z displacement > 100"), T_mode0.Z > 100.f);

    // Mode 1 -> tip moves +Y
    Actor->ModeIndex = 1;
    Actor->BuildAtPhase(0.f);
    const FProcMeshSection* S1 = Actor->GetMeshComponent()->GetProcMeshSection(0);
    const FVector T_mode1 = TipCenter(S1);
    TestTrue(FString::Printf(TEXT("Mode 1: tip Y > 100 (got %.4f)"), T_mode1.Y),
             T_mode1.Y > 100.f);
    TestTrue(FString::Printf(TEXT("Mode 1: tip Z near 0 (got %.4f)"), T_mode1.Z),
             FMath::Abs(T_mode1.Z) < 5.f);

    Actor->Destroy();
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
