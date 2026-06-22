// v3.4 Phase 1 test -- builds a 2 m cantilever from FFrameModelDef alone (no JSON, no
// engine reach-around), runs UFrameModelBuilder::ValidateModel, and asserts true. Mirrors
// the F68 standalone fixture: rectangular 100x100 section, S235-like material, fixed at
// the origin node, +Z tip load at the far node. This is the BP-side equivalent of "I have
// a model and I want to know whether the engine considers it solvable."

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "FrameCoreUE/FrameCoreUEModelTypes.h"
#include "FrameCoreUE/FrameCoreUEModelBuilder.h"
#include "FrameCoreUE/FrameCoreUEMaterialLibrary.h"
#include "FrameCoreUE/FrameCoreUESectionLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEBuildAndValidateTest,
    "FrameCore.UE.BuildAndValidate",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEBuildAndValidateTest::RunTest(const FString& /*Parameters*/)
{
    FFrameModelDef Def;
    Def.Materials.Add(UFrameMaterialLibrary::GetS235());
    Def.Sections.Add(UFrameSectionLibrary::MakeRectangular(100.f, 100.f));

    // Two nodes: fixed origin + free tip at (2000, 0, 0).
    {
        FFrameNode N;
        N.Id = 0;
        N.Pos = FVector(0.f, 0.f, 0.f);
        N.Fixed   = { true, true, true, true, true, true };
        Def.Nodes.Add(N);
    }
    {
        FFrameNode N;
        N.Id = 1;
        N.Pos = FVector(2000.f, 0.f, 0.f);
        // Free DOFs (leave Fixed empty -- marshal treats empty as all-free)
        Def.Nodes.Add(N);
    }

    // One member, mat/sec indices.
    {
        FFrameMember M;
        M.Id = 0;
        M.I = 0; M.J = 1;
        M.MatIdx = 0; M.SecIdx = 0;
        Def.Members.Add(M);
    }

    // Tip load +Z, 1000 N.
    {
        FFrameNodalLoad L;
        L.Node = 1;
        L.Comp = { 0.f, 0.f, 1000.f, 0.f, 0.f, 0.f };
        Def.NodalLoads.Add(L);
    }

    FString Err;
    const bool bOk = UFrameModelBuilder::ValidateModel(Def, Err);
    TestTrue(TEXT("ValidateModel returns true for cantilever fixture"), bOk);
    TestTrue(TEXT("OutError empty on success"), Err.IsEmpty());

    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
