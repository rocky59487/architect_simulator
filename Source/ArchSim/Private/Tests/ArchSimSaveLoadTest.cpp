// Sprint S-01 / A1-07 -- ArchSim.Persistence.SaveLoadRoundTrip
//
// Verifies that the UArchSimModelRegistry contract Member.Id == GetMemberIdx()
// survives a save/load roundtrip on UArchSimMemberData::MemberIdx, alongside
// stability of node count and member count.
//
// SPUD is enabled by default (Plugins/SPUD/SPUD.uplugin, EngineVersion 5.7), but
// driving USpudSubsystem requires a live World+Level+GameInstance and produces
// disk artefacts that are fragile under -nullrhi -unattended. Per HANDOFF_v0.1
// §4 item #6 ("SPUD UE5.5 risk deferred"), this test uses the canonical UE
// SaveGame proxy archive (FObjectAndNameAsStringProxyArchive with ArIsSaveGame =
// true) as an equivalent in-memory stub. It exercises the same UPROPERTY(SaveGame)
// reflection path SPUD uses for component data, so MemberData's serialise/restore
// behaviour is genuinely covered; only the SPUD subsystem orchestration layer is
// stubbed.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#include "Subsystems/ArchSimModelRegistry.h"
#include "Components/ArchSimMemberData.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    // Locate a world from GEngine's contexts -- UWorld::CreateWorld crashes inside
    // an Automation commandlet because the editor GameInstance template is not
    // loaded. Mirrors the pattern in FrameCoreUEActorStressMeshTest.
    UWorld* FindSpawnWorld()
    {
        if (!GEngine) return nullptr;
        for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
        {
            if (Ctx.World()) return Ctx.World();
        }
        return nullptr;
    }

    // Headless automation often has no live UGameInstance subsystem manager that
    // would surface UArchSimModelRegistry; fall back to a transient instance so the
    // contract under test (RegisterMember + Members.Id == MemberIdx) is exercised
    // in isolation. Same pattern as FrameCoreUEInteractiveSubsystemTest.
    UArchSimModelRegistry* AcquireFreshRegistry()
    {
        return NewObject<UArchSimModelRegistry>();
    }

    // Serialise a UObject's UPROPERTY(SaveGame) fields into a byte buffer using the
    // canonical UE SaveGame proxy archive. This is exactly what USaveGame /
    // SaveGameToSlot use internally; SPUD layers on top of the same serialiser.
    TArray<uint8> SaveGameRoundTripCapture(UObject* Obj)
    {
        TArray<uint8> Bytes;
        FMemoryWriter MemWriter(Bytes, /*bIsPersistent=*/true);
        FObjectAndNameAsStringProxyArchive Ar(MemWriter, /*bInLoadIfFindFails=*/true);
        Ar.ArIsSaveGame = true;
        Obj->Serialize(Ar);
        return Bytes;
    }

    void SaveGameRoundTripApply(UObject* Obj, const TArray<uint8>& Bytes)
    {
        FMemoryReader MemReader(Bytes, /*bIsPersistent=*/true);
        FObjectAndNameAsStringProxyArchive Ar(MemReader, /*bInLoadIfFindFails=*/true);
        Ar.ArIsSaveGame = true;
        Obj->Serialize(Ar);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArchSimSaveLoadRoundTripTest,
    "ArchSim.Persistence.SaveLoadRoundTrip",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FArchSimSaveLoadRoundTripTest::RunTest(const FString& /*Parameters*/)
{
    UE_LOG(LogTemp, Display,
           TEXT("ArchSim.Persistence.SaveLoadRoundTrip: SPUD UE5.5 risk deferred "
                "per HANDOFF_v0.1 section 4 item #6 -- using in-memory "
                "FObjectAndNameAsStringProxyArchive stub for SaveGame UPROPERTY "
                "roundtrip; SPUD subsystem orchestration is out of scope."));

    UWorld* World = FindSpawnWorld();
    TestNotNull(TEXT("Spawn world located from GEngine contexts"), World);
    if (!World) return false;

    UArchSimModelRegistry* Registry = AcquireFreshRegistry();
    TestNotNull(TEXT("Fresh ArchSimModelRegistry constructed"), Registry);
    if (!Registry) return false;
    TestEqual(TEXT("Registry starts empty"), Registry->GetRegisteredCount(), 0);

    // ---- (a) Spawn 5 fake actors + components ------------------------------
    // Each component keeps the default EndI/J offsets of -50 / +50 cm along local
    // +X, i.e. a 1 m horizontal beam. Spread the actors by 200 cm along world +X
    // so adjacent actors' endpoints sit 1 m == 1000 mm apart -- well above the
    // 1 mm node-merge tolerance. With 5 disjoint beams we expect 10 unique nodes.
    constexpr int32 NumMembers = 5;
    constexpr float ActorSepCm = 200.f;

    TArray<TStrongObjectPtr<AActor>> KeepActors;
    TArray<UArchSimMemberData*>      Comps;
    KeepActors.Reserve(NumMembers);
    Comps.Reserve(NumMembers);

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for (int32 i = 0; i < NumMembers; ++i)
    {
        const FVector ActorLoc(ActorSepCm * static_cast<float>(i), 0.f, 0.f);
        // Spawn at identity, then graft on a SceneComponent root and SetActorLocation.
        // SpawnActor(...,Location,...) on base AActor drops the location silently because
        // AActor has no default RootComponent -- so we built our own.
        AActor* Actor = World->SpawnActor<AActor>(
            AActor::StaticClass(), FTransform::Identity, SP);
        TestNotNull(TEXT("Fake actor spawned"), Actor);
        if (!Actor) return false;

        USceneComponent* Root = NewObject<USceneComponent>(
            Actor, USceneComponent::StaticClass(), TEXT("Root"));
        TestNotNull(TEXT("Root scene component constructed"), Root);
        if (!Root) return false;
        Actor->SetRootComponent(Root);
        Root->RegisterComponent();
        Actor->SetActorLocation(ActorLoc);
        TestEqual(FString::Printf(TEXT("Actor[%d] location applied"), i),
                  Actor->GetActorLocation().X, ActorLoc.X);

        KeepActors.Emplace(Actor);

        UArchSimMemberData* Comp = NewObject<UArchSimMemberData>(
            Actor, UArchSimMemberData::StaticClass(), NAME_None, RF_Transient);
        TestNotNull(TEXT("Component constructed"), Comp);
        if (!Comp) return false;
        Comp->RegisterComponent();
        Comps.Add(Comp);

        const int32 Idx = Registry->RegisterMember(Comp);
        TestNotEqual(TEXT("RegisterMember returns valid idx"), Idx, -1);
        TestEqual(FString::Printf(TEXT("Member %d assigned in-order"), i), Idx, i);
        TestEqual(FString::Printf(TEXT("Pre: Component[%d].MemberIdx == assigned idx"), i),
                  Comp->MemberIdx, i);
        TestTrue(FString::Printf(TEXT("Pre: Component[%d].bRegistered"), i),
                 Comp->bRegistered);
    }

    // ---- (b) Snapshot pre-roundtrip state ----------------------------------
    const FFrameModelDef& ModelPre   = Registry->GetCurrentModel();
    const int32 NodeCountPre         = ModelPre.Nodes.Num();
    const int32 MemberCountPre       = ModelPre.Members.Num();
    const int32 RegisteredCountPre   = Registry->GetRegisteredCount();
    TestEqual(TEXT("Pre: registry has 5 members"), MemberCountPre, NumMembers);
    TestEqual(TEXT("Pre: registry has 10 nodes (5 disjoint beams)"),
              NodeCountPre, NumMembers * 2);
    TestEqual(TEXT("Pre: registered count == member count"),
              RegisteredCountPre, NumMembers);

    TArray<int32> MemberIdListPre;
    MemberIdListPre.Reserve(MemberCountPre);
    for (const FFrameMember& M : ModelPre.Members)
    {
        MemberIdListPre.Add(M.Id);
    }

    // Contract A pre-roundtrip: Member.Id == component MemberIdx for every i.
    for (int32 i = 0; i < NumMembers; ++i)
    {
        TestEqual(FString::Printf(TEXT("Pre: Member[%d].Id == Component MemberIdx"), i),
                  ModelPre.Members[i].Id, Comps[i]->MemberIdx);
    }

    // MaxRank surface: Registry's debounce ladder bounds itself at MaxRankBeforeRebaseline = 96
    // (Subsystems/ArchSimModelRegistry.h:105, private constexpr). With 5 registered we
    // should be well under that ceiling; the test spec quotes "MaxRank=96" for the
    // contract here, so we verify the equivalent observable: registered count <= 96.
    TestTrue(TEXT("Pre: registered count below MaxRank=96 ceiling"),
             RegisteredCountPre <= 96);

    // ---- (c) Save: capture each component's SaveGame fields ----------------
    // UArchSimMemberData has three UPROPERTY(SaveGame) fields: MemberIdx,
    // StructureGroupId, CachedUtilization. Components/ArchSimMemberData.h:24-34.
    TArray<TArray<uint8>> Buffers;
    Buffers.Reserve(NumMembers);
    for (UArchSimMemberData* Comp : Comps)
    {
        Buffers.Add(SaveGameRoundTripCapture(Comp));
    }
    TestEqual(TEXT("One save-buffer per component"), Buffers.Num(), NumMembers);

    // ---- (d) Simulate world tear-down: scribble the SaveGame fields ---------
    // SPUD would tear down the world here and rebuild it from the save slot.
    // We approximate by overwriting the SaveGame fields with sentinel values
    // so a successful (e) provably restores from the buffer rather than no-op.
    for (UArchSimMemberData* Comp : Comps)
    {
        Comp->MemberIdx         = -1;
        Comp->StructureGroupId  = -1;
        Comp->CachedUtilization = -1.f;
    }
    for (int32 i = 0; i < NumMembers; ++i)
    {
        TestEqual(TEXT("Mid: MemberIdx scribbled to -1"), Comps[i]->MemberIdx, -1);
    }

    // ---- (e) Load: apply each buffer back -----------------------------------
    for (int32 i = 0; i < NumMembers; ++i)
    {
        SaveGameRoundTripApply(Comps[i], Buffers[i]);
    }

    // ---- (f) Verify contract post-roundtrip --------------------------------
    // f.1  Registry-side model unchanged (the registry itself was not saved or
    //      loaded; this checks our save/clear/load dance didn't accidentally
    //      mutate it through component back-references).
    const FFrameModelDef& ModelPost = Registry->GetCurrentModel();
    TestEqual(TEXT("Post: node count unchanged"), ModelPost.Nodes.Num(), NodeCountPre);
    TestEqual(TEXT("Post: member count unchanged"), ModelPost.Members.Num(), MemberCountPre);
    TestEqual(TEXT("Post: registered count unchanged"),
              Registry->GetRegisteredCount(), RegisteredCountPre);

    // f.2  Member.Id list identical to pre-snapshot.
    for (int32 i = 0; i < MemberCountPre; ++i)
    {
        TestEqual(FString::Printf(TEXT("Post: Member[%d].Id matches pre-snapshot"), i),
                  ModelPost.Members[i].Id, MemberIdListPre[i]);
    }

    // f.3  Core contract: Member.Id == MemberIdx for every component, AFTER the
    //      component's MemberIdx field has been wiped to -1 and restored from the
    //      save buffer. This is the actual SaveLoad guarantee the spec asks for.
    for (int32 i = 0; i < NumMembers; ++i)
    {
        TestEqual(FString::Printf(TEXT("Post: Component[%d].MemberIdx restored from buffer"), i),
                  Comps[i]->MemberIdx, MemberIdListPre[i]);
        TestEqual(FString::Printf(TEXT("Post: Member[%d].Id == Component MemberIdx (contract)"), i),
                  ModelPost.Members[i].Id, Comps[i]->MemberIdx);
    }

    // f.4  MaxRank ceiling preserved.
    TestTrue(TEXT("Post: registered count still below MaxRank=96"),
             Registry->GetRegisteredCount() <= 96);

    // ---- cleanup -----------------------------------------------------------
    for (TStrongObjectPtr<AActor>& A : KeepActors)
    {
        if (A.IsValid()) { A->Destroy(); }
    }
    KeepActors.Reset();

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
