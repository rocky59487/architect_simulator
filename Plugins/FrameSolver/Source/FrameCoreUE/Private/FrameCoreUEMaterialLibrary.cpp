#include "FrameCoreUE/FrameCoreUEMaterialLibrary.h"

#include <cmath>

namespace
{
    // Build a FFrameMaterial with FFrameCapacity::Make-style derived fields
    // (Bend = min(Comp,Tens), Tors = Shear, VM = min(Comp,Tens)) so the BP-side
    // capacity matches what `Capacity::make` would produce on the engine side.
    FFrameMaterial MakeMaterial(float E, float G, float Nu, float Rho, float Fy,
                                float CapComp, float CapTens, float CapShear)
    {
        FFrameMaterial M;
        M.E   = E;
        M.G   = G;
        M.Nu  = Nu;
        M.Rho = Rho;
        M.Fy  = Fy;
        M.Cap.Comp  = CapComp;
        M.Cap.Tens  = CapTens;
        M.Cap.Shear = CapShear;
        M.Cap.Bend  = FMath::Min(CapComp, CapTens);
        M.Cap.Tors  = CapShear;
        M.Cap.VM    = FMath::Min(CapComp, CapTens);
        return M;
    }

    // Steel preset: shared elastic properties, per-grade fy.
    //   E = 210 GPa = 210000 MPa, Nu = 0.3 (so G = E / (2*(1+Nu)) = 80769.23... MPa),
    //   rho = 7850 kg/m^3.
    //   Allowable: Comp = Tens = fy (elastic screen at yield); Shear = fy/sqrt(3) (von Mises).
    FFrameMaterial MakeSteel(float Fy)
    {
        const float E   = 210000.f;
        const float Nu  = 0.3f;
        const float G   = E / (2.f * (1.f + Nu));
        const float Rho = 7850.f;
        const float ShearAllow = Fy / 1.7320508f;   // 1/sqrt(3)
        return MakeMaterial(E, G, Nu, Rho, Fy, Fy, Fy, ShearAllow);
    }

    // Concrete preset (mean elastic per EN 1992-1-1; rho 2400 kg/m^3).
    //   Comp = fck (compressive characteristic), Tens = 0.1 * fck (very rough);
    //   Shear = 0.5 * Tens. The elastic screen on concrete is conservative anyway --
    //   real RC design lives outside FrameCore's elastic D/C ratio.
    FFrameMaterial MakeConcrete(float Ecm, float Fck)
    {
        const float Nu  = 0.2f;
        const float G   = Ecm / (2.f * (1.f + Nu));
        const float Rho = 2400.f;
        return MakeMaterial(Ecm, G, Nu, Rho, /*Fy*/0.f, /*Comp*/Fck, /*Tens*/0.1f * Fck, /*Shear*/0.05f * Fck);
    }
}

FFrameMaterial UFrameMaterialLibrary::GetS235() { return MakeSteel(235.f); }
FFrameMaterial UFrameMaterialLibrary::GetS275() { return MakeSteel(275.f); }
FFrameMaterial UFrameMaterialLibrary::GetS355() { return MakeSteel(355.f); }
FFrameMaterial UFrameMaterialLibrary::GetS460() { return MakeSteel(460.f); }

FFrameMaterial UFrameMaterialLibrary::GetConcreteC30() { return MakeConcrete(33000.f, 30.f); }
FFrameMaterial UFrameMaterialLibrary::GetConcreteC40() { return MakeConcrete(35000.f, 40.f); }
FFrameMaterial UFrameMaterialLibrary::GetConcreteC50() { return MakeConcrete(37000.f, 50.f); }

FFrameMaterial UFrameMaterialLibrary::GetAluminum6061()
{
    // 6061-T6: E = 69 GPa, Nu = 0.33, fy = 276 MPa, rho = 2700 kg/m^3.
    const float E   = 69000.f;
    const float Nu  = 0.33f;
    const float G   = E / (2.f * (1.f + Nu));
    const float Rho = 2700.f;
    const float Fy  = 276.f;
    return MakeMaterial(E, G, Nu, Rho, Fy, Fy, Fy, Fy / 1.7320508f);
}

FFrameMaterial UFrameMaterialLibrary::MakeCustomMaterial(float E, float G, float Rho, float Nu, float Fy,
                                                         float CapComp, float CapTens, float CapShear)
{
    return MakeMaterial(E, G, Nu, Rho, Fy, CapComp, CapTens, CapShear);
}
