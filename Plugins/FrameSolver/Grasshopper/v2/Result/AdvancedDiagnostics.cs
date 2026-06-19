// AdvancedDiagnostics.cs -- per-result diagnostic payload exposed in ADVANCED profile only.
//
// In SIMPLE profile every result's AdvancedDiagnostics property is null. In ADVANCED profile
// it carries the silent-path traces the engine would otherwise have hidden:
//
//   * which factor backend ran (LDLT / SelfBuiltSupernodal / CHOLMODOracle), and when
//   * pivot-margin trace per stage
//   * SnSession iterative-refinement residual history
//   * which guards (e.g. shell curvature) were disabled
//   * near-singular mechanism candidates
//   * per-analysis traces (tension-only iteration trace, size-opt convergence history,
//     reanalysis tier ladder, ...)
//
// Forward-compat rule: ALL fields are nullable, ALL collections may be empty. A NEW engine field
// older SDKs don't know about is still readable through FrameResultBase.RawHeader. A NEW SDK
// field talking to an OLDER engine just stays null. No exception on either side.

namespace FrameCore.Bridge.Result;

/// <summary>Per-result diagnostic payload. Null in simple profile; populated in advanced profile.</summary>
public sealed class AdvancedDiagnostics
{
    /// <summary>Factor method actually used ("LDLT" or "SupernodalPrimary").</summary>
    public string? FactorMethod { get; init; }

    /// <summary>Factor backend ("SimplicialLDLT" / "SelfBuiltSupernodal" / "CHOLMODOracle").</summary>
    public string? FactorBackend { get; init; }

    /// <summary>Assemble + factor wall-clock in ms.</summary>
    public double? FactorTimeMs { get; init; }

    /// <summary>Forward/back-substitution wall-clock in ms.</summary>
    public double? SolveTimeMs { get; init; }

    /// <summary>|D|_min / |D|_max per recorded stage during assemble+factor.</summary>
    public IReadOnlyList<double>? PivotMarginTrace { get; init; }

    /// <summary>SnSession Neumaier iterative-refinement residual per step (mode=supernodal).</summary>
    public IReadOnlyList<double>? SnIrResidualHistory { get; init; }

    /// <summary>
    /// Names of guards the user explicitly turned off (e.g. <c>shellCurvature</c>). Listed even
    /// when the model would have passed -- the point is that the user opted into the silent
    /// behaviour, not the engine.
    /// </summary>
    public IReadOnlyList<string>? GuardsDisabled { get; init; }

    /// <summary>Sorted near-singular DOF candidates (only when pivotMargin &lt; 10x pivotTol).</summary>
    public IReadOnlyList<MechanismCandidate>? MechanismCandidates { get; init; }

    // ----- per-analysis sub-traces (any may be null depending on which method ran) ------------

    public TensionOnlyTrace?     TensionOnly     { get; init; }
    public SizeOptTrace?         SizeOpt         { get; init; }
    public DynCollapseTrace?     DynCollapse     { get; init; }
    public CorotationalTrace?    Corotational    { get; init; }
    public ArcLengthTrace?       ArcLength       { get; init; }
    public ModalTrace?           Modal           { get; init; }
    public BucklingTrace?        Buckling        { get; init; }
    public ReanalysisTrace?      Reanalysis      { get; init; }
}

public sealed record MechanismCandidate(int NodeId, int Dof, double NormalizedPivot);

public sealed class TensionOnlyTrace
{
    /// <summary>Slack-set delta per iteration: each entry is the set of member ids that flipped.</summary>
    public IReadOnlyList<IReadOnlyList<int>> IterationDeltas { get; init; } = Array.Empty<IReadOnlyList<int>>();
    public IReadOnlyList<int> ReactivationHistory { get; init; } = Array.Empty<int>();
}

public sealed class SizeOptTrace
{
    /// <summary>D/C max per iteration.</summary>
    public IReadOnlyList<double> ConvergenceHistory { get; init; } = Array.Empty<double>();
    /// <summary>Members the engine treated as zero-force (Amin not applied).</summary>
    public IReadOnlyList<int> ZeroForceMembers { get; init; } = Array.Empty<int>();
    /// <summary>materialIdx -> (cap.comp, cap.tens, cap.shear) actually applied.</summary>
    public IReadOnlyDictionary<int, (double Comp, double Tens, double Shear)>? MaterialCapsApplied { get; init; }
}

public sealed class DynCollapseTrace
{
    public int? RitzBasisSize { get; init; }
    public double? RitzResidual { get; init; }
    /// <summary>(t, energyBefore, energyAfter, dissipated) per event.</summary>
    public IReadOnlyList<(double T, double Before, double After, double Dissipated)>? EnergyTrace { get; init; }
    /// <summary>Per-event basis-inheritance projection residual.</summary>
    public IReadOnlyList<double>? BasisInheritanceTruncationResidual { get; init; }
    /// <summary>Detailed fragment cluster info -- only present when dyn_collapse.fragment_detail capability is on.</summary>
    public IReadOnlyList<FragmentClusterDetail>? FragmentClusters { get; init; }
}

public sealed record FragmentClusterDetail(
    double Mass,
    double[] InertiaTensor,    // 3x3 row-major
    double[] LinearVelocity,   // length 3
    double[] AngularVelocity,  // length 3
    IReadOnlyList<int> NodeIds);

public sealed class CorotationalTrace
{
    public IReadOnlyList<double> NewtonRaphsonResidualHistory { get; init; } = Array.Empty<double>();
    public IReadOnlyList<double> TangentStiffnessConditioning { get; init; } = Array.Empty<double>();
}

public sealed class ArcLengthTrace
{
    public IReadOnlyList<double> PredictorDirection { get; init; } = Array.Empty<double>();
    public IReadOnlyList<double> CorrectorResidualHistory { get; init; } = Array.Empty<double>();
}

public sealed class ModalTrace
{
    public double? RitzTruncationResidual { get; init; }
    public int? SubspaceIterations { get; init; }
}

public sealed class BucklingTrace
{
    public string? LambdaShiftStrategy { get; init; }
    public int? SubspaceIterations { get; init; }
}

public sealed class ReanalysisTrace
{
    /// <summary>Tier taken per solve in the session ladder (1/2/3).</summary>
    public IReadOnlyList<int> TierLadder { get; init; } = Array.Empty<int>();
    public int? PcgIterations { get; init; }
    public double? PcgRelativeResidual { get; init; }
    public int? WoodburyRankUsage { get; init; }
}
