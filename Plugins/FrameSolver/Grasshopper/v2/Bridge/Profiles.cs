// Profiles.cs -- Simple vs Advanced profile contracts.
//
// The two profiles share ONE wire schema, ONE C ABI, ONE engine. The difference lives entirely
// in the session.open `profile` field and how the dispatcher/SDK treat silent paths around it.
// See docs/specs/S6b_rhino_bridge_v2.md § ⑭.
//
//   SIMPLE   — UE5-friendly. Missing options get sensible defaults; silent fallbacks (supernodal
//              SPD fail -> LDLT) are taken without complaint; singular returns result.Singular=true
//              instead of throwing; DYNC returns a summary + per-frame peak only.
//
//   ADVANCED — Rhino / academic / official Rhino-UE5 bridge. Missing options are an error; silent
//              fallbacks are turned OFF (failure becomes a structured error); singular becomes a
//              RemoteException; DYNC defaults to binary u/v frames + fragment cluster detail; each
//              Result carries an AdvancedDiagnostics object with the full per-stage trace.
//
// The C# SDK exposes the choice through two NAMED FrameSession entry points (OpenSimpleAsync /
// OpenAdvancedAsync), and through strongly typed *Strict* option records that have NO defaults --
// the compiler refuses to instantiate them without explicit values, mirroring the wire-level
// strict validation.

namespace FrameCore.Bridge;

public enum BridgeProfile
{
    /// <summary>
    /// UE5-friendly: missing options take sensible defaults; silent fallbacks are accepted;
    /// singular returns flag-on-result. This is the default profile if neither
    /// <see cref="FrameSession.OpenSimpleAsync"/> nor <see cref="FrameSession.OpenAdvancedAsync"/>
    /// is called explicitly.
    /// </summary>
    Simple = 0,

    /// <summary>
    /// Strict / no-silent: every option must be explicit, silent fallbacks become structured
    /// errors, singular throws, DYNC streams full binary u/v + fragment cluster detail, every
    /// result carries <see cref="Result.AdvancedDiagnostics"/>. Requires the engine to advertise
    /// the <c>profile.advanced</c> capability.
    /// </summary>
    Advanced = 1,
}

/// <summary>
/// STRICT engine-session options. All fields are <c>required init</c> so the C# compiler refuses
/// to construct an instance without explicit values -- the SDK-layer mirror of advanced profile's
/// wire-level strict validation. Use <see cref="EngineSessionOptions"/> instead for simple
/// profile (defaults filled-in).
/// </summary>
public sealed record AdvancedEngineSessionOptions
{
    public required EngineSessionMode Mode { get; init; }

    /// <summary>Mechanism-detection pivot tolerance (relative to max|D|).</summary>
    public required double PivotTol { get; init; }
    public required bool EnableReleases { get; init; }
    public required bool UseTimoshenko { get; init; }
    public required bool UseIncompatibleMembrane { get; init; }
    public required bool UseDKQPlate { get; init; }
    public required bool ShellGeometricStiffness { get; init; }
    public required bool UseWarpingCorrection { get; init; }
    public required double WarpTolerance { get; init; }
    /// <summary>
    /// 0 = guard explicitly off (will land in <c>advancedDiagnostics.guardsDisabled</c>);
    /// &gt; 0 = enforce coarse-curved-mesh rejection at this angle.
    /// </summary>
    public required double ShellCurvatureMaxAngleDeg { get; init; }
    public required bool UseSupernodalPrimary { get; init; }

    // SnSession knobs -- required when Mode == Supernodal; in advanced you set them
    // regardless and the engine refuses if you contradict Mode.
    public required int SnIrSteps { get; init; }
    public required int SnAmalgMaxCol { get; init; }
    public required int SnNumThreads { get; init; }
    public required bool SnFallbackOnFail { get; init; }

    /// <summary>
    /// When true the engine pushes <c>diagnostic</c> events per stage (validate/assemble/
    /// factor/solve/recover/...) to the same request id. Engine must advertise
    /// <c>diagnostic.stream</c>.
    /// </summary>
    public required bool DiagnosticStream { get; init; }
}

/// <summary>
/// STRICT dynamic-collapse options for advanced profile. Defaults stripped; the SDK never fills
/// these in. Pair with <see cref="FrameSessionExtensions.SolveDynCollapseAdvancedAsync"/>.
/// </summary>
public sealed record AdvancedDynCollapseOptions
{
    public required double Dt { get; init; }
    public required double MaxTime { get; init; }
    public required int BasisSize { get; init; }
    public required bool UseRitzVectors { get; init; }
    public required double RayleighAlpha { get; init; }
    public required double RayleighBeta { get; init; }
    public required double RemoveThreshold { get; init; }
    public required int ScreenEvery { get; init; }
    public required double QuietKineticRatio { get; init; }
    public required int MaxEvents { get; init; }
    public required int FrameStride { get; init; }
    public required IReadOnlyList<int> InitialRemovals { get; init; }
    public required IReadOnlyList<int> InitialShellRemovals { get; init; }
    /// <summary>Advanced default = true. Wire flag forces <c>dyn_collapse.full_frames</c> capability.</summary>
    public required bool BinaryFrames { get; init; }
    /// <summary>Advanced default = true. Stream fragment cluster mass / inertia / vel / angVel.</summary>
    public required bool FragmentDetail { get; init; }
}
