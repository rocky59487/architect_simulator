// v3.5 Phase 8 tests -- 4 sub-checks (2 per actor).
//
//   Response spectrum:
//     1. EnvelopePulse  -- BuildAtPhase(0) -> Amplitude * cos(0) = Amplitude; tip Uz = peak Uz.
//     2. Static         -- BuildAtPhase(0) -> mesh section present; empty PeakDisplacements
//                         -> tip == undeformed.
//
//   Real-time dynamic:
//     3. EndToEndPlayback -- 3-step history, scrub to last step time -> tip == last disp.
//     4. Midstep         -- scrub to t midway -> tip == lerp of two snapshots.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"

#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCoreUE/FrameCoreUEAnalysisTypes.h"
#include "FrameCoreUE/FrameResponseSpectrumActor.h"
#include "FrameCoreUE/FrameRealTimeDynamicActor.h"
#include "ProceduralMeshComponent.h"
#include "FrameCoreUETestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    using FrameCoreUETestHelpers::GetSpawnWorld;
    using FrameCoreUETestHelpers::TipCenter;

    FFrameMemberGeometry MakeCantileverGeom()
    {
        FFrameMemberGeometry G;
        G.MemberIdx = 0;
        G.Start = FVector::ZeroVector;
        G.End   = FVector(2000.f, 0.f, 0.f);
        G.Width = 100.f; G.Depth = 100.f;
        G.EndINodeIdx = 0; G.EndJNodeIdx = 1;
        return G;
    }
}

// === 1. ResponseSpectrum EnvelopePulse ======================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEResponseSpectrumEnvelopeTest,
    "FrameCore.UE.ResponseSpectrum.EnvelopePulse",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEResponseSpectrumEnvelopeTest::RunTest(const FString& /*Parameters*/)
{
    UWorld* World = GetSpawnWorld();
    if (!World) return false;
    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFrameResponseSpectrumActor* Actor =
        World->SpawnActor<AFrameResponseSpectrumActor>(AFrameResponseSpectrumActor::StaticClass(),
                                                       FVector::ZeroVector, FRotator::ZeroRotator, SP);
    if (!Actor) return false;

    FFrameNodalDisplacement P0; P0.NodeIndex = 0; P0.NodeId = 0;
    FFrameNodalDisplacement P1; P1.NodeIndex = 1; P1.NodeId = 1; P1.Uz = 5.f;
    Actor->Response.PeakDisplacements = { P0, P1 };
    Actor->MemberGeometry = { MakeCantileverGeom() };
    Actor->Amplitude = 50.f;     // 5 * 50 = 250
    Actor->EnvelopeHz = 0.5f;

    Actor->BuildAtPhase(0.f);
    const FProcMeshSection* Sec = Actor->GetMeshComponent()->GetProcMeshSection(0);
    if (!Sec) { Actor->Destroy(); return false; }
    const FVector Tip = TipCenter(Sec);
    TestTrue(FString::Printf(TEXT("Phase 0 tip Z == 250 (got %.4f)"), Tip.Z),
             FMath::IsNearlyEqual(Tip.Z, 250.f, 1.f));

    Actor->Destroy();
    return true;
}

// === 2. ResponseSpectrum Static (empty Peak) ===============================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEResponseSpectrumStaticTest,
    "FrameCore.UE.ResponseSpectrum.Static",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEResponseSpectrumStaticTest::RunTest(const FString& /*Parameters*/)
{
    UWorld* World = GetSpawnWorld();
    if (!World) return false;
    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFrameResponseSpectrumActor* Actor =
        World->SpawnActor<AFrameResponseSpectrumActor>(AFrameResponseSpectrumActor::StaticClass(),
                                                       FVector::ZeroVector, FRotator::ZeroRotator, SP);
    if (!Actor) return false;

    Actor->MemberGeometry = { MakeCantileverGeom() };
    Actor->BuildAtPhase(0.f);
    UProceduralMeshComponent* PMC = Actor->GetMeshComponent();
    TestEqual(TEXT("1 section built"), PMC->GetNumSections(), 1);
    const FProcMeshSection* Sec = PMC->GetProcMeshSection(0);
    if (!Sec) { Actor->Destroy(); return false; }
    const FVector Tip = TipCenter(Sec);
    TestTrue(FString::Printf(TEXT("Empty peak -> undeformed (tip Z=%.4f)"), Tip.Z),
             FMath::Abs(Tip.Z) < 1e-3f);

    Actor->Destroy();
    return true;
}

// === 3. RealTimeDynamic EndToEndPlayback ===================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUERealTimeDynamicPlaybackTest,
    "FrameCore.UE.RealTimeDynamic.Playback",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUERealTimeDynamicPlaybackTest::RunTest(const FString& /*Parameters*/)
{
    UWorld* World = GetSpawnWorld();
    if (!World) return false;
    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFrameRealTimeDynamicActor* Actor =
        World->SpawnActor<AFrameRealTimeDynamicActor>(AFrameRealTimeDynamicActor::StaticClass(),
                                                      FVector::ZeroVector, FRotator::ZeroRotator, SP);
    if (!Actor) return false;

    // 3 steps: t=0 (Uz=0), t=0.5 (Uz=100), t=1 (Uz=200).
    FFrameModalTimeStep S0; S0.StepIndex = 0; S0.Time = 0.f;
    FFrameModalTimeStep S1; S1.StepIndex = 1; S1.Time = 0.5f;
    FFrameModalTimeStep S2; S2.StepIndex = 2; S2.Time = 1.f;
    FFrameNodalDisplacement N0; N0.NodeIndex = 0; N0.NodeId = 0;
    {
        FFrameNodalDisplacement N1a; N1a.NodeIndex = 1; N1a.NodeId = 1; N1a.Uz = 0.f;   S0.Displacements = { N0, N1a };
        FFrameNodalDisplacement N1b; N1b.NodeIndex = 1; N1b.NodeId = 1; N1b.Uz = 100.f; S1.Displacements = { N0, N1b };
        FFrameNodalDisplacement N1c; N1c.NodeIndex = 1; N1c.NodeId = 1; N1c.Uz = 200.f; S2.Displacements = { N0, N1c };
    }
    Actor->History.Steps = { S0, S1, S2 };
    Actor->MemberGeometry = { MakeCantileverGeom() };
    Actor->DeflectionScale = 1.f;

    Actor->SetPlaybackTime(1.f);
    const FProcMeshSection* Sec = Actor->GetMeshComponent()->GetProcMeshSection(0);
    if (!Sec) { Actor->Destroy(); return false; }
    const FVector Tip = TipCenter(Sec);
    TestTrue(FString::Printf(TEXT("End step tip Z == 200 (got %.4f)"), Tip.Z),
             FMath::IsNearlyEqual(Tip.Z, 200.f, 1.f));

    Actor->SetPlaybackTime(0.f);
    Sec = Actor->GetMeshComponent()->GetProcMeshSection(0);
    const FVector Tip0 = TipCenter(Sec);
    TestTrue(TEXT("First step tip Z == 0"), FMath::Abs(Tip0.Z) < 1e-3f);

    Actor->Destroy();
    return true;
}

// === 4. RealTimeDynamic Midstep =============================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUERealTimeDynamicMidstepTest,
    "FrameCore.UE.RealTimeDynamic.Midstep",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUERealTimeDynamicMidstepTest::RunTest(const FString& /*Parameters*/)
{
    UWorld* World = GetSpawnWorld();
    if (!World) return false;
    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFrameRealTimeDynamicActor* Actor =
        World->SpawnActor<AFrameRealTimeDynamicActor>(AFrameRealTimeDynamicActor::StaticClass(),
                                                      FVector::ZeroVector, FRotator::ZeroRotator, SP);
    if (!Actor) return false;

    FFrameModalTimeStep S0; S0.StepIndex = 0; S0.Time = 0.f;
    FFrameModalTimeStep S1; S1.StepIndex = 1; S1.Time = 1.f;
    FFrameNodalDisplacement N0; N0.NodeIndex = 0; N0.NodeId = 0;
    FFrameNodalDisplacement N1a; N1a.NodeIndex = 1; N1a.NodeId = 1; N1a.Uz = 0.f;
    FFrameNodalDisplacement N1b; N1b.NodeIndex = 1; N1b.NodeId = 1; N1b.Uz = 200.f;
    S0.Displacements = { N0, N1a };
    S1.Displacements = { N0, N1b };
    Actor->History.Steps = { S0, S1 };
    Actor->MemberGeometry = { MakeCantileverGeom() };
    Actor->DeflectionScale = 1.f;

    // Midway: t = 0.5 -> tip Uz = lerp(0, 200, 0.5) = 100.
    Actor->SetPlaybackTime(0.5f);
    const FProcMeshSection* Sec = Actor->GetMeshComponent()->GetProcMeshSection(0);
    if (!Sec) { Actor->Destroy(); return false; }
    const FVector Tip = TipCenter(Sec);
    TestTrue(FString::Printf(TEXT("Midway tip Z == 100 (got %.4f)"), Tip.Z),
             FMath::IsNearlyEqual(Tip.Z, 100.f, 1.f));

    Actor->Destroy();
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
