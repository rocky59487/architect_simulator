// v3.5 Phase 4 — dyn-collapse replay actor. Reads FFrameDynCollapseResult.Frames (6N flat
// disp + vel per Newmark step), drives a PMC deformed-shape mesh through them. Supports
// scrubbing via SetPlaybackTime, play/pause, speed control, and a delegate fired when an
// FFrameDynCollapseEvent crosses CurrentTime during a Tick.
//
// Memory caveat: 1000-frame replay of a 50K-DOF model materialises 50K * 6 * 1000 = 300 MB
// in FFrameDynCollapseResult.Frames; this actor reads-only into that array, no extra copy.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCoreUE/FrameCoreUEAnalysisTypes.h"
#include "FrameDynCollapseReplayActor.generated.h"

class UProceduralMeshComponent;

// UHT marshal note: dynamic-multicast delegate USTRUCT params must be passed by value, not
// const-ref. Const-ref struct params either trip a UHT diagnostic or silently corrupt the
// BP graph boundary depending on UE minor version. FFrameDynCollapseEvent's copy cost
// (small TArrays at event rate, not per-tick) is acceptable.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFrameDynCollapseEventDelegate,
                                            FFrameDynCollapseEvent, Event);

UCLASS(Blueprintable, Category="FrameCore",
       meta=(DisplayName="Frame Dyn Collapse Replay Actor"))
class FRAMECOREUE_API AFrameDynCollapseReplayActor : public AActor
{
    GENERATED_BODY()

public:
    AFrameDynCollapseReplayActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|DynCollapse")
    FFrameDynCollapseResult CollapseResult;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|DynCollapse")
    TArray<FFrameMemberGeometry> MemberGeometry;

    // Number of nodes in the source model -- needed to slice the flat UFlat array into
    // per-node 6-component sub-vectors. If 0, the actor inspects Frames[0].UFlat.Num() / 6
    // and uses that. Setting it explicitly is recommended when MemberGeometry references
    // sparse node indices.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|DynCollapse",
              meta=(ClampMin="0", UIMin="0"))
    int32 NodeCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|DynCollapse",
              meta=(ClampMin="0.0", UIMin="0.0"))
    float PlaybackSpeed = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|DynCollapse")
    bool bLoop = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|DynCollapse",
              meta=(ClampMin="0.0", UIMin="0.0"))
    float DeflectionScale = 1.f;

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse")
    float CurrentTime = 0.f;

    UPROPERTY(BlueprintReadOnly, Category="FrameCore|DynCollapse")
    bool bPlaying = true;

    // Multicast delegate fired when an event time crosses CurrentTime during a Tick. The
    // event itself is passed by const ref so BP can inspect Mode / Time / removed elements.
    UPROPERTY(BlueprintAssignable, Category="FrameCore|DynCollapse")
    FFrameDynCollapseEventDelegate OnEventReached;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category="FrameCore|DynCollapse")
    UProceduralMeshComponent* GetMeshComponent() const { return MeshComponent; }

    UFUNCTION(BlueprintCallable, Category="FrameCore|DynCollapse") void Play()  { bPlaying = true; }
    UFUNCTION(BlueprintCallable, Category="FrameCore|DynCollapse") void Pause() { bPlaying = false; }

    // Sets CurrentTime and rebuilds the mesh at that scrub position. Does NOT fire event
    // delegates (scrub is for the editor; only forward playback dispatches events).
    UFUNCTION(BlueprintCallable, Category="FrameCore|DynCollapse")
    void SetPlaybackTime(float NewTime);

    // Rebuild the mesh at CurrentTime (lerp Frames[k] and Frames[k+1]). Returns true if at
    // least one section was built.
    UFUNCTION(BlueprintCallable, Category="FrameCore|DynCollapse")
    bool RebuildAtCurrentTime();

    virtual void Tick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category="FrameCore|DynCollapse")
    TObjectPtr<UProceduralMeshComponent> MeshComponent;

    void BuildOneMemberSection(int32 SectionIdx,
                               const FFrameMemberGeometry& Geom,
                               const TArray<float>& Disp,
                               int32 NodesInFrame);

    // Fires OnEventReached for every event whose Time falls in the half-open interval
    // (A, B]. Caller is responsible for bracketing on a loop wrap (call once for
    // (Prev, TotalT] then once for (-eps, wrapped] so end-of-cycle events still fire).
    void DispatchEventsIn(float A, float B);
};
