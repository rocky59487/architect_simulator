// UtilizationFringeComponent.cs -- D/C color fringe along beams (+ shells in C2). This is the
// "stoplight" view every commercial FEA UI ships:  green where utilization is low, red where
// the section is at capacity. Karamba's "BeamForceDisplay" + "UtilDisplay" combined into one
// component.

using System;
using System.Collections.Generic;
using System.Drawing;
using FrameCore.Bridge.Result;
using FrameCore.Gh.Common;
using Grasshopper.Kernel;
using Rhino.Geometry;

namespace FrameCore.Gh.Components.Display
{
    public sealed class UtilizationFringeComponent : PreviewFrameComponent
    {
        public UtilizationFringeComponent()
            : base("Utilization Fringe", "Util",
                   "Color-code each member by its peak D/C (demand-to-capacity ratio). " +
                   "Outputs a mesh tube along each member with per-vertex color from the chosen ramp.",
                   Tabs.Category, Tabs.Display) { }

        public override Guid ComponentGuid => new("D1F4C1AA-2A0B-4D3E-9C8E-FEE100000702");
        protected override Bitmap? Icon => Resources.LoadIcon("display-util");

        protected override void RegisterInputParams(GH_InputParamManager p)
        {
            p.AddParameter(new GhParameters.Param_LinearResult(), "Result", "R", "Linear result.", GH_ParamAccess.item);
            p.AddCurveParameter("Members", "M", "Member curves in Assemble order.", GH_ParamAccess.list);
            p.AddNumberParameter("Cap", "C", "Allowable stress for D/C denominator (MPa). Defaults to 235.",
                                 GH_ParamAccess.item, 235);
            p.AddTextParameter("Ramp", "P", "Color ramp: Viridis / Jet.", GH_ParamAccess.item, "Viridis");
            p.AddNumberParameter("TubeRadius", "T", "Tube radius for the fringe mesh (model units).",
                                 GH_ParamAccess.item, 30);
        }

        protected override void RegisterOutputParams(GH_OutputParamManager p)
        {
            p.AddMeshParameter ("Fringe", "F", "Colored tube mesh per member.", GH_ParamAccess.list);
            p.AddNumberParameter("DC",    "D", "Peak D/C per member.", GH_ParamAccess.list);
        }

        protected override void SolveInstance(IGH_DataAccess da)
        {
            GH_LinearResult? gr = null;
            if (!da.GetData(0, ref gr) || gr?.Value is null) return;
            var curves = new List<Curve>(); da.GetDataList(1, curves);
            double cap = 235; da.GetData(2, ref cap);
            _ = cap; // legacy input retained for file compatibility; native D/C drives the fringe.
            string rampName = "Viridis"; da.GetData(3, ref rampName);
            double radius = 30; da.GetData(4, ref radius);

            Func<double, Color> ramp = rampName.Equals("Jet", StringComparison.OrdinalIgnoreCase)
                ? ColorRamps.Jet : (Func<double, Color>)ColorRamps.Viridis;

            var meshes = new List<Mesh>(); var dcs = new List<double>();
            _drawCommands.Clear(); _bbox = BoundingBox.Empty;

            var r = gr.Value!;
            bool warnedNoDc = false;
            for (int i = 0; i < curves.Count; ++i)
            {
                if (!r.MemberForces.ContainsKey(i))
                {
                    AddRuntimeMessage(GH_RuntimeMessageLevel.Warning, $"No force for member {i}.");
                    meshes.Add(new Mesh()); dcs.Add(0); continue;
                }
                double dcI = double.NaN, dcJ = double.NaN, dcPeak = double.NaN;
                if (r.MemberUtilization.TryGetValue(i, out var util))
                {
                    dcI = Math.Min(1.5, util.EndI);
                    dcJ = Math.Min(1.5, util.EndJ);
                    dcPeak = Math.Max(dcI, dcJ);
                }
                else if (!warnedNoDc)
                {
                    AddRuntimeMessage(GH_RuntimeMessageLevel.Warning,
                        "Result has no native D/C data. Re-run Solve against a DLL that supports solve.linear wantDC.");
                    warnedNoDc = true;
                }

                var c = curves[i];
                var pipe = MakePipe(c, radius);
                ColorPipe(pipe, ramp, dcI, dcJ);

                meshes.Add(pipe); dcs.Add(dcPeak);
                _drawCommands.Add(new MeshDraw(pipe, Color.White));   // color is per-vertex
                _bbox.Union(pipe.GetBoundingBox(true));
            }
            da.SetDataList(0, meshes);
            da.SetDataList(1, dcs);
            double maxD = MaxD(dcs);
            Message = double.IsNaN(maxD) ? "D/C unavailable" : $"max D/C {maxD:F3}";
        }

        private static Mesh MakePipe(Curve c, double r)
        {
            var ms = Mesh.CreateFromCurvePipe(c, r, 8, 1, MeshPipeCapStyle.Flat, false);
            return ms ?? new Mesh();
        }

        private static void ColorPipe(Mesh pipe, Func<double, Color> ramp, double dcI, double dcJ)
        {
            pipe.VertexColors.Clear();
            if (pipe.Vertices.Count == 0) return;
            var bb = pipe.GetBoundingBox(true);
            if (bb.Diagonal.Length < 1e-9) return;
            // Approximate parameter along bbox X axis for color interpolation.
            for (int i = 0; i < pipe.Vertices.Count; ++i)
            {
                var p = pipe.Vertices[i];
                double t = (p.X - bb.Min.X) / Math.Max(1e-9, bb.Max.X - bb.Min.X);
                double dc = dcI * (1 - t) + dcJ * t;
                pipe.VertexColors.Add(double.IsNaN(dc) ? Color.Gray : ramp(Math.Min(1.0, dc)));
            }
        }

        private static double MaxD(List<double> v)
        {
            double m = double.NaN;
            foreach (var x in v)
                if (!double.IsNaN(x) && (double.IsNaN(m) || x > m)) m = x;
            return m;
        }
    }
}
