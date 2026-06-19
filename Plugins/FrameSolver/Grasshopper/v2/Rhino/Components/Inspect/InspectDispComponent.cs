// InspectDispComponent.cs -- Tab 8 "high-frequency interactive read". The user has a slider
// driving a node id and just wants that one displacement; we send `inspect.disp` with the ids
// only, which the engine answers from cached state without re-solving. Average round-trip is
// 0.5-2 ms (10x faster than parsing the whole LinearResult).

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Threading;
using System.Threading.Tasks;
using FrameCore.Bridge;
using FrameCore.Gh.Common;
using Grasshopper.Kernel;

namespace FrameCore.Gh.Components.Inspect
{
    public sealed class InspectDispComponent : AsyncFrameComponent<InspectDispComponent.Inputs,
                                                                    Dictionary<int, double[]>>
    {
        public InspectDispComponent()
            : base("Inspect Disp", "DI",
                   "Read displacements for specific node ids without re-solving. Ideal for " +
                   "slider-driven interactive inspection.",
                   Tabs.Category, Tabs.Inspect) { }

        public override Guid ComponentGuid => new("D1F4C1AA-2A0B-4D3E-9C8E-FEE100000601");
        protected override Bitmap? Icon => Resources.LoadIcon("inspect-disp");

        public sealed class Inputs
        {
            public required FrameSession Session;
            public required string EngineSession;
            public required IReadOnlyList<int> NodeIds;
        }

        protected override void RegisterInputParams(GH_InputParamManager p)
        {
            p.AddParameter(new GhParameters.Param_FrameSession(),  "Session", "S", "", GH_ParamAccess.item);
            p.AddParameter(new GhParameters.Param_EngineSession(), "ES", "ES", "", GH_ParamAccess.item);
            p.AddIntegerParameter("NodeIds", "N", "Node ids to inspect.", GH_ParamAccess.list);
        }

        protected override void RegisterOutputParams(GH_OutputParamManager p)
        {
            p.AddNumberParameter("Ux", "Ux", "Ux per node id.", GH_ParamAccess.list);
            p.AddNumberParameter("Uy", "Uy", "Uy per node id.", GH_ParamAccess.list);
            p.AddNumberParameter("Uz", "Uz", "Uz per node id.", GH_ParamAccess.list);
            p.AddNumberParameter("Rx", "Rx", "Rx per node id.", GH_ParamAccess.list);
            p.AddNumberParameter("Ry", "Ry", "Ry per node id.", GH_ParamAccess.list);
            p.AddNumberParameter("Rz", "Rz", "Rz per node id.", GH_ParamAccess.list);
        }

        protected override bool TryReadInputs(IGH_DataAccess da, out Inputs input)
        {
            input = null!;
            GH_FrameSession? fs = null; GH_EngineSession? es = null;
            var ids = new List<int>();
            if (!da.GetData(0, ref fs) || fs?.Value is null) return false;
            if (!da.GetData(1, ref es) || string.IsNullOrEmpty(es?.Value)) return false;
            if (!da.GetDataList(2, ids)) return false;
            Profile = fs.Value.Profile;
            input = new Inputs { Session = fs.Value, EngineSession = es.Value!, NodeIds = ids };
            return true;
        }

        protected override async Task<Dictionary<int, double[]>> RunAsync(Inputs input, CancellationToken ct)
        {
            var dict = await input.Session.InspectDispAsync(input.EngineSession, input.NodeIds, ct);
            return new Dictionary<int, double[]>(dict);
        }

        protected override void WriteOutputs(IGH_DataAccess da, Dictionary<int, double[]> r)
        {
            var ux = new List<double>(); var uy = new List<double>(); var uz = new List<double>();
            var rx = new List<double>(); var ry = new List<double>(); var rz = new List<double>();
            foreach (var kv in r)
            {
                ux.Add(kv.Value[0]); uy.Add(kv.Value[1]); uz.Add(kv.Value[2]);
                rx.Add(kv.Value[3]); ry.Add(kv.Value[4]); rz.Add(kv.Value[5]);
            }
            da.SetDataList(0, ux); da.SetDataList(1, uy); da.SetDataList(2, uz);
            da.SetDataList(3, rx); da.SetDataList(4, ry); da.SetDataList(5, rz);
        }
    }
}
