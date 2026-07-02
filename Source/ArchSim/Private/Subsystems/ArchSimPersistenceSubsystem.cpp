// ArchSim - UArchSimPersistenceSubsystem implementation.
// AS-08-u1 (Sprint S-08). Initial sidecar v1 implementation.
// AS-41-u1 (Sprint S-09). Sidecar format v2: full model-state persistence.
//
// See header for full design rationale and SPUD missing-property behaviour citation.

#include "Subsystems/ArchSimPersistenceSubsystem.h"
#include "Subsystems/ArchSimModelRegistry.h"
#include "Components/ArchSimMemberData.h"

// SPUD public API
#include "SpudSubsystem.h"       // USpudSubsystem (GameInstanceSubsystem)

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "TimerManager.h"
#include "EngineUtils.h"            // TActorIterator

DEFINE_LOG_CATEGORY_STATIC(LogArchSimPersistence, Log, All);

// Stable name used to register this subsystem with SPUD's global-object store.
// Must match across save/load cycles; changing it is a breaking format change.
// (SpudSubsystem::AddPersistentGlobalObjectWithName keyed by FString identity.)
static const FString kSpudGlobalName = TEXT("ArchSimPersistenceSidecar");

// Slot name prefix for manually-named user slots.
static const FString kSlotPrefix = TEXT("ArchSimSlot_");

// Current format version written by SnapshotCurrentModel().
static constexpr int32 kCurrentFormatVersion = 2;

// ---- UGameInstanceSubsystem lifecycle ----------------------------------------

void UArchSimPersistenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Register ourselves as a SPUD named global object so our UPROPERTY(SaveGame)
    // TArrays are included in every save/load cycle without needing a level actor.
    //
    // WHY AddPersistentGlobalObjectWithName (not AddPersistentGlobalObject):
    //   UGameInstanceSubsystem instances can have non-deterministic FNames across
    //   sessions (the subsystem manager may produce suffixes like _0, _1).
    //   Named registration uses our stable kSpudGlobalName as the identity key so
    //   SPUD always finds the same record in GOBS regardless of runtime FName.
    //   (SpudSubsystem::AddPersistentGlobalObjectWithName → NamedGlobalObjects.Add;
    //   SpudSubsystem.cpp:1017-1019.)
    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        UE_LOG(LogArchSimPersistence, Warning,
               TEXT("Initialize: no GameInstance; SPUD registration skipped."));
        return;
    }

    USpudSubsystem* Spud = GI->GetSubsystem<USpudSubsystem>();
    if (!Spud)
    {
        UE_LOG(LogArchSimPersistence, Warning,
               TEXT("Initialize: USpudSubsystem not found; SPUD registration skipped."));
        return;
    }

    // AddPersistentGlobalObjectWithName is idempotent for the same Name key
    // (NamedGlobalObjects is a TMap; Add overwrites the same key).
    Spud->AddPersistentGlobalObjectWithName(this, kSpudGlobalName);

    // Subscribe to PostLoadGame so we can replay the sidecar after a full load.
    // WHY PostLoadGame (not PostLevelRestore): PostLoadGame fires after both
    // global-object restore AND the level restore are complete (SpudSubsystem.cpp:
    // 981 LoadComplete), so level actors spawned by SPUD are fully available.
    Spud->PostLoadGame.AddDynamic(
        this, &UArchSimPersistenceSubsystem::OnPostLoadGame);

    UE_LOG(LogArchSimPersistence, Display,
           TEXT("Initialize: registered with SPUD as '%s'."), *kSpudGlobalName);
}

void UArchSimPersistenceSubsystem::Deinitialize()
{
    UGameInstance* GI = GetGameInstance();
    if (GI)
    {
        if (USpudSubsystem* Spud = GI->GetSubsystem<USpudSubsystem>())
        {
            Spud->RemovePersistentGlobalObject(this);

            // Unbind delegate before we go away.
            Spud->PostLoadGame.RemoveDynamic(
                this, &UArchSimPersistenceSubsystem::OnPostLoadGame);
        }
    }

    Super::Deinitialize();
}

// ============================================================================
// v2 Snapshot helpers (private)
// ============================================================================

void UArchSimPersistenceSubsystem::SnapshotMaterials(const TArray<FFrameMaterial>& Materials)
{
    MaterialLibrary.Reset();
    MaterialLibrary.Reserve(Materials.Num());
    for (const FFrameMaterial& M : Materials)
    {
        FArchSimMaterialRecord Rec;
        Rec.E       = M.E;
        Rec.G       = M.G;
        Rec.Nu      = M.Nu;
        Rec.Rho     = M.Rho;
        Rec.Fy      = M.Fy;
        Rec.CapComp  = M.Cap.Comp;
        Rec.CapTens  = M.Cap.Tens;
        Rec.CapShear = M.Cap.Shear;
        Rec.CapBend  = M.Cap.Bend;
        Rec.CapTors  = M.Cap.Tors;
        Rec.CapVM    = M.Cap.VM;
        MaterialLibrary.Add(Rec);
    }
}

void UArchSimPersistenceSubsystem::SnapshotSections(const TArray<FFrameSection>& Sections)
{
    SectionLibrary.Reset();
    SectionLibrary.Reserve(Sections.Num());
    for (const FFrameSection& S : Sections)
    {
        FArchSimSectionRecord Rec;
        Rec.A   = S.A;
        Rec.Iy  = S.Iy;
        Rec.Iz  = S.Iz;
        Rec.J   = S.J;
        Rec.Cy  = S.Cy;
        Rec.Cz  = S.Cz;
        Rec.Asy = S.Asy;
        Rec.Asz = S.Asz;
        Rec.Zy  = S.Zy;
        Rec.Zz  = S.Zz;
        Rec.Shape = static_cast<uint8>(S.Shape);
        SectionLibrary.Add(Rec);
    }
}

void UArchSimPersistenceSubsystem::SnapshotNodalLoads(const TArray<FFrameNodalLoad>& Loads,
                                                        const TArray<FFrameNode>& Nodes)
{
    NodalLoads.Reset();
    NodalLoads.Reserve(Loads.Num());
    for (const FFrameNodalLoad& L : Loads)
    {
        // Resolve node id → position for replay matching.
        if (!Nodes.IsValidIndex(L.Node))
        {
            UE_LOG(LogArchSimPersistence, Warning,
                   TEXT("SnapshotNodalLoads: load references node id=%d which is out of range (%d nodes); skipped."),
                   L.Node, Nodes.Num());
            continue;
        }
        FArchSimNodalLoadRecord Rec;
        Rec.NodePosMm = Nodes[L.Node].Pos;
        Rec.Comp      = L.Comp;
        NodalLoads.Add(Rec);
    }
}

void UArchSimPersistenceSubsystem::SnapshotUDLs(const TArray<FFrameMemberUDL>& UDLs)
{
    MemberUDLs.Reset();
    MemberUDLs.Reserve(UDLs.Num());
    for (const FFrameMemberUDL& U : UDLs)
    {
        FArchSimUDLRecord Rec;
        // WHY FFrameMemberUDL::Member == MemberRecordIdx:
        //   RegisterMember keeps FFrameMember.Id == MemberIdx == the value passed back
        //   to the caller (ArchSimModelRegistry.cpp: "Member.Id = NewMemberIdx").
        //   FFrameMemberUDL::Member is set to the same user-id. Therefore
        //   U.Member == the 0-based MemberRecords[] array index for that member.
        //   Invariant: FFrameMemberUDL::Member must be < MemberRecords.Num().
        //   If this invariant breaks (e.g. a UDL references a member id that was never
        //   registered), the replay ReplayUDLs will skip the UDL and log a warning rather
        //   than crash (RecordIdxToMemberId out-of-range guard in ReplayUDLs).
        // N-01 defensive guard: skip silently rather than storing an invalid index.
        // This is a shipping-safe check (not ensureMsgf) because a corrupt model
        // (UDL with out-of-range member id) should not crash the game — it logs and
        // continues, and the user loses only the orphan UDL (acceptable degradation).
        if (U.Member < 0)
        {
            UE_LOG(LogArchSimPersistence, Warning,
                   TEXT("SnapshotUDLs: FFrameMemberUDL has negative Member=%d; "
                        "UDL skipped (invariant: Member == MemberRecords array index)."),
                   U.Member);
            continue;
        }
        Rec.MemberRecordIdx = U.Member;
        Rec.WLocal          = U.WLocal;
        MemberUDLs.Add(Rec);
    }
}

void UArchSimPersistenceSubsystem::SnapshotShells(const TArray<FFrameShellQuad>& ShellQuads,
                                                    const TArray<FFrameShellPressure>& ShellPressures,
                                                    const TArray<FFrameNode>& Nodes)
{
    Shells.Reset();
    Shells.Reserve(ShellQuads.Num());

    // Build pressure lookup by shell user id.
    TMap<int32, float> PressureById;
    for (const FFrameShellPressure& P : ShellPressures)
    {
        PressureById.Add(P.Shell, P.P);
    }

    for (const FFrameShellQuad& Q : ShellQuads)
    {
        FArchSimShellRecord Rec;
        Rec.MatIdx  = Q.MatIdx;
        Rec.T       = Q.T;
        Rec.bActive = Q.bActive;

        // Look up pressure for this shell (0 if not in map).
        if (const float* PressurePtr = PressureById.Find(Q.Id))
        {
            Rec.Pressure = *PressurePtr;
        }

        // Convert corner node ids → positions for replay.
        bool bAllCornersValid = true;
        Rec.CornerPosMm.Reserve(Q.N.Num());
        for (int32 NodeId : Q.N)
        {
            if (!Nodes.IsValidIndex(NodeId))
            {
                UE_LOG(LogArchSimPersistence, Warning,
                       TEXT("SnapshotShells: shell id=%d references out-of-range node id=%d (%d nodes); shell skipped."),
                       Q.Id, NodeId, Nodes.Num());
                bAllCornersValid = false;
                break;
            }
            Rec.CornerPosMm.Add(Nodes[NodeId].Pos);
        }

        if (!bAllCornersValid) continue;
        Shells.Add(Rec);
    }
}

void UArchSimPersistenceSubsystem::SnapshotNodeFixities(const TArray<FFrameNode>& Nodes)
{
    NodeFixities.Reset();
    for (const FFrameNode& Node : Nodes)
    {
        // Record any node with at least one fixed DOF.
        bool bAnyFixed = false;
        for (bool F : Node.Fixed)
        {
            if (F) { bAnyFixed = true; break; }
        }
        if (!bAnyFixed) continue;

        FArchSimNodeFixityRecord Rec;
        Rec.NodePosMm  = Node.Pos;
        Rec.Fixed      = Node.Fixed;
        Rec.Prescribed = Node.Prescribed;
        NodeFixities.Add(Rec);
    }
}

// ============================================================================
// v2 Replay helpers (private)
// ============================================================================

void UArchSimPersistenceSubsystem::ReplayMaterials(UArchSimModelRegistry* Registry) const
{
    if (MaterialLibrary.IsEmpty()) return;

    TArray<FFrameMaterial> Materials;
    Materials.Reserve(MaterialLibrary.Num());
    for (const FArchSimMaterialRecord& R : MaterialLibrary)
    {
        FFrameMaterial M;
        M.E   = R.E;
        M.G   = R.G;
        M.Nu  = R.Nu;
        M.Rho = R.Rho;
        M.Fy  = R.Fy;
        M.Cap.Comp  = R.CapComp;
        M.Cap.Tens  = R.CapTens;
        M.Cap.Shear = R.CapShear;
        M.Cap.Bend  = R.CapBend;
        M.Cap.Tors  = R.CapTors;
        M.Cap.VM    = R.CapVM;
        Materials.Add(M);
    }

    TArray<FFrameSection> Sections;
    Sections.Reserve(SectionLibrary.Num());
    for (const FArchSimSectionRecord& R : SectionLibrary)
    {
        FFrameSection S;
        S.A   = R.A;
        S.Iy  = R.Iy;
        S.Iz  = R.Iz;
        S.J   = R.J;
        S.Cy  = R.Cy;
        S.Cz  = R.Cz;
        S.Asy = R.Asy;
        S.Asz = R.Asz;
        S.Zy  = R.Zy;
        S.Zz  = R.Zz;
        S.Shape = static_cast<EFrameSectionShape>(R.Shape);
        Sections.Add(S);
    }

    const bool bOk = Registry->RestoreLibraries(Materials, Sections);
    if (!bOk)
    {
        UE_LOG(LogArchSimPersistence, Warning,
               TEXT("ReplayMaterials: RestoreLibraries returned false; libraries not restored."));
    }
}

void UArchSimPersistenceSubsystem::ReplaySections(UArchSimModelRegistry* /*Registry*/) const
{
    // Section restore is handled inside ReplayMaterials (both called together
    // via RestoreLibraries). This method kept for symmetry / future extension.
}

void UArchSimPersistenceSubsystem::ReplayNodalLoads(UArchSimModelRegistry* Registry) const
{
    if (NodalLoads.IsEmpty()) return;

    const TArray<FFrameNode>& Nodes = Registry->GetCurrentModel().Nodes;

    TArray<FFrameNodalLoad> Loads;
    Loads.Reserve(NodalLoads.Num());
    int32 FailCount = 0;

    for (const FArchSimNodalLoadRecord& R : NodalLoads)
    {
        // Find node by position (1 mm tolerance — same as FindOrAddNode).
        int32 FoundNodeIdx = -1;
        for (int32 i = 0; i < Nodes.Num(); ++i)
        {
            if (FVector::Dist(Nodes[i].Pos, R.NodePosMm) < 1.0)
            {
                FoundNodeIdx = i;
                break;
            }
        }

        if (FoundNodeIdx < 0)
        {
            UE_LOG(LogArchSimPersistence, Warning,
                   TEXT("ReplayNodalLoads: no node within 1 mm of (%.1f,%.1f,%.1f) mm; load skipped."),
                   R.NodePosMm.X, R.NodePosMm.Y, R.NodePosMm.Z);
            ++FailCount;
            continue;
        }

        FFrameNodalLoad Load;
        Load.Node = FoundNodeIdx;
        Load.Comp = R.Comp;
        Loads.Add(Load);
    }

    Registry->InjectNodalLoads(Loads);
    UE_LOG(LogArchSimPersistence, Display,
           TEXT("ReplayNodalLoads: %d/%d loads applied (%d failed/skipped)."),
           Loads.Num(), NodalLoads.Num(), FailCount);
}

void UArchSimPersistenceSubsystem::ReplayUDLs(UArchSimModelRegistry* Registry,
                                                const TArray<int32>& RecordIdxToMemberId) const
{
    if (MemberUDLs.IsEmpty()) return;

    TArray<FFrameMemberUDL> UDLs;
    UDLs.Reserve(MemberUDLs.Num());
    int32 FailCount = 0;

    for (const FArchSimUDLRecord& R : MemberUDLs)
    {
        // R.MemberRecordIdx == FFrameMemberUDL::Member == user id == MemberIdx in our scheme.
        // The RecordIdxToMemberId map gives us the replayed member id for that record slot.
        if (!RecordIdxToMemberId.IsValidIndex(R.MemberRecordIdx))
        {
            UE_LOG(LogArchSimPersistence, Warning,
                   TEXT("ReplayUDLs: MemberRecordIdx=%d out of range (record count=%d); UDL skipped."),
                   R.MemberRecordIdx, RecordIdxToMemberId.Num());
            ++FailCount;
            continue;
        }

        const int32 MemberId = RecordIdxToMemberId[R.MemberRecordIdx];
        if (MemberId < 0)
        {
            // Member failed to register; skip its UDLs.
            ++FailCount;
            continue;
        }

        FFrameMemberUDL UDL;
        UDL.Member = MemberId;
        UDL.WLocal = R.WLocal;
        UDLs.Add(UDL);
    }

    Registry->InjectMemberUDLs(UDLs);
    UE_LOG(LogArchSimPersistence, Display,
           TEXT("ReplayUDLs: %d/%d UDLs applied (%d failed/skipped)."),
           UDLs.Num(), MemberUDLs.Num(), FailCount);
}

void UArchSimPersistenceSubsystem::ReplayShells(UArchSimModelRegistry* Registry) const
{
    if (Shells.IsEmpty()) return;

    TArray<FFrameShellQuad>     ShellQuads;
    TArray<FFrameShellPressure> Pressures;
    ShellQuads.Reserve(Shells.Num());
    int32 FailCount = 0;

    for (int32 Si = 0; Si < Shells.Num(); ++Si)
    {
        const FArchSimShellRecord& R = Shells[Si];

        if (R.CornerPosMm.Num() != 4)
        {
            UE_LOG(LogArchSimPersistence, Warning,
                   TEXT("ReplayShells: shell[%d] has %d corners (expected 4); skipped."),
                   Si, R.CornerPosMm.Num());
            ++FailCount;
            continue;
        }

        // Resolve corner positions → node ids (FindOrAddNodePublic = 1 mm tolerance).
        FFrameShellQuad Q;
        Q.Id     = Si;      // Assign a fresh id per replayed shell.
        Q.MatIdx = R.MatIdx;
        Q.T      = R.T;
        Q.bActive= R.bActive;
        Q.N.Reserve(4);

        bool bOk = true;
        for (const FVector& CornerMm : R.CornerPosMm)
        {
            if (CornerMm.ContainsNaN())
            {
                UE_LOG(LogArchSimPersistence, Warning,
                       TEXT("ReplayShells: shell[%d] corner has NaN position; shell skipped."), Si);
                bOk = false;
                break;
            }
            const int32 NodeIdx = Registry->FindOrAddNodePublic(CornerMm);
            Q.N.Add(NodeIdx);
        }

        if (!bOk) { ++FailCount; continue; }

        ShellQuads.Add(Q);

        if (R.Pressure != 0.f)
        {
            FFrameShellPressure P;
            P.Shell = Q.Id;
            P.P     = R.Pressure;
            Pressures.Add(P);
        }
    }

    Registry->InjectShells(ShellQuads);
    if (!Pressures.IsEmpty())
    {
        Registry->InjectShellPressures(Pressures);
    }

    UE_LOG(LogArchSimPersistence, Display,
           TEXT("ReplayShells: %d/%d shells replayed (%d failed/skipped), %d pressures."),
           ShellQuads.Num(), Shells.Num(), FailCount, Pressures.Num());
}

void UArchSimPersistenceSubsystem::ReplayNodeFixities(UArchSimModelRegistry* Registry) const
{
    if (NodeFixities.IsEmpty()) return;

    int32 OkCount   = 0;
    int32 FailCount = 0;

    for (const FArchSimNodeFixityRecord& R : NodeFixities)
    {
        const bool bOk = Registry->ApplyFixityAt(R.NodePosMm, R.Fixed, R.Prescribed);
        if (bOk) ++OkCount; else ++FailCount;
    }

    UE_LOG(LogArchSimPersistence, Display,
           TEXT("ReplayNodeFixities: %d/%d fixity records applied (%d failed/skipped)."),
           OkCount, NodeFixities.Num(), FailCount);
}

// ---- Save -------------------------------------------------------------------

void UArchSimPersistenceSubsystem::SnapshotCurrentModel()
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) return;
    UArchSimModelRegistry* Registry = GI->GetSubsystem<UArchSimModelRegistry>();
    if (!Registry)
    {
        UE_LOG(LogArchSimPersistence, Warning,
               TEXT("SnapshotCurrentModel: UArchSimModelRegistry not found."));
        return;
    }

    const FFrameModelDef& Model = Registry->GetCurrentModel();

    // ---- v2: clear all sidecar arrays before rebuild ----
    MemberRecords.Reset();
    MemberActiveFlags.Reset();
    SupportPositions.Reset();
    MaterialLibrary.Reset();
    SectionLibrary.Reset();
    NodalLoads.Reset();
    MemberUDLs.Reset();
    Shells.Reset();
    NodeFixities.Reset();

    // ---- v2: snapshot libraries first (authoritative from CurrentModel) ----
    SnapshotMaterials(Model.Materials);
    SnapshotSections(Model.Sections);

    // ---- Rebuild MemberRecords from the currently registered components ----
    // v2 includes deactivated members (bActive=false) too — their geometry is
    // preserved for future reactivation; only active members need a live actor.
    //
    // Strategy: walk Model.Members[] in order. For active members, search the
    // world for a component with matching MemberIdx. For deactivated members,
    // record geometry from the model's node positions (no actor needed).
    UWorld* World = GI->GetWorld();

    for (int32 Idx = 0; Idx < Model.Members.Num(); ++Idx)
    {
        const FFrameMember& M = Model.Members[Idx];
        const bool bActive = M.bActive;

        if (bActive)
        {
            // Active member: find its component in the world.
            bool bFoundComponent = false;
            if (World)
            {
                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    AActor* Actor = *It;
                    if (!Actor) continue;

                    TArray<UActorComponent*> Comps;
                    Actor->GetComponents(UArchSimMemberData::StaticClass(), Comps);
                    for (UActorComponent* C : Comps)
                    {
                        UArchSimMemberData* MD = Cast<UArchSimMemberData>(C);
                        if (!MD || MD->MemberIdx != Idx) continue;

                        FArchSimMemberRecord Rec;
                        Rec.WorldTransform   = Actor->GetActorTransform();
                        Rec.EndIOffsetUE     = MD->EndIOffsetUE;
                        Rec.EndJOffsetUE     = MD->EndJOffsetUE;
                        Rec.StructureGroupId = MD->StructureGroupId;
                        Rec.MaterialId       = MD->MaterialId;
                        Rec.SectionId        = MD->SectionId;
                        // N-00 wire: capture bTensionOnly and Release from CurrentModel
                        // (not from the component — those flags live on the FFrameMember row,
                        // not on UArchSimMemberData). Idx is the current loop variable.
                        Rec.bTensionOnly = M.bTensionOnly;
                        Rec.Release      = M.Release;
                        MemberRecords.Add(Rec);
                        MemberActiveFlags.Add(true);
                        bFoundComponent = true;
                        break;
                    }
                    if (bFoundComponent) break;
                }
            }

            if (!bFoundComponent)
            {
                UE_LOG(LogArchSimPersistence, Warning,
                       TEXT("SnapshotCurrentModel: no component found for active MemberIdx=%d; "
                            "this member will not be saved."), Idx);
            }
        }
        else
        {
            // Deactivated member: reconstruct a record from model node positions.
            // WHY: no live actor, but we still want to preserve the geometry so
            // that the deactivated-save test can verify round-trip correctness.
            // We use the node positions stored in the model (mm) and construct
            // a placeholder actor transform (identity) with offsets derived from
            // the node world positions. Since deactivated members will be re-
            // deactivated during replay (not re-activated), the actor is spawned
            // only briefly to allow RegisterMember, then DeactivateMember is called.
            //
            // Node I / J world position in mm → convert to UE cm for EndI/J offsets.
            // We store a synthetic identity transform + EndI/J as absolute positions
            // in UE cm so the replay can recompute the mm positions correctly.
            const bool bIValid = Model.Nodes.IsValidIndex(M.I);
            const bool bJValid = Model.Nodes.IsValidIndex(M.J);
            if (!bIValid || !bJValid)
            {
                UE_LOG(LogArchSimPersistence, Warning,
                       TEXT("SnapshotCurrentModel: deactivated member %d has invalid node refs "
                            "(I=%d J=%d, Nodes.Num=%d); skipped."),
                       Idx, M.I, M.J, Model.Nodes.Num());
                continue;
            }

            // Store as identity transform + EndI/J offsets = node positions in UE cm.
            //
            // N-02 calculation chain (snapshot ↔ replay inverse):
            //
            //  SNAPSHOT side (here):
            //    NodeIMm (FrameCore mm) → Rec.EndIOffsetUE = NodeIMm * 0.1   [UE cm]
            //    Rec.WorldTransform = FTransform::Identity
            //
            //  REPLAY side (ReplayLoadedSidecar, Step 2):
            //    ActorTransform = Rec.WorldTransform = Identity
            //    WorldPosCm = ActorT.TransformPosition(Rec.EndIOffsetUE)
            //               = Identity * (NodeIMm * 0.1) = NodeIMm * 0.1       [UE cm]
            //    PosIMm = WorldPosCm * 10 = NodeIMm * 0.1 * 10 = NodeIMm       [mm] ✓
            //
            //  WHY identity transform: a deactivated member has no live actor, so we
            //  cannot read a world transform. Identity + absolute offsets is equivalent
            //  to "actor placed at world origin, endpoint == offset". The 0.1/10 round-
            //  trip is exact for all representable float values (no drift across cycles).
            const FVector NodeIMm = Model.Nodes[M.I].Pos;
            const FVector NodeJMm = Model.Nodes[M.J].Pos;
            constexpr float kMmToCm = 0.1f;

            FArchSimMemberRecord Rec;
            Rec.WorldTransform   = FTransform::Identity;
            Rec.EndIOffsetUE     = NodeIMm * kMmToCm;  // UE cm
            Rec.EndJOffsetUE     = NodeJMm * kMmToCm;  // UE cm
            Rec.StructureGroupId = -1;
            Rec.MaterialId       = M.MatIdx;
            Rec.SectionId        = M.SecIdx;
            // N-00 wire: capture bTensionOnly and Release for deactivated members too.
            // These flags survive the deactivation path (DeactivateMember only clears
            // bActive; it does NOT reset bTensionOnly or Release).
            Rec.bTensionOnly = M.bTensionOnly;
            Rec.Release      = M.Release;
            MemberRecords.Add(Rec);
            MemberActiveFlags.Add(false);
        }
    }

    // ---- v2: snapshot loads, shells, fixity from CurrentModel ----
    SnapshotNodalLoads(Model.NodalLoads, Model.Nodes);
    SnapshotUDLs(Model.MemberUDLs);
    SnapshotShells(Model.Shells, Model.ShellPressures, Model.Nodes);
    SnapshotNodeFixities(Model.Nodes);

    // ---- v1 compat: SupportPositions left empty for v2 archives ----
    // (NodeFixities now covers all fixity nodes; v1 replay uses SupportPositions,
    // v2 replay uses NodeFixities. v2 does NOT populate SupportPositions.)
    SupportPositions.Reset();

    // ---- Set format version ----
    SidecarFormatVersion = kCurrentFormatVersion;

    // Count active vs deactivated for the log line.
    int32 ActiveCount = 0;
    for (bool bFlag : MemberActiveFlags) { if (bFlag) ++ActiveCount; }
    const int32 DeactivatedLogCount = MemberActiveFlags.Num() - ActiveCount;

    UE_LOG(LogArchSimPersistence, Display,
           TEXT("SnapshotCurrentModel (v2): %d members (%d active, %d deactivated) + "
                "%d materials + %d sections + %d nodal loads + %d UDLs + "
                "%d shells + %d fixity nodes."),
           MemberRecords.Num(), ActiveCount, DeactivatedLogCount,
           MaterialLibrary.Num(), SectionLibrary.Num(),
           NodalLoads.Num(), MemberUDLs.Num(),
           Shells.Num(), NodeFixities.Num());
}

bool UArchSimPersistenceSubsystem::SaveToSlot(const FString& SlotName,
                                               const bool bAllowEmptyOverwrite)
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) return false;

    USpudSubsystem* Spud = GI->GetSubsystem<USpudSubsystem>();
    if (!Spud)
    {
        UE_LOG(LogArchSimPersistence, Warning, TEXT("SaveToSlot: USpudSubsystem not found."));
        return false;
    }

    if (!Spud->IsIdle())
    {
        UE_LOG(LogArchSimPersistence, Warning,
               TEXT("SaveToSlot: SPUD not in RunningIdle; save refused."));
        return false;
    }

    // Snapshot BEFORE handing off to SPUD so the sidecar arrays are current.
    // SPUD calls StoreGlobalObject (SpudState.cpp:405) which scans our
    // UPROPERTY(SaveGame) fields via VisitPersistentProperties; by the time
    // FinishSaveGame runs those fields must already hold the latest state.
    SnapshotCurrentModel();

    // --- Partial snapshot detection -----------------------------------
    // v2: deactivated members ARE included in MemberRecords (with bActive=false
    // in MemberActiveFlags). The partial-snapshot guard must count only
    // active-member records against IndexToComponent.
    //
    // Count active captures in MemberRecords (those with MemberActiveFlags[i]==true).
    // Compare against Registry->GetRegisteredCount() which counts IndexToComponent
    // entries (only ever incremented for active, registered members).
    {
        UArchSimModelRegistry* Registry = GI->GetSubsystem<UArchSimModelRegistry>();
        if (Registry)
        {
            int32 ActiveCaptured = 0;
            for (int32 i = 0; i < MemberActiveFlags.Num(); ++i)
            {
                if (i < MemberRecords.Num() && MemberActiveFlags[i])
                {
                    ++ActiveCaptured;
                }
            }

            const int32 RegisteredCount = Registry->GetRegisteredCount();
            if (ActiveCaptured < RegisteredCount)
            {
                UE_LOG(LogArchSimPersistence, Error,
                       TEXT("SaveToSlot: partial snapshot detected — %d active members registered "
                            "but only %d active members captured in sidecar. Save REFUSED to prevent "
                            "overwriting a good slot with incomplete data. "
                            "(Check for actors with missing UArchSimMemberData or "
                            "inactive-but-registered components in the current world.)"),
                       RegisteredCount, ActiveCaptured);
                return false;
            }
        }
    }

    // --- Empty-overwrite guard ----------------------------------------
    // If the snapshot is completely empty (0 members + 0 supports) AND the target
    // slot already exists, refuse to overwrite unless the caller explicitly opts in.
    if (MemberRecords.Num() == 0 && SupportPositions.Num() == 0 && NodeFixities.Num() == 0)
    {
        const bool bSlotExists = (Spud->GetSaveGameInfo(SlotName) != nullptr);
        if (bSlotExists && !bAllowEmptyOverwrite)
        {
            UE_LOG(LogArchSimPersistence, Error,
                   TEXT("SaveToSlot: sidecar is empty (0 members, 0 supports/fixities) and slot '%s' "
                        "already exists. Save REFUSED to protect existing save data. "
                        "To intentionally overwrite with an empty save, pass "
                        "bAllowEmptyOverwrite=true."),
                   *SlotName);
            return false;
        }

        if (bSlotExists)
        {
            UE_LOG(LogArchSimPersistence, Warning,
                   TEXT("SaveToSlot: sidecar is empty; bAllowEmptyOverwrite=true — "
                        "proceeding to overwrite slot '%s' with empty data."), *SlotName);
        }
        else
        {
            UE_LOG(LogArchSimPersistence, Warning,
                   TEXT("SaveToSlot: sidecar is empty (no members, no supports). "
                        "Saving an empty slot '%s' (no prior data to protect)."), *SlotName);
        }
    }

    // Delegate to SPUD. SaveGame is async; caller can bind PostSaveGame delegate
    // if it needs completion notification.
    Spud->SaveGame(SlotName, FText::FromString(SlotName),
                   /*bTakeScreenshot=*/false);

    UE_LOG(LogArchSimPersistence, Display,
           TEXT("SaveToSlot: issued SaveGame for slot '%s' "
                "(v%d: %d members, %d materials, %d sections, %d loads, %d UDLs, %d shells, %d fixities)."),
           *SlotName, SidecarFormatVersion,
           MemberRecords.Num(), MaterialLibrary.Num(), SectionLibrary.Num(),
           NodalLoads.Num(), MemberUDLs.Num(), Shells.Num(), NodeFixities.Num());
    return true;
}

// ---- Load -------------------------------------------------------------------

bool UArchSimPersistenceSubsystem::LoadFromSlot(const FString& SlotName)
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) return false;

    USpudSubsystem* Spud = GI->GetSubsystem<USpudSubsystem>();
    if (!Spud)
    {
        UE_LOG(LogArchSimPersistence, Warning, TEXT("LoadFromSlot: USpudSubsystem not found."));
        return false;
    }

    if (!Spud->IsIdle())
    {
        UE_LOG(LogArchSimPersistence, Warning,
               TEXT("LoadFromSlot: SPUD not in RunningIdle; load refused."));
        return false;
    }

    // Pre-check slot existence (GetSaveGameInfo returns null if not on disk).
    if (Spud->GetSaveGameInfo(SlotName) == nullptr)
    {
        UE_LOG(LogArchSimPersistence, Warning,
               TEXT("LoadFromSlot: slot '%s' does not exist; load refused. "
                    "(Check that a prior SaveToSlot completed successfully.)"),
               *SlotName);
        return false;
    }

    Spud->LoadGame(SlotName);

    UE_LOG(LogArchSimPersistence, Display,
           TEXT("LoadFromSlot: issued LoadGame for slot '%s'. "
                "Replay will fire in OnPostLoadGame."), *SlotName);
    return true;
}

// ---- Reset ------------------------------------------------------------------

void UArchSimPersistenceSubsystem::ResetRegistry()
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) return;
    UArchSimModelRegistry* Registry = GI->GetSubsystem<UArchSimModelRegistry>();
    if (!Registry) return;
    Registry->Reset();
}

// ---- PostLoadGame delegate --------------------------------------------------

void UArchSimPersistenceSubsystem::OnPostLoadGame(const FString& SlotName, bool bSuccess)
{
    if (!bSuccess)
    {
        UE_LOG(LogArchSimPersistence, Warning,
               TEXT("OnPostLoadGame: load of slot '%s' failed; replay skipped."),
               *SlotName);
        return;
    }

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    UWorld* World = GI->GetWorld();
    if (!World)
    {
        UE_LOG(LogArchSimPersistence, Warning,
               TEXT("OnPostLoadGame: no World available; replay skipped."));
        return;
    }

    ReplayLoadedSidecar(World);
}

void UArchSimPersistenceSubsystem::ReplayLoadedSidecar(UWorld* World)
{
    UGameInstance* GI = World->GetGameInstance();
    if (!GI) return;

    UArchSimModelRegistry* Registry = GI->GetSubsystem<UArchSimModelRegistry>();
    if (!Registry)
    {
        UE_LOG(LogArchSimPersistence, Warning,
               TEXT("ReplayLoadedSidecar: Registry not available."));
        return;
    }

    // Clear stale model from pre-load state. Registry::Reset() also clears the
    // timer so no debounce fires on stale model.
    Registry->Reset();

    UE_LOG(LogArchSimPersistence, Display,
           TEXT("ReplayLoadedSidecar: SidecarFormatVersion=%d, %d members, "
                "%d materials, %d sections, %d fixities."),
           SidecarFormatVersion, MemberRecords.Num(),
           MaterialLibrary.Num(), SectionLibrary.Num(), NodeFixities.Num());

    // ===================================================================
    // VERSION BRANCH: v2 vs v1
    // ===================================================================
    const bool bIsV2 = (SidecarFormatVersion >= 2);

    // ---- Step 0 (v2): restore library (materials + sections) ----
    // Must come before member registration so MatIdx/SecIdx are valid.
    // v1: EnsureDefaultLibraries() is called automatically inside RegisterMember.
    if (bIsV2 && !MaterialLibrary.IsEmpty())
    {
        ReplayMaterials(Registry);
    }

    // ---- Step 1: replay fixed supports (v1: SupportPositions; v2: NodeFixities) ----
    // WHY supports-first: member endpoint nodes dedup against support positions;
    // registering supports before members ensures Fixed flag is set correctly.
    if (!bIsV2)
    {
        // v1 path: all-fixed nodes from SupportPositions.
        for (const FVector& SupportPosMm : SupportPositions)
        {
            const int32 NodeIdx = Registry->RegisterFixedSupport(SupportPosMm);
            if (NodeIdx < 0)
            {
                UE_LOG(LogArchSimPersistence, Warning,
                       TEXT("ReplayLoadedSidecar: RegisterFixedSupport failed for "
                            "(%.1f,%.1f,%.1f) mm."),
                       SupportPosMm.X, SupportPosMm.Y, SupportPosMm.Z);
            }
        }
    }
    // For v2 we replay NodeFixities AFTER members (so nodes from member endpoints
    // exist before we try to apply fixity by position). See Step 5 below.

    // ---- Step 2: replay member placements ----
    // RecordIdxToMemberId maps MemberRecords[] index → assigned MemberIdx (-1 if failed).
    TArray<int32> RecordIdxToMemberId;
    RecordIdxToMemberId.Init(-1, MemberRecords.Num());

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    int32 ReplayedCount  = 0;
    int32 DeactivatedCount = 0;

    for (int32 RecIdx = 0; RecIdx < MemberRecords.Num(); ++RecIdx)
    {
        const FArchSimMemberRecord& Rec = MemberRecords[RecIdx];
        // bIsV2: use MemberActiveFlags if available (parallel array).
        const bool bRecordActive = !bIsV2 || !MemberActiveFlags.IsValidIndex(RecIdx) || MemberActiveFlags[RecIdx];

        // Spawn at Identity; set transform after root is established.
        AActor* Actor = World->SpawnActor<AActor>(
            AActor::StaticClass(), FTransform::Identity, SP);
        if (!Actor)
        {
            UE_LOG(LogArchSimPersistence, Warning,
                   TEXT("ReplayLoadedSidecar: SpawnActor failed for record %d; "
                        "member skipped."), RecIdx);
            continue;
        }

        // Graft a root SceneComponent so SetActorTransform takes effect.
        USceneComponent* Root = NewObject<USceneComponent>(
            Actor, USceneComponent::StaticClass(), TEXT("ReplayRoot"));
        if (!Root)
        {
            UE_LOG(LogArchSimPersistence, Warning,
                   TEXT("ReplayLoadedSidecar: NewObject<USceneComponent> failed "
                        "for record %d; member skipped."), RecIdx);
            Actor->Destroy();
            continue;
        }
        Actor->SetRootComponent(Root);
        Root->RegisterComponent();
        Actor->SetActorTransform(Rec.WorldTransform);

        // Add the ArchSimMemberData instance component.
        UArchSimMemberData* MD = NewObject<UArchSimMemberData>(
            Actor, UArchSimMemberData::StaticClass(), NAME_None);
        if (!MD)
        {
            UE_LOG(LogArchSimPersistence, Warning,
                   TEXT("ReplayLoadedSidecar: NewObject<UArchSimMemberData> failed "
                        "for record %d; member skipped."), RecIdx);
            Actor->Destroy();
            continue;
        }
        MD->EndIOffsetUE     = Rec.EndIOffsetUE;
        MD->EndJOffsetUE     = Rec.EndJOffsetUE;
        MD->StructureGroupId = Rec.StructureGroupId;
        MD->MaterialId       = Rec.MaterialId;
        MD->SectionId        = Rec.SectionId;

        Actor->AddInstanceComponent(MD);
        MD->RegisterComponent();

        const int32 NewIdx = Registry->RegisterMember(MD);
        if (NewIdx < 0)
        {
            UE_LOG(LogArchSimPersistence, Warning,
                   TEXT("ReplayLoadedSidecar: RegisterMember failed for record %d "
                        "(zero-length, non-finite endpoints, or validation error); "
                        "member skipped and spawned actor destroyed."),
                   RecIdx);
            World->DestroyActor(Actor);
            continue;
        }

        RecordIdxToMemberId[RecIdx] = NewIdx;
        ++ReplayedCount;

        // N-00 wire: restore bTensionOnly + Release from the sidecar record.
        // WHY after RegisterMember: RegisterMember assigns Member.bTensionOnly=false and
        // Release.Init(false,12). We overwrite those defaults here with the saved values.
        // Only call when Release is non-empty (v1 archives leave it empty → keep defaults).
        // SetMemberFlags returns false only on out-of-range or wrong Release.Num; both
        // are impossible here because NewIdx was just returned by RegisterMember (valid)
        // and Rec.Release was snapshotted with exactly 12 elements or left empty.
        if (bIsV2 && (Rec.bTensionOnly || !Rec.Release.IsEmpty()))
        {
            (void)Registry->SetMemberFlags(NewIdx, Rec.bTensionOnly, Rec.Release);
        }

        // v2: re-deactivate members that were deactivated at save time.
        if (bIsV2 && !bRecordActive)
        {
            Registry->DeactivateMember(NewIdx);
            ++DeactivatedCount;
        }
    }

    const int32 FailedCount = MemberRecords.Num() - ReplayedCount;

    // ---- Step 3 (v2): inject loads, UDLs, shells ----
    if (bIsV2)
    {
        ReplayNodalLoads(Registry);
        ReplayUDLs(Registry, RecordIdxToMemberId);
        ReplayShells(Registry);
    }

    // ---- Step 4 (v2): apply per-node fixity ----
    // WHY after members: member registration creates the endpoint nodes that fixity
    // records reference. Applying fixity before member nodes exist would silently skip
    // all fixity entries (ApplyFixityAt finds no matching node).
    if (bIsV2 && !NodeFixities.IsEmpty())
    {
        ReplayNodeFixities(Registry);
    }

    UE_LOG(LogArchSimPersistence, Display,
           TEXT("ReplayLoadedSidecar (v%d): replayed %d/%d members "
                "(%d failed/skipped, %d re-deactivated) + %d supports. "
                "Registry now has %d registered."),
           SidecarFormatVersion,
           ReplayedCount, MemberRecords.Num(), FailedCount, DeactivatedCount,
           SupportPositions.Num(),
           Registry->GetRegisteredCount());

    // ---- Step 5: kick a solve ----
    FFrameModelPatch EmptyPatch{};
    Registry->RequestSolve(EmptyPatch);
}
