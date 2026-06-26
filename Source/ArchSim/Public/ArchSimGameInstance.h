// ArchSim - UArchSimGameInstance : owns the per-frame tick driver that bridges
// placed UArchSimMemberData actors to the FrameCore solver via UArchSimModelRegistry.
//
// Sprint S-02 (AS-02a). Spec: docs/logs/S-02/plan_2026-06-26T1033.md § AS-02a.
// The Tick body that walks Members[] / triggers RequestSolve is the AS-02b
// deliverable; this skeleton only wires lifetime + FTickableGameObject + Config
// so AS-02b can fill the loop without touching scaffolding.
//
// Why FTickableGameObject instead of Actor::Tick or TimerManager?
//   GameInstance has no scene/Actor context, so UGameInstance does NOT route through
//   the World's actor-tick pipeline. FTickableGameObject is the sanctioned UE5 pattern
//   for attaching a tick to a non-Actor UObject (see UE source: TickTaskManager.cpp).
//   It gives us a tick that fires every engine frame but is fully independent of
//   level lifetime — exactly what a cross-Level solver driver needs.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Tickable.h"                     // FTickableGameObject

#include "ArchSimGameInstance.generated.h"

// LogArchSim: module-level log category for the ArchSim game body.
// Declared here so every game-body TU can share it without re-declaring.
// Only one DEFINE_LOG_CATEGORY(LogArchSim) must exist — in ArchSimGameInstance.cpp.
DECLARE_LOG_CATEGORY_EXTERN(LogArchSim, Log, All);

UCLASS()
class ARCHSIM_API UArchSimGameInstance : public UGameInstance, public FTickableGameObject
{
    GENERATED_BODY()

public:
    // ---- UGameInstance ----------------------------------------------------------
    virtual void Init() override;
    virtual void Shutdown() override;

    // ---- FTickableGameObject ----------------------------------------------------
    virtual void Tick(float DeltaSeconds) override;

    // Conditional tick: only fire when game is running, world is valid, and we are
    // not a Class Default Object. This is the safest sub-set for a solver driver.
    virtual ETickableTickType GetTickableTickType() const override
        { return ETickableTickType::Conditional; }

    [[nodiscard]] virtual bool IsTickable() const override;

    // Editor ticks would fire in the viewport without an active play session —
    // the solver has no meaningful work to do there yet.
    virtual bool IsTickableInEditor() const override { return false; }

    // Pause means the game loop is intentionally suspended; solver output during
    // pause would be stale by the time the game resumes.
    virtual bool IsTickableWhenPaused() const override { return false; }

    virtual TStatId GetStatId() const override;

    // ---- BP-readable lifecycle telemetry (AS-02c will extend) -------------------
    // Counts how many engine frames have elapsed since Init(). Purely additive;
    // AS-02b's driver logic writes no per-frame results back here.
    UFUNCTION(BlueprintPure, Category="ArchSim|Lifecycle")
    [[nodiscard]] int32 GetTickCount() const { return TickCount; }

    // Wall-clock accumulation (UE DeltaSeconds, unscaled). Lets a smoke test assert
    // "some time has passed" without measuring exact frame timing.
    UFUNCTION(BlueprintPure, Category="ArchSim|Lifecycle")
    [[nodiscard]] float GetAccumulatedTime() const { return AccumulatedSeconds; }

private:
    // ---- Tick telemetry ---------------------------------------------------------
    // Pure counters; no per-frame allocations (++int32 / += float are branch-
    // predictor-friendly O(1) and never alloc). AS-02b fills Tick() body above
    // these two lines, so the diff is surgically isolated.
    int32 TickCount = 0;
    float AccumulatedSeconds = 0.f;

    // ---- Tick gate --------------------------------------------------------------
    // Set true at end of Init(); false at start of Shutdown(). The ordering is
    // deliberate: setting LAST in Init protects against premature IsTickable()
    // queries inside Super::Init(); setting FIRST in Shutdown prevents a race where
    // Tick() fires between "Shutdown started" and "subsystems torn down".
    bool bIsActive = false;
};
