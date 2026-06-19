// DiagnosticPanelComponent.cs -- the Advanced-profile "Inspector". Outputs human-readable lines
// from a result's AdvancedDiagnostics payload. Pair with a Grasshopper Panel to see the live
// engine telemetry on the canvas. Right-click "Open Inspector" opens the fuller WPF panel (C3).
//
// Simple-profile results have null AdvancedDiagnostics; this component emits a one-line note
// asking the user to switch profiles.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Text;
using FrameCore.Bridge.Result;
using FrameCore.Gh.Common;
using Grasshopper.Kernel;

namespace FrameCore.Gh.Components.Advanced
{
    public sealed class DiagnosticPanelComponent : GH_Component
    {
        public DiagnosticPanelComponent()
            : base("Diagnostic Panel", "Diag",
                   "Read the AdvancedDiagnostics payload of a result. Connect to a Panel.",
                   Tabs.Category, Tabs.Advanced) { }

        public override Guid ComponentGuid => new("D1F4C1AA-2A0B-4D3E-9C8E-FEE100000801");
        protected override Bitmap? Icon => Resources.LoadIcon("advanced-panel");

        protected override void RegisterInputParams(GH_InputParamManager p)
        {
            p.AddParameter(new GhParameters.Param_LinearResult(), "Result", "R",
                "Any LinearResult (or compatible). Advanced profile required for full content.",
                GH_ParamAccess.item);
        }

        protected override void RegisterOutputParams(GH_OutputParamManager p)
        {
            p.AddTextParameter("Summary",      "S", "One-line summary.",       GH_ParamAccess.item);
            p.AddTextParameter("Detail",       "D", "Multi-line detail block.", GH_ParamAccess.item);
            p.AddNumberParameter("PivotMargin", "P", "Final pivot margin.",     GH_ParamAccess.item);
            p.AddTextParameter("FactorBackend", "B", "LDLT / Supernodal / CHOLMOD.", GH_ParamAccess.item);
            p.AddNumberParameter("FactorMs",    "Tf", "Assemble+factor wall-clock (ms).", GH_ParamAccess.item);
            p.AddNumberParameter("SolveMs",     "Ts", "Forward/back-sub wall-clock (ms).", GH_ParamAccess.item);
        }

        protected override void SolveInstance(IGH_DataAccess da)
        {
            GH_LinearResult? gr = null;
            if (!da.GetData(0, ref gr) || gr?.Value is null) return;
            var r = gr.Value!;
            var d = r.AdvancedDiagnostics;

            if (d is null)
            {
                da.SetData(0, "simple profile — no AdvancedDiagnostics. Switch Open FrameCore to Advanced.");
                da.SetData(1, "");
                da.SetData(2, r.PivotMargin);
                da.SetData(3, "");
                da.SetData(4, 0.0);
                da.SetData(5, 0.0);
                Message = "simple";
                return;
            }

            var sum = new StringBuilder();
            sum.Append(d.FactorMethod ?? "-").Append(" via ").Append(d.FactorBackend ?? "-");
            sum.Append(" · factor ").Append(F(d.FactorTimeMs)).Append("ms");
            sum.Append(" · solve ").Append(F(d.SolveTimeMs)).Append("ms");
            sum.Append(" · pivot ").Append(r.PivotMargin.ToString("G3"));

            var det = new StringBuilder();
            det.AppendLine("# AdvancedDiagnostics");
            det.AppendLine("FactorMethod    : " + (d.FactorMethod ?? "<n/a>"));
            det.AppendLine("FactorBackend   : " + (d.FactorBackend ?? "<n/a>"));
            det.AppendLine("FactorTimeMs    : " + F(d.FactorTimeMs));
            det.AppendLine("SolveTimeMs     : " + F(d.SolveTimeMs));
            det.AppendLine("PivotMargin     : " + r.PivotMargin.ToString("G6"));
            if (d.PivotMarginTrace is { Count: > 0 } pm)
                det.AppendLine("PivotMarginTrace: [" + Join(pm) + "]");
            if (d.SnIrResidualHistory is { Count: > 0 } ir)
                det.AppendLine("SnIrResidual    : [" + Join(ir) + "]");
            if (d.GuardsDisabled is { Count: > 0 } gd)
                det.AppendLine("GuardsDisabled  : " + string.Join(", ", gd));
            if (d.MechanismCandidates is { Count: > 0 } mc)
            {
                det.AppendLine("MechanismCandidates:");
                foreach (var m in mc)
                    det.AppendLine($"  node {m.NodeId} dof {m.Dof} -> {m.NormalizedPivot:G3}");
            }
            if (d.Reanalysis is { TierLadder: { Count: > 0 } } ra)
                det.AppendLine("TierLadder      : [" + string.Join(",", ra.TierLadder) + "]");
            if (d.SizeOpt is { ConvergenceHistory: { Count: > 0 } } so)
                det.AppendLine("SizeOpt history : [" + Join(so.ConvergenceHistory) + "]");
            if (d.DynCollapse is { } dc)
            {
                if (dc.RitzBasisSize is { } b) det.AppendLine($"RitzBasisSize   : {b}");
                if (dc.RitzResidual  is { } rr) det.AppendLine($"RitzResidual    : {rr:G3}");
            }

            da.SetData(0, sum.ToString());
            da.SetData(1, det.ToString());
            da.SetData(2, r.PivotMargin);
            da.SetData(3, d.FactorBackend ?? "");
            da.SetData(4, d.FactorTimeMs  ?? double.NaN);
            da.SetData(5, d.SolveTimeMs   ?? double.NaN);
            Message = "advanced";
        }

        private static string F(double? v) => v.HasValue ? v.Value.ToString("F2") : "-";
        private static string Join(IReadOnlyList<double> v)
        {
            var sb = new StringBuilder();
            for (int i = 0; i < v.Count; ++i) { if (i > 0) sb.Append(", "); sb.Append(v[i].ToString("G3")); }
            return sb.ToString();
        }
    }
}
