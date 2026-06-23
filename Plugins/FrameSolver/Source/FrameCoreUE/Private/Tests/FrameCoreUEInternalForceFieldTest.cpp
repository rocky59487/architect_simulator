// v3.6 Phase 3 tests for AFrameInternalForceFieldActor (C6 沿桿 BMD/SFD).
//
//   1. Cantilever.Vz       -- positive +Z tip load -> Vz constant along member,
//                             ribbon flat at non-zero height.
//   2. SSBeam.Mz           -- mid-span moment ribbon peaks at midspan, vanishes at
//                             both supports (parabolic Mz profile).
//   3. EmptyTrace          -- empty Field -> BuildMesh false, no sections.
//   4. ComponentSwitch     -- switching Component from My to Mz changes ribbon height.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"

#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCoreUE/FrameInternalForceFieldActor.h"
#include "ProceduralMeshComponent.h"
#include "FrameCoreUETestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    using FrameCoreUETestHelpers::GetSpawnWorld;

    AFrameInternalForceFieldActor* Spawn(UWorld* W)
    {
        FActorSpawnParameters SP;
        SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        return W->SpawnActor<AFrameInternalForceFieldActor>(
            AFrameInternalForceFieldActor::StaticClass(),
            FVector::ZeroVector, FRotator::ZeroRotator, SP);
    }

    FFrameStressField MakeConstantField(float Component, EFrameForceComponent Which, int32 NSamples = 11)
    {
        FFrameStressField F;
        FFrameMemberStressTrace T;
        T.MemberIdx = 0; T.MemberId = 0;
        for (int32 k = 0; k < NSamples; ++k)
        {
            FFrameStressFieldSample S;
            switch (Which)
            {
                case EFrameForceComponent::AxialN:   S.N  = Component; break;
                case EFrameForceComponent::ShearVy:  S.Vy = Component; break;
                case EFrameForceComponent::ShearVz:  S.Vz = Component; break;
                case EFrameForceComponent::TorsionT: S.T  = Component; break;
                case EFrameForceComponent::MomentMy: S.My = Component; break;
                case EFrameForceComponent::MomentMz: S.Mz = Component; break;
            }
            T.Samples.Add(S);
        }
        F.Members.Add(T);
        return F;
    }

    FFrameStressField MakeParabolicMz(float Peak, int32 N = 11)
    {
        FFrameStressField F;
        FFrameMemberStressTrace T;
        T.MemberIdx = 0; T.MemberId = 0;
        for (int32 k = 0; k < N; ++k)
        {
            FFrameStressFieldSample S;
            const float t = (float)k / (float)(N - 1);
            S.Mz = Peak * 4.f * t * (1.f - t);   // parabolic: 0 at ends, peak at mid
            T.Samples.Add(S);
        }
        F.Members.Add(T);
        return F;
    }

    FFrameMemberGeometry MakeGeom()
    {
        FFrameMemberGeometry G;
        G.MemberIdx = 0;
        G.Start = FVector::ZeroVector;
        G.End   = FVector(2000.f, 0.f, 0.f);
        return G;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEForceFieldVzTest,
    "FrameCore.UE.ForceField.CantileverVz",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEForceFieldVzTest::RunTest(const FString&)
{
    UWorld* W = GetSpawnWorld(); if (!W) return false;
    AFrameInternalForceFieldActor* A = Spawn(W); if (!A) return false;
    A->Field = MakeConstantField(100.f, EFrameForceComponent::ShearVz);
    A->Component = EFrameForceComponent::ShearVz;
    A->MemberGeometry = { MakeGeom() };
    A->HeightScale = 1.f;
    TestTrue(TEXT("BuildMesh"), A->BuildMesh());

    const FProcMeshSection* Sec = A->GetMeshComponent()->GetProcMeshSection(0);
    TestNotNull(TEXT("Section 0"), Sec);
    if (!Sec) { A->Destroy(); return false; }
    // 11 samples -> 22 vertices. For a +X member, MemberLocalAxes gives RefY=+Z, RefZ=-Y.
    // Vz extrudes along RefZ = -Y, so the tip top vertex Y should be ~-100.
    TestEqual(TEXT("22 vertices"), Sec->ProcVertexBuffer.Num(), 22);
    const float TipY = Sec->ProcVertexBuffer[1].Position.Y;
    TestTrue(FString::Printf(TEXT("ribbon Vz extrusion magnitude ~100 (got |%.4f|)"), TipY),
             FMath::IsNearlyEqual(FMath::Abs(TipY), 100.f, 1.f));

    A->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEForceFieldMzParabolicTest,
    "FrameCore.UE.ForceField.SSBeamMzParabolic",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEForceFieldMzParabolicTest::RunTest(const FString&)
{
    UWorld* W = GetSpawnWorld(); if (!W) return false;
    AFrameInternalForceFieldActor* A = Spawn(W); if (!A) return false;
    A->Field = MakeParabolicMz(250.f);
    A->Component = EFrameForceComponent::MomentMz;
    A->MemberGeometry = { MakeGeom() };
    A->HeightScale = 1.f;
    TestTrue(TEXT("BuildMesh"), A->BuildMesh());

    const FProcMeshSection* Sec = A->GetMeshComponent()->GetProcMeshSection(0);
    if (!Sec) { A->Destroy(); return false; }
    // Mz extrudes along RefY = +Z for a +X member, so midspan top vertex Z ~ +250.
    const float MidZ = Sec->ProcVertexBuffer[5 * 2 + 1].Position.Z;
    const float EndZ = Sec->ProcVertexBuffer[0 * 2 + 1].Position.Z;
    TestTrue(FString::Printf(TEXT("midspan Z ~250 (got %.4f)"), MidZ),
             FMath::IsNearlyEqual(MidZ, 250.f, 1.f));
    TestTrue(FString::Printf(TEXT("endpoint Z ~0 (got %.4f)"), EndZ),
             FMath::Abs(EndZ) < 1.f);

    A->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEForceFieldEmptyTest,
    "FrameCore.UE.ForceField.EmptyTrace",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEForceFieldEmptyTest::RunTest(const FString&)
{
    UWorld* W = GetSpawnWorld(); if (!W) return false;
    AFrameInternalForceFieldActor* A = Spawn(W); if (!A) return false;
    A->MemberGeometry = { MakeGeom() };
    TestFalse(TEXT("Empty field -> no sections"), A->BuildMesh());
    TestEqual(TEXT("0 sections"), A->GetMeshComponent()->GetNumSections(), 0);
    A->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEForceFieldComponentSwitchTest,
    "FrameCore.UE.ForceField.ComponentSwitch",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FFrameCoreUEForceFieldComponentSwitchTest::RunTest(const FString&)
{
    UWorld* W = GetSpawnWorld(); if (!W) return false;
    AFrameInternalForceFieldActor* A = Spawn(W); if (!A) return false;
    FFrameStressField F;
    FFrameMemberStressTrace T;
    for (int32 k = 0; k < 11; ++k)
    {
        FFrameStressFieldSample S;
        S.Mz = 100.f; S.My = 50.f;
        T.Samples.Add(S);
    }
    F.Members.Add(T);
    A->Field = F;
    A->MemberGeometry = { MakeGeom() };
    A->HeightScale = 1.f;

    A->Component = EFrameForceComponent::MomentMz;
    A->BuildMesh();
    const FVector MzTop = A->GetMeshComponent()->GetProcMeshSection(0)->ProcVertexBuffer[1].Position;

    A->Component = EFrameForceComponent::MomentMy;
    A->BuildMesh();
    const FVector MyTop = A->GetMeshComponent()->GetProcMeshSection(0)->ProcVertexBuffer[1].Position;

    TestTrue(TEXT("Mz ribbon != My ribbon"),
             !MzTop.Equals(MyTop, 1e-3f));

    A->Destroy();
    return true;
}

#endif
