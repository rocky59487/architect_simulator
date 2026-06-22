// v3.3 Phase 4 / U-01 test — verifies UFrameCoreStressFieldLibrary::ComputeFromJsonModel
// can parse the dispatcher's model.set JSON schema subset (materials/sections/nodes/
// members/nodalLoads), solve, and return a sensible FFrameStressField. Also covers the
// missing-file path and an invalid-schema path so the function never crashes a BP graph.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"

#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCoreUE/FrameCoreUELibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEMarshalJsonTest,
    "FrameCore.UE.MarshalJsonTest",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEMarshalJsonTest::RunTest(const FString& /*Parameters*/)
{
    // (1) Cantilever fixture written as JSON. Schema mirrors the dispatcher's model.set body.
    // Section values match Section::Rectangular(100, 100): A = 10000, Iy = Iz = 8.333e6,
    // J = 1.41e7 (Saint-Venant rectangular constant for 100x100), cy = cz = 50,
    // Zy = Zz = 250000, Asy = Asz = 8333.33 (5/6 of A). Wz used by F68's analytic oracle
    // is Iz / cz = 166666.66... so sigma at root = P*L / Wz = 1000*2000/166666.67 = 12 MPa.
    const FString JsonText = TEXT(R"({
        "materials": [
            { "E": 210000.0, "G": 80769.0, "rho": 7850.0, "nu": 0.3,
              "cap": { "comp": 300.0, "tens": 300.0, "shear": 180.0 } }
        ],
        "sections": [
            { "A": 10000.0, "Iy": 8333333.333333334, "Iz": 8333333.333333334,
              "J": 14100000.0, "cy": 50.0, "cz": 50.0,
              "Asy": 8333.333333333333, "Asz": 8333.333333333333,
              "Zy": 250000.0, "Zz": 250000.0, "shape": "rectangular" }
        ],
        "nodes": [
            { "id": 0, "x": 0.0,    "y": 0.0, "z": 0.0,
              "fixed": [true, true, true, true, true, true] },
            { "id": 1, "x": 2000.0, "y": 0.0, "z": 0.0,
              "fixed": [false, false, false, false, false, false] }
        ],
        "members": [
            { "id": 0, "i": 0, "j": 1, "mat": 0, "sec": 0, "active": true }
        ],
        "nodalLoads": [
            { "node": 1, "comp": [0.0, 0.0, 1000.0, 0.0, 0.0, 0.0] }
        ]
    })");

    const FString TempDir  = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("FrameCoreUETests"));
    IFileManager::Get().MakeDirectory(*TempDir, /*Tree=*/true);
    const FString JsonPath = FPaths::Combine(TempDir, TEXT("cantilever_v33.json"));

    TestTrue(TEXT("write test JSON to disk"),
             FFileHelper::SaveStringToFile(JsonText, *JsonPath,
                                           FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

    // (2) Happy path: load + solve + compare against the in-code cantilever fixture.
    const FFrameStressField FromJson =
        UFrameCoreStressFieldLibrary::ComputeFromJsonModel(JsonPath, 11);
    const FFrameStressField FromCode =
        UFrameCoreStressFieldLibrary::ComputeCantileverFixture(1000.f, 2000.f, 11);

    TestEqual(TEXT("JSON load returns 1 member"), FromJson.Members.Num(), 1);
    TestEqual(TEXT("JSON load member has 11 samples"),
              FromJson.Members.Num() > 0 ? FromJson.Members[0].Samples.Num() : 0, 11);
    TestEqual(TEXT("JSON load: governing member idx == 0 (cantilever slot 0)"),
              FromJson.GoverningMemberIdx, 0);
    TestEqual(TEXT("JSON load: per-member user id == 0 (matches JSON members[0].id)"),
              FromJson.Members.Num() > 0 ? FromJson.Members[0].MemberId : -999, 0);
    TestEqual(TEXT("JSON load: governing shell idx == -1 (no shells)"),
              FromJson.GoverningShellIdx, -1);

    // Numeric consistency: JSON-loaded path should match the hard-coded cantilever fixture
    // bit-identically through engine -> USTRUCT marshal (both compute the same FrameModel
    // and feed it the same solver). Compare GlobalMaxFiberSigma rel<1e-6 (float-lossy).
    const float DenomG = FMath::Max(FromCode.GlobalMaxFiberSigma, 1e-6f);
    const float RelG   = FMath::Abs(FromJson.GlobalMaxFiberSigma - FromCode.GlobalMaxFiberSigma) / DenomG;
    TestTrue(TEXT("JSON load: GlobalMaxFiberSigma matches code fixture (rel<1e-6)"),
             RelG < 1e-6f);

    // Sample-level consistency: root sample (sample[0], where cantilever sigma peaks)
    // matches bit-identically. F68 says |sigma|_root = 12 MPa for this fixture.
    if (FromJson.Members.Num() > 0 && FromJson.Members[0].Samples.Num() > 0
        && FromCode.Members.Num() > 0 && FromCode.Members[0].Samples.Num() > 0)
    {
        const float SigJ = FromJson.Members[0].Samples[0].SigmaCompMax;
        const float SigC = FromCode.Members[0].Samples[0].SigmaCompMax;
        const float DenomS = FMath::Max(SigC, 1e-6f);
        const float RelS   = FMath::Abs(SigJ - SigC) / DenomS;
        TestTrue(TEXT("JSON load: root sigmaCompMax matches code fixture (rel<1e-6)"),
                 RelS < 1e-6f);
    }

    // (3) Missing file path: must not crash; must return a default-constructed FFrameStressField
    // (governing idx == -1, no members).
    const FFrameStressField FromMissing =
        UFrameCoreStressFieldLibrary::ComputeFromJsonModel(
            FPaths::Combine(TempDir, TEXT("__does_not_exist__.json")), 11);
    TestEqual(TEXT("Missing file: Members empty"),         FromMissing.Members.Num(), 0);
    TestEqual(TEXT("Missing file: GoverningMemberIdx -1"), FromMissing.GoverningMemberIdx, -1);
    TestEqual(TEXT("Missing file: GoverningShellIdx -1"),  FromMissing.GoverningShellIdx, -1);

    // (4) Malformed JSON path: must not crash; must return default field.
    const FString BadPath = FPaths::Combine(TempDir, TEXT("cantilever_v33_bad.json"));
    TestTrue(TEXT("write malformed JSON"),
             FFileHelper::SaveStringToFile(TEXT("{ not valid json !"),
                                           *BadPath,
                                           FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    const FFrameStressField FromBad =
        UFrameCoreStressFieldLibrary::ComputeFromJsonModel(BadPath, 11);
    TestEqual(TEXT("Bad JSON: Members empty"),         FromBad.Members.Num(), 0);
    TestEqual(TEXT("Bad JSON: GoverningMemberIdx -1"), FromBad.GoverningMemberIdx, -1);

    // Cleanup (best-effort; test passes either way).
    IFileManager::Get().Delete(*JsonPath, /*RequireExists=*/false);
    IFileManager::Get().Delete(*BadPath,  /*RequireExists=*/false);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
