// FrameSessionExtensions.cs -- the user-facing high-level API. Wraps FrameSession's raw
// SendAndAwait / SendAndStream with typed Model and Result. Lives in a partial / extension
// layer so we can grow this surface without touching the dispatcher.
//
// EVERY method in this file is the "natural" call site a GH component author writes. None of
// these signatures should change between v2.0 and v2.x (forward-compat SLA); new methods
// arrive only as ADDITIONS.

using System.Text.Json;
using FrameCore.Bridge.Model;
using FrameCore.Bridge.Result;

namespace FrameCore.Bridge;

public static class FrameSessionExtensions
{
    // ----- Session lifecycle ------------------------------------------------------------------

    /// <summary>
    /// Open an engine-side session in SIMPLE profile. Missing options take engine defaults
    /// (e.g. material cap = 300/300/180 MPa, supernodal SPD failure -&gt; silent LDLT fallback).
    /// Requires the parent <see cref="FrameSession"/> was opened with
    /// <see cref="FrameSession.OpenSimpleAsync"/>; throws otherwise.
    /// </summary>
    public static async Task<string> OpenEngineSessionAsync(this FrameSession s,
                                                            EngineSessionOptions? opts = null,
                                                            CancellationToken ct = default)
    {
        if (s.Profile != BridgeProfile.Simple)
            throw new InvalidOperationException(
                "OpenEngineSessionAsync is the simple-profile entry; current session is advanced. " +
                "Use OpenEngineSessionAdvancedAsync(AdvancedEngineSessionOptions) instead.");

        opts ??= new EngineSessionOptions();
        var id = s.NextId();
        var body = new
        {
            mode = opts.Mode.ToString().ToLowerInvariant(),
            options = new
            {
                pivotTol                  = opts.PivotTol,
                enableReleases            = opts.EnableReleases,
                useTimoshenko             = opts.UseTimoshenko,
                useIncompatibleMembrane   = opts.UseIncompatibleMembrane,
                useDKQPlate               = opts.UseDKQPlate,
                shellGeometricStiffness   = opts.ShellGeometricStiffness,
                useWarpingCorrection      = opts.UseWarpingCorrection,
                warpTolerance             = opts.WarpTolerance,
                shellCurvatureMaxAngleDeg = opts.ShellCurvatureMaxAngleDeg,
                useSupernodalPrimary      = opts.UseSupernodalPrimary
            },
            snSession = opts.Mode == EngineSessionMode.Supernodal ? new
            {
                irSteps      = opts.SnIrSteps,
                amalgMaxCol  = opts.SnAmalgMaxCol,
                numThreads   = opts.SnNumThreads
            } : (object?)null
        };
        using var rsp = await s.SendAndAwaitSingleAsync("session.open", id, body, ct).ConfigureAwait(false);
        return rsp.Header.RootElement.GetProperty("body").GetProperty("session").GetString()!;
    }

    /// <summary>
    /// Open an engine-side session in ADVANCED profile. Every option is required; silent
    /// fallbacks are off; <c>diagnosticStream</c> opt-in pushes per-stage <c>diagnostic</c>
    /// events alongside the response. Requires the parent <see cref="FrameSession"/> was opened
    /// with <see cref="FrameSession.OpenAdvancedAsync"/>; throws otherwise.
    /// </summary>
    public static async Task<string> OpenEngineSessionAdvancedAsync(this FrameSession s,
                                                                     AdvancedEngineSessionOptions opts,
                                                                     CancellationToken ct = default)
    {
        if (s.Profile != BridgeProfile.Advanced)
            throw new InvalidOperationException(
                "OpenEngineSessionAdvancedAsync requires a session opened via FrameSession.OpenAdvancedAsync.");
        if (opts is null) throw new ArgumentNullException(nameof(opts));
        if (opts.DiagnosticStream && !s.HasCapability("diagnostic.stream"))
            throw new NotSupportedException("engine does not advertise 'diagnostic.stream'");

        var id = s.NextId();
        var body = new
        {
            mode             = opts.Mode.ToString().ToLowerInvariant(),
            profile          = "advanced",
            diagnosticStream = opts.DiagnosticStream,
            options = new
            {
                pivotTol                  = opts.PivotTol,
                enableReleases            = opts.EnableReleases,
                useTimoshenko             = opts.UseTimoshenko,
                useIncompatibleMembrane   = opts.UseIncompatibleMembrane,
                useDKQPlate               = opts.UseDKQPlate,
                shellGeometricStiffness   = opts.ShellGeometricStiffness,
                useWarpingCorrection      = opts.UseWarpingCorrection,
                warpTolerance             = opts.WarpTolerance,
                shellCurvatureMaxAngleDeg = opts.ShellCurvatureMaxAngleDeg,
                useSupernodalPrimary      = opts.UseSupernodalPrimary
            },
            snSession = new
            {
                irSteps        = opts.SnIrSteps,
                amalgMaxCol    = opts.SnAmalgMaxCol,
                numThreads     = opts.SnNumThreads,
                fallbackOnFail = opts.SnFallbackOnFail
            }
        };
        using var rsp = await s.SendAndAwaitSingleAsync("session.open", id, body, ct).ConfigureAwait(false);
        return rsp.Header.RootElement.GetProperty("body").GetProperty("session").GetString()!;
    }

    public static async Task CloseEngineSessionAsync(this FrameSession s, string sessionId, CancellationToken ct = default)
    {
        var id = s.NextId();
        using var _ = await s.SendAndAwaitSingleAsync("session.close", id, new { session = sessionId }, ct).ConfigureAwait(false);
    }

    // ----- Model + solve ----------------------------------------------------------------------

    /// <summary>Send a full model to the engine; returns dofCount as a sanity check.</summary>
    public static async Task<int> SetModelAsync(this FrameSession s, string sessionId,
                                                 FrameModel model, CancellationToken ct = default)
    {
        var id = s.NextId();
        var body = ModelToJson(sessionId, model);
        using var rsp = await s.SendAndAwaitSingleAsync("model.set", id, body, ct).ConfigureAwait(false);
        return rsp.Header.RootElement.GetProperty("body").GetProperty("dofCount").GetInt32();
    }

    /// <summary>Linear static solve.</summary>
    public static async Task<LinearResult> SolveLinearAsync(this FrameSession s, string sessionId,
                                                             bool wantReactions = true,
                                                             CancellationToken ct = default)
    {
        if (!s.HasCapability("solve.linear"))
            throw new NotSupportedException("engine does not advertise 'solve.linear'");
        var id = s.NextId();
        var body = new { session = sessionId, wantReactions, binaryDisp = false };
        using var rsp = await s.SendAndAwaitSingleAsync("solve.linear", id, body, ct).ConfigureAwait(false);
        return ParseLinearResult(id, rsp);
    }

    /// <summary>Tension-only active-set solve. Engine must advertise <c>solve.tension_only</c>.</summary>
    public static async Task<TensionOnlyResult> SolveTensionOnlyAsync(this FrameSession s,
                                                                       string sessionId,
                                                                       int maxIter = 32,
                                                                       bool allowReactivation = true,
                                                                       CancellationToken ct = default)
    {
        if (!s.HasCapability("solve.tension_only"))
            throw new NotSupportedException("engine does not advertise 'solve.tension_only'");
        var id = s.NextId();
        var body = new { session = sessionId, maxIter, allowReactivation };
        using var rsp = await s.SendAndAwaitSingleAsync("solve.tension_only", id, body, ct).ConfigureAwait(false);
        var b = rsp.Header.RootElement.GetProperty("body");

        var inner = ParseLinearResult(id, rsp);
        return new TensionOnlyResult
        {
            ReqId        = id,
            RawHeader    = rsp.Header.RootElement.Clone(),
            Singular     = b.GetProperty("singular").GetBoolean(),
            Diagnostic   = TryGetString(b, "diagnostic") ?? "",
            PivotMargin  = inner.PivotMargin,
            Converged    = b.GetProperty("converged").GetBoolean(),
            Cycled       = b.GetProperty("cycled").GetBoolean(),
            Iterations   = b.GetProperty("iterations").GetInt32(),
            Slack        = ReadIntArray(b, "slack"),
            Final        = inner
        };
    }

    /// <summary>FSD size optimisation. Engine must advertise <c>solve.size_opt</c>.</summary>
    public static async Task<SizeOptResult> SolveSizeOptAsync(this FrameSession s, string sessionId,
                                                                double aMin, int maxIter = 40, double dcTol = 1e-8,
                                                                CancellationToken ct = default)
    {
        if (!s.HasCapability("solve.size_opt"))
            throw new NotSupportedException("engine does not advertise 'solve.size_opt'");
        var id = s.NextId();
        var body = new { session = sessionId, aMin, maxIter, dcTol };
        using var rsp = await s.SendAndAwaitSingleAsync("solve.size_opt", id, body, ct).ConfigureAwait(false);
        var b = rsp.Header.RootElement.GetProperty("body");
        var areas = new Dictionary<int, (double Area, double DC)>();
        foreach (var e in b.GetProperty("areas").EnumerateObject())
            areas[int.Parse(e.Name)] = (e.Value[0].GetDouble(), e.Value[1].GetDouble());
        return new SizeOptResult
        {
            ReqId          = id,
            RawHeader      = rsp.Header.RootElement.Clone(),
            Singular       = b.GetProperty("singular").GetBoolean(),
            Diagnostic     = TryGetString(b, "diagnostic") ?? "",
            Converged      = b.GetProperty("converged").GetBoolean(),
            Iterations     = b.GetProperty("iterations").GetInt32(),
            SizeOptSingular= b.TryGetProperty("sizeOptSingular", out var so) && so.GetBoolean(),
            Areas          = areas,
            WeightVolume   = b.GetProperty("weightVolume").GetDouble()
        };
    }

    /// <summary>
    /// Dynamic collapse, streaming. Events / frames arrive as you await over the enumerator;
    /// the final summary is returned alongside the stream.
    /// </summary>
    public static async IAsyncEnumerable<object> StreamDynCollapseAsync(this FrameSession s,
                                                                         string sessionId,
                                                                         double dt, double maxTime,
                                                                         IEnumerable<int>? initialRemovals = null,
                                                                         bool binaryFrames = true,
                                                                         [System.Runtime.CompilerServices.EnumeratorCancellation] CancellationToken ct = default)
    {
        if (!s.HasCapability("solve.dyn_collapse"))
            throw new NotSupportedException("engine does not advertise 'solve.dyn_collapse'");
        var id = s.NextId();
        var body = new
        {
            session = sessionId, dt, maxTime,
            initialRemovals = initialRemovals?.ToArray() ?? Array.Empty<int>(),
            streamFrames    = true,
            binaryFrames
        };
        await foreach (var f in s.SendAndStreamAsync("solve.dyn_collapse", id, body, ct).ConfigureAwait(false))
        {
            // Caller sees raw WireFrames; the GH component layer maps them to DynCollapseEvent /
            // DynCollapseFrame / DynCollapseResult depending on the header `kind` + body fields.
            yield return f;
        }
    }

    /// <summary>Inspect (partial read) the displacements of a few specific nodes.</summary>
    public static async Task<IReadOnlyDictionary<int, double[]>> InspectDispAsync(
            this FrameSession s, string sessionId, IEnumerable<int> nodeIds, CancellationToken ct = default)
    {
        if (!s.HasCapability("inspect.disp"))
            throw new NotSupportedException("engine does not advertise 'inspect.disp'");
        var id = s.NextId();
        var body = new { session = sessionId, nodes = nodeIds.ToArray() };
        using var rsp = await s.SendAndAwaitSingleAsync("inspect.disp", id, body, ct).ConfigureAwait(false);
        var disp = new Dictionary<int, double[]>();
        foreach (var e in rsp.Header.RootElement.GetProperty("body").GetProperty("disp").EnumerateObject())
        {
            var arr = new double[6];
            int k = 0;
            foreach (var v in e.Value.EnumerateArray()) arr[k++] = v.GetDouble();
            disp[int.Parse(e.Name)] = arr;
        }
        return disp;
    }

    // ----- helpers ----------------------------------------------------------------------------

    private static LinearResult ParseLinearResult(string id, WireFrame rsp)
    {
        var b = rsp.Header.RootElement.GetProperty("body");
        if (b.TryGetProperty("_stub", out var stub) && stub.ValueKind == JsonValueKind.True)
            throw new RemoteException("NOT_IMPLEMENTED",
                "solve.linear returned a B2 stub payload; engine-backed solve.linear is not wired in this DLL.");
        var disp      = new Dictionary<int, double[]>();
        var reactions = new Dictionary<int, double[]>();
        var mf        = new Dictionary<int, MemberEndPair>();
        var sf        = new Dictionary<int, ShellForce>();

        if (b.TryGetProperty("disp", out var dispObj) && dispObj.ValueKind == JsonValueKind.Object)
        {
            foreach (var e in dispObj.EnumerateObject())
                disp[int.Parse(e.Name)] = ReadDoubleArray(e.Value);
        }
        if (b.TryGetProperty("reactions", out var rxObj) && rxObj.ValueKind == JsonValueKind.Object)
        {
            foreach (var e in rxObj.EnumerateObject())
                reactions[int.Parse(e.Name)] = ReadDoubleArray(e.Value);
        }
        if (b.TryGetProperty("memberForces", out var mfObj))
        {
            foreach (var e in mfObj.EnumerateObject())
            {
                var a = ReadDoubleArray(e.Value);
                mf[int.Parse(e.Name)] = new MemberEndPair(
                    a[0], a[1], a[2], a[3], a[4], a[5],
                    a[6], a[7], a[8], a[9], a[10], a[11]);
            }
        }
        if (b.TryGetProperty("shellForces", out var sfObj))
        {
            foreach (var e in sfObj.EnumerateObject())
            {
                var a = ReadDoubleArray(e.Value);
                sf[int.Parse(e.Name)] = new ShellForce(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
            }
        }

        return new LinearResult
        {
            ReqId               = id,
            RawHeader           = rsp.Header.RootElement.Clone(),
            BinaryPayload       = rsp.Payload,
            Singular            = b.TryGetProperty("singular", out var sg) && sg.GetBoolean(),
            Diagnostic          = TryGetString(b, "diagnostic") ?? "",
            PivotMargin         = b.TryGetProperty("pivotMargin", out var pm) ? pm.GetDouble() : 0.0,
            Disp                = disp,
            Reactions           = reactions,
            MemberForces        = mf,
            ShellForces         = sf,
            AdvancedDiagnostics = ParseAdvancedDiagnostics(b),
            DefaultsApplied     = ParseDefaultsApplied(b)
        };
    }

    /// <summary>
    /// Reads <c>body.advancedDiagnostics</c> into the typed payload. Returns null when absent
    /// (simple profile). Tolerant of missing sub-fields -- unknown future fields are still
    /// readable via <c>RawHeader</c>.
    /// </summary>
    private static AdvancedDiagnostics? ParseAdvancedDiagnostics(JsonElement body)
    {
        if (!body.TryGetProperty("advancedDiagnostics", out var d) || d.ValueKind != JsonValueKind.Object)
            return null;

        var diag = new AdvancedDiagnostics
        {
            FactorMethod        = TryGetString(d, "factorMethod"),
            FactorBackend       = TryGetString(d, "factorBackend"),
            FactorTimeMs        = TryGetDouble(d, "factorTimeMs"),
            SolveTimeMs         = TryGetDouble(d, "solveTimeMs"),
            PivotMarginTrace    = TryGetDoubleArray(d, "pivotMarginTrace"),
            SnIrResidualHistory = TryGetDoubleArray(d, "snIrResidualHistory"),
            GuardsDisabled      = TryGetStringArray(d, "guardsDisabled"),
            // Per-analysis sub-traces parsed on demand by their respective extension methods to
            // avoid bloating this hot path. Reach through RawHeader for now; B3 wires them in.
        };
        return diag;
    }

    private static IReadOnlyList<string>? ParseDefaultsApplied(JsonElement body)
        => body.TryGetProperty("defaultsApplied", out var el) && el.ValueKind == JsonValueKind.Array
            ? TryGetStringArray(body, "defaultsApplied")
            : null;

    private static double? TryGetDouble(JsonElement parent, string name)
        => parent.TryGetProperty(name, out var el) && el.ValueKind == JsonValueKind.Number ? el.GetDouble() : null;

    private static IReadOnlyList<double>? TryGetDoubleArray(JsonElement parent, string name)
    {
        if (!parent.TryGetProperty(name, out var el) || el.ValueKind != JsonValueKind.Array) return null;
        var list = new List<double>();
        foreach (var v in el.EnumerateArray()) list.Add(v.GetDouble());
        return list;
    }

    private static IReadOnlyList<string>? TryGetStringArray(JsonElement parent, string name)
    {
        if (!parent.TryGetProperty(name, out var el) || el.ValueKind != JsonValueKind.Array) return null;
        var list = new List<string>();
        foreach (var v in el.EnumerateArray())
        {
            var s = v.GetString();
            if (s is not null) list.Add(s);
        }
        return list;
    }

    private static double[] ReadDoubleArray(JsonElement el)
    {
        var list = new List<double>();
        foreach (var v in el.EnumerateArray()) list.Add(v.GetDouble());
        return list.ToArray();
    }
    private static IReadOnlyList<int> ReadIntArray(JsonElement parent, string name)
    {
        if (!parent.TryGetProperty(name, out var el) || el.ValueKind != JsonValueKind.Array) return Array.Empty<int>();
        var list = new List<int>();
        foreach (var v in el.EnumerateArray()) list.Add(v.GetInt32());
        return list;
    }
    private static string? TryGetString(JsonElement parent, string name)
        => parent.TryGetProperty(name, out var el) && el.ValueKind == JsonValueKind.String ? el.GetString() : null;

    private static object ModelToJson(string sessionId, FrameModel m)
    {
        return new
        {
            session = sessionId,
            materials = m.Materials.Select(mat => new
            {
                E = mat.E, G = mat.G, rho = mat.Rho, nu = mat.Nu,
                cap = mat.Cap is null ? null : (object)new { comp = mat.Cap.Value.Comp, tens = mat.Cap.Value.Tens, shear = mat.Cap.Value.Shear }
            }).ToArray(),
            sections = m.Sections.Select(s => new
            {
                A = s.A, Iy = s.Iy, Iz = s.Iz, J = s.J,
                cy = s.Cy, cz = s.Cz, Asy = s.Asy, Asz = s.Asz
            }).ToArray(),
            nodes = m.Nodes.Select(n => new
            {
                id = n.Id, x = n.X, y = n.Y, z = n.Z,
                @fixed = n.Fixed.Select(b => b ? 1 : 0).ToArray(),
                prescribed = n.Prescribed
            }).ToArray(),
            members = m.Members.Select(mem => new
            {
                id = mem.Id, i = mem.I.Id, j = mem.J.Id,
                mat = mem.Material.Index, sec = mem.Section.Index,
                refVec = new[] { mem.RefVec.X, mem.RefVec.Y, mem.RefVec.Z },
                active = mem.Active, tensionOnly = mem.TensionOnly,
                release = mem.Release
            }).ToArray(),
            shells = m.Shells.Select(s => new
            {
                id = s.Id,
                n = new[] { s.N0.Id, s.N1.Id, s.N2.Id, s.N3.Id },
                mat = s.Material.Index,
                t = s.Thickness,
                active = s.Active
            }).ToArray(),
            nodalLoads = m.NodalLoads.Select(l => new { node = l.Node.Id, comp = l.Components }).ToArray(),
            memberUdls = m.MemberUdls.Select(u => new { member = u.Member.Id, w = new[] { u.Local.Wx, u.Local.Wy, u.Local.Wz } }).ToArray(),
            shellPressures = m.ShellPressures.Select(s => new { shell = s.Shell.Id, p = s.P }).ToArray(),
            hinges = m.Hinges.Select(h => new { member = h.Member.Id, dof = h.Dof, mp = h.Mp }).ToArray()
        };
    }
}

public enum EngineSessionMode { Default, Supernodal, Resolve }

public sealed record EngineSessionOptions
{
    public EngineSessionMode Mode { get; init; } = EngineSessionMode.Default;
    public double PivotTol { get; init; } = 1e-12;
    public bool EnableReleases { get; init; }
    public bool UseTimoshenko { get; init; }
    public bool UseIncompatibleMembrane { get; init; }
    public bool UseDKQPlate { get; init; }
    public bool ShellGeometricStiffness { get; init; }
    public bool UseWarpingCorrection { get; init; }
    public double WarpTolerance { get; init; } = 1e-6;
    public double ShellCurvatureMaxAngleDeg { get; init; }
    public bool UseSupernodalPrimary { get; init; }

    public int SnIrSteps { get; init; }
    public int SnAmalgMaxCol { get; init; } = 64;
    public int SnNumThreads { get; init; }
}
