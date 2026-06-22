// v3.4 Phase 1 test -- a member referencing a non-existent node id should fail
// validation with a one-line diagnostic, NOT silently pass and then crash on solve.
// Covers two error modes: (1) Member.J points at an undefined NodeId; (2) Member.MatIdx
// out of range. Both should return false + non-empty OutError.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "FrameCoreUE/FrameCoreUEModelTypes.h"
#include "FrameCoreUE/FrameCoreUEModelBuilder.h"
#include "FrameCoreUE/FrameCoreUEMaterialLibrary.h"
#include "FrameCoreUE/FrameCoreUESectionLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEInvalidMemberRefTest,
    "FrameCore.UE.InvalidMemberRef",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

static FFrameModelDef MakeBaseFixture()
{
    FFrameModelDef Def;
    Def.Materials.Add(UFrameMaterialLibrary::GetS235());
    Def.Sections.Add(UFrameSectionLibrary::MakeRectangular(100.f, 100.f));

    FFrameNode N0;
    N0.Id  = 0;
    N0.Pos = FVector(0.f, 0.f, 0.f);
    N0.Fixed = { true, true, true, true, true, true };
    Def.Nodes.Add(N0);

    FFrameNode N1;
    N1.Id  = 1;
    N1.Pos = FVector(2000.f, 0.f, 0.f);
    Def.Nodes.Add(N1);

    return Def;
}

bool FFrameCoreUEInvalidMemberRefTest::RunTest(const FString& /*Parameters*/)
{
    // (1) Member.J references a non-existent NodeId.
    {
        FFrameModelDef Def = MakeBaseFixture();
        FFrameMember M;
        M.Id = 0;
        M.I = 0;
        M.J = 999;   // does not exist
        M.MatIdx = 0;
        M.SecIdx = 0;
        Def.Members.Add(M);

        FString Err;
        const bool bOk = UFrameModelBuilder::ValidateModel(Def, Err);
        TestFalse(TEXT("ValidateModel rejects member referencing unknown node id"), bOk);
        TestFalse(TEXT("OutError populated on dangling NodeId"), Err.IsEmpty());
    }

    // (2) Member.MatIdx out of range.
    {
        FFrameModelDef Def = MakeBaseFixture();
        FFrameMember M;
        M.Id = 0;
        M.I = 0;
        M.J = 1;
        M.MatIdx = 7;    // out of range (Materials.Num() == 1)
        M.SecIdx = 0;
        Def.Members.Add(M);

        FString Err;
        const bool bOk = UFrameModelBuilder::ValidateModel(Def, Err);
        TestFalse(TEXT("ValidateModel rejects member with out-of-range MatIdx"), bOk);
        TestFalse(TEXT("OutError populated on MatIdx OOB"), Err.IsEmpty());
    }

    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
