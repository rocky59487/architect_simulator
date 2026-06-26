// v3.6 Phase 5 tests for AFrameRedundancyFieldActor (C8 redundancy sample).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Engine/GameInstance.h"

#include "FrameCoreUE/FrameRedundancyFieldActor.h"
#include "FrameCoreUE/FrameInteractiveSubsystem.h"
#include "FrameCoreUE/FrameCoreUEModelTypes.h"
#include "FrameCoreUE/FrameCoreUEResultTypes.h"
#include "FrameCoreUE/FrameCoreUEAnalysisTypes.h"
#include "FrameCoreUE/FrameCoreUEVisualTypes.h"
#include "FrameCoreUETestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    using FrameCoreUETestHelpers::GetSpawnWorld;

    UFrameInteractiveSubsystem* GetSub()
    {
        if (GEngine)
        {
            for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
            {
                if (Ctx.OwningGameInstance)
                {
                    if (UFrameInteractiveSubsystem* S =
                            Ctx.OwningGameInstance->GetSubsystem<UFrameInteractiveSubsystem>())
                    {
                        return S;
                    }
                }
            }
        }
        // AS-24: GetTransientPackage() outer suppresses ClassWithin (UGameInstance)
        // ensure() fired in UObjectGlobals.cpp when outer=null for a ClassWithin-constrained
        // class. In isolated single-test runs the ensure cascades to NotNull.cpp fatal.
        return NewObject<UFrameInteractiveSubsystem>(GetTransientPackage());
    }

    AFrameRedundancyFieldActor* SpawnActor(UWorld* W)
    {
        FActorSpawnParameters SP;
        SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return W->SpawnActor<AFrameRedundancyFieldActor>(
            AFrameRedundancyFieldActor::StaticClass(),
            FVector::ZeroVector, FRotator::ZeroRotator, SP);
    }

    FFrameModelDef BuildCantileverDef()
    {
        FFrameModelDef Def;
        FFrameMaterial Mat;
        Mat.E = 210000.f; Mat.G = 80769.f; Mat.Nu = 0.3f; Mat.Rho = 7850.f; Mat.Fy = 235.f;
        Mat.Cap.Comp = 300.f; Mat.Cap.Tens = 300.f; Mat.Cap.Shear = 180.f;
        Def.Materials = { Mat };
        FFrameSection Sec;
        Sec.A = 10000.f; Sec.Iy = 8.333333e6f; Sec.Iz = 8.333333e6f;
        Sec.J = 1.4e7f; Sec.Zy = 250000.f; Sec.Zz = 250000.f;
        Sec.Shape = EFrameSectionShape::Rectangular;
        Def.Sections = { Sec };
        FFrameNode N0; N0.Id = 0; N0.Pos = FVector::ZeroVector;
        N0.Fixed = { true, true, true, true, true, true };
        N0.Prescribed = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
        FFrameNode N1; N1.Id = 1; N1.Pos = FVector(2000.f, 0.f, 0.f);
        N1.Fixed = { false, false, false, false, false, false };
        N1.Prescribed = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
        Def.Nodes = { N0, N1 };
        FFrameMember M; M.Id = 0; M.I = 0; M.J = 1; M.MatIdx = 0; M.SecIdx = 0;
        M.Release.Init(false, 12);
        Def.Members = { M };
        FFrameNodalLoad L; L.Node = 1; L.Comp = { 0.f, 0.f, 1000.f, 0.f, 0.f, 0.f };
        Def.NodalLoads = { L };
        return Def;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUERedundancySingleMemberTest,
    "FrameCore.UE.Redundancy.SingleMember",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUERedundancySingleMemberTest::RunTest(const FString&)
{
    UFrameInteractiveSubsystem* Sub = GetSub();
    if (!Sub) { AddError(TEXT("Subsystem unavailable")); return false; }
    FFrameModelDef Def = BuildCantileverDef();
    FFrameSolveOptions Opts;
    FFrameReanalysisOptions ReOpts;
    FString Err;
    TestTrue(TEXT("Start"), Sub->StartSession(Def, Opts, ReOpts, Err));

    UWorld* W = GetSpawnWorld(); if (!W) { Sub->EndSession(); return false; }
    AFrameRedundancyFieldActor* A = SpawnActor(W);
    A->Subsystem = Sub;
    A->WatchedMemberIds = { 0 };
    FFrameMemberGeometry G;
    G.MemberIdx = 0; G.Start = FVector::ZeroVector; G.End = FVector(2000.f, 0.f, 0.f);
    G.Width = 100.f; G.Depth = 100.f;
    A->MemberGeometry = { G };

    const int32 N = A->ComputeRedundancy();
    TestEqual(TEXT("Probed 1 member"), N, 1);
    TestEqual(TEXT("LastJumps has 1 entry"), A->LastJumps.Num(), 1);
    if (A->LastJumps.Num() >= 1)
    {
        // Removing the only member -> mechanism -> +inf jump.
        TestTrue(FString::Printf(TEXT("single-member jump huge (got %g)"), A->LastJumps[0]),
                 A->LastJumps[0] > 1e6f);
    }

    TestTrue(TEXT("BuildMesh"), A->BuildMesh());
    TestEqual(TEXT("1 PMC section"), A->GetMeshComponent()->GetNumSections(), 1);

    A->Destroy();
    Sub->EndSession();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUERedundancyNoSubsystemTest,
    "FrameCore.UE.Redundancy.NoSubsystem",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUERedundancyNoSubsystemTest::RunTest(const FString&)
{
    UWorld* W = GetSpawnWorld(); if (!W) return false;
    AFrameRedundancyFieldActor* A = SpawnActor(W);
    A->Subsystem = nullptr;
    A->WatchedMemberIds = { 0 };
    const int32 N = A->ComputeRedundancy();
    TestEqual(TEXT("No subsystem -> 0 probes"), N, 0);
    TestEqual(TEXT("No subsystem -> empty jumps"), A->LastJumps.Num(), 0);
    A->Destroy();
    return true;
}

#endif
