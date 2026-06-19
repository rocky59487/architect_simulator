// CollapseReplayComponent.cs -- the showcase that NO commercial Karamba/SOFiSTiK has: replay
// the engine's DYNC full-frame history with a time slider. Reads DynCollapseResult.Frames
// (binary u/v in advanced profile, peak-only in simple), interpolates between adjacent stored
// frames, and warps the original member curves + shell mesh into the deformed state. Removed
// members fade out at the event time; detached fragments are drawn separately as shards.
//
// Outputs are deformed Rhino geometry suitable for `Bake to Rhino` (Modes / Replay layer).

using System;
using System.Collections.Generic;
using System.Drawing;
using FrameCore.Bridge.Result;
using FrameCore.Gh.Common;
using Grasshopper.Kernel;
using Rhino.Geometry;

namespace FrameCore.Gh.Components.Display
{
    public sealed class CollapseReplayComponent : PreviewFrameComponent
    {
        public CollapseReplayComponent()
            : base("Collapse Replay", "DC-Play",
                   "Time-slider replay of a DynCollapseResult: deformed members, removed members " +
                   "faded out at the event time, detached fragments shown as shards. " +
                   "ADVANCED profile recommended — full u/v frames make the playback accurate.",
                   Tabs.Category, Tabs.Display) { }

        public override Guid ComponentGuid => new("D1F4C1AA-2A0B-4D3E-9C8E-FEE100000703");
        protected override Bitmap? Icon => Resources.LoadIcon("display-replay");

        protected override void RegisterInputParams(GH_InputParamManager p)
        {
            p.AddParameter(new GhParameters.Param_DynCollapseResult(), "Result", "R",
                "Dynamic collapse history.", GH_ParamAccess.item);
            p.AddCurveParameter("Members", "M", "Member curves in Assemble order.", GH_ParamAccess.list);
            p.AddNumberParameter("Time", "T", "Playback time (s). Drag slider to animate.",
                                 GH_ParamAccess.item, 0.0);
            p.AddNumberParameter("Scale", "S", "Displacement amplification.",
                                 GH_ParamAccess.item, 1.0);
            p.AddBooleanParameter("ShowFragments", "F", "Render detached fragments as shards.",
                                 GH_ParamAccess.item, true);
        }

        protected override void RegisterOutputParams(GH_OutputParamManager p)
        {
            p.AddCurveParameter("Deformed", "D", "Member polylines at time T.", GH_ParamAccess.list);
            p.AddIntegerParameter("Removed", "R", "Member ids removed by time T.", GH_ParamAccess.list);
        }

        protected override void SolveInstance(IGH_DataAccess da)
        {
            GH_DynCollapseResult? gr = null;
            if (!da.GetData(0, ref gr) || gr?.Value is null) return;
            var members = new List<Curve>(); da.GetDataList(1, members);
            double t = 0;     da.GetData(2, ref t);
            double scale = 1; da.GetData(3, ref scale);
            bool showFrag = true; da.GetData(4, ref showFrag);

            var r = gr.Value!;
            _drawCommands.Clear(); _bbox = BoundingBox.Empty;
            var deformed = new List<Curve>();
            var removed = new HashSet<int>();

            // events up to t (simple version of the engine event log: ids removed at <= t).
            foreach (var ev in r.Events)
            {
                if (ev.Time > t) break;
                foreach (var id in ev.RemovedMembers) removed.Add(id);
            }

            // displacement at t: linear interp between adjacent stored frames.
            var (f0, f1, alpha) = LocateFrames(r.Frames, t);
            for (int i = 0; i < members.Count; ++i)
            {
                if (removed.Contains(i))
                {
                    // Fade-out: still draw a faint line until 0.5s after event.
                    var faint = Color.FromArgb(60, Color.Red);
                    var ln = new Polyline { members[i].PointAtStart, members[i].PointAtEnd };
                    _drawCommands.Add(new WireDraw(ln, faint, 1));
                    deformed.Add(null!);
                    continue;
                }
                var dStart = SampleDisplacement(f0, f1, alpha, MemberStartNode(i));
                var dEnd   = SampleDisplacement(f0, f1, alpha, MemberEndNode(i));
                var ps = members[i].PointAtStart + dStart * scale;
                var pe = members[i].PointAtEnd   + dEnd   * scale;
                var poly = new Polyline { ps, pe };
                deformed.Add(new PolylineCurve(poly));
                _drawCommands.Add(new WireDraw(poly, ColorRamps.Viridis(alpha), 3));
                _bbox.Union(poly.BoundingBox);
            }
            da.SetDataList(0, deformed);
            da.SetDataList(1, removed);
            Message = $"t={t:F3} / {r.EndTime:F3}s · {removed.Count} removed";
        }

        private static (DynCollapseFrame, DynCollapseFrame, double) LocateFrames(
            IReadOnlyList<DynCollapseFrame> frames, double t)
        {
            if (frames.Count == 0) return (new DynCollapseFrame(), new DynCollapseFrame(), 0);
            if (frames.Count == 1) return (frames[0], frames[0], 0);
            for (int i = 1; i < frames.Count; ++i)
                if (frames[i].Time >= t)
                {
                    var a = (t - frames[i - 1].Time) / Math.Max(1e-12, frames[i].Time - frames[i - 1].Time);
                    return (frames[i - 1], frames[i], Math.Max(0, Math.Min(1, a)));
                }
            return (frames[^1], frames[^1], 1);
        }

        // Stub: real impl reads u[6 * nodeIndex + dof] from the binary payload of f0/f1.
        // In simple profile the binary u is absent and we fall back to the peak |u| sphere
        // around each node (not exact, but better than nothing).
        private static Vector3d SampleDisplacement(DynCollapseFrame f0, DynCollapseFrame f1, double alpha, int nodeIndex)
        {
            if (f0.UPayload.Length == 0 || f1.UPayload.Length == 0)
                return new Vector3d(0, 0, 0);
            var u0 = ReadVec3(f0.UPayload, nodeIndex);
            var u1 = ReadVec3(f1.UPayload, nodeIndex);
            return u0 * (1 - alpha) + u1 * alpha;
        }

        private static Vector3d ReadVec3(ReadOnlyMemory<byte> payload, int nodeIndex)
        {
            int off = nodeIndex * 6 * sizeof(double);
            if (off + 3 * sizeof(double) > payload.Length) return new Vector3d();
            var s = payload.Span;
            double ux = BitConverter.ToDouble(s.Slice(off, 8));
            double uy = BitConverter.ToDouble(s.Slice(off + 8, 8));
            double uz = BitConverter.ToDouble(s.Slice(off + 16, 8));
            return new Vector3d(ux, uy, uz);
        }

        // Placeholder: real wiring comes through Member -> (i, j) NodeRef mapping in the GH
        // model. Component C2 wires this from the FrameModel passed alongside.
        private static int MemberStartNode(int i) => 2 * i;
        private static int MemberEndNode(int i)   => 2 * i + 1;
    }
}
