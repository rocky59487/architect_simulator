// ArchSim — Headless smoke test for UArchSimScenarioWidget u3 additions:
//   K2/K4 placement UFunction reflection, EArchSimTutorialState UENUM, tutorial state
//   machine (AdvanceTutorialStep transitions), GetCurrentPromptText, ResetWidgetState,
//   and BlueprintImplementableEvent UFunction reflection.
//
// Sprint S-05, SPIKE-Scenario-u3. Plan ref: docs/logs/S-05/plan_*.md § SPIKE-Scenario-u3.
//
// What this test CAN verify in headless (-nullrhi -unattended):
//   1. PlaceK2Beam UFunction exists in reflection with BlueprintCallable + correct sig
//   2. PlaceK4Brace UFunction exists in reflection with BlueprintCallable + correct sig
//   3. EArchSimTutorialState UENUM has exactly 6 values (Welcome..FreeExplore)
//   4. TutorialState UPROPERTY: BlueprintReadOnly + initial value Welcome
//   5. AdvanceTutorialStep transient instance call: all state transitions in order,
//      terminal FreeExplore stays FreeExplore
//   6. GetCurrentPromptText: returns non-empty FText for every state
//   7. ResetWidgetState: NewObject → call AdvanceTutorialStep twice → Reset → state=Welcome
//      No crash, no dangling delegate (safe in headless; GEditor null path tested).
//   8. OnTutorialStateChanged + OnVoicePromptShouldPlay: UFunction reflection
//      (BlueprintImplementableEvent exists in reflection table)
//   [DEFERRED sub-check 9] PIE 5 min smoke — USER-DRIVEN; see docs/logs/S-05/u3_pie_smoke.md
//
// What this test CANNOT verify in headless (honest-defer per AS-07 lesson #1):
//   - K2/K4 actual spawn (requires live Editor World / GEditor context)
//   - Registry registration for K2/K4 (requires live PIE / GameInstance)
//   - OnTutorialStateChanged / OnVoicePromptShouldPlay actually fired in BP
//     (BlueprintImplementableEvent C++ impl is a no-op stub — BP override not reachable headless)
//   - PIE 5 min student trial smoke (user-driven, see u3_pie_smoke.md)
//
// Naming: ArchSim.Gameplay.ScenarioTutorial → NEW class → $ExpectedUeTests 147→148
//
// FROZEN guard: zero lines under Plugins/FrameSolver/Source/FrameCore/ (v4.0.0 FROZEN).
// Registry / InteractiveSubsystem source not touched (consume-only per task spec).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_EDITOR

#include "Editor/ArchSimScenarioWidget.h"
#include "EditorUtilityWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FArchSimScenarioTutorialTest,
    "ArchSim.Gameplay.ScenarioTutorial",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FArchSimScenarioTutorialTest::RunTest(const FString& Parameters)
{
    // ---- Baseline: widget class + CDO must be reachable -------------------------
    UClass* WidgetClass = UArchSimScenarioWidget::StaticClass();
    if (!TestNotNull(TEXT("Baseline: UArchSimScenarioWidget::StaticClass() non-null"), WidgetClass))
        return false;

    UArchSimScenarioWidget* CDO = WidgetClass->GetDefaultObject<UArchSimScenarioWidget>();
    if (!TestNotNull(TEXT("Baseline: GetDefaultObject<UArchSimScenarioWidget>() non-null"), CDO))
        return false;

    // ---- Sub-check 1: PlaceK2Beam UFunction reflection -------------------------
    // Verify the UFunction exists with BlueprintCallable and returns AActor* with 1 FVector
    // input param (same signature contract as PlaceK1Column per u1 test).
    UFunction* K2Fn = WidgetClass->FindFunctionByName(TEXT("PlaceK2Beam"));
    if (!TestNotNull(TEXT("Sub-check 1: PlaceK2Beam UFunction exists in reflection"), K2Fn))
        return false;

    // Return type must be an object property (AActor*).
    FProperty* K2RetProp = K2Fn->GetReturnProperty();
    if (TestNotNull(TEXT("Sub-check 1a: PlaceK2Beam has a return property"), K2RetProp))
    {
        TestTrue(TEXT("Sub-check 1b: PlaceK2Beam return type is FObjectPropertyBase (AActor*)"),
                 K2RetProp->IsA<FObjectPropertyBase>());
    }

    // Must have exactly 1 non-return parameter (FVector LocationWorld).
    {
        int32 ParamCount = 0;
        for (TFieldIterator<FProperty> It(K2Fn); It; ++It)
        {
            if ((It->PropertyFlags & CPF_Parm) && !(It->PropertyFlags & CPF_ReturnParm))
                ++ParamCount;
        }
        TestEqual(TEXT("Sub-check 1c: PlaceK2Beam has exactly 1 input parameter (FVector LocationWorld)"),
                  ParamCount, 1);
    }

    // ---- Sub-check 2: PlaceK4Brace UFunction reflection ------------------------
    UFunction* K4Fn = WidgetClass->FindFunctionByName(TEXT("PlaceK4Brace"));
    if (!TestNotNull(TEXT("Sub-check 2: PlaceK4Brace UFunction exists in reflection"), K4Fn))
        return false;

    FProperty* K4RetProp = K4Fn->GetReturnProperty();
    if (TestNotNull(TEXT("Sub-check 2a: PlaceK4Brace has a return property"), K4RetProp))
    {
        TestTrue(TEXT("Sub-check 2b: PlaceK4Brace return type is FObjectPropertyBase (AActor*)"),
                 K4RetProp->IsA<FObjectPropertyBase>());
    }

    {
        int32 ParamCount = 0;
        for (TFieldIterator<FProperty> It(K4Fn); It; ++It)
        {
            if ((It->PropertyFlags & CPF_Parm) && !(It->PropertyFlags & CPF_ReturnParm))
                ++ParamCount;
        }
        TestEqual(TEXT("Sub-check 2c: PlaceK4Brace has exactly 1 input parameter (FVector LocationWorld)"),
                  ParamCount, 1);
    }

    // ---- Sub-check 3: EArchSimTutorialState UENUM has 6 values -----------------
    // UEnum::GetNumEnumerators() counts the enum values (excludes the implicit MAX entry
    // if present; UHT adds it for UENUM(BlueprintType) when the name is mangled).
    // We verify all 6 named values are reachable by name via GetValueByName().
    // WHY by-name check (not just count): ensures no accidental value collisions and that
    // UHT generated the correct display names.
    UEnum* TutorialEnum = StaticEnum<EArchSimTutorialState>();
    if (!TestNotNull(TEXT("Sub-check 3: EArchSimTutorialState UENUM non-null"), TutorialEnum))
        return false;

    // Check all 6 expected named entries exist in the enum reflection table.
    const TArray<FName> ExpectedNames = {
        TEXT("EArchSimTutorialState::Welcome"),
        TEXT("EArchSimTutorialState::PromptPlaceK1"),
        TEXT("EArchSimTutorialState::PromptPlaceK2"),
        TEXT("EArchSimTutorialState::PromptPlaceK4"),
        TEXT("EArchSimTutorialState::PromptPressTest"),
        TEXT("EArchSimTutorialState::FreeExplore"),
    };
    for (const FName& EnumName : ExpectedNames)
    {
        const int64 Val = TutorialEnum->GetValueByName(EnumName);
        TestTrue(FString::Printf(TEXT("Sub-check 3: UENUM value '%s' exists (val != INDEX_NONE)"),
                                  *EnumName.ToString()),
                 Val != INDEX_NONE);
    }

    // Also verify the enum reports exactly 6 enumerators (no spurious extra entries).
    // GetNumEnumerators() counts values WITHOUT the MAX entry (UE5 BlueprintType convention).
    // ASSUMPTION: UHT auto-generates exactly one MAX entry per UENUM(BlueprintType); if a future
    // change adds a manual `EArchSimTutorialState_MAX` or another sentinel, `NumEnums() - 1`
    // will under-count. The 6 named-value `GetValueByName != INDEX_NONE` checks above already
    // give us strong guarantees on the canonical states; this count check is a defence-in-depth
    // signal against silent enum growth.
    const int32 NumEnumerators = TutorialEnum->NumEnums() - 1; // -1 to exclude MAX entry
    TestEqual(TEXT("Sub-check 3: EArchSimTutorialState has exactly 6 values (including FreeExplore)"),
              NumEnumerators, 6);

    // ---- Sub-check 4: TutorialState UPROPERTY reflection + initial value -------
    // Verify TutorialState UPROPERTY exists with BlueprintReadOnly.
    FProperty* TutorialStateProp = WidgetClass->FindPropertyByName(TEXT("TutorialState"));
    if (!TestNotNull(TEXT("Sub-check 4: TutorialState UPROPERTY exists in reflection"), TutorialStateProp))
        return false;

    // Must be a UEnum-backed byte property (UE5 stores UENUM properties as FByteProperty
    // or FEnumProperty depending on the underlying type; EArchSimTutorialState : uint8
    // maps to FByteProperty with an associated UEnum, or FEnumProperty in newer UE5).
    // IsA<FByteProperty> OR IsA<FNumericProperty> both cover it.
    const bool bEnumPropType = TutorialStateProp->IsA<FByteProperty>()
                             || TutorialStateProp->IsA<FEnumProperty>();
    TestTrue(TEXT("Sub-check 4a: TutorialState property type is FByteProperty or FEnumProperty (uint8 UENUM)"),
             bEnumPropType);

    // Verify BlueprintReadOnly flag. UPROPERTY(BlueprintReadOnly) sets CPF_BlueprintVisible
    // but NOT CPF_BlueprintReadOnly directly in the PropertyFlags. The canonical check is
    // HasAnyPropertyFlags(CPF_BlueprintReadOnly). WHY: UHT maps BlueprintReadOnly to
    // CPF_BlueprintVisible | CPF_BlueprintReadOnly.
    TestTrue(TEXT("Sub-check 4b: TutorialState UPROPERTY has BlueprintReadOnly flag"),
             TutorialStateProp->HasAnyPropertyFlags(CPF_BlueprintReadOnly));

    // Verify CDO initial value is Welcome (== 0 as uint8).
    // WHY query via CDO: the CDO default is the declaration initializer value.
    // WHY ContainerPtrToValuePtr + cast: the canonical UE5 way to read a uint8-backed enum
    // UPROPERTY from a UObject instance. GetValue_InContainer is templatized and may not be
    // available on all FProperty paths; ContainerPtrToValuePtr is the stable API.
    {
        const uint8* RawPtr = TutorialStateProp->ContainerPtrToValuePtr<uint8>(CDO);
        if (TestNotNull(TEXT("Sub-check 4c pre: TutorialState CDO raw pointer non-null"), RawPtr))
        {
            const EArchSimTutorialState CDOState = static_cast<EArchSimTutorialState>(*RawPtr);
            TestEqual(TEXT("Sub-check 4c: TutorialState CDO initial value is EArchSimTutorialState::Welcome"),
                      CDOState, EArchSimTutorialState::Welcome);
        }
    }

    // ---- Sub-check 5: AdvanceTutorialStep transitions (transient instance) ------
    // Create a transient instance (not CDO) to exercise state transitions without
    // polluting the CDO state. We step through all 5 transitions and verify each.
    UArchSimScenarioWidget* Widget =
        NewObject<UArchSimScenarioWidget>(
            GetTransientPackage(),
            WidgetClass,
            NAME_None,
            RF_Transient);
    if (!TestNotNull(TEXT("Sub-check 5: transient widget instance created"), Widget))
        return false;

    // Initial state must be Welcome (per UPROPERTY default initializer).
    TestEqual(TEXT("Sub-check 5a: initial TutorialState is Welcome"),
              Widget->TutorialState, EArchSimTutorialState::Welcome);

    // Advance Welcome → PromptPlaceK1
    Widget->AdvanceTutorialStep();
    TestEqual(TEXT("Sub-check 5b: after 1 advance → PromptPlaceK1"),
              Widget->TutorialState, EArchSimTutorialState::PromptPlaceK1);

    // Advance PromptPlaceK1 → PromptPlaceK2
    Widget->AdvanceTutorialStep();
    TestEqual(TEXT("Sub-check 5c: after 2 advances → PromptPlaceK2"),
              Widget->TutorialState, EArchSimTutorialState::PromptPlaceK2);

    // Advance PromptPlaceK2 → PromptPlaceK4
    Widget->AdvanceTutorialStep();
    TestEqual(TEXT("Sub-check 5d: after 3 advances → PromptPlaceK4"),
              Widget->TutorialState, EArchSimTutorialState::PromptPlaceK4);

    // Advance PromptPlaceK4 → PromptPressTest
    Widget->AdvanceTutorialStep();
    TestEqual(TEXT("Sub-check 5e: after 4 advances → PromptPressTest"),
              Widget->TutorialState, EArchSimTutorialState::PromptPressTest);

    // Advance PromptPressTest → FreeExplore
    Widget->AdvanceTutorialStep();
    TestEqual(TEXT("Sub-check 5f: after 5 advances → FreeExplore (terminal)"),
              Widget->TutorialState, EArchSimTutorialState::FreeExplore);

    // Advance again — must stay FreeExplore (no-op terminal)
    Widget->AdvanceTutorialStep();
    TestEqual(TEXT("Sub-check 5g: after 6 advances → still FreeExplore (terminal, no wrap)"),
              Widget->TutorialState, EArchSimTutorialState::FreeExplore);

    // ---- Sub-check 6: GetCurrentPromptText returns non-empty FText for all states --
    // Create a fresh transient instance to iterate all states.
    UArchSimScenarioWidget* Widget2 =
        NewObject<UArchSimScenarioWidget>(
            GetTransientPackage(),
            WidgetClass,
            NAME_None,
            RF_Transient);
    if (!TestNotNull(TEXT("Sub-check 6: Widget2 transient instance created"), Widget2))
        return false;

    const TArray<EArchSimTutorialState> AllStates = {
        EArchSimTutorialState::Welcome,
        EArchSimTutorialState::PromptPlaceK1,
        EArchSimTutorialState::PromptPlaceK2,
        EArchSimTutorialState::PromptPlaceK4,
        EArchSimTutorialState::PromptPressTest,
        EArchSimTutorialState::FreeExplore,
    };
    for (EArchSimTutorialState State : AllStates)
    {
        // Directly write the enum state via UPROPERTY container pointer to test each state.
        // WHY ContainerPtrToValuePtr + memcpy: canonical UE5 way to write a uint8-backed
        // enum UPROPERTY into a live instance without going through the state machine.
        // We want to test GetCurrentPromptText independently of AdvanceTutorialStep.
        uint8* RawPtr2 = TutorialStateProp->ContainerPtrToValuePtr<uint8>(Widget2);
        if (RawPtr2)
        {
            *RawPtr2 = static_cast<uint8>(State);
        }
        const FText PromptText = Widget2->GetCurrentPromptText();
        TestFalse(FString::Printf(TEXT("Sub-check 6: GetCurrentPromptText for state %d is non-empty"),
                                   static_cast<int32>(State)),
                  PromptText.IsEmpty());
    }

    // ---- Sub-check 7: ResetWidgetState is headless-safe (no crash, state=Welcome) --
    // Use a Widget3 that has been advanced 2 steps before reset.
    UArchSimScenarioWidget* Widget3 =
        NewObject<UArchSimScenarioWidget>(
            GetTransientPackage(),
            WidgetClass,
            NAME_None,
            RF_Transient);
    if (!TestNotNull(TEXT("Sub-check 7: Widget3 transient instance created"), Widget3))
        return false;

    // Advance twice to get to PromptPlaceK2.
    Widget3->AdvanceTutorialStep(); // → PromptPlaceK1
    Widget3->AdvanceTutorialStep(); // → PromptPlaceK2
    TestEqual(TEXT("Sub-check 7 pre-reset: Widget3 is at PromptPlaceK2"),
              Widget3->TutorialState, EArchSimTutorialState::PromptPlaceK2);

    // Reset — must not crash even though GEditor is null in headless.
    // The unsubscribe path (SolveCompleteDelegateHandle.IsValid() == false initially)
    // is a no-op; HeatmapActor is null → IsValid(null) == false → no Destroy call.
    // Tutorial state must return to Welcome.
    bool bResetDidNotCrash = true;
    Widget3->ResetWidgetState();
    // If we reach this line, no crash occurred (UE Automation framework would abort on Check/Ensure).
    TestTrue(TEXT("Sub-check 7a: ResetWidgetState did not crash (headless, null GEditor)"),
             bResetDidNotCrash);
    TestEqual(TEXT("Sub-check 7b: TutorialState after Reset is Welcome"),
              Widget3->TutorialState, EArchSimTutorialState::Welcome);

    // PlacedActors must be empty after reset (even if no actors were actually spawned
    // headless — PlacedActors starts empty; Reset calls Empty() which is a no-op on
    // empty array; still verifies the contract).
    TestEqual(TEXT("Sub-check 7c: PlacedActors.Num() == 0 after Reset"),
              Widget3->PlacedActors.Num(), 0);

    // ---- Sub-check 8: BlueprintImplementableEvent UFunction reflection ----------
    // Verify OnTutorialStateChanged and OnVoicePromptShouldPlay are registered in the
    // reflection table as UFunctions (BlueprintImplementableEvent generates a UFunction
    // stub with the FUNC_BlueprintEvent flag — UE5.7 does not have a separate
    // FUNC_BlueprintImplementableEvent flag; both BlueprintImplementableEvent and
    // BlueprintNativeEvent share FUNC_BlueprintEvent. See Engine/Source/Runtime/
    // CoreUObject/Public/UObject/Script.h L163. Phase 3 inline-fix 2026-06-27).
    UFunction* OnStateFn = WidgetClass->FindFunctionByName(TEXT("OnTutorialStateChanged"));
    if (!TestNotNull(TEXT("Sub-check 8: OnTutorialStateChanged UFunction exists in reflection"),
                     OnStateFn))
        return false;
    TestTrue(TEXT("Sub-check 8a: OnTutorialStateChanged has BlueprintEvent flag (BP-overridable)"),
             OnStateFn->HasAnyFunctionFlags(FUNC_BlueprintEvent));

    // Verify parameter count: 2 (EArchSimTutorialState NewState + FText PromptText).
    {
        int32 ParamCount = 0;
        for (TFieldIterator<FProperty> It(OnStateFn); It; ++It)
        {
            if ((It->PropertyFlags & CPF_Parm) && !(It->PropertyFlags & CPF_ReturnParm))
                ++ParamCount;
        }
        TestEqual(TEXT("Sub-check 8b: OnTutorialStateChanged has 2 input parameters"),
                  ParamCount, 2);
    }

    UFunction* OnVoiceFn = WidgetClass->FindFunctionByName(TEXT("OnVoicePromptShouldPlay"));
    if (!TestNotNull(TEXT("Sub-check 8c: OnVoicePromptShouldPlay UFunction exists in reflection"),
                     OnVoiceFn))
        return false;
    TestTrue(TEXT("Sub-check 8d: OnVoicePromptShouldPlay has BlueprintEvent flag (BP-overridable)"),
             OnVoiceFn->HasAnyFunctionFlags(FUNC_BlueprintEvent));

    // Verify parameter count: 1 (FString PromptText).
    {
        int32 ParamCount = 0;
        for (TFieldIterator<FProperty> It(OnVoiceFn); It; ++It)
        {
            if ((It->PropertyFlags & CPF_Parm) && !(It->PropertyFlags & CPF_ReturnParm))
                ++ParamCount;
        }
        TestEqual(TEXT("Sub-check 8e: OnVoicePromptShouldPlay has 1 input parameter (FString PromptText)"),
                  ParamCount, 1);
    }

    // ---- Sub-check 9: PIE 5min smoke [DEFERRED to user-driven] -----------------
    // WHY deferred: tutorial overlay, K2/K4 actual placement, heatmap response, and the
    // 5+ min free-explore loop all require a live PIE world, UMG viewport, and player input.
    // Headless commandlet (-nullrhi -unattended) has none of these.
    // USER-DRIVEN smoke instructions: docs/logs/S-05/u3_pie_smoke.md
    // Following the AS-13 precedent for driver-loop and PIE-input-runtime.
    TestTrue(TEXT("Sub-check 9: PIE 5min smoke deferred to user-driven fixture "
                  "[NEW CODE / DEFERRED] — see docs/logs/S-05/u3_pie_smoke.md"),
             true);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
#endif // WITH_EDITOR
