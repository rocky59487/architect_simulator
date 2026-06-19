// BMDComponent.cs -- the flagship Display component. Sample Mz / My along each beam between
// the LinearResult's end forces, offset perpendicular to the local axis by `scale * M`, draw
// the polyline AND fill the area between baseline and curve with a translucent mesh — the
// look Karamba "BeamDisplay" ships and what makes the canvas feel "FEA-grade".
//
// SAMPLING NOTE: in linear static with no inter-node UDL the diagram is linear and N+1
// samples is overkill, but N+1 lets us keep one shape for both the linear case AND the UDL
// case (where the parabola needs >2 points). N=20 default is the sweet spot — visually smooth
// without burning the canvas refresh budget.

using System;
using System.Collections.Generic;
using System.Drawing;
using FrameCore.Bridge.Result;
using FrameCore.Gh.Common;
using Grasshopper.Kernel;
using Rhino.Geometry;

namespace FrameCore.Gh.Components.Display
{
    public sealed class BMDComponent : PreviewFrameComponent
    {
        public BMDComponent()
            : base("BMD", "BMD",
                   "Bending moment diagram along each member. Outputs polylines + filled meshes; " +
                   "also draws straight into Rhino viewport (commercial-grade preview).",
                   Tabs.Category, Tabs.Display) { }

        public override Guid ComponentGuid => new("D1F4C1AA-2A0B-4D3E-9C8E-FEE100000701");
        protected override Bitmap? Icon => Resources.LoadIcon("display-bmd");

        protected override void RegisterInputParams(GH_InputParamManager p)
        {
            p.AddParameter(new GhParameters.Param_LinearResult(), "Result", "R", "Linear result.", GH_ParamAccess.item);
            p.AddCurveParameter("Members", "M", "Member curves (one per member, IN THE SAME ORDER as Assemble).", GH_ParamAccess.list);
            p.AddTextParameter ("Axis", "A", "Mz (strong) or My (weak).", GH_ParamAccess.item, "Mz");
            p.AddNumberParameter("Scale", "S", "Drawing scale: mm of diagram per N·mm of moment.", GH_ParamAccess.item, 1e-4);
            p.AddIntegerParameter("Samples", "N", "Samples along each member (≥ 2).", GH_ParamAccess.item, 20);
        }

        protected override void RegisterOutputParams(GH_OutputParamManager p)
        {
            p.AddCurveParameter("Diagram", "D", "BMD polyline per member.", GH_ParamAccess.list);
            p.AddMeshParameter ("Filled",  "F", "Filled BMD mesh per member.", GH_ParamAccess.list);
            p.AddNumberParameter("Peak",   "P", "Peak |M| per member.", GH_ParamAccess.list);
        }

        protected override void SolveInstance(IGH_DataAccess da)
        {
            GH_LinearResult? gr = null;
            if (!da.GetData(0, ref gr) || gr?.Value is null) return;
            var curves = new List<Curve>(); da.GetDataList(1, curves);
            string axis = "Mz"; da.GetData(2, ref axis);
            double scale = 1e-4; da.GetData(3, ref scale);
            int n = 20; da.GetData(4, ref n);
            n = Math.Max(2, n);

            var polys = new List<Polyline>();
            var fills = new List<Mesh>();
            var peaks = new List<double>();
            _drawCommands.Clear();
            _bbox = BoundingBox.Empty;

            var r = gr.Value!;
            for (int i = 0; i < curves.Count; ++i)
            {
                if (!r.MemberForces.TryGetValue(i, out var mf))
                {
                    AddRuntimeMessage(GH_RuntimeMessageLevel.Warning, $"No member force for id {i}; skipped.");
                    polys.Add(new Polyline()); fills.Add(new Mesh()); peaks.Add(0);
                    continue;
                }
                var c = curves[i];
                var line = new Line(c.PointAtStart, c.PointAtEnd);
                var xHat = line.Direction; xHat.Unitize();
                // local y / z: pick world Z if not parallel, otherwise world Y. Same convention
                // as the engine (FrameTypes.h header note: localAxes z=x*ref, y=z*x).
                var refV = Math.Abs(Vector3d.Multiply(xHat, Vector3d.ZAxis)) > 0.999
                            ? Vector3d.YAxis : Vector3d.ZAxis;
                var yHat = Vector3d.CrossProduct(refV, xHat); yHat.Unitize();
                var zHat = Vector3d.CrossProduct(xHat, yHat); zHat.Unitize();
                var normal = axis == "My" ? zHat : yHat;

                double Mi = axis == "My" ? mf.Myi : mf.Mzi;
                double Mj = axis == "My" ? -mf.Myj : -mf.Mzj;
                double peak = Math.Max(Math.Abs(Mi), Math.Abs(Mj));

                var poly = new Polyline();
                var mesh = new Mesh();
                for (int k = 0; k <= n; ++k)
                {
                    double t = (double)k / n;
                    double M = Lerp(Mi, Mj, t);
                    var basePt = line.PointAt(t);
                    var top    = basePt + normal * (M * scale);
                    poly.Add(top);
                    mesh.Vertices.Add(basePt);
                    mesh.Vertices.Add(top);
                    if (k > 0)
                        mesh.Faces.AddFace(2 * (k - 1), 2 * k, 2 * k + 1, 2 * (k - 1) + 1);
                }
                mesh.Normals.ComputeNormals();
                polys.Add(poly); fills.Add(mesh); peaks.Add(peak);

                // Cache for viewport preview
                var col = ColorRamps.Viridis(peak == 0 ? 0.5 : Math.Min(1.0, Math.Abs(Mi) / Math.Max(peak, 1e-30)));
                _drawCommands.Add(new MeshDraw(mesh, Color.FromArgb(120, col)));
                _drawCommands.Add(new WireDraw(poly, Color.FromArgb(255, col), 2));
                _bbox.Union(poly.BoundingBox);
            }

            da.SetDataList(0, polys);
            da.SetDataList(1, fills);
            da.SetDataList(2, peaks);
            Message = $"{axis} · peak {MaxPeak(peaks):G3}";
        }

        private static double Lerp(double a, double b, double t) => a * (1 - t) + b * t;
        private static double MaxPeak(List<double> p) { double m = 0; foreach (var v in p) if (v > m) m = v; return m; }
    }
}
