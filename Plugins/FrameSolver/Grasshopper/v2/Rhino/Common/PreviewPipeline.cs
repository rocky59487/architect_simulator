// PreviewPipeline.cs -- base class for any Display component that draws into Rhino viewport
// (BMD, SFD, NFD, UtilizationFringe, DeformedShape, ModeAnimation, CollapseReplay, ...).
//
// Implements IGH_PreviewObject so the component participates in Rhino viewport refresh; the
// derived class fills _drawCommands during SolveInstance and we replay them on every viewport
// refresh until the next SolveInstance clears them.
//
// USAGE
//   protected override void SolveInstance(IGH_DataAccess da) {
//       _drawCommands.Clear();
//       _drawCommands.Add(new MeshDraw(mesh, color));
//       _drawCommands.Add(new PolylineDraw(poly, color, thickness));
//       _bbox = ...;
//       da.SetData(0, mesh);
//   }
//
// Why cache instead of redraw-from-data each viewport refresh?
//   Rhino fires DrawViewportMeshes/Wires per viewport per frame. Recomputing geometry on every
//   one of those calls would burn the CPU. We compute once per SolveInstance, then replay.
//
// Why not just put geometry on the output port and let GH preview it?
//   GH's preview handles geometry-shaped data faithfully but it cannot color a polyline along
//   its length, draw a filled BMD shape with per-vertex gradient, animate a mode shape, or
//   render fragment velocity arrows — the "commercial-grade" looks that S6c § ⑤ promises.

using System.Collections.Generic;
using System.Drawing;
using Grasshopper.Kernel;
using Rhino.Display;
using Rhino.Geometry;

namespace FrameCore.Gh.Common
{
    public abstract class PreviewFrameComponent : GH_Component, IGH_PreviewObject
    {
        protected PreviewFrameComponent(string name, string nickname, string description,
                                         string category, string subcategory)
            : base(name, nickname, description, category, subcategory) { }

        protected readonly List<DrawCommand> _drawCommands = new();
        protected BoundingBox _bbox = BoundingBox.Empty;

        public bool Hidden { get; set; }
        public bool IsPreviewCapable => true;
        public override BoundingBox ClippingBox => _bbox;

        public override void DrawViewportMeshes(IGH_PreviewArgs args)
        {
            if (Hidden) return;
            foreach (var d in _drawCommands) d.DrawMesh(args.Display);
        }

        public override void DrawViewportWires(IGH_PreviewArgs args)
        {
            if (Hidden) return;
            foreach (var d in _drawCommands) d.DrawWire(args.Display);
        }

        public override void RemovedFromDocument(GH_Document document)
        {
            _drawCommands.Clear();
            _bbox = BoundingBox.Empty;
            base.RemovedFromDocument(document);
        }
    }

    /// <summary>One drawable primitive cached by a preview component.</summary>
    public abstract class DrawCommand
    {
        public virtual void DrawMesh(DisplayPipeline d) { }
        public virtual void DrawWire(DisplayPipeline d) { }
    }

    public sealed class MeshDraw : DrawCommand
    {
        public Mesh Mesh; public Color Color;
        public MeshDraw(Mesh m, Color c) { Mesh = m; Color = c; }
        public override void DrawMesh(DisplayPipeline d)
        {
            var mat = new DisplayMaterial(Color, 0.0);
            d.DrawMeshShaded(Mesh, mat);
        }
    }

    public sealed class WireDraw : DrawCommand
    {
        public Polyline Poly; public Color Color; public int Thickness;
        public WireDraw(Polyline p, Color c, int t = 2) { Poly = p; Color = c; Thickness = t; }
        public override void DrawWire(DisplayPipeline d) => d.DrawPolyline(Poly, Color, Thickness);
    }

    public sealed class ArrowDraw : DrawCommand
    {
        public Point3d From; public Vector3d Direction; public Color Color;
        public ArrowDraw(Point3d f, Vector3d v, Color c) { From = f; Direction = v; Color = c; }
        public override void DrawWire(DisplayPipeline d)
        {
            var to = From + Direction;
            d.DrawArrow(new Line(From, to), Color, 0.0, 0.0);
        }
    }

    public sealed class TextDotDraw : DrawCommand
    {
        public Point3d At; public string Text; public Color Color;
        public TextDotDraw(Point3d p, string t, Color c) { At = p; Text = t; Color = c; }
        public override void DrawMesh(DisplayPipeline d)
            => d.DrawDot(At, Text, Color, Color.Black);
    }

    /// <summary>Simple linear color ramp 0..1. UtilizationFringe / Stress contour use this.</summary>
    public static class ColorRamps
    {
        public static Color Viridis(double t)
        {
            t = System.Math.Max(0.0, System.Math.Min(1.0, t));
            // 5-stop sample (close enough; full LUT can ship in C2)
            var stops = new[] {
                (0.0, Color.FromArgb( 68,   1,  84)),
                (0.25, Color.FromArgb( 59,  82, 139)),
                (0.50, Color.FromArgb( 33, 144, 141)),
                (0.75, Color.FromArgb( 93, 201,  99)),
                (1.0, Color.FromArgb(253, 231,  37))
            };
            for (int i = 1; i < stops.Length; ++i)
            {
                if (t <= stops[i].Item1)
                {
                    var (t0, c0) = stops[i - 1];
                    var (t1, c1) = stops[i];
                    var u = (t - t0) / (t1 - t0);
                    return Color.FromArgb(
                        (int)(c0.R + (c1.R - c0.R) * u),
                        (int)(c0.G + (c1.G - c0.G) * u),
                        (int)(c0.B + (c1.B - c0.B) * u));
                }
            }
            return stops[^1].Item2;
        }

        public static Color Jet(double t)
        {
            t = System.Math.Max(0.0, System.Math.Min(1.0, t));
            double r = System.Math.Min(System.Math.Max(1.5 - System.Math.Abs(4 * t - 3), 0), 1);
            double g = System.Math.Min(System.Math.Max(1.5 - System.Math.Abs(4 * t - 2), 0), 1);
            double b = System.Math.Min(System.Math.Max(1.5 - System.Math.Abs(4 * t - 1), 0), 1);
            return Color.FromArgb((int)(r * 255), (int)(g * 255), (int)(b * 255));
        }
    }
}
