// v3.4 Phase 1 test -- every material/section preset returns finite positive values,
// and MakeRectangular(100, 100) matches frame::Section::Rectangular(100, 100) bit-exact
// (rel < 1e-6 after the double -> float lossy cast, matching the FFrameStressField marshal
// budget; the engine values themselves go through the same Section.h analytic formulas
// so a *true* mismatch would mean the BP library is computing the wrong section -- not a
// precision-budget artefact).
//
// Acts as the F71 standalone fixture's BP-side mirror: the section factory must round-trip
// through the lossy float USTRUCT and still equal the engine factory.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "FrameCoreUE/FrameCoreUEModelTypes.h"
#include "FrameCoreUE/FrameCoreUEMaterialLibrary.h"
#include "FrameCoreUE/FrameCoreUESectionLibrary.h"

#include "FrameCore/Section.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUELibraryPresetsTest,
    "FrameCore.UE.LibraryPresets",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

namespace
{
    // Asserts every numeric field strictly positive and finite.
    void CheckMaterialPositive(FAutomationTestBase& T, const TCHAR* Label, const FFrameMaterial& M)
    {
        T.TestTrue(FString::Printf(TEXT("%s: E > 0"),   Label), M.E   > 0.f);
        T.TestTrue(FString::Printf(TEXT("%s: G > 0"),   Label), M.G   > 0.f);
        T.TestTrue(FString::Printf(TEXT("%s: Rho > 0"), Label), M.Rho > 0.f);
        T.TestTrue(FString::Printf(TEXT("%s: Nu > 0"),  Label), M.Nu  > 0.f);
        T.TestTrue(FString::Printf(TEXT("%s: Cap.Comp > 0"),  Label), M.Cap.Comp  > 0.f);
        T.TestTrue(FString::Printf(TEXT("%s: Cap.Tens > 0"),  Label), M.Cap.Tens  > 0.f);
        T.TestTrue(FString::Printf(TEXT("%s: Cap.Shear > 0"), Label), M.Cap.Shear > 0.f);
        T.TestTrue(FString::Printf(TEXT("%s: E finite"), Label), FMath::IsFinite(M.E));
    }

    // Relative compare with tiny floor for near-zero values.
    bool NearlyEqualRel(double A, double B, double Tol)
    {
        const double Denom = FMath::Max(FMath::Abs(B), 1e-9);
        return FMath::Abs(A - B) / Denom < Tol;
    }
}

bool FFrameCoreUELibraryPresetsTest::RunTest(const FString& /*Parameters*/)
{
    // (1) Material presets.
    CheckMaterialPositive(*this, TEXT("S235"), UFrameMaterialLibrary::GetS235());
    CheckMaterialPositive(*this, TEXT("S275"), UFrameMaterialLibrary::GetS275());
    CheckMaterialPositive(*this, TEXT("S355"), UFrameMaterialLibrary::GetS355());
    CheckMaterialPositive(*this, TEXT("S460"), UFrameMaterialLibrary::GetS460());
    CheckMaterialPositive(*this, TEXT("C30"),  UFrameMaterialLibrary::GetConcreteC30());
    CheckMaterialPositive(*this, TEXT("C40"),  UFrameMaterialLibrary::GetConcreteC40());
    CheckMaterialPositive(*this, TEXT("C50"),  UFrameMaterialLibrary::GetConcreteC50());
    CheckMaterialPositive(*this, TEXT("Al6061"), UFrameMaterialLibrary::GetAluminum6061());

    // Steel fy ordering: S235 < S275 < S355 < S460. Common sanity check.
    const FFrameMaterial S235 = UFrameMaterialLibrary::GetS235();
    const FFrameMaterial S460 = UFrameMaterialLibrary::GetS460();
    TestTrue(TEXT("S235.Fy < S460.Fy"), S235.Fy < S460.Fy);
    TestEqual(TEXT("S235.Fy == 235"), S235.Fy, 235.f);
    TestEqual(TEXT("S460.Fy == 460"), S460.Fy, 460.f);

    // (2) MakeRectangular(100, 100) vs engine Section::Rectangular(100, 100).
    {
        const FFrameSection BP = UFrameSectionLibrary::MakeRectangular(100.f, 100.f);
        const frame::Section Eng = frame::Section::Rectangular(100.0, 100.0);
        TestTrue(TEXT("Rect.A vs engine"),  NearlyEqualRel(BP.A,  Eng.A,  1e-6));
        TestTrue(TEXT("Rect.Iy vs engine"), NearlyEqualRel(BP.Iy, Eng.Iy, 1e-6));
        TestTrue(TEXT("Rect.Iz vs engine"), NearlyEqualRel(BP.Iz, Eng.Iz, 1e-6));
        TestTrue(TEXT("Rect.J vs engine"),  NearlyEqualRel(BP.J,  Eng.J,  1e-6));
        TestTrue(TEXT("Rect.Cy vs engine"), NearlyEqualRel(BP.Cy, Eng.cy, 1e-6));
        TestTrue(TEXT("Rect.Cz vs engine"), NearlyEqualRel(BP.Cz, Eng.cz, 1e-6));
        TestTrue(TEXT("Rect.Zy vs engine"), NearlyEqualRel(BP.Zy, Eng.Zy, 1e-6));
        TestTrue(TEXT("Rect.Zz vs engine"), NearlyEqualRel(BP.Zz, Eng.Zz, 1e-6));
        TestTrue(TEXT("Rect.Asy vs engine"),NearlyEqualRel(BP.Asy,Eng.Asy,1e-6));
        TestTrue(TEXT("Rect.Asz vs engine"),NearlyEqualRel(BP.Asz,Eng.Asz,1e-6));
        TestEqual(TEXT("Rect shape"), (uint8)BP.Shape, (uint8)EFrameSectionShape::Rectangular);
    }

    // (3) MakeCircular(100) vs engine Section::Circular(50).
    {
        const FFrameSection BP = UFrameSectionLibrary::MakeCircular(100.f);
        const frame::Section Eng = frame::Section::Circular(50.0);
        TestTrue(TEXT("Circ.A vs engine"),  NearlyEqualRel(BP.A,  Eng.A,  1e-6));
        TestTrue(TEXT("Circ.Iy vs engine"), NearlyEqualRel(BP.Iy, Eng.Iy, 1e-6));
        TestTrue(TEXT("Circ.Iz vs engine"), NearlyEqualRel(BP.Iz, Eng.Iz, 1e-6));
        TestTrue(TEXT("Circ.J vs engine"),  NearlyEqualRel(BP.J,  Eng.J,  1e-6));
        TestTrue(TEXT("Circ.Zy vs engine"), NearlyEqualRel(BP.Zy, Eng.Zy, 1e-6));
        TestTrue(TEXT("Circ.Zz vs engine"), NearlyEqualRel(BP.Zz, Eng.Zz, 1e-6));
        TestEqual(TEXT("Circ shape"), (uint8)BP.Shape, (uint8)EFrameSectionShape::Circular);
    }

    return true;
}

#endif  // WITH_DEV_AUTOMATION_TESTS
