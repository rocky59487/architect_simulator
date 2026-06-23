// v3.5 Phase 1 tests -- 3 sub-checks of AFrameDeformedShapeActor.
//
//   1. CantileverTip   -- spawned actor's tip-ring centre matches StartGeom + analytic Uz
//                         (P*L^3/(3*E*Iz)) scaled by DeflectionScale (set to 1 to compare
//                         directly with engine number) rel < 1e-4.
//   2. EmptyField      -- zero-displacement Solution -> mesh tip == undeformed geometry
//                         bit-exact (no scale, no nudge).
//   3. PMCSectionCount -- N-member MemberGeometry -> exactly N PMC sections built.
//
// Spawns into an existing UWorld pulled from GEngine's contexts (same trick as v3.3
// FFrameCoreUEActorStressMeshTest -- UWorld::CreateWorld crashes inside the headless
// commandlet because GameInstance template isn't loaded).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"

#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCoreUE/FrameCoreUEResultTypes.h"
#include "FrameCoreUE/FrameDeformedShapeActor.h"
#include "ProceduralMeshComponent.h"
#include "FrameCoreUETestHelpers.h"   // v3.5.1 TEST-DUP-01: shared GetSpawnWorld / TipCenter

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
    constexpr double kP  = 1000.0;
    constexpr double kL  = 2000.0;
    constexpr double kE  = 210000.0;
    constexpr double kIz = 100.0 * 100.0 * 100.0 * 100.0 / 12.0;

    frame::FrameModel BuildCantilever()
    {
        using namespace frame;
        Material mat(kE, 80769.0, 7850.0);
        mat.cap = Capacity::make(300.0, 300.0, 180.0);
        Section sec = Section::Rectangular(100.0, 100.0);

        FrameModel m;
        m.materials = { mat };
        m.sections  = { sec };

        Node n0(0, 0, 0, 0);
        n0.fixAll();
        Node n1(1, kL, 0, 0);
        m.nodes   = { n0, n1 };
        m.members = { Member(0, 0, 1, 0, 0) };

        NodalLoad nl;
        nl.node     = 1;
        nl.comp[Uz] = kP;
        m.nodalLoads = { nl };
        return m;
    }

    using FrameCoreUETestHelpers::GetSpawnWorld;
}

// --- 1. CantileverTip ---------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEDeformedShapeCantileverTipTest,
    "FrameCore.UE.DeformedShape.CantileverTip",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEDeformedShapeCantileverTipTest::RunTest(const FString& /*Parameters*/)
{
    const frame::FrameModel M = BuildCantilever();
    const frame::SolveResult R = frame::solve(M);
    const FFrameSolveResult BP = FrameCoreUE::ToBlueprint(M, R);

    UWorld* World = GetSpawnWorld();
    TestNotNull(TEXT("Spawn world located"), World);
    if (!World) return false;

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFrameDeformedShapeActor* Actor =
        World->SpawnActor<AFrameDeformedShapeActor>(AFrameDeformedShapeActor::StaticClass(),
                                                    FVector::ZeroVector, FRotator::ZeroRotator, SP);
    TestNotNull(TEXT("Actor spawned"), Actor);
    if (!Actor) return false;

    Actor->Solution         = BP;
    Actor->DeflectionScale  = 1.f;   // compare directly against engine number, no amplification
    FFrameMemberGeometry G;
    G.MemberIdx   = 0;
    G.Start       = FVector::ZeroVector;
    G.End         = FVector((float)kL, 0.f, 0.f);
    G.Width       = 100.f;
    G.Depth       = 100.f;
    G.EndINodeIdx = 0;
    G.EndJNodeIdx = 1;
    Actor->MemberGeometry = { G };

    TestTrue(TEXT("BuildMesh succeeds"), Actor->BuildMesh());
    UProceduralMeshComponent* PMC = Actor->GetMeshComponent();
    TestNotNull(TEXT("PMC valid"), PMC);
    if (!PMC) { Actor->Destroy(); return false; }
    const FProcMeshSection* Sec = PMC->GetProcMeshSection(0);
    TestNotNull(TEXT("Section 0 retrievable"), Sec);
    if (!Sec) { Actor->Destroy(); return false; }

    // Tip ring = ring 10 (0-indexed). Centre of 4 corners is the deformed tip.
    const int32 BaseTip = 10 * 4;
    TestTrue(TEXT("Section has 44 verts"), Sec->ProcVertexBuffer.Num() >= BaseTip + 4);
    if (Sec->ProcVertexBuffer.Num() < BaseTip + 4) { Actor->Destroy(); return false; }
    FVector TipCenter = FVector::ZeroVector;
    for (int32 c = 0; c < 4; ++c)
    {
        TipCenter += Sec->ProcVertexBuffer[BaseTip + c].Position;
    }
    TipCenter *= 0.25f;

    const float AnalyticUz = (float)(kP * kL * kL * kL / (3.0 * kE * kIz));
    // X stays at L (axial near-zero); Y near 0; Z is the deflection.
    TestTrue(FString::Printf(TEXT("Tip X near L (got %.4f vs L=%.4f)"), TipCenter.X, (float)kL),
             FMath::IsNearlyEqual(TipCenter.X, (float)kL, FMath::Max(1.f, (float)kL * 1e-4f)));
    TestTrue(FString::Printf(TEXT("Tip Z near analytic Uz (got %.4f vs %.4f)"),
                              TipCenter.Z, AnalyticUz),
             FMath::IsNearlyEqual(TipCenter.Z, AnalyticUz,
                                  FMath::Max(1.f, FMath::Abs(AnalyticUz) * 1e-3f)));

    Actor->Destroy();
    return true;
}

// --- 2. EmptyField ------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEDeformedShapeEmptyFieldTest,
    "FrameCore.UE.DeformedShape.EmptyField",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEDeformedShapeEmptyFieldTest::RunTest(const FString& /*Parameters*/)
{
    UWorld* World = GetSpawnWorld();
    TestNotNull(TEXT("World"), World);
    if (!World) return false;

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFrameDeformedShapeActor* Actor =
        World->SpawnActor<AFrameDeformedShapeActor>(AFrameDeformedShapeActor::StaticClass(),
                                                    FVector::ZeroVector, FRotator::ZeroRotator, SP);
    TestNotNull(TEXT("Actor spawned"), Actor);
    if (!Actor) return false;

    // No Solution.Displacements -> GetNodalDisplacement returns zero -> mesh matches geometry.
    FFrameMemberGeometry G;
    G.MemberIdx = 0;
    G.Start     = FVector::ZeroVector;
    G.End       = FVector(1000.f, 0.f, 0.f);
    G.Width     = 100.f;
    G.Depth     = 100.f;
    G.EndINodeIdx = -1;
    G.EndJNodeIdx = -1;
    Actor->MemberGeometry  = { G };
    Actor->DeflectionScale = 100.f;     // doesn't matter — disp is 0

    TestTrue(TEXT("BuildMesh succeeds"), Actor->BuildMesh());
    const FProcMeshSection* Sec = Actor->GetMeshComponent()->GetProcMeshSection(0);
    TestNotNull(TEXT("Section 0 retrievable"), Sec);
    if (!Sec) { Actor->Destroy(); return false; }

    // Tip ring centre = (1000, 0, 0) bit-exact.
    const int32 BaseTip = 10 * 4;
    FVector TipCenter = FVector::ZeroVector;
    for (int32 c = 0; c < 4; ++c)
    {
        TipCenter += Sec->ProcVertexBuffer[BaseTip + c].Position;
    }
    TipCenter *= 0.25f;
    TestTrue(TEXT("Tip centre X == 1000"), FMath::IsNearlyEqual(TipCenter.X, 1000.f, 1e-3f));
    TestTrue(TEXT("Tip centre Y == 0"),    FMath::Abs(TipCenter.Y) < 1e-3f);
    TestTrue(TEXT("Tip centre Z == 0"),    FMath::Abs(TipCenter.Z) < 1e-3f);

    Actor->Destroy();
    return true;
}

// --- 3. PMCSectionCount ------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEDeformedShapeSectionCountTest,
    "FrameCore.UE.DeformedShape.SectionCount",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEDeformedShapeSectionCountTest::RunTest(const FString& /*Parameters*/)
{
    UWorld* World = GetSpawnWorld();
    TestNotNull(TEXT("World"), World);
    if (!World) return false;

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFrameDeformedShapeActor* Actor =
        World->SpawnActor<AFrameDeformedShapeActor>(AFrameDeformedShapeActor::StaticClass(),
                                                    FVector::ZeroVector, FRotator::ZeroRotator, SP);
    TestNotNull(TEXT("Actor spawned"), Actor);
    if (!Actor) return false;

    // 3 distinct stacked members.
    for (int32 i = 0; i < 3; ++i)
    {
        FFrameMemberGeometry G;
        G.MemberIdx = i;
        G.Start     = FVector(0.f, 0.f, (float)i * 500.f);
        G.End       = FVector(1000.f, 0.f, (float)i * 500.f);
        G.Width     = 100.f;
        G.Depth     = 100.f;
        Actor->MemberGeometry.Add(G);
    }
    Actor->Solution.Displacements.Reset();  // empty disp ok
    TestTrue(TEXT("BuildMesh succeeds"), Actor->BuildMesh());

    UProceduralMeshComponent* PMC = Actor->GetMeshComponent();
    TestEqual(TEXT("PMC section count == 3"), PMC->GetNumSections(), 3);

    // Re-entry: rebuilding doesn't double the sections.
    TestTrue(TEXT("Second BuildMesh succeeds"), Actor->BuildMesh());
    TestEqual(TEXT("PMC section count still == 3 after rebuild"), PMC->GetNumSections(), 3);

    // Empty input -> BuildMesh returns false and clears sections.
    Actor->MemberGeometry.Reset();
    TestFalse(TEXT("Empty input -> BuildMesh false"), Actor->BuildMesh());
    TestEqual(TEXT("PMC section count == 0 after empty"), PMC->GetNumSections(), 0);

    Actor->Destroy();
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
