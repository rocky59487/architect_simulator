// AssembleModelComponent.cs -- Tab 7 anchor. Reads Geometry / Material / Section / Load /
// Boundary, builds a FrameModel via the SDK builder, opens an engine session, and pushes the
// model to the engine via model.set. Output is an EngineSession id that downstream Solve /
// Modal / PDelta / DynCollapse components reuse.
//
// FACTOR REUSE -- by keeping one engine session alive across SolveInstance calls (slider drag
// re-runs SolveInstance but the engine session stays put), the engine's PreparedSystem stays
// factored and a subsequent Solve runs forward-back substitution only.
//
// P1.1 fix: previous version used `async void SolveInstance` and wrote to IGH_DataAccess after
// the await. This rewrite uses the cache + ExpireSolution two-pass pattern matching
// OpenFrameCoreComponent and AsyncFrameComponent.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Threading;
using System.Threading.Tasks;
using FrameCore.Bridge;
using FrameCore.Bridge.Model;
using FrameCore.Gh.Common;
using Grasshopper.Kernel;
using Rhino.Geometry;

namespace FrameCore.Gh.Components.Analyze
{
    public sealed class AssembleModelComponent : GH_Component
    {
        public AssembleModelComponent()
            : base("Assemble Model", "Assemble",
                   "Push a built model to the engine. Returns an EngineSession id that downstream " +
                   "solvers reuse — the engine's factor stays valid across slider drags.",
                   Tabs.Category, Tabs.Analyze) { }

        public override Guid ComponentGuid => new("D1F4C1AA-2A0B-4D3E-9C8E-FEE100000501");
        protected override Bitmap? Icon => Resources.LoadIcon("analyze-assemble");

        protected override void RegisterInputParams(GH_InputParamManager p)
        {
            p.AddParameter(new GhParameters.Param_FrameSession(), "Session", "S",
                "FrameCore session from Open FrameCore.", GH_ParamAccess.item);
            p.AddParameter(new GhParameters.Param_Material(), "Materials", "Mat", "Materials.", GH_ParamAccess.list);
            p.AddParameter(new GhParameters.Param_Section(),  "Sections",  "Sec", "Sections.",  GH_ParamAccess.list);
            p.AddPointParameter("Nodes", "N", "Node points (id = list index).", GH_ParamAccess.list);
            p.AddCurveParameter("Members", "M",
                "Member curves. Each curve's start/end snaps to the closest Node.",
                GH_ParamAccess.list);
        }

        protected override void RegisterOutputParams(GH_OutputParamManager p)
        {
            p.AddParameter(new GhParameters.Param_EngineSession(), "EngineSession", "ES",
                "Engine session id to reuse across solves.", GH_ParamAccess.item);
            p.AddIntegerParameter("DofCount", "n", "Engine-side dof count.", GH_ParamAccess.item);
        }

        private string?                 _engineSessionId;
        private int                     _dofCount;
        private CancellationTokenSource _cts = new();
        private Task?                   _assembling;
        private string                  _error = "";
        // Cache the input fingerprint so a slider drag that doesn't change anything skips work.
        private (int nMat, int nSec, int nNode, int nMem, int hash) _lastInputs;
        // P1.3 fix (third audit round): the engine-session id is meaningless without the
        // FrameSession it was opened against. If OpenFrameCore reconnects (Reset / DLL update
        // / profile switch) the upstream FrameSession identity changes but the model
        // fingerprint might NOT, so the cache would happily emit the OLD id against the NEW
        // FrameSession -> "unknown session" errors downstream. We pin the cache to the
        // FrameSession AND its profile/buildSha so any of those changes triggers a fresh
        // OpenEngineSession + model.set instead of a cache hit.
        private FrameSession?           _cachedFs;
        private string                  _cachedFsBuildSha = "";
        private BridgeProfile           _cachedFsProfile  = BridgeProfile.Simple;
        private long                    _assembleGeneration = 0;

        protected override void SolveInstance(IGH_DataAccess da)
        {
            // 1. Read inputs synchronously on the GH thread (DA is only valid here).
            GH_FrameSession? sessGoo = null;
            if (!da.GetData(0, ref sessGoo) || sessGoo?.Value is null) return;
            var fs = sessGoo.Value;

            var matList = new List<GH_Material>(); da.GetDataList(1, matList);
            var secList = new List<GH_Section>();  da.GetDataList(2, secList);
            var nodes   = new List<Point3d>();      da.GetDataList(3, nodes);
            var members = new List<Curve>();        da.GetDataList(4, members);

            // 2. Build the immutable model. This is fast and deterministic; we keep it on the
            //    GH thread so any builder validation errors are reported with the correct DA
            //    runtime-message wiring.
            FrameModel model;
            try
            {
                var builder = new FrameModelBuilder();
                var matRefs = new List<MaterialRef>();
                foreach (var m in matList) matRefs.Add(builder.AddMaterial(m.Value!));
                var secRefs = new List<SectionRef>();
                foreach (var s in secList) secRefs.Add(builder.AddSection(s.Value!));
                for (int i = 0; i < nodes.Count; ++i)
                    builder.AddFreeNode(i, nodes[i].X, nodes[i].Y, nodes[i].Z);
                for (int i = 0; i < members.Count; ++i)
                {
                    var c = members[i];
                    int iIdx = SnapToNode(c.PointAtStart, nodes);
                    int jIdx = SnapToNode(c.PointAtEnd,   nodes);
                    if (iIdx < 0 || jIdx < 0)
                    {
                        AddRuntimeMessage(GH_RuntimeMessageLevel.Error,
                            $"Member {i}: end points do not snap to any Node.");
                        return;
                    }
                    builder.AddMember(i, new NodeRef(iIdx), new NodeRef(jIdx),
                                       matRefs[i % System.Math.Max(1, matRefs.Count)],
                                       secRefs[i % System.Math.Max(1, secRefs.Count)]);
                }
                model = builder.Build();
            }
            catch (Exception ex)
            {
                AddRuntimeMessage(GH_RuntimeMessageLevel.Error, ex.Message);
                return;
            }

            // 3. P1.3 fix: detect upstream FrameSession change BEFORE consulting the cache. If
            //    the user reset OpenFrameCore or switched profile, _cachedFs no longer matches
            //    fs and the cached engineSessionId belongs to a dead FrameSession. Invalidate
            //    the cache so step 5 below opens a fresh engineSession on the NEW fs.
            bool fsChanged = !ReferenceEquals(_cachedFs, fs)
                          || !string.Equals(_cachedFsBuildSha, fs.EngineBuildSha, StringComparison.Ordinal)
                          || _cachedFsProfile != fs.Profile;
            if (fsChanged)
            {
                Interlocked.Increment(ref _assembleGeneration);
                _cts.Cancel();
                _assembling      = null;
                _engineSessionId = null;
                _lastInputs      = default;
                _error           = "";
                _cachedFs        = fs;
                _cachedFsBuildSha = fs.EngineBuildSha;
                _cachedFsProfile  = fs.Profile;
            }

            // 4. If we already have a cached engineSession AND the inputs are unchanged, write
            //    the cache straight back. Slider drag that does NOT move a node/member becomes
            //    a no-op here -- the engine factor stays valid.
            var fingerprint = ComputeFingerprint(matList, secList, nodes, members);
            if (_engineSessionId is not null
                && _assembling is null
                && _lastInputs.hash == fingerprint
                && _lastInputs.nMat  == matList.Count
                && _lastInputs.nSec  == secList.Count
                && _lastInputs.nNode == nodes.Count
                && _lastInputs.nMem  == members.Count
                && string.IsNullOrEmpty(_error))
            {
                da.SetData(0, new GH_EngineSession(_engineSessionId));
                da.SetData(1, _dofCount);
                Message = $"{nodes.Count}n {members.Count}m · {_dofCount} dof (cached)";
                return;
            }

            // 5. Surface the last error if assembly previously failed.
            if (!string.IsNullOrEmpty(_error))
            {
                AddRuntimeMessage(GH_RuntimeMessageLevel.Error, _error);
                Message = "error";
                return;
            }

            // 6. Pass A: kick off the async model.set; return without writing DA.
            if (_assembling is null)
            {
                _cts.Cancel(); _cts = new CancellationTokenSource();
                long thisGen = Interlocked.Increment(ref _assembleGeneration);
                _assembling = AssembleAsync(fs, model, thisGen,
                                             new InputsFingerprint(matList.Count, secList.Count,
                                                                    nodes.Count, members.Count, fingerprint),
                                             _cts.Token);
                Message = "assembling...";
                return;
            }

            // 7. While a new model.set is in flight, do not emit a stale engineSessionId from
            // a previous fingerprint. The next ExpireSolution from AssembleAsync will publish
            // the updated cache.
            if (_assembling is not null)
            {
                Message = "assembling...";
                return;
            }

            // 8. Pass B: cache is ready, write outputs.
            if (_engineSessionId is not null)
            {
                da.SetData(0, new GH_EngineSession(_engineSessionId));
                da.SetData(1, _dofCount);
                Message = $"{nodes.Count}n {members.Count}m · {_dofCount} dof";
            }
        }

        private async Task AssembleAsync(FrameSession fs, FrameModel model, long thisGen,
                                          InputsFingerprint fingerprint, CancellationToken ct)
        {
            try
            {
                string? engineSessionId = _engineSessionId;
                if (engineSessionId is null)
                {
                    engineSessionId = fs.Profile == BridgeProfile.Advanced
                        ? await fs.OpenEngineSessionAdvancedAsync(BuildAdvancedDefaults(), ct).ConfigureAwait(false)
                        : await fs.OpenEngineSessionAsync(null, ct).ConfigureAwait(false);
                }
                int dofCount = await fs.SetModelAsync(engineSessionId, model, ct).ConfigureAwait(false);
                if (ct.IsCancellationRequested
                    || Interlocked.Read(ref _assembleGeneration) != thisGen
                    || !ReferenceEquals(_cachedFs, fs))
                    return;
                _engineSessionId = engineSessionId;
                _dofCount   = dofCount;
                _lastInputs = (fingerprint.NMat, fingerprint.NSec, fingerprint.NNode, fingerprint.NMem, fingerprint.Hash);
                _error      = "";
            }
            catch (OperationCanceledException) { /* expected on slider thrash */ }
            catch (RemoteException ex)
            {
                if (ct.IsCancellationRequested || Interlocked.Read(ref _assembleGeneration) != thisGen)
                    return;
                _error = $"[{ex.Code}] {ex.Message}";
            }
            catch (Exception ex)
            {
                if (ct.IsCancellationRequested || Interlocked.Read(ref _assembleGeneration) != thisGen)
                    return;
                _error = ex.Message;
            }
            finally
            {
                if (Interlocked.Read(ref _assembleGeneration) == thisGen)
                {
                    _assembling = null;
                    Grasshopper.Instances.DocumentEditor?.BeginInvoke((Action)(() =>
                    {
                        try { ExpireSolution(true); } catch { /* document gone */ }
                    }));
                }
            }
        }

        private readonly record struct InputsFingerprint(int NMat, int NSec, int NNode, int NMem, int Hash);

        private static int ComputeFingerprint(List<GH_Material> mats, List<GH_Section> secs,
                                               List<Point3d> nodes, List<Curve> members)
        {
            unchecked
            {
                int h = 17;
                h = h * 31 + mats.Count;
                h = h * 31 + secs.Count;
                // P1 fix (review pass after B5): the previous fingerprint hashed only the
                // material / section LIST LENGTHS, so an upstream sweep that changed
                // E / G / Rho / Nu / Cap (or A / Iy / Iz / J / Cy / Cz / Asy / Asz) without
                // changing the count silently hit the cache. The cached engineSessionId then
                // pointed at a model built from STALE values, so B3-wired solve.linear would
                // return forces from the wrong stiffness. Fold every numeric field into the
                // hash; Cap=null contributes a distinct bucket so "had a cap" -> "no cap"
                // also bumps the fingerprint.
                foreach (var gm in mats)
                {
                    var m = gm.Value;
                    h = h * 31 + m.E.GetHashCode();
                    h = h * 31 + m.G.GetHashCode();
                    h = h * 31 + m.Rho.GetHashCode();
                    h = h * 31 + m.Nu.GetHashCode();
                    if (m.Cap is { } cap)
                    {
                        h = h * 31 + cap.Comp.GetHashCode();
                        h = h * 31 + cap.Tens.GetHashCode();
                        h = h * 31 + cap.Shear.GetHashCode();
                    }
                    else
                    {
                        h = h * 31 + 0;   // distinguish "no cap" from "cap = (0,0,0)"
                        h = h * 31 - 1;
                    }
                }
                foreach (var gs in secs)
                {
                    var s = gs.Value;
                    h = h * 31 + s.A.GetHashCode();
                    h = h * 31 + s.Iy.GetHashCode();
                    h = h * 31 + s.Iz.GetHashCode();
                    h = h * 31 + s.J.GetHashCode();
                    h = h * 31 + s.Cy.GetHashCode();
                    h = h * 31 + s.Cz.GetHashCode();
                    h = h * 31 + s.Asy.GetHashCode();
                    h = h * 31 + s.Asz.GetHashCode();
                }
                // v2.4 release-hardening fix (D-15): `+` binds tighter than `^`, so the
                // unparenthesised form `h * 31 + X ^ Y ^ Z` was `(h * 31 + X) ^ Y ^ Z`,
                // which silently lost the `h` accumulation when only Y or Z changed —
                // GH slider drags on Y/Z node coordinates would hit a stale cached engine
                // session. Parenthesise the XOR group so it folds into one int per node /
                // edge before accumulating.
                foreach (var n in nodes)
                    h = h * 31 + (n.X.GetHashCode() ^ n.Y.GetHashCode() ^ n.Z.GetHashCode());
                foreach (var c in members)
                    h = h * 31 + (c.PointAtStart.GetHashCode() ^ c.PointAtEnd.GetHashCode());
                return h;
            }
        }

        private static int SnapToNode(Point3d p, List<Point3d> nodes, double tol = 1e-3)
        {
            int best = -1; double bestD = tol;
            for (int i = 0; i < nodes.Count; ++i)
            {
                var d = p.DistanceTo(nodes[i]);
                if (d < bestD) { bestD = d; best = i; }
            }
            return best;
        }

        private static AdvancedEngineSessionOptions BuildAdvancedDefaults() => new()
        {
            Mode                       = EngineSessionMode.Default,
            PivotTol                   = 1e-12,
            EnableReleases             = false,
            UseTimoshenko              = false,
            UseIncompatibleMembrane    = false,
            UseDKQPlate                = false,
            ShellGeometricStiffness    = false,
            UseWarpingCorrection       = false,
            WarpTolerance              = 1e-6,
            ShellCurvatureMaxAngleDeg  = 0,
            UseSupernodalPrimary       = false,
            SnIrSteps                  = 0,
            SnAmalgMaxCol              = 64,
            SnNumThreads               = 0,
            SnFallbackOnFail           = false,
            DiagnosticStream           = false
        };
    }
}
