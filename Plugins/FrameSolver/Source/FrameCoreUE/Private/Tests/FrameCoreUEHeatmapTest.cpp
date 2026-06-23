// v3.5 Phase 2 tests -- 3 sub-checks of AFrameUtilizationHeatmapActor.
//
//   1. CantileverDC    -- cantilever root member, expect member colour at the red end of
//                         the ramp (R > 0.5) when SaturationDC = MaxDC.
//   2. UnstressedModel -- zero load, all member Risk == 0 -> member colour at blue end
//                         (B > 0.9, R < 0.1).
//   3. VertexCount     -- 2 members + 1 shell -> exactly 3 PMC sections, member sections
//                         have 11*4 = 44 verts, shell section has 4 verts.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"

#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCoreUE/FrameCoreUEResultTypes.h"
#include "FrameCoreUE/FrameCoreUEVisualTypes.h"
#include "FrameCoreUE/FrameUtilizationHeatmapActor.h"
#include "ProceduralMeshComponent.h"
#include "FrameCoreUETestHelpers.h"

#include "FrameCore/FrameTypes.h"
#include "FrameCore/Node.h"
#include "FrameCore/Member.h"
#include "FrameCore/Material.h"
#include "FrameCore/Section.h"
#include "FrameCore/Load.h"
#include "FrameCore/FrameModel.h"
#include "FrameCore/FrameSolver.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    using FrameCoreUETestHelpers::GetSpawnWorld;

    frame::FrameModel BuildCantilever(double P)
    {
        using namespace frame;
        Material mat(210000.0, 80769.0, 7850.0);
        mat.cap = Capacity::make(300.0, 300.0, 180.0);
        Section sec = Section::Rectangular(100.0, 100.0);

        FrameModel m;
        m.materials = { mat };
        m.sections  = { sec };
        Node n0(0, 0, 0, 0); n0.fixAll();
        Node n1(1, 2000.0, 0, 0);
        m.nodes   = { n0, n1 };
        m.members = { Member(0, 0, 1, 0, 0) };
        if (P != 0.0)
        {
            NodalLoad nl; nl.node = 1; nl.comp[Uz] = P;
            m.nodalLoads = { nl };
        }
        return m;
    }
}

// --- 1. CantileverDC ---------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEHeatmapCantileverDCTest,
    "FrameCore.UE.Heatmap.CantileverDC",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEHeatmapCantileverDCTest::RunTest(const FString& /*Parameters*/)
{
    const frame::FrameModel M = BuildCantilever(1000.0);
    const frame::SolveResult R = frame::solve(M);
    const FFrameSolveResult BP = FrameCoreUE::ToBlueprint(M, R);

    UWorld* World = GetSpawnWorld();
    if (!World) return false;

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFrameUtilizationHeatmapActor* Actor =
        World->SpawnActor<AFrameUtilizationHeatmapActor>(
            AFrameUtilizationHeatmapActor::StaticClass(),
            FVector::ZeroVector, FRotator::ZeroRotator, SP);
    TestNotNull(TEXT("Actor spawned"), Actor);
    if (!Actor) return false;

    Actor->Solution = BP;
    FFrameMemberGeometry G;
    G.MemberIdx = 0; G.Start = FVector::ZeroVector; G.End = FVector(2000.f, 0.f, 0.f);
    G.Width = 100.f; G.Depth = 100.f;
    Actor->MemberGeometry = { G };
    // Pin saturation to the peak risk so the colour is right at the red end of the ramp.
    const float PeakRisk = (BP.MemberUtilization.Num() > 0) ? BP.MemberUtilization[0].Peak.Risk : 1.f;
    TestTrue(TEXT("Cantilever peak risk > 0"), PeakRisk > 0.f);
    Actor->SaturationDC = PeakRisk;

    TestTrue(TEXT("BuildHeatmap succeeds"), Actor->BuildHeatmap());
    const FProcMeshSection* Sec = Actor->GetMeshComponent()->GetProcMeshSection(0);
    TestNotNull(TEXT("Section 0 retrievable"), Sec);
    if (!Sec || Sec->ProcVertexBuffer.Num() == 0) { Actor->Destroy(); return false; }

    const FColor Col = Sec->ProcVertexBuffer[0].Color;
    TestTrue(FString::Printf(TEXT("Member colour at red end (got R=%d G=%d B=%d)"),
                              Col.R, Col.G, Col.B),
             Col.R > 128 && Col.B < 64);

    Actor->Destroy();
    return true;
}

// --- 2. UnstressedModel ------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEHeatmapUnstressedTest,
    "FrameCore.UE.Heatmap.Unstressed",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEHeatmapUnstressedTest::RunTest(const FString& /*Parameters*/)
{
    UWorld* World = GetSpawnWorld();
    if (!World) return false;

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFrameUtilizationHeatmapActor* Actor =
        World->SpawnActor<AFrameUtilizationHeatmapActor>(
            AFrameUtilizationHeatmapActor::StaticClass(),
            FVector::ZeroVector, FRotator::ZeroRotator, SP);
    if (!Actor) return false;

    // No Solution.MemberUtilization -> GetMemberRisk = 0 -> ramp(0) = pure blue.
    FFrameMemberGeometry G;
    G.MemberIdx = 0; G.Start = FVector::ZeroVector; G.End = FVector(1000.f, 0.f, 0.f);
    G.Width = 100.f; G.Depth = 100.f;
    Actor->MemberGeometry  = { G };
    Actor->SaturationDC    = 1.0f;
    TestTrue(TEXT("BuildHeatmap succeeds"), Actor->BuildHeatmap());

    const FProcMeshSection* Sec = Actor->GetMeshComponent()->GetProcMeshSection(0);
    TestNotNull(TEXT("Section 0 retrievable"), Sec);
    if (!Sec || Sec->ProcVertexBuffer.Num() == 0) { Actor->Destroy(); return false; }

    const FColor Col = Sec->ProcVertexBuffer[0].Color;
    TestTrue(FString::Printf(TEXT("Unstressed colour blue end (got R=%d B=%d)"), Col.R, Col.B),
             Col.B > 230 && Col.R < 32);

    Actor->Destroy();
    return true;
}

// --- 3. VertexCount ----------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEHeatmapVertexCountTest,
    "FrameCore.UE.Heatmap.VertexCount",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEHeatmapVertexCountTest::RunTest(const FString& /*Parameters*/)
{
    UWorld* World = GetSpawnWorld();
    if (!World) return false;

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFrameUtilizationHeatmapActor* Actor =
        World->SpawnActor<AFrameUtilizationHeatmapActor>(
            AFrameUtilizationHeatmapActor::StaticClass(),
            FVector::ZeroVector, FRotator::ZeroRotator, SP);
    if (!Actor) return false;

    for (int32 i = 0; i < 2; ++i)
    {
        FFrameMemberGeometry G;
        G.MemberIdx = i;
        G.Start     = FVector(0.f, 0.f, (float)i * 500.f);
        G.End       = FVector(1000.f, 0.f, (float)i * 500.f);
        G.Width = 100.f; G.Depth = 100.f;
        Actor->MemberGeometry.Add(G);
    }
    FFrameShellGeometry S;
    S.ShellIdx = 0;
    S.Corners = { FVector(0.f, 0.f, 0.f), FVector(1000.f, 0.f, 0.f),
                  FVector(1000.f, 1000.f, 0.f), FVector(0.f, 1000.f, 0.f) };
    S.CornerNodeIndices = { 0, 1, 2, 3 };
    Actor->ShellGeometry = { S };

    TestTrue(TEXT("BuildHeatmap succeeds"), Actor->BuildHeatmap());
    UProceduralMeshComponent* PMC = Actor->GetMeshComponent();
    TestEqual(TEXT("PMC section count == 3"), PMC->GetNumSections(), 3);

    const FProcMeshSection* M0 = PMC->GetProcMeshSection(0);
    const FProcMeshSection* M1 = PMC->GetProcMeshSection(1);
    const FProcMeshSection* SH = PMC->GetProcMeshSection(2);
    TestNotNull(TEXT("Member 0 section"), M0);
    TestNotNull(TEXT("Member 1 section"), M1);
    TestNotNull(TEXT("Shell section"), SH);
    if (M0) TestEqual(TEXT("Member 0 verts == 44"), M0->ProcVertexBuffer.Num(), 44);
    if (M1) TestEqual(TEXT("Member 1 verts == 44"), M1->ProcVertexBuffer.Num(), 44);
    if (SH) TestEqual(TEXT("Shell verts == 4"),     SH->ProcVertexBuffer.Num(), 4);

    Actor->Destroy();
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
