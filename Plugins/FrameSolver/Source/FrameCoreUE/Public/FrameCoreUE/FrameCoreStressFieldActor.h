// v3.3 (U-03): runtime stress-field renderer. Reads an FFrameStressField (produced via
// UFrameCoreStressFieldLibrary) and a per-member geometry list (FFrameMemberGeometry array),
// emits a UProceduralMeshComponent sigma-band box mesh along every member. Thin slice:
// member mesh only; shell heat-map / Niagara particles / collapse replay are out of scope
// for v3.3 minor. See docs/specs/S11_v3.3_schema_migration.md for the schema rationale
// and docs/HANDOFF_v3.3.0.md (forthcoming) for renderer scope / non-goals.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FrameCoreUE/FrameCoreUETypes.h"
#include "FrameCoreStressFieldActor.generated.h"

class UProceduralMeshComponent;

UCLASS(Blueprintable, Category="FrameCore",
       meta=(DisplayName="Frame Core Stress Field Actor"))
class FRAMECOREUE_API AFrameCoreStressFieldActor : public AActor
{
    GENERATED_BODY()

public:
    AFrameCoreStressFieldActor();

    // Input data. Field comes from UFrameCoreStressFieldLibrary (or a BP that builds one
    // from JSON via the v3.3 BP load entrypoint). MemberGeometry is supplied by the
    // designer — one entry per active member, MemberIdx pairs against Field.Members[].
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|StressField")
    FFrameStressField Field;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|StressField")
    TArray<FFrameMemberGeometry> MemberGeometry;

    // When true (default), call BuildMesh() once at BeginPlay so a placed actor without
    // BP scripting still renders. Set to false when the BP supplies Field / Geometry
    // dynamically and wants to drive BuildMesh() itself.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FrameCore|StressField")
    bool bAutoBuildOnBeginPlay = true;

    // Returns the procedural mesh component the actor builds into. BP can inspect or
    // override material if it wants a different shader.
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="FrameCore|StressField")
    UProceduralMeshComponent* GetMeshComponent() const { return MeshComponent; }

    // Rebuild the procedural mesh from the current Field + MemberGeometry. Idempotent:
    // existing sections are cleared first. Returns true if at least one member produced
    // a mesh section (false on empty Field / empty Geometry / mismatched MemberIdx).
    UFUNCTION(BlueprintCallable, Category="FrameCore|StressField")
    bool BuildMesh();

    // Convenience: builds a one-member cantilever geometry list (suitable for use with
    // UFrameCoreStressFieldLibrary::ComputeCantileverFixture(P, L, *)) at world origin,
    // extruding +X. Designers can call this from BP to wire a quick demo without hand-
    // assembling FFrameMemberGeometry.
    UFUNCTION(BlueprintCallable, BlueprintPure, Category="FrameCore|StressField",
              meta=(DisplayName="Make Cantilever Demo Geometry"))
    static TArray<FFrameMemberGeometry> MakeCantileverDemoGeometry(
        float L = 2000.f, float Width = 100.f, float Depth = 100.f);

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category="FrameCore|StressField")
    TObjectPtr<UProceduralMeshComponent> MeshComponent;

    // Emit one PMC section per (member, geometry) pair. Vertex colors encode normalised
    // sigma (sample.sigmaCompMax / Field.GlobalMaxFiberSigma) via a blue->green->red
    // 3-stop ramp. 4 vertices per ring × samplesPerSpan rings; 4 side faces × 2 tris ×
    // (samplesPerSpan - 1) segments + 2 end caps × 2 tris.
    void BuildOneMemberSection(int32 SectionIdx,
                               const FFrameMemberStressTrace& Trace,
                               const FFrameMemberGeometry& Geom);
};
