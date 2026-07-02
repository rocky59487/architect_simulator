// ArchSim - UArchSimPersistenceSubsystem : SPUD-wired persistence orchestrator.
// AS-08-u1 (Sprint S-08). Closes the persistence chain begun in S-01/A1-07.
// AS-41-u1 (Sprint S-09). Sidecar format v2: full model-state persistence
//   (materials, sections, loads, UDLs, shells, per-node fixity, active state).
//
// Design (Placement-Replay Sidecar):
//   SPUD cannot save runtime-spawned bare AActor members directly because:
//     (a) UArchSimMemberData is an AddInstanceComponent component; SPUD's property
//         scan walks the Actor object's UPROPERTY(SaveGame) fields, not dynamically-
//         added instance components (SpudState.cpp:755 RestoreObjectProperties scans
//         Actor->GetClass() properties, not the instance-component list).
//     (b) EndIOffsetUE/EndJOffsetUE on UArchSimMemberData have no SaveGame flag
//         (ArchSimMemberData.h:50-54), so geometry is not recovered from SaveGame
//         archives on any path.
//     (c) Runtime actors require a SPUD SpudGuid property (FGuid) to be tracked
//         across save/load (SpudState.cpp:314 GetSpawnedActorData; SpudPropertyUtil.cpp
//         :492 WriteActorRefPropertyData); our K-set actors are bare AActor with no
//         such property.
//   Support nodes have no actor representation at all (pure FFrameModelDef state).
//
//   Chosen path:
//     This subsystem registers itself as a SPUD global object
//     (SpudSubsystem::AddPersistentGlobalObjectWithName). Its UPROPERTY(SaveGame)
//     TArrays capture member placement records + support positions at SaveGame time.
//     On LoadGame completion the arrays are replayed: Registry is cleared, supports
//     are re-registered, member actors are respawned via a replay helper.
//
//   MemberIdx semantics after load:
//     Indices are re-assigned monotonically from 0 (replay order preserves logical
//     member order). CachedUtilization is re-computed after the post-replay Solve;
//     the stored SaveGame value is a display fallback shown before the first Solve.
//
// Slot convention:
//   Manual saves: "ArchSimSlot_<N>" (N = 1-based, callers use SaveToSlot(1..N)).
//   Auto-save:    SPUD built-in AutoSave slot (slot name "__AutoSave__").
//   Quick-save:   SPUD built-in QuickSave slot (slot name "__QuickSave__").
//   Only one user-slot at a time is expected in the current MVP; multi-slot UI
//   is deferred to a later task.
//
// SPUD integration notes:
//   * This subsystem must be registered with SPUD after SpudSubsystem is available.
//     We do that in UArchSimGameInstance::Init via GetSubsystem<USpudSubsystem>.
//   * SPUD global-object restore fires BEFORE PostLoadMap level restore
//     (SpudSubsystem.cpp:963-972 shows GlobalObjects restored before OpenLevel).
//     Our own PostLoadGame delegate fires AFTER the full load completes (SpudSubsystem
//     .cpp:981 LoadComplete broadcasts PostLoadGame). We subscribe PostLoadGame to
//     trigger the replay on the now-fully-loaded level.
//
// PIE timing caveat (for AS-08-u2):
//   SPUD's NewGame call is deferred 0.2 s in PIE mode (SpudSubsystem.cpp Initialize).
//   SaveGame/LoadGame require RunningIdle state. PIE smoke must wait for SPUD
//   NewGame to complete before calling Save or Load.
//
// Sidecar format versions:
//   v1 (SidecarFormatVersion absent / 0 or 1):
//       MemberRecords (Transform/EndI/EndJ/Group/MatId/SecId) + SupportPositions (all-fixed).
//       Active-only members stored; deactivated members NOT recorded.
//   v2 (SidecarFormatVersion == 2):
//       All v1 fields retained (append-only; v1 archives load cleanly — SPUD slow-path
//       restores only stored fields, new v2 UPROPERTY(SaveGame) fields get their
//       C++ default values when loading a v1 archive; SpudState.cpp:1071-1078 confirms
//       the skip-and-default behaviour for properties not found in the stored class def).
//       New: MaterialLibrary / SectionLibrary (full parameter roundtrip) /
//            MemberActiveFlags (deactivated members now recorded) /
//            NodalLoads / MemberUDLs / ShellPressures / Shells (geometry + mat/thick) /
//            NodeFixities (per-node general fixity, not just all-fixed supports).
//
// Deliberately excluded from the sidecar (honest record):
//   RefVec (FFrameMember::RefVec): NOT persisted. Rationale: RefVec is re-derived
//   during RegisterMember by PickRefVecForAxis(AxisUnit), which picks a non-parallel
//   reference vector from the member axis geometry alone. Any user-supplied RefVec
//   override is therefore silently overwritten on replay. Boundary: members whose
//   orientation depends on a custom RefVec (non-default reference plane) will replay
//   with the geometric default. This affects only members where the intended local
//   y/z frame differs from the automatically picked frame — typically unusual
//   orientations. Persisting RefVec is deferred; no current game-body consumer
//   sets a custom RefVec.
//
// SPUD missing-property behaviour (v1 compat guarantee):
//   When loading a v1 archive into a v2-class object, SPUD's RestoreSlowPropertyVisitor
//   encounters v2 properties not present in the stored ClassDef. At SpudState.cpp:1071
//   ("Skipping property %s ... not found in class definition") and SpudState.cpp:1077
//   ("data not found"), it logs at Log level and returns true (continues restore without
//   writing the property). The v2 fields therefore retain their C++ default values:
//     SidecarFormatVersion = 0 (default int32) → interpreted as v1 by our version branch.
//     MaterialLibrary, SectionLibrary, etc. = empty TArray (default) → v1 replay path
//     skips them and calls EnsureDefaultLibraries() as before.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FrameCoreUE/FrameCoreUEModelTypes.h"  // FFrameMaterial, FFrameSection, FFrameNodalLoad, etc.
#include "ArchSimPersistenceSubsystem.generated.h"

// ============================================================================
// v1 record (retained as-is for append-only compat)
// ============================================================================

// Per-member placement record stored in the sidecar for save/load replay.
// All geometry in UE centimetres (world-space for world-transform fields,
// local-space for EndI/J offset fields — mirrors UArchSimMemberData layout).
USTRUCT(BlueprintType)
struct ARCHSIM_API FArchSimMemberRecord
{
    GENERATED_BODY()

    // World-space actor transform at placement time.
    // Used to reposition the replayed AActor via SetActorTransform.
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    FTransform WorldTransform = FTransform::Identity;

    // Local-space end-I offset in UE cm (mirrors UArchSimMemberData::EndIOffsetUE).
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    FVector EndIOffsetUE = FVector(-50.f, 0.f, 0.f);

    // Local-space end-J offset in UE cm (mirrors UArchSimMemberData::EndJOffsetUE).
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    FVector EndJOffsetUE = FVector(+50.f, 0.f, 0.f);

    // Optional grouping id; -1 = ungrouped (mirrors UArchSimMemberData::StructureGroupId).
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    int32 StructureGroupId = -1;

    // MaterialId index (mirrors UArchSimMemberData::MaterialId; 0 = default S275).
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    int32 MaterialId = 0;

    // SectionId index (mirrors UArchSimMemberData::SectionId; 0 = default 200×200 rect).
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    int32 SectionId = 0;

    // WHY appended (USTRUCT append-only policy): v1 archives that lack these two
    // fields will have them default-initialized by SPUD's skip-and-default path
    // (SpudState.cpp:1077), giving bTensionOnly=false and Release=[] — both match
    // FFrameMember defaults, so v1 archives replay correctly without any branch.
    //
    // bTensionOnly: mirrors FFrameMember::bTensionOnly. Stored so that tension-only
    // members survive a save/load cycle. Default false = normal two-way member.
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    bool bTensionOnly = false;

    // Release: 12-bool per-DOF end release [node-i 6][node-j 6], mirrors
    // FFrameMember::Release. Stored so that pinned/hinged members survive round-trip.
    // Empty on v1 archive load (SPUD default) → replay leaves Release.Init(false,12)
    // in RegisterMember, which is correct (no release = rigid connection).
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    TArray<bool> Release;
};

// ============================================================================
// v2 new record types (APPENDED to subsystem, not inserted before v1 fields)
// ============================================================================

// Stores one material's full parameter set for library roundtrip.
// Units: E/G in MPa; Rho in kg/m³; Fy/Cap.* in MPa. Mirrors FFrameMaterial.
USTRUCT(BlueprintType)
struct ARCHSIM_API FArchSimMaterialRecord
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float E = 0.f;       // Young's modulus (MPa)
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float G = 0.f;       // Shear modulus (MPa)
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float Nu = 0.f;      // Poisson ratio (dimensionless)
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float Rho = 0.f;     // Density (kg/m³)
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float Fy = 0.f;      // Yield strength (MPa)
    // Capacity allowables (MPa) — FFrameCapacity fields flattened here for
    // SPUD struct-nesting compatibility (SPUD supports TArray<USTRUCT> where
    // USTRUCT contains only POD/TArray/FVector — not nested USTRUCT within
    // the first level; flattening avoids any nesting-depth concern).
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float CapComp = 0.f;
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float CapTens = 0.f;
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float CapShear = 0.f;
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float CapBend = 0.f;
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float CapTors = 0.f;
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float CapVM = 0.f;
};

// Stores one section's full parameter set for library roundtrip.
// Units: A in mm²; Iy/Iz/J in mm⁴; Cy/Cz in mm; Asy/Asz in mm²; Zy/Zz in mm³.
// Mirrors FFrameSection; all section dims are in FrameCore millimetres.
USTRUCT(BlueprintType)
struct ARCHSIM_API FArchSimSectionRecord
{
    GENERATED_BODY()

    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float A = 0.f;
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float Iy = 0.f;
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float Iz = 0.f;
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float J = 0.f;
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float Cy = 0.f;
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float Cz = 0.f;
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float Asy = 0.f;
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float Asz = 0.f;
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float Zy = 0.f;
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float Zz = 0.f;
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    uint8 Shape = 0;  // EFrameSectionShape serialised as uint8 (0=Rectangular, 1=Circular)
};

// Stores one nodal load record. Node index is into the rebuilt node array
// (position-matched during replay); DOF components in N and N·mm (FrameCore SI).
// WHY store by position, not index: node indices are re-assigned during replay;
// matching by position ensures loads land on the correct structural node.
// Units: Comp[0..2] = Fx,Fy,Fz (N); Comp[3..5] = Mx,My,Mz (N·mm).
USTRUCT(BlueprintType)
struct ARCHSIM_API FArchSimNodalLoadRecord
{
    GENERATED_BODY()

    // Node position in FrameCore mm for matching during replay.
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    FVector NodePosMm = FVector::ZeroVector;

    // 6-DOF load components [Fx,Fy,Fz (N), Mx,My,Mz (N·mm)]. Length always 6.
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    TArray<float> Comp;
};

// Stores one member UDL record. WLocal in N/mm (force per mm, member-local axes).
// Member index is relative to the replay-built model (same replay-order as
// MemberRecords[] — UDL index i refers to the i-th replayed member).
USTRUCT(BlueprintType)
struct ARCHSIM_API FArchSimUDLRecord
{
    GENERATED_BODY()

    // 0-based index into the replayed member list (matches MemberRecords[] order).
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    int32 MemberRecordIdx = 0;

    // Distributed load vector in member-local axes, N/mm.
    // Mirrors FFrameMemberUDL::WLocal (FrameCore SI convention).
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    FVector WLocal = FVector::ZeroVector;
};

// Stores one shell element's geometry and material data for replay.
// Node positions in FrameCore mm (4 corners, CCW about +normal).
USTRUCT(BlueprintType)
struct ARCHSIM_API FArchSimShellRecord
{
    GENERATED_BODY()

    // 4 corner node positions in FrameCore mm (CCW about +normal).
    // WHY positions not indices: node indices are re-assigned during replay.
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    TArray<FVector> CornerPosMm;

    // Material library index (index into the replayed MaterialLibrary).
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    int32 MatIdx = -1;

    // Shell thickness in mm.
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float T = 0.f;     // shell thickness (mm)

    // Shell active flag (mirrors FFrameShellQuad::bActive).
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    bool bActive = true;

    // Shell pressure for this element (N/mm²), 0 if no pressure applied.
    // Stored alongside the shell geometry rather than separately to avoid
    // index-alignment fragility across separate arrays.
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    float Pressure = 0.f;  // N/mm² along facet +local-z
};

// Stores per-node fixity and prescribed displacement for replay.
// Covers both all-fixed supports AND partial fixity nodes (v2 generalises v1's
// SupportPositions which only stored fully-fixed nodes).
// Units: Prescribed in mm (translation DOFs) or radians (rotation DOFs).
USTRUCT(BlueprintType)
struct ARCHSIM_API FArchSimNodeFixityRecord
{
    GENERATED_BODY()

    // Node position in FrameCore mm (used to re-locate the node during replay).
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    FVector NodePosMm = FVector::ZeroVector;

    // 6-bool fixity [Ux,Uy,Uz,Rx,Ry,Rz]. True = fixed DOF.
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    TArray<bool> Fixed;

    // 6-float prescribed displacement [Ux,Uy,Uz mm; Rx,Ry,Rz rad]. Honoured at fixed DOFs.
    UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim|Persistence")
    TArray<float> Prescribed;
};


UCLASS()
class ARCHSIM_API UArchSimPersistenceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // UGameInstanceSubsystem
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ---- Save surface (BP-callable) -----------------------------------------

    // Persist the current model state to the named slot.
    // Slot name "ArchSimSlot_1" is the default MVP slot. Pass any non-empty name
    // to use an alternate slot.
    //
    // Returns false + logs (and does NOT call SPUD SaveGame) in any of these cases:
    //   (a) SPUD is not in RunningIdle state.
    //   (b) Partial snapshot: RegisteredCount > snapshot active-member count (some
    //       component was not found in the world — saving would corrupt the slot with
    //       incomplete data). Fix the world/registry inconsistency and retry.
    //       NOTE v2: deactivated members ARE included in MemberRecords (with
    //       bActive=false) but do NOT have actor components; the partial-snapshot
    //       guard now compares only active-member capture count against active-member
    //       registered count (IndexToComponent entries whose bRegistered==true).
    //   (c) Empty overwrite guard: sidecar has 0 members + 0 supports AND the target
    //       slot already exists AND bAllowEmptyOverwrite==false (default). Pass
    //       bAllowEmptyOverwrite=true to overwrite an existing slot with an empty save
    //       (e.g. "clear save" feature). Overwriting a non-existent slot with empty
    //       data is always permitted.
    //
    // On success returns true immediately (SaveGame is async; subscribe to SPUD's
    // PostSaveGame delegate for completion notification).
    UFUNCTION(BlueprintCallable, Category="ArchSim|Persistence")
    bool SaveToSlot(const FString& SlotName = TEXT("ArchSimSlot_1"),
                    bool bAllowEmptyOverwrite = false);

    // ---- Load surface (BP-callable) -----------------------------------------

    // Restore the model from the named slot.
    //
    // Returns false + logs in any of these cases:
    //   (a) SPUD is not in RunningIdle state.
    //   (b) The named slot does not exist on disk (checked via GetSaveGameInfo before
    //       calling LoadGame, fulfilling the documented contract below).
    //
    // Returns true when the load request was successfully issued AND the slot exists.
    // "true" does NOT mean persistence is complete — LoadGame is async. The map
    // reloads, SPUD restores global objects, and ReplayLoadedSidecar fires when
    // PostLoadGame fires (SpudSubsystem.cpp:981). Subscribe to SPUD's PostLoadGame
    // delegate (or observe OnSolveComplete on UArchSimModelRegistry) for completion.
    UFUNCTION(BlueprintCallable, Category="ArchSim|Persistence")
    bool LoadFromSlot(const FString& SlotName = TEXT("ArchSimSlot_1"));

    // ---- Snapshot API (called by widget / gameplay code before Save) ---------

    // Capture the current world's member placements + support positions into
    // the SaveGame arrays. Must be called before SaveToSlot so the sidecar is
    // up-to-date. In production, SaveToSlot calls this internally.
    UFUNCTION(BlueprintCallable, Category="ArchSim|Persistence")
    void SnapshotCurrentModel();

    // ---- Reset API (called before replay on load) ----------------------------

    // Clear the Registry's CurrentModel, IndexToComponent, and all session state.
    // Call before replaying a loaded sidecar to avoid stale model data.
    // WHY: Registry has no public Reset; we add one in ArchSimModelRegistry (see
    // ArchSimModelRegistry.h/cpp addendum). This is the minimal production change.
    UFUNCTION(BlueprintCallable, Category="ArchSim|Persistence")
    void ResetRegistry();

    // ---- read-only accessors (tests) ----------------------------------------
    [[nodiscard]] int32 GetMemberRecordCount() const { return MemberRecords.Num(); }
    // NOTE (v0.6.1): v1-only accessor. Format-v2 snapshots leave SupportPositions
    // empty (all fixity lives in NodeFixities) so this returns 0 on any v2 sidecar.
    // Kept for v1-archive introspection; new callers should use GetNodeFixityCount().
    [[nodiscard]] int32 GetSupportCount() const { return SupportPositions.Num(); }
    [[nodiscard]] int32 GetFormatVersion() const { return SidecarFormatVersion; }
    [[nodiscard]] int32 GetMaterialLibraryCount() const { return MaterialLibrary.Num(); }
    [[nodiscard]] int32 GetSectionLibraryCount() const { return SectionLibrary.Num(); }
    [[nodiscard]] int32 GetNodalLoadCount() const { return NodalLoads.Num(); }
    [[nodiscard]] int32 GetUDLCount() const { return MemberUDLs.Num(); }
    [[nodiscard]] int32 GetShellCount() const { return Shells.Num(); }
    [[nodiscard]] int32 GetNodeFixityCount() const { return NodeFixities.Num(); }

private:
    // ============================================================
    // UPROPERTY(SaveGame) sidecar arrays — SPUD global-object path.
    // ORDERING: all v1 fields first (MUST NOT reorder), v2 appended at end.
    // SPUD scans CPF_SaveGame properties (SpudPropertyUtil.cpp:24-31).
    // ============================================================

    // ---- v1 fields (ordering FROZEN for backward compat) --------------------

    // One record per placed structural member (active members, in registration order).
    // v2: includes deactivated members too (with bActive=false in MemberActiveFlags).
    UPROPERTY(SaveGame)
    TArray<FArchSimMemberRecord> MemberRecords;

    // Fixed support node positions in FrameCore mm.
    // WHY stored in mm (not UE cm): RegisterFixedSupport takes mm; avoids a
    // double-conversion round-trip that could accumulate floating-point drift
    // across save-load cycles.
    // v2: superseded by NodeFixities (which covers all-fixed and partial-fixity nodes).
    //     Kept for v1 compat — v1 archives still populate this; v2 snapshots populate
    //     NodeFixities instead and leave SupportPositions empty. Replay checks version:
    //     v1 → use SupportPositions; v2 → use NodeFixities.
    UPROPERTY(SaveGame)
    TArray<FVector> SupportPositions;

    // ---- v2 fields (appended at end; absent in v1 archives → SPUD default) ---

    // Format version tag. Default 0 = no v2 fields (v1 compat).
    // Set to 2 by SnapshotCurrentModel() when writing a v2 sidecar.
    // v1 archives will leave this at 0 (SPUD skip → int32 default = 0) → replay
    // branch treats 0 as v1 semantics.
    UPROPERTY(SaveGame)
    int32 SidecarFormatVersion = 0;

    // Full material library as saved. Empty = v1 archive; replay calls
    // EnsureDefaultLibraries() (restores default S275 + 200×200 rect section).
    // Each entry is the full FFrameMaterial parameter set (not just an index).
    UPROPERTY(SaveGame)
    TArray<FArchSimMaterialRecord> MaterialLibrary;

    // Full section library as saved. Parallel to MaterialLibrary (same policy).
    UPROPERTY(SaveGame)
    TArray<FArchSimSectionRecord> SectionLibrary;

    // Active/deactivated flag per member record (parallel to MemberRecords[]).
    // True = active (placed, structural); false = deactivated (DeactivateMember called).
    // Deactivated members are included in MemberRecords so their actor geometry is
    // preserved (enables future reactivation). The partial-snapshot guard is adjusted
    // to count only active-flagged records against IndexToComponent.
    UPROPERTY(SaveGame)
    TArray<bool> MemberActiveFlags;

    // Nodal loads: one entry per FFrameNodalLoad in CurrentModel at snapshot time.
    // Position-keyed to survive node index reassignment during replay.
    UPROPERTY(SaveGame)
    TArray<FArchSimNodalLoadRecord> NodalLoads;

    // Member UDLs: one entry per FFrameMemberUDL in CurrentModel at snapshot time.
    // Index stored as MemberRecordIdx (position in MemberRecords[]).
    UPROPERTY(SaveGame)
    TArray<FArchSimUDLRecord> MemberUDLs;

    // Shell elements (geometry + mat + thickness + pressure + active flag).
    UPROPERTY(SaveGame)
    TArray<FArchSimShellRecord> Shells;

    // Per-node general fixity records (generalises v1 SupportPositions).
    // Stores only nodes with at least one fixed DOF (free-only nodes not stored).
    UPROPERTY(SaveGame)
    TArray<FArchSimNodeFixityRecord> NodeFixities;

    // ============================================================
    // Internal helpers
    // ============================================================

    // Bound to USpudSubsystem::PostLoadGame. Fires after the full load is complete
    // (SpudSubsystem.cpp:981 LoadComplete). Triggers replay on the loaded level.
    UFUNCTION()
    void OnPostLoadGame(const FString& SlotName, bool bSuccess);

    // Replay MemberRecords + supports/fixities + v2 model state into the live Registry.
    // WHY deferred to a separate method: called from OnPostLoadGame (delegate)
    // so the level actors are fully available (BeginPlay has run).
    void ReplayLoadedSidecar(UWorld* World);

    // ---- v2 snapshot helpers ------------------------------------------------

    // Snapshot materials library from CurrentModel into MaterialLibrary[].
    void SnapshotMaterials(const TArray<FFrameMaterial>& Materials);
    // Snapshot sections library from CurrentModel into SectionLibrary[].
    void SnapshotSections(const TArray<FFrameSection>& Sections);
    // Snapshot nodal loads from CurrentModel into NodalLoads[].
    void SnapshotNodalLoads(const TArray<FFrameNodalLoad>& Loads,
                            const TArray<FFrameNode>& Nodes);
    // Snapshot member UDLs from CurrentModel into MemberUDLs[].
    void SnapshotUDLs(const TArray<FFrameMemberUDL>& UDLs);
    // Snapshot shells + pressures from CurrentModel.
    void SnapshotShells(const TArray<FFrameShellQuad>& ShellQuads,
                        const TArray<FFrameShellPressure>& ShellPressures,
                        const TArray<FFrameNode>& Nodes);
    // Snapshot per-node fixity (any node with ≥1 fixed DOF).
    void SnapshotNodeFixities(const TArray<FFrameNode>& Nodes);

    // ---- v2 replay helpers --------------------------------------------------

    // Restore material library into Registry::CurrentModel.Materials.
    // Called before members so MatIdx references are valid.
    void ReplayMaterials(class UArchSimModelRegistry* Registry) const;
    // Restore section library into Registry::CurrentModel.Sections.
    void ReplaySections(class UArchSimModelRegistry* Registry) const;
    // Replay nodal loads: match by position then inject into CurrentModel.
    void ReplayNodalLoads(class UArchSimModelRegistry* Registry) const;
    // Replay UDLs: map MemberRecordIdx to replayed member id.
    void ReplayUDLs(class UArchSimModelRegistry* Registry,
                    const TArray<int32>& RecordIdxToMemberId) const;
    // Replay shells (spawn node positions + register shell).
    void ReplayShells(class UArchSimModelRegistry* Registry) const;
    // Replay per-node fixity after members/supports are rebuilt.
    void ReplayNodeFixities(class UArchSimModelRegistry* Registry) const;
};
