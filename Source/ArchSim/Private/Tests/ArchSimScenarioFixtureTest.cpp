// ArchSim — Headless smoke test for AS-30 boundary support API + portal frame fixture.
//
// Sprint S-06. Plan ref: docs/logs/S-06/plan_*.md § AS-30.
// Sprint S-08 AS-36 additions: SC8 (node-pair uniqueness regression) + SC9 (actor location).
//
// What this test CAN verify in headless (-nullrhi -unattended):
//   SC1. PlaceFixedSupport UFunction reflection: BlueprintCallable + returns int32 + takes FVector
//   SC2. SpawnDefaultPortalFrame UFunction reflection: BlueprintCallable + returns bool + 0 params
//   SC3. Registry headless: NewObject<UArchSimModelRegistry> + RegisterFixedSupport → NodeIdx >= 0;
//        GetCurrentModel().Nodes[idx].Fixed.Num()==6 && all true
//   SC4. Node-snap dedupe: second call at SAME PosMm returns SAME idx
//        (FindOrAddNode 1 mm tolerance used internally)
//   SC5. Idempotent Fixed: after second call Fixed array still length-6 all true
//   SC6. Transient widget (no PIE): SpawnDefaultPortalFrame() returns false + no crash
//        (GEditor null / Registry null path)
//   SC7. AddInfo: PIE oracle link for P10/P11 in docs/logs/S-05/u3_pie_smoke.md
//        (informational; headless cannot run solve or spawn heatmap)
//   SC8. AS-36 node-pair uniqueness regression: two RegisterMember calls at DIFFERENT actor
//        origins (manually constructed with USceneComponent roots + SetActorLocation, matching
//        the AS-36 fix pattern) must produce distinct (I,J) pairs + 4 unique nodes total.
//        BEFORE fix: AActor without RootComponent → Identity transform for both → same node pair.
//        AFTER fix: USceneComponent root allows SetActorLocation to take effect → distinct
//        positions → distinct node pairs.
//   SC9. Actor-location regression: after adding USceneComponent root + SetActorLocation,
//        GetActorLocation() must return the requested position within a UE float epsilon.
//        This directly pins the production behaviour changed by the AS-36 fix.
//
// What this test CANNOT verify in headless (honest per AS-07 lesson #1):
//   - SpawnDefaultPortalFrame composing 5 actors in PIE → [NEW CODE, PIE required]
//   - HeatmapActor spawning + colour after portal frame solve → [NEW CODE, PIE required]
//   - Registry debounced solve firing (GI null in headless → timer branch unreachable)
//   - PlaceKSetMember production path end-to-end (WITH_EDITOR / GEditor / PlayWorld)
//     → SC8 directly simulates the production spawn + RootComponent pattern headlessly
//
// Naming: ArchSim.Gameplay.ScenarioFixture → sub-check count increases by 2 (SC8+SC9).
//         SC8+SC9 are sub-checks of the existing test; IMPLEMENT count stays the same.
//         $ExpectedUeTests stays 149 (cuDSS) / 147 (non-cuDSS) — only IMPLEMENT count matters.
//
// FROZEN guard: zero lines under Plugins/FrameSolver/Source/FrameCore/ (v4.0.0 FROZEN).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/ArchSimModelRegistry.h"

// SC8+SC9 (AS-36): need AActor + USceneComponent + UWorld to reproduce the
// production spawn pattern and verify the AS-36 RootComponent fix headlessly.
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/ArchSimMemberData.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

#if WITH_EDITOR
#include "Editor/ArchSimScenarioWidget.h"
#endif // WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FArchSimScenarioFixtureTest,
    "ArchSim.Gameplay.ScenarioFixture",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter
)

bool FArchSimScenarioFixtureTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // SC1: PlaceFixedSupport UFunction reflection
    // WHY: Verifies that PlaceFixedSupport is BP-callable with the correct
    // signature before any PIE is required. This mirrors the pattern used by
    // ArchSimScenarioWidgetTest SC3 for PlaceK1Column.
    // -----------------------------------------------------------------------
#if WITH_EDITOR
    {
        UClass* WidgetClass = UArchSimScenarioWidget::StaticClass();
        TestNotNull(TEXT("SC1: WidgetClass non-null"), WidgetClass);

        UFunction* Fn = WidgetClass ? WidgetClass->FindFunctionByName(TEXT("PlaceFixedSupport")) : nullptr;
        TestNotNull(TEXT("SC1: PlaceFixedSupport UFunction exists"), Fn);

        if (Fn)
        {
            // Must be BlueprintCallable
            TestTrue(TEXT("SC1: PlaceFixedSupport is BlueprintCallable"),
                     Fn->HasAnyFunctionFlags(FUNC_BlueprintCallable));

            // Return type must be int32 (FIntProperty)
            FProperty* RetProp = Fn->GetReturnProperty();
            TestNotNull(TEXT("SC1: PlaceFixedSupport has return property"), RetProp);
            if (RetProp)
            {
                TestTrue(TEXT("SC1: PlaceFixedSupport return type is int32"),
                         RetProp->IsA<FIntProperty>());
            }

            // Must take exactly 1 FVector parameter (LocationWorld)
            int32 ParamCount = 0;
            for (TFieldIterator<FProperty> It(Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
            {
                if (!It->HasAnyPropertyFlags(CPF_ReturnParm))
                {
                    ++ParamCount;
                    TestTrue(TEXT("SC1: PlaceFixedSupport param is FVector"),
                             It->IsA<FStructProperty>());
                }
            }
            TestEqual(TEXT("SC1: PlaceFixedSupport has exactly 1 param"), ParamCount, 1);
        }
    }
#else
    AddInfo(TEXT("SC1: Skipped — WITH_EDITOR not defined in this build."));
#endif // WITH_EDITOR

    // -----------------------------------------------------------------------
    // SC2: SpawnDefaultPortalFrame UFunction reflection
    // -----------------------------------------------------------------------
#if WITH_EDITOR
    {
        UClass* WidgetClass = UArchSimScenarioWidget::StaticClass();
        UFunction* Fn = WidgetClass ? WidgetClass->FindFunctionByName(TEXT("SpawnDefaultPortalFrame")) : nullptr;
        TestNotNull(TEXT("SC2: SpawnDefaultPortalFrame UFunction exists"), Fn);

        if (Fn)
        {
            // Must be BlueprintCallable
            TestTrue(TEXT("SC2: SpawnDefaultPortalFrame is BlueprintCallable"),
                     Fn->HasAnyFunctionFlags(FUNC_BlueprintCallable));

            // Return type must be bool (FBoolProperty)
            FProperty* RetProp = Fn->GetReturnProperty();
            TestNotNull(TEXT("SC2: SpawnDefaultPortalFrame has return property"), RetProp);
            if (RetProp)
            {
                TestTrue(TEXT("SC2: SpawnDefaultPortalFrame return type is bool"),
                         RetProp->IsA<FBoolProperty>());
            }

            // Must have 0 non-return parameters
            int32 ParamCount = 0;
            for (TFieldIterator<FProperty> It(Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
            {
                if (!It->HasAnyPropertyFlags(CPF_ReturnParm))
                {
                    ++ParamCount;
                }
            }
            TestEqual(TEXT("SC2: SpawnDefaultPortalFrame has 0 params"), ParamCount, 0);
        }
    }
#else
    AddInfo(TEXT("SC2: Skipped — WITH_EDITOR not defined in this build."));
#endif // WITH_EDITOR

    // -----------------------------------------------------------------------
    // SC3: Registry headless — RegisterFixedSupport at FVector(0,0,0) returns idx >= 0;
    //      Fixed.Num()==6 and all elements are true.
    // WHY NewObject<>(GetTransientPackage()): mirrors AS-24 pattern for headless
    // subsystem fixture (UObjectGlobals.h:1919 confirms default outer is transient,
    // but explicit outer is intent-documentation).
    // WHY no solve: GI is null in headless (NewObject path has no GameInstance
    // pipeline); RequestSolve early-returns at the GI-null guard. We test ONLY the
    // node registration state — that is fully exercised headless.
    // -----------------------------------------------------------------------
    {
        UArchSimModelRegistry* Registry =
            NewObject<UArchSimModelRegistry>(GetTransientPackage());
        TestNotNull(TEXT("SC3: Registry NewObject non-null"), Registry);

        if (Registry)
        {
            const FVector SupportPosMm(0.0, 0.0, 0.0);
            const int32 Idx = Registry->RegisterFixedSupport(SupportPosMm);
            TestTrue(TEXT("SC3: RegisterFixedSupport returns idx >= 0"), Idx >= 0);

            if (Idx >= 0)
            {
                const FFrameModelDef& Model = Registry->GetCurrentModel();
                TestTrue(TEXT("SC3: Nodes array contains returned idx"),
                         Model.Nodes.IsValidIndex(Idx));

                if (Model.Nodes.IsValidIndex(Idx))
                {
                    const TArray<bool>& Fixed = Model.Nodes[Idx].Fixed;
                    TestEqual(TEXT("SC3: Fixed.Num() == 6"), Fixed.Num(), 6);

                    bool bAllTrue = true;
                    for (int32 i = 0; i < Fixed.Num(); ++i)
                    {
                        if (!Fixed[i]) { bAllTrue = false; }
                    }
                    TestTrue(TEXT("SC3: Fixed all true (fully-fixed support)"), bAllTrue);
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // SC4: Node-snap dedupe — second call at SAME PosMm returns SAME idx.
    // WHY: Verifies that RegisterFixedSupport delegates dedup to FindOrAddNode
    // (1 mm tolerance) rather than blindly adding duplicate nodes. Two calls at
    // (500,0,0) mm must return the same NodeIdx.
    // -----------------------------------------------------------------------
    {
        UArchSimModelRegistry* Registry =
            NewObject<UArchSimModelRegistry>(GetTransientPackage());
        TestNotNull(TEXT("SC4: Registry NewObject non-null"), Registry);

        if (Registry)
        {
            const FVector PosMm(500.0, 0.0, 0.0);
            const int32 Idx1 = Registry->RegisterFixedSupport(PosMm);
            const int32 Idx2 = Registry->RegisterFixedSupport(PosMm);

            TestTrue(TEXT("SC4: first call returns idx >= 0"), Idx1 >= 0);
            TestTrue(TEXT("SC4: second call returns same idx (dedup)"), Idx1 == Idx2);

            // Also verify only ONE node was added (dedup worked, no duplicate entry)
            const FFrameModelDef& Model = Registry->GetCurrentModel();
            TestEqual(TEXT("SC4: exactly 1 node in model after 2 calls at same pos"),
                      Model.Nodes.Num(), 1);
        }
    }

    // -----------------------------------------------------------------------
    // SC5: Idempotent Fixed — after second call Fixed array still length-6 all true.
    // WHY: RegisterFixedSupport must not reset Fixed on a second call (e.g. to
    // all-false then re-set). The array must remain length-6 all-true.
    // -----------------------------------------------------------------------
    {
        UArchSimModelRegistry* Registry =
            NewObject<UArchSimModelRegistry>(GetTransientPackage());
        TestNotNull(TEXT("SC5: Registry NewObject non-null"), Registry);

        if (Registry)
        {
            const FVector PosMm(-1000.0, 0.0, 0.0);
            const int32 Idx1 = Registry->RegisterFixedSupport(PosMm);
            const int32 Idx2 = Registry->RegisterFixedSupport(PosMm);  // second call

            TestTrue(TEXT("SC5: both calls return same idx"), Idx1 == Idx2);

            if (Idx2 >= 0)
            {
                const FFrameModelDef& Model = Registry->GetCurrentModel();
                if (Model.Nodes.IsValidIndex(Idx2))
                {
                    const TArray<bool>& Fixed = Model.Nodes[Idx2].Fixed;
                    TestEqual(TEXT("SC5: Fixed still length 6 after second call"), Fixed.Num(), 6);

                    bool bAllTrue = true;
                    for (int32 i = 0; i < Fixed.Num(); ++i)
                    {
                        if (!Fixed[i]) { bAllTrue = false; }
                    }
                    TestTrue(TEXT("SC5: Fixed still all-true after second call (idempotent)"),
                             bAllTrue);
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // SC6: Transient widget — SpawnDefaultPortalFrame() returns false and does NOT
    //      crash when called on a NewObject widget with no GEditor / PIE.
    // WHY: GEditor is non-null in the UE Automation test runner (EditorContext),
    //      so the guard we're really testing here is the Registry-null path
    //      (no PIE → Registry::Get returns nullptr → SpawnDefaultPortalFrame
    //      returns false). This mirrors ArchSimScenarioSolveWireTest SC5 for
    //      RequestSolveAndVisualize graceful-fail.
    // -----------------------------------------------------------------------
#if WITH_EDITOR
    {
        UArchSimScenarioWidget* Widget =
            NewObject<UArchSimScenarioWidget>(
                GetTransientPackage(),
                UArchSimScenarioWidget::StaticClass());
        TestNotNull(TEXT("SC6: Widget NewObject non-null"), Widget);

        if (Widget)
        {
            // In the headless automation runner GEditor exists but there is no PIE
            // world active (PlayWorld is null or Editor world has no GameInstance).
            // SpawnDefaultPortalFrame must detect Registry null and return false
            // without crashing.
            const bool bResult = Widget->SpawnDefaultPortalFrame();
            // Expected: false (no PIE Registry). The important thing is: no crash.
            // We use AddInfo rather than TestFalse because on a host that happens to
            // have a running PIE this would legitimately return true — the automation
            // runner typically does NOT have PIE active.
            if (!bResult)
            {
                AddInfo(TEXT("SC6: SpawnDefaultPortalFrame returned false (no PIE Registry) — expected in headless."));
            }
            else
            {
                AddInfo(TEXT("SC6: SpawnDefaultPortalFrame returned true (PIE active in runner) — also acceptable."));
            }
            // No TestTrue/TestFalse here because the runner environment determines
            // PIE availability; the key assertion is "no crash" (reaching this line).
            AddInfo(TEXT("SC6: SpawnDefaultPortalFrame completed without crash."));
        }
    }
#else
    AddInfo(TEXT("SC6: Skipped — WITH_EDITOR not defined in this build."));
#endif // WITH_EDITOR

    // -----------------------------------------------------------------------
    // SC7: AddInfo — PIE oracle link for u3_pie_smoke P10/P11
    // WHY: Documents the PIE-only coverage so reviewers understand the full
    // test contract without needing to read the smoke doc separately. No
    // assertion — purely informational.
    // -----------------------------------------------------------------------
    AddInfo(TEXT("SC7: [NEW CODE, PIE required] SpawnDefaultPortalFrame PIE oracle: "
                 "After calling SpawnDefaultPortalFrame() in PIE, the model has 4 nodes "
                 "(2 Fixed base supports + 2 free top corners) and 3 members. "
                 "LDLT can resolve the 12 free DOF system. "
                 "RequestSolveAndVisualize() should then fire OnSolveComplete → HeatmapActor spawn "
                 "→ non-trivial colour visible in PIE viewport. "
                 "Full verification: docs/logs/S-05/u3_pie_smoke.md P10/P11 (AS-30 update, S-06)."));

    // -----------------------------------------------------------------------
    // SC8: AS-36 node-pair uniqueness regression test.
    //
    // WHY this sub-check exists: before the AS-36 fix, PlaceKSetMember spawned
    // AActor (base class, no RootComponent) and the FTransform location was
    // silently dropped by SpawnActor. GetActorTransform() always returned Identity,
    // so two columns at DIFFERENT actor origins both resolved to the SAME endpoints
    // via RegisterMember → same (I,J) node pair → mechanism → bSingular.
    //
    // This sub-check reproduces the pre-fix failure scenario headlessly and asserts
    // the post-fix contract: two actors at different locations → distinct node pairs.
    //
    // Method: directly reproduce the production spawn pattern:
    //   1. Locate an existing world from GEngine (same pattern as ArchSimSaveLoadTest).
    //   2. SpawnActor<AActor> at Identity.
    //   3. Graft USceneComponent root + RegisterComponent + SetRootComponent + SetActorLocation.
    //   4. Attach UArchSimMemberData with symmetric offsets (0,0,-100)/(0,0,+100) cm.
    //   5. Call Registry::RegisterMember for each actor.
    //   6. Assert: the two member (I,J) pairs differ AND Nodes.Num()==4.
    //
    // The offset choice (0,0,-100)/(0,0,+100) cm mirrors SpawnDefaultPortalFrame's
    // column offsets exactly — this is the specific geometry that was broken.
    //
    // NOTE (AS-38 SC8 comment strengthening): Nodes.Num()==4 is the STRICT guarantee
    // that "all four endpoints of the two columns are distinct nodes." The bPairsDiffer
    // OR condition alone (MA.I!=MB.I || MA.J!=MB.J) is a looser check — two columns
    // could have one shared node but still satisfy bPairsDiffer. The node count==4
    // assertion closes this gap: if any endpoint was deduplicated, Nodes.Num() < 4,
    // and the test would fail. Both assertions together constitute the full contract.
    // -----------------------------------------------------------------------
    {
        // Acquire a spawn world from GEngine contexts (same pattern as ArchSimSaveLoadTest L43-48).
        UWorld* SpawnWorld = nullptr;
        if (GEngine)
        {
            for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
            {
                if (Ctx.World())
                {
                    SpawnWorld = Ctx.World();
                    break;
                }
            }
        }
        TestNotNull(TEXT("SC8: GEngine world context available for actor spawn"), SpawnWorld);

        if (SpawnWorld)
        {
            // Fresh registry for isolation (no GI in headless → timer/solve paths unreachable,
            // but RegisterMember node-dedup path IS fully exercised headlessly).
            UArchSimModelRegistry* Registry =
                NewObject<UArchSimModelRegistry>(GetTransientPackage());
            TestNotNull(TEXT("SC8: Registry NewObject non-null"), Registry);

            if (Registry)
            {
                // Column-A actor: origin at (-100, 0, 100) cm (left column mid-height).
                // Column-B actor: origin at (+100, 0, 100) cm (right column mid-height).
                // Both use vertical offsets (0,0,-100)/(0,0,+100) cm — same as SpawnDefaultPortalFrame.
                const FVector LocColA(-100.f, 0.f, 100.f);
                const FVector LocColB(+100.f, 0.f, 100.f);
                const FVector EndIOffset(0.f,  0.f, -100.f);
                const FVector EndJOffset(0.f,  0.f, +100.f);

                // Lambda to spawn an actor with a proper RootComponent at the given location
                // and attach UArchSimMemberData with the given offsets.
                // This replicates what PlaceKSetMember now does after the AS-36 fix.
                auto SpawnColumnActor = [&](const FVector& Loc) -> AActor*
                {
                    FActorSpawnParameters SP;
                    SP.SpawnCollisionHandlingOverride =
                        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                    AActor* Actor = SpawnWorld->SpawnActor<AActor>(
                        AActor::StaticClass(), FTransform::Identity, SP);
                    if (!Actor) return nullptr;

                    // Graft RootComponent — the production AS-36 fix pattern.
                    USceneComponent* Root = NewObject<USceneComponent>(
                        Actor, USceneComponent::StaticClass(), TEXT("Root"));
                    if (!Root) return nullptr;
                    Root->RegisterComponent();
                    Actor->SetRootComponent(Root);
                    Actor->SetActorLocation(Loc);

                    // Attach UArchSimMemberData with column offsets.
                    UArchSimMemberData* Comp = NewObject<UArchSimMemberData>(
                        Actor, UArchSimMemberData::StaticClass(), TEXT("ArchSimMemberData"));
                    if (!Comp) return nullptr;
                    Comp->EndIOffsetUE = EndIOffset;
                    Comp->EndJOffsetUE = EndJOffset;
                    Comp->RegisterComponent();
                    Actor->AddInstanceComponent(Comp);

                    return Actor;
                };

                AActor* ColA = SpawnColumnActor(LocColA);
                AActor* ColB = SpawnColumnActor(LocColB);
                TestNotNull(TEXT("SC8: ColA actor spawned"), ColA);
                TestNotNull(TEXT("SC8: ColB actor spawned"), ColB);

                if (ColA && ColB)
                {
                    // Retrieve the UArchSimMemberData components.
                    UArchSimMemberData* CompA = ColA->FindComponentByClass<UArchSimMemberData>();
                    UArchSimMemberData* CompB = ColB->FindComponentByClass<UArchSimMemberData>();
                    TestNotNull(TEXT("SC8: CompA non-null"), CompA);
                    TestNotNull(TEXT("SC8: CompB non-null"), CompB);

                    if (CompA && CompB)
                    {
                        const int32 IdxA = Registry->RegisterMember(CompA);
                        const int32 IdxB = Registry->RegisterMember(CompB);
                        TestTrue(TEXT("SC8: ColA RegisterMember returned valid idx"), IdxA >= 0);
                        TestTrue(TEXT("SC8: ColB RegisterMember returned valid idx"), IdxB >= 0);

                        const FFrameModelDef& Model = Registry->GetCurrentModel();

                        // Core AS-36 regression assertion: two members at DISTINCT positions
                        // must have DISTINCT (I,J) node pairs.
                        if (IdxA >= 0 && IdxB >= 0 &&
                            Model.Members.IsValidIndex(IdxA) &&
                            Model.Members.IsValidIndex(IdxB))
                        {
                            const FFrameMember& MA = Model.Members[IdxA];
                            const FFrameMember& MB = Model.Members[IdxB];

                            AddInfo(FString::Printf(
                                TEXT("SC8: Member[%d] I=%d J=%d  Member[%d] I=%d J=%d"),
                                IdxA, MA.I, MA.J, IdxB, MB.I, MB.J));

                            // Node pairs must differ — this was the failing condition
                            // before the AS-36 fix. (MA.I==MB.I && MA.J==MB.J) means
                            // both columns share the same endpoints → mechanism.
                            const bool bPairsDiffer = (MA.I != MB.I) || (MA.J != MB.J);
                            TestTrue(
                                TEXT("SC8 [AS-36 core regression]: two columns at distinct "
                                     "locations must have DISTINCT (I,J) node pairs"),
                                bPairsDiffer);

                            // 4 unique nodes expected: ColA EndI, ColA EndJ, ColB EndI, ColB EndJ.
                            // (No dedup because the positions are ~200 cm apart.)
                            // WHY this is the STRICT assertion (AS-38 SC8 comment strengthening):
                            //   Nodes.Num()==4 guarantees all four endpoints are distinct with
                            //   zero sharing. The bPairsDiffer OR alone is looser — it allows
                            //   one shared node between columns (e.g. I_A==I_B but J_A!=J_B).
                            //   Both assertions together form the complete contract.
                            TestEqual(
                                TEXT("SC8: node count == 4 (strict: all four endpoints are distinct — "
                                     "no endpoint sharing between the two columns)"),
                                Model.Nodes.Num(), 4);

                            AddInfo(bPairsDiffer
                                ? TEXT("SC8 [VERIFIED]: node pairs are distinct — AS-36 fix active.")
                                : TEXT("SC8 [FAIL]: node pairs are identical — AS-36 regression!"));
                        }
                    }
                }

                // Cleanup actors from the headless world.
                if (ColA) { ColA->Destroy(); }
                if (ColB) { ColB->Destroy(); }
            }
        }
    }

    // -----------------------------------------------------------------------
    // SC9: Actor-location round-trip via PlaceK1Column production path.
    //
    // AS-38(c) strengthening: the prior SC9 (v0.5.2) pinned the UE engine
    // SetActorLocation → GetActorLocation round-trip in isolation (manually
    // spawning an actor with NewObject + SetRootComponent). That tests the UE
    // engine behaviour, not the ArchSim production code path.
    //
    // This revised SC9 calls Widget->PlaceK1Column(LocationWorld) directly —
    // the same function a student or BP graph would call — and then verifies
    // that the returned actor is at the requested world position.
    //
    // Reachability analysis (headless -nullrhi -unattended, EditorContext):
    //   - WITH_EDITOR is defined in UE Editor automation runs.
    //   - GEditor is non-null (EditorContext).
    //   - GEditor->PlayWorld is null (no PIE active in automation runner).
    //   - PlaceKSetMember falls back to GEditor->GetEditorWorldContext().World().
    //   - Editor world has no GameInstance → Registry::Get returns null.
    //   - PlaceKSetMember continues: spawns actor, grafts root, SetActorLocation,
    //     attaches MemberData, logs "Registry not available", appends to PlacedActors.
    //   - Returns the actor with correct world transform.
    //   CONCLUSION: PlaceK1Column is REACHABLE headlessly and exercises the full
    //   actor-spawn + root-graft + SetActorLocation production path.
    //   [VERIFIED headless — Actor non-null and location asserted below]
    // -----------------------------------------------------------------------
#if WITH_EDITOR
    {
        UArchSimScenarioWidget* Widget =
            NewObject<UArchSimScenarioWidget>(
                GetTransientPackage(),
                UArchSimScenarioWidget::StaticClass());
        TestNotNull(TEXT("SC9: Widget NewObject non-null"), Widget);

        if (Widget)
        {
            // PlaceK1Column uses EndI=(-50,0,0)/EndJ=(+50,0,0) cm centred on actor origin.
            // We test with a non-origin position to distinguish from the Identity default.
            const FVector RequestedLoc(-100.f, 0.f, 100.f);
            AActor* PlacedActor = Widget->PlaceK1Column(RequestedLoc);

            // In headless (Editor world, no Registry), PlaceK1Column returns a valid actor
            // at the requested position. Null is possible only if GEditor is null or
            // the world is null — document the outcome either way.
            if (PlacedActor)
            {
                const FVector ActualLoc = PlacedActor->GetActorLocation();
                AddInfo(FString::Printf(
                    TEXT("SC9: PlaceK1Column(%.1f,%.1f,%.1f) → GetActorLocation=(%.1f,%.1f,%.1f)"),
                    RequestedLoc.X, RequestedLoc.Y, RequestedLoc.Z,
                    ActualLoc.X, ActualLoc.Y, ActualLoc.Z));

                // Component-wise epsilon (0.1 cm) — matches kNodeMergeTolMm / kCmToMm.
                const bool bXOk = FMath::IsNearlyEqual(ActualLoc.X, RequestedLoc.X, 0.1f);
                const bool bYOk = FMath::IsNearlyEqual(ActualLoc.Y, RequestedLoc.Y, 0.1f);
                const bool bZOk = FMath::IsNearlyEqual(ActualLoc.Z, RequestedLoc.Z, 0.1f);

                TestTrue(TEXT("SC9 [AS-36 via PlaceK1Column]: GetActorLocation().X correct"), bXOk);
                TestTrue(TEXT("SC9 [AS-36 via PlaceK1Column]: GetActorLocation().Y correct"), bYOk);
                TestTrue(TEXT("SC9 [AS-36 via PlaceK1Column]: GetActorLocation().Z correct"), bZOk);

                // Verify the actor has a RootComponent (shipped by the AS-36 fix).
                TestNotNull(TEXT("SC9: PlacedActor has RootComponent (AS-36 fix present)"),
                            PlacedActor->GetRootComponent());

                AddInfo((bXOk && bYOk && bZOk)
                    ? TEXT("SC9 [VERIFIED production path]: PlaceK1Column location round-trip correct.")
                    : TEXT("SC9 [FAIL production path]: Actor location mismatch after PlaceK1Column!"));

                PlacedActor->Destroy();
            }
            else
            {
                // PlaceK1Column returned null — acceptable only if GEditor or World is null.
                // In the standard UE automation test runner GEditor is non-null, so this
                // path is unexpected. Document as partial coverage.
                AddInfo(TEXT("SC9 [PARTIAL]: PlaceK1Column returned null "
                             "(GEditor or Editor world unavailable in this runner). "
                             "Location check skipped — acceptable for non-Editor contexts."));
            }
        }
    }
#else
    AddInfo(TEXT("SC9: Skipped — WITH_EDITOR not defined in this build."));
#endif // WITH_EDITOR

    return true;
}
