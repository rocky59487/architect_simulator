// v3.5 Phase 6 test -- 1 sub-check of AFrameInfluenceLineActor.
//
//   1. SSBeamMidspanMoment -- SS beam unit influence line peaks at midspan; ribbon top vert
//                             height matches Line.ReactionAtPosition[midspan] * HeightScale.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"

#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCoreUE/FrameCoreUEAnalysisTypes.h"
#include "FrameCoreUE/FrameInfluenceLineActor.h"
#include "ProceduralMeshComponent.h"
#include "FrameCoreUETestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    using FrameCoreUETestHelpers::GetSpawnWorld;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEInfluenceLineSSBeamTest,
    "FrameCore.UE.InfluenceLine.SSBeamMidspanMoment",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEInfluenceLineSSBeamTest::RunTest(const FString& /*Parameters*/)
{
    UWorld* World = GetSpawnWorld();
    if (!World) return false;

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFrameInfluenceLineActor* Actor =
        World->SpawnActor<AFrameInfluenceLineActor>(AFrameInfluenceLineActor::StaticClass(),
                                                    FVector::ZeroVector, FRotator::ZeroRotator, SP);
    TestNotNull(TEXT("Actor spawned"), Actor);
    if (!Actor) return false;

    // SS beam, span L = 1000 mm, 5 evenly spaced load positions, midspan-moment influence:
    // triangular: 0 -> L/4 (=250) at midspan -> 0. Sample at x = 0, 250, 500, 750, 1000.
    Actor->Line.ReactNode = 1; Actor->Line.ReactDof = 5;     // Mz at midspan
    Actor->Line.LoadNodes = { 0, 1, 2, 3, 4 };
    Actor->Line.ReactionAtPosition = { 0.f, 125.f, 250.f, 125.f, 0.f };
    Actor->HeightScale = 1.f;
    for (int32 i = 0; i < 5; ++i)
    {
        FFrameMemberGeometry G;
        G.MemberIdx = i;
        G.Start = FVector((float)i * 250.f, 0.f, 0.f);
        G.End   = FVector((float)i * 250.f + 250.f, 0.f, 0.f);
        Actor->PathGeometry.Add(G);
    }
    TestTrue(TEXT("BuildMesh succeeds"), Actor->BuildMesh());

    UProceduralMeshComponent* PMC = Actor->GetMeshComponent();
    const FProcMeshSection* Sec = PMC->GetProcMeshSection(0);
    TestNotNull(TEXT("Section 0"), Sec);
    if (!Sec) { Actor->Destroy(); return false; }

    TestEqual(TEXT("Vertex count == 10 (5 path steps * 2 strip verts)"),
              Sec->ProcVertexBuffer.Num(), 10);
    if (Sec->ProcVertexBuffer.Num() >= 6)
    {
        // Step 2 (midspan) top vertex Z should be 250 (== ReactionAtPosition[2] * HeightScale).
        const float MidTopZ = Sec->ProcVertexBuffer[2 * 2 + 1].Position.Z;
        TestTrue(FString::Printf(TEXT("Midspan top Z == 250 (got %.4f)"), MidTopZ),
                 FMath::IsNearlyEqual(MidTopZ, 250.f, 0.1f));
        // Step 0 top should be at 0 (endpoint).
        const float EndTopZ = Sec->ProcVertexBuffer[0 * 2 + 1].Position.Z;
        TestTrue(FString::Printf(TEXT("End top Z == 0 (got %.4f)"), EndTopZ),
                 FMath::Abs(EndTopZ) < 0.1f);
    }

    Actor->Destroy();
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
