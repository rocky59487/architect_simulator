// SolveComponent.cs -- linear static solve. Trivially small because all the async/cancel/wire-tip
// plumbing is in AsyncFrameComponent<TIn, TOut>. This is the SDK-shape every solver component
// (PDelta / TensionOnly / SizeOpt / Modal / Buckling / Corotational / ArcLength) follows; they
// only override TryReadInputs / RunAsync / WriteOutputs / CountDefaults.

using System;
using System.Drawing;
using System.Threading;
using System.Threading.Tasks;
using FrameCore.Bridge;
using FrameCore.Bridge.Result;
using FrameCore.Gh.Common;
using Grasshopper.Kernel;

namespace FrameCore.Gh.Components.Analyze
{
    public sealed class SolveComponent : AsyncFrameComponent<SolveComponent.Inputs, LinearResult>
    {
        public SolveComponent()
            : base("Solve", "Solve",
                   "Linear static analysis. Reuses the engine factor when wired to the same " +
                   "EngineSession across slider drags (< 30 ms response when wiring is hot).",
                   Tabs.Category, Tabs.Analyze) { }

        public override Guid ComponentGuid => new("D1F4C1AA-2A0B-4D3E-9C8E-FEE100000502");
        protected override Bitmap? Icon => Resources.LoadIcon("analyze-solve");

        public sealed class Inputs
        {
            public required FrameSession Session;
            public required string EngineSession;
            public bool WantReactions = true;
        }

        protected override void RegisterInputParams(GH_InputParamManager p)
        {
            p.AddParameter(new GhParameters.Param_FrameSession(),  "Session", "S",
                "FrameCore session.", GH_ParamAccess.item);
            p.AddParameter(new GhParameters.Param_EngineSession(), "EngineSession", "ES",
                "Engine session from Assemble Model.", GH_ParamAccess.item);
            p.AddBooleanParameter("WantReactions", "R", "Also compute reactions.",
                GH_ParamAccess.item, true);
        }

        protected override void RegisterOutputParams(GH_OutputParamManager p)
        {
            p.AddParameter(new GhParameters.Param_LinearResult(), "Result", "R",
                "Linear solve result.", GH_ParamAccess.item);
            p.AddBooleanParameter("Singular", "X",
                "True if the engine detected a mechanism (simple profile only; advanced throws).",
                GH_ParamAccess.item);
            p.AddNumberParameter("PivotMargin", "P",
                "min|D| / max|D| of K_ff. Watch this approach pivotTol.",
                GH_ParamAccess.item);
        }

        protected override bool TryReadInputs(IGH_DataAccess da, out Inputs input)
        {
            input = null!;
            GH_FrameSession? fs = null; GH_EngineSession? es = null; bool wantR = true;
            if (!da.GetData(0, ref fs) || fs?.Value is null) return false;
            if (!da.GetData(1, ref es) || string.IsNullOrEmpty(es?.Value)) return false;
            da.GetData(2, ref wantR);
            Profile = fs.Value.Profile;
            input = new Inputs { Session = fs.Value, EngineSession = es.Value!, WantReactions = wantR };
            return true;
        }

        protected override Task<LinearResult> RunAsync(Inputs input, CancellationToken ct)
            => input.Session.SolveLinearAsync(input.EngineSession, input.WantReactions, ct);

        protected override void WriteOutputs(IGH_DataAccess da, LinearResult r)
        {
            da.SetData(0, new GH_LinearResult(r));
            da.SetData(1, r.Singular);
            da.SetData(2, r.PivotMargin);
        }

        protected override int CountDefaults(LinearResult r) => r.DefaultsApplied?.Count ?? 0;
    }
}
