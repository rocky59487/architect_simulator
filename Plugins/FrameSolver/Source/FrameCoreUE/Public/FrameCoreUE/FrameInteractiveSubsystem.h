// v3.5 Phase 7 — interactive S1 ReSolve subsystem. Wraps a long-lived `frame::ReSolveSession`
// across BP ticks so a designer can deactivate/reactivate members and shells without paying
// a full factorisation on every solve. Target perf: 60 fps @ 10K DOF interactive (engine R2
// lane already hits this on standalone; this subsystem is a thin BP wrapper).
//
// Lifetime: subsystem state belongs to the owning UGameInstance — StartSession allocates the
// engine ReSolveSession, EndSession deallocates. Re-Start replaces the session.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FrameCoreUE/FrameCoreUEModelTypes.h"
#include "FrameCoreUE/FrameCoreUEResultTypes.h"
#include "FrameCoreUE/FrameCoreUEAnalysisTypes.h"
#include "FrameCoreUE/FrameCoreUEVisualTypes.h"
#include "FrameInteractiveSubsystem.generated.h"

namespace frame {
    class ReSolveSession;
}

UCLASS(BlueprintType, Category="FrameCore")
class FRAMECOREUE_API UFrameInteractiveSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UFrameInteractiveSubsystem();
    virtual ~UFrameInteractiveSubsystem();

    // Audit D-05: UObject-derived classes are non-copyable / non-movable; make the
    // compiler-synthesised copy / move constructors explicit deletions so a future UE
    // revision that changes GENERATED_BODY()'s default-member visibility can't silently
    // expose them on a class holding a TUniquePtr<incomplete FCachedModel>.
    UFrameInteractiveSubsystem(const UFrameInteractiveSubsystem&) = delete;
    UFrameInteractiveSubsystem& operator=(const UFrameInteractiveSubsystem&) = delete;

    // Begin / End the engine ReSolveSession around the given model. Returns true on success;
    // on failure (singular baseline, model validation error) OutError holds the diagnostic
    // and the subsystem is left without an active session.
    UFUNCTION(BlueprintCallable, Category="FrameCore|Interactive")
    bool StartSession(const FFrameModelDef& Def,
                      const FFrameSolveOptions& Opts,
                      const FFrameReanalysisOptions& ReOpts,
                      FString& OutError);

    UFUNCTION(BlueprintCallable, Category="FrameCore|Interactive")
    void EndSession();

    // Returns true iff there is an active session (StartSession succeeded, EndSession not
    // called since).
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="FrameCore|Interactive")
    bool IsSessionActive() const { return Session != nullptr; }

    // Apply a patch (toggle members / shells) and resolve. Returns true on success; OutResult
    // is the marshalled FFrameSolveResult. On no-session OutResult.bSingular = true.
    UFUNCTION(BlueprintCallable, Category="FrameCore|Interactive")
    bool ApplyPatchAndResolve(const FFrameModelPatch& Patch, FFrameSolveResult& OutResult);

    // Force a Tier-3 rebaseline (fresh factorisation on current active set). Useful when the
    // designer expects the ladder rank to grow large and wants to reset the cost amortisation.
    UFUNCTION(BlueprintCallable, Category="FrameCore|Interactive")
    void Rebaseline();

    // Solve without applying a patch (resolves the current active set). Useful for performance
    // baseline tests.
    UFUNCTION(BlueprintCallable, Category="FrameCore|Interactive")
    bool ResolveCurrent(FFrameSolveResult& OutResult);

protected:
    virtual void Deinitialize() override;

private:
    // Engine state — opaque pointer because frame::ReSolveSession is non-copyable and lives
    // outside any UE memory pool. Cleared by EndSession / Deinitialize.
    frame::ReSolveSession* Session = nullptr;

    // Cached model so solve results can be marshalled (the marshal needs the model to walk
    // per-node indices). We keep a snapshot of the model from StartSession; if the model is
    // patched outside the subsystem the marshal still uses the snapshot — caller is
    // responsible for restarting the session if structural data changes.
    struct FCachedModel;
    TUniquePtr<FCachedModel> Cached;
};
