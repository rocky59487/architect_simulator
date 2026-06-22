#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FrameCoreUE/FrameCoreUEModelTypes.h"
#include "FrameCoreUE/FrameCoreUEResultTypes.h"
#include "FrameCoreUE/FrameCoreUEAnalysisTypes.h"
#include "FrameCoreUEAnalysisLibrary.generated.h"

// v3.4 Phase 3 + Phase 4 BP entry points to every analysis the engine exposes via
// FRAMECORE_API. Every entry takes a `FFrameModelDef` (and where relevant an options
// USTRUCT) and returns a marshalled result USTRUCT. On marshal failure (invalid model
// shape, OOB indices, length mismatches) the returned result has `bSingular = true`
// and Diagnostic populated -- matches engine "singular" semantics so BP graphs can
// branch on one boolean.
UCLASS()
class FRAMECOREUE_API UFrameAnalysisLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:

    // -- Phase 3 linear ------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category="FrameCore|Analysis|Linear",
              meta=(DisplayName="Solve Linear"))
    static FFrameSolveResult SolveLinear(const FFrameModelDef& Def, const FFrameSolveOptions& Opts);

    UFUNCTION(BlueprintCallable, Category="FrameCore|Analysis|Linear",
              meta=(DisplayName="Analysis Modal"))
    static FFrameModalResult AnalysisModal(const FFrameModelDef& Def,
                                           const FFrameSolveOptions& Opts,
                                           const FFrameModalOptions& ModalOpts);

    UFUNCTION(BlueprintCallable, Category="FrameCore|Analysis|Linear",
              meta=(DisplayName="Analysis Buckling"))
    static FFrameBucklingResult AnalysisBuckling(const FFrameModelDef& Def,
                                                 const FFrameSolveOptions& Opts,
                                                 const FFrameBucklingOptions& BucklingOpts);

    UFUNCTION(BlueprintCallable, Category="FrameCore|Analysis|Linear",
              meta=(DisplayName="Load Combine Envelope"))
    static FFrameLoadEnvelope LoadCombineEnvelope(const FFrameModelDef& BaseDef,
                                                  const FFrameSolveOptions& Opts,
                                                  const TArray<FFrameSizeOptLoadCase>& Cases);

    UFUNCTION(BlueprintCallable, Category="FrameCore|Analysis|Linear",
              meta=(DisplayName="Reaction Influence Line"))
    static FFrameInfluenceLine InfluenceLine(const FFrameModelDef& Def,
                                             const FFrameSolveOptions& Opts,
                                             const TArray<int32>& LoadNodes,
                                             int32 ReactNode,
                                             int32 ReactDof);

    UFUNCTION(BlueprintCallable, Category="FrameCore|Analysis|Linear",
              meta=(DisplayName="Response Spectrum"))
    static FFrameResponseSpectrumResult ResponseSpectrum(const FFrameModelDef& Def,
                                                         const FFrameSolveOptions& Opts,
                                                         const FFrameModalOptions& ModalOpts,
                                                         const FFrameSpectrum& Spectrum,
                                                         int32 ExcDof,
                                                         EFrameSpectrumCombo Combo,
                                                         float Zeta);

    UFUNCTION(BlueprintCallable, Category="FrameCore|Analysis|Linear",
              meta=(DisplayName="Real-Time Dynamic"))
    static FFrameModalTimeHistory RealTimeDynamic(const FFrameModelDef& Def,
                                                  const FFrameSolveOptions& Opts,
                                                  const FFrameModalOptions& ModalOpts,
                                                  const FFrameModalDynamicsOptions& DynOpts);

    // ReanalysisSolve stateless library form (v3.4): build a ReSolveSession per call, apply
    // the deactivate lists, solve. v3.5 InteractiveSubsystem will own a long-lived session
    // and expose deactivate/reactivate/solve as stateful BP events.
    UFUNCTION(BlueprintCallable, Category="FrameCore|Analysis|Linear",
              meta=(DisplayName="Reanalysis Solve (stateless)"))
    static FFrameSolveResult ReanalysisSolve(const FFrameModelDef& Def,
                                             const FFrameSolveOptions& Opts,
                                             const FFrameReanalysisOptions& ReOpts,
                                             const TArray<int32>& DeactivateMemberIds,
                                             const TArray<int32>& DeactivateShellIds);

    // -- Phase 4 nonlinear ---------------------------------------------------

    UFUNCTION(BlueprintCallable, Category="FrameCore|Analysis|Nonlinear",
              meta=(DisplayName="Solve P-Delta"))
    static FFramePDeltaResult SolvePDelta(const FFrameModelDef& Def,
                                          const FFramePDeltaOptions& PDOpts);

    UFUNCTION(BlueprintCallable, Category="FrameCore|Analysis|Nonlinear",
              meta=(DisplayName="Solve Tension Only"))
    static FFrameTensionOnlyResult SolveTensionOnly(const FFrameModelDef& Def,
                                                    const FFrameTensionOnlyOptions& TOpts);

    UFUNCTION(BlueprintCallable, Category="FrameCore|Analysis|Nonlinear",
              meta=(DisplayName="Solve Size Optimisation"))
    static FFrameSizeOptResult SolveSizeOpt(const FFrameModelDef& Def,
                                            const FFrameSizeOptOptions& SOpts,
                                            const TArray<int32>& SizableMembers);

    UFUNCTION(BlueprintCallable, Category="FrameCore|Analysis|Nonlinear",
              meta=(DisplayName="Solve BESO Topology"))
    static FFrameBESOResult SolveBESO(const FFrameModelDef& Def,
                                      const FFrameBESOOptions& BOpts,
                                      const TArray<int32>& DesignMembers);

    UFUNCTION(BlueprintCallable, Category="FrameCore|Analysis|Nonlinear",
              meta=(DisplayName="Solve Co-rotational"))
    static FFrameCorotationalResult SolveCorotational(const FFrameModelDef& Def,
                                                      const FFrameCorotationalOptions& COpts);

    // Forces opts.bUseArcLength = true regardless of the input; otherwise identical to
    // SolveCorotational. PathLambda / PathDisp are the snap-through curve trace.
    UFUNCTION(BlueprintCallable, Category="FrameCore|Analysis|Nonlinear",
              meta=(DisplayName="Solve Arc-length"))
    static FFrameCorotationalResult SolveArcLength(const FFrameModelDef& Def,
                                                   const FFrameCorotationalOptions& COpts);

    UFUNCTION(BlueprintCallable, Category="FrameCore|Analysis|Nonlinear",
              meta=(DisplayName="Solve Dynamic Collapse (blocking)"))
    static FFrameDynCollapseResult SolveDynCollapse(const FFrameModelDef& Def,
                                                    const FFrameDynCollapseOptions& DOpts);
};
