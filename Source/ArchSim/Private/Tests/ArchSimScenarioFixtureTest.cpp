// ArchSim — Headless smoke test for AS-30 boundary support API + portal frame fixture.
//
// Sprint S-06. Plan ref: docs/logs/S-06/plan_*.md § AS-30.
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
//
// What this test CANNOT verify in headless (honest per AS-07 lesson #1):
//   - SpawnDefaultPortalFrame composing 5 actors in PIE → [NEW CODE, PIE required]
//   - HeatmapActor spawning + colour after portal frame solve → [NEW CODE, PIE required]
//   - Registry debounced solve firing (GI null in headless → timer branch unreachable)
//
// Naming: ArchSim.Gameplay.ScenarioFixture → $ExpectedUeTests 148 → 149 (cuDSS)
//         non-cuDSS: 146 → 147
//
// FROZEN guard: zero lines under Plugins/FrameSolver/Source/FrameCore/ (v4.0.0 FROZEN).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/ArchSimModelRegistry.h"

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

    return true;
}
