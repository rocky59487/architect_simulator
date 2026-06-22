// v3.3 Phase 3 / U-03 test — verifies AFrameCoreStressFieldActor builds a procedural
// mesh whose shape, vertex count, and per-vertex colour ramp match the underlying
// FFrameStressField. Spawns into a transient UWorld so the actor's CreateDefaultSubobject
// machinery for UProceduralMeshComponent goes through the normal SpawnActor flow
// (NewObject alone crashes inside the ctor — CreateDefaultSubobject requires a
// FObjectInitializer that only SpawnActor sets up).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"

#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCoreUE/FrameCoreUELibrary.h"
#include "FrameCoreUE/FrameCoreStressFieldActor.h"
#include "ProceduralMeshComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEActorStressMeshTest,
    "FrameCore.UE.ActorStressMeshTest",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEActorStressMeshTest::RunTest(const FString& /*Parameters*/)
{
    // (1) Produce the stress field via the v3.2 BP library entry. Cantilever fixture,
    // P=1000, L=2000, samplesPerSpan=11 -> 1 member trace with 11 samples.
    const FFrameStressField Field =
        UFrameCoreStressFieldLibrary::ComputeCantileverFixture(1000.f, 2000.f, 11);
    TestEqual(TEXT("Field has 1 member"), Field.Members.Num(), 1);
    if (Field.Members.Num() != 1) { return false; }
    TestEqual(TEXT("Field member 0 has 11 samples"), Field.Members[0].Samples.Num(), 11);

    // (2) Locate an existing world from GEngine's contexts (UWorld::CreateWorld crashes
    // in a -ExecCmds=Automation commandlet because the editor's GameInstance template is
    // not loaded). The commandlet always has at least one world context; spawn into it.
    UWorld* World = nullptr;
    if (GEngine)
    {
        for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
        {
            if (Ctx.World())
            {
                World = Ctx.World();
                break;
            }
        }
    }
    TestNotNull(TEXT("Spawn world located from GEngine contexts"), World);
    if (!World) { return false; }

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFrameCoreStressFieldActor* Actor =
        World->SpawnActor<AFrameCoreStressFieldActor>(AFrameCoreStressFieldActor::StaticClass(),
                                                     FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    TestNotNull(TEXT("Actor spawned"), Actor);
    if (!Actor) { return false; }
    Actor->Field          = Field;
    Actor->MemberGeometry = AFrameCoreStressFieldActor::MakeCantileverDemoGeometry(2000.f, 100.f, 100.f);
    TestEqual(TEXT("Demo geometry produced 1 entry"), Actor->MemberGeometry.Num(), 1);

    const bool bBuilt = Actor->BuildMesh();
    TestTrue(TEXT("BuildMesh returns true (at least one section built)"), bBuilt);

    UProceduralMeshComponent* PMC = Actor->GetMeshComponent();
    TestNotNull(TEXT("MeshComponent valid"), PMC);
    if (!PMC) { return false; }

    // (3) Shape contract: exactly one section (one member); 11 rings * 4 vertices = 44
    // verts; 4 sides * 10 segments * 2 + 2 end caps * 2 = 84 triangles -> 252 indices.
    TestEqual(TEXT("MeshComponent has 1 section (1 member)"), PMC->GetNumSections(), 1);
    const FProcMeshSection* Sec = PMC->GetProcMeshSection(0);
    TestNotNull(TEXT("Section 0 retrievable"), Sec);
    if (!Sec) { return false; }

    const int32 ExpVerts   = 11 * 4;
    const int32 ExpIndices = (10 * 4 * 2 + 2 + 2) * 3;
    TestEqual(TEXT("Section 0 vertex count == 44"),   Sec->ProcVertexBuffer.Num(), ExpVerts);
    TestEqual(TEXT("Section 0 index  count == 252"), Sec->ProcIndexBuffer.Num(),  ExpIndices);

    // (4) Colour contract: the cantilever's governing sample is at end-i (root), so
    // ring 0's vertex colour must be redder (higher R, lower B) than ring 10's (tip).
    // Cantilever sigma at root = P*L/Wz > 0; at tip = 0 -> ramp(0) is pure blue.
    TestTrue(TEXT("Section 0 has at least 1 vertex coloured"),
             Sec->ProcVertexBuffer.Num() > 0);

    const FColor RootColor = Sec->ProcVertexBuffer[0].Color;          // ring 0 corner 0
    const FColor TipColor  = Sec->ProcVertexBuffer[10 * 4].Color;     // ring 10 corner 0
    TestTrue(TEXT("Root vertex is redder than tip (root.R > tip.R)"),
             RootColor.R > TipColor.R);
    TestTrue(TEXT("Tip vertex is bluer than root (tip.B > root.B)"),
             TipColor.B > RootColor.B);
    TestEqual(TEXT("Tip vertex is pure blue (sigma == 0 maps to ramp(0))"),
              static_cast<int32>(TipColor.B), 255);

    // (5) Re-entry safety: BuildMesh again must clear and rebuild, not duplicate sections.
    TestTrue(TEXT("BuildMesh re-entry returns true"), Actor->BuildMesh());
    TestEqual(TEXT("Section count still 1 after rebuild"), PMC->GetNumSections(), 1);

    // (6) Edge case: empty Field -> no section.
    Actor->Field = FFrameStressField{};
    TestFalse(TEXT("BuildMesh on empty Field returns false"), Actor->BuildMesh());
    TestEqual(TEXT("Section count == 0 after empty-field rebuild"), PMC->GetNumSections(), 0);

    // (7) Teardown — destroy the actor we spawned. World is shared (we did not create it),
    // so we leave it intact for downstream tests.
    Actor->Destroy();

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
