// FrameResults.cs -- strongly typed result POCOs.
//
// FORWARD-COMPATIBILITY CONTRACT
//   Every result type carries a RawHeader (JsonElement) field. Anything the server emits that
//   this SDK version doesn't recognise is still accessible through RawHeader.GetProperty(...).
//   When the engine adds a new field to LinearResult, an OLD GH component using this SDK keeps
//   working (it just ignores the new field); when a NEW GH component built against a newer
//   SDK runs against an OLD engine, the missing field is signalled by HasCapability() rather
//   than by an exception inside Result construction.

using System.Text.Json;

namespace FrameCore.Bridge.Result;

/// <summary>Base for every typed result. Holds the original JSON header for forward-compat.</summary>
public abstract class FrameResultBase
{
    /// <summary>Request id this result was bound to.</summary>
    public required string ReqId { get; init; }

    /// <summary>Full server response header (cloned). Read unknown fields here.</summary>
    public required JsonElement RawHeader { get; init; }

    /// <summary>Raw binary payload (e.g. packed double array for u/reactions). Empty when none.</summary>
    public ReadOnlyMemory<byte> BinaryPayload { get; init; } = ReadOnlyMemory<byte>.Empty;

    /// <summary>Engine emitted singular flag (mechanism / instability).</summary>
    public bool Singular { get; init; }

    /// <summary>Engine diagnostic string (empty when no diagnostic).</summary>
    public string Diagnostic { get; init; } = "";

    /// <summary>Engine pivotMargin (criticality / proximity-to-singular indicator).</summary>
    public double PivotMargin { get; init; }

    /// <summary>
    /// Per-stage / per-analysis diagnostic payload. <c>null</c> when the session was opened
    /// with <see cref="BridgeProfile.Simple"/>; populated when opened with
    /// <see cref="BridgeProfile.Advanced"/>. See <see cref="AdvancedDiagnostics"/>.
    /// </summary>
    public AdvancedDiagnostics? AdvancedDiagnostics { get; init; }

    /// <summary>
    /// Names of options the SIMPLE profile silently filled-in with engine defaults
    /// (e.g. <c>materials[0].cap</c>, <c>options.warpTolerance</c>). <c>null</c> when no
    /// silent default was applied OR when the session is in advanced profile (advanced
    /// rejects rather than fills). Read this when debugging "why did UE5 mode return a
    /// different value than I configured?".
    /// </summary>
    public IReadOnlyList<string>? DefaultsApplied { get; init; }
}

/// <summary>Member end forces in LOCAL coordinates ([Ni Vyi Vzi Ti Myi Mzi] at each end).</summary>
public readonly record struct MemberEndPair(
    double Ni, double Vyi, double Vzi, double Ti, double Myi, double Mzi,
    double Nj, double Vyj, double Vzj, double Tj, double Myj, double Mzj);

/// <summary>Shell facet stress resultants (centre values).</summary>
public readonly record struct ShellForce(
    double Mxx, double Myy, double Mxy, double Qx, double Qy,
    double Nxx, double Nyy, double Nxy);

/// <summary>Native elastic-screen D/C for a member, as emitted by solve.linear wantDC.</summary>
public readonly record struct MemberUtilization(
    double EndI, string ModeI,
    double EndJ, string ModeJ,
    double Peak, string GoverningEnd, string GoverningMode);

/// <summary>Native shell surface-stress D/C for one shell facet.</summary>
public readonly record struct ShellUtilization(double DC, int Corner, bool Top);

/// <summary>Linear static result (also the base shape of P-Delta, ReSolve, Corotational outputs).</summary>
public sealed class LinearResult : FrameResultBase
{
    /// <summary>Nodal displacements keyed by node id. Each array is [Ux..Rz], length 6.</summary>
    public required IReadOnlyDictionary<int, double[]> Disp { get; init; }
    /// <summary>Nodal reactions keyed by node id. Each array is [Fx..Mz], length 6, zero on free DOFs.</summary>
    public required IReadOnlyDictionary<int, double[]> Reactions { get; init; }
    /// <summary>Member end forces keyed by member id (local frame).</summary>
    public required IReadOnlyDictionary<int, MemberEndPair> MemberForces { get; init; }
    /// <summary>Shell forces keyed by shell id (centre, local frame).</summary>
    public required IReadOnlyDictionary<int, ShellForce> ShellForces { get; init; }
    /// <summary>Member D/C keyed by member id. Empty when the server did not emit wantDC data.</summary>
    public IReadOnlyDictionary<int, MemberUtilization> MemberUtilization { get; init; }
        = new Dictionary<int, MemberUtilization>();
    /// <summary>Shell D/C keyed by shell id. Empty when the server did not emit wantDC data.</summary>
    public IReadOnlyDictionary<int, ShellUtilization> ShellUtilization { get; init; }
        = new Dictionary<int, ShellUtilization>();
}

public sealed class TensionOnlyResult : FrameResultBase
{
    public bool Converged   { get; init; }
    public bool Cycled      { get; init; }
    public int  Iterations  { get; init; }
    /// <summary>Member ids left slack at convergence.</summary>
    public required IReadOnlyList<int> Slack { get; init; }
    /// <summary>Final state (same shape as <see cref="LinearResult"/>).</summary>
    public required LinearResult Final { get; init; }
}

public sealed class SizeOptResult : FrameResultBase
{
    public bool Converged { get; init; }
    public int  Iterations { get; init; }
    public bool SizeOptSingular { get; init; }
    /// <summary>member id -> (final area, D/C ratio).</summary>
    public required IReadOnlyDictionary<int, (double Area, double DC)> Areas { get; init; }
    /// <summary>Material volume sum(A * L) in mm^3.</summary>
    public double WeightVolume { get; init; }
}

public enum DynCollapseOutcome { Stable = 0, Collapsed = 1, MaxSteps = 2, Invalid = 3 }

public sealed class DynCollapseEvent
{
    public double Time { get; init; }
    public string Mode { get; init; } = "";   // FailMode enum string ("Brittle", "Hinge", "None", etc.)
    public IReadOnlyList<int> RemovedMembers { get; init; } = Array.Empty<int>();
    public IReadOnlyList<int> RemovedShells  { get; init; } = Array.Empty<int>();
    public IReadOnlyList<int> DetachedFragmentSizes { get; init; } = Array.Empty<int>();
}

public sealed class DynCollapseFrame
{
    public double Time { get; init; }
    public double MaxAbsU { get; init; }
    /// <summary>Optional packed u (6N doubles); empty when streamFrames=false / binaryFrames=false.</summary>
    public ReadOnlyMemory<byte> UPayload { get; init; }
    public ReadOnlyMemory<byte> VPayload { get; init; }
}

public sealed class DynCollapseResult : FrameResultBase
{
    public DynCollapseOutcome Outcome { get; init; }
    public int NEvents { get; init; }
    public int NFrames { get; init; }
    public double EndTime { get; init; }
    public IReadOnlyList<DynCollapseEvent> Events { get; init; } = Array.Empty<DynCollapseEvent>();
    public IReadOnlyList<DynCollapseFrame> Frames { get; init; } = Array.Empty<DynCollapseFrame>();
}

public sealed class CorotationalResult : FrameResultBase
{
    public bool Converged { get; init; }
    public bool Diverged  { get; init; }
    public int LoadStepsCompleted { get; init; }
    public int TotalIterations { get; init; }
    public required LinearResult Final { get; init; }
}

public sealed class ArcLengthResult : FrameResultBase
{
    public bool Converged { get; init; }
    public bool Diverged  { get; init; }
    public int  NSteps    { get; init; }
    public double LambdaPeak { get; init; }
    public required IReadOnlyList<(int Index, double Lambda, double Disp)> Path { get; init; }
    public required LinearResult Final { get; init; }
}

public sealed class ModalResult : FrameResultBase
{
    /// <summary>Angular frequencies (rad/s).</summary>
    public required IReadOnlyList<double> Omegas { get; init; }
    /// <summary>Mode shapes, packed as 6N doubles per mode in BinaryPayload (when binaryModes=true).</summary>
    public int ModeCount { get; init; }
}

public sealed class BucklingResult : FrameResultBase
{
    /// <summary>Buckling load factors lambda_i.</summary>
    public required IReadOnlyList<double> Lambdas { get; init; }
    public int ModeCount { get; init; }
}

public sealed class Progress
{
    public required double Pct { get; init; }     // 0..1
    public string Stage { get; init; } = "";
    public string Note  { get; init; } = "";
}
