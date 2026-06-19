// GhParameters.cs -- GH parameter classes for each Goo type. A Goo wrapper alone is enough to
// stick a value on a wire, but for a STRONG-TYPED input port you need a custom IGH_Param so
// the wire-color matches and the casting rules are explicit. Each class here is a 5-line shell
// over the matching GH_Goo<T>.
//
// IGH_PersistentParam adds support for "save default value" on right-click "Set one X" — we
// only enable that for the leaf parameters that make sense (Material, Section).

using FrameCore.Bridge.Model;
using FrameCore.Gh.Common;
using Grasshopper.Kernel;
using Grasshopper.Kernel.Types;

namespace FrameCore.Gh.GhParameters
{
    public sealed class Param_FrameSession : GH_Param<GH_FrameSession>
    {
        public Param_FrameSession()
            : base(new GH_InstanceDescription("FrameSession", "S", "FrameCore engine session.",
                                               Tabs.Category, Tabs.Setup)) { }
        public override System.Guid ComponentGuid => new("D1F4C1AA-2A0B-4D3E-9C8E-FEE100000201");
        public override GH_ParamKind Kind => GH_ParamKind.floating;
    }

    public sealed class Param_EngineSession : GH_Param<GH_EngineSession>
    {
        public Param_EngineSession()
            : base(new GH_InstanceDescription("EngineSession", "ES",
                                               "Engine-side session id (factor reuse).",
                                               Tabs.Category, Tabs.Setup)) { }
        public override System.Guid ComponentGuid => new("D1F4C1AA-2A0B-4D3E-9C8E-FEE100000202");
        public override GH_ParamKind Kind => GH_ParamKind.floating;
    }

    public sealed class Param_FrameModel : GH_Param<GH_FrameModel>
    {
        public Param_FrameModel()
            : base(new GH_InstanceDescription("FrameModel", "M",
                                               "Strongly typed structural model.",
                                               Tabs.Category, Tabs.Geometry)) { }
        public override System.Guid ComponentGuid => new("D1F4C1AA-2A0B-4D3E-9C8E-FEE100000203");
        public override GH_ParamKind Kind => GH_ParamKind.floating;
    }

    public sealed class Param_Material : GH_Param<GH_Material>
    {
        public Param_Material()
            : base(new GH_InstanceDescription("Material", "Mat", "Material.",
                                               Tabs.Category, Tabs.Material)) { }
        public override System.Guid ComponentGuid => new("D1F4C1AA-2A0B-4D3E-9C8E-FEE100000204");
        public override GH_ParamKind Kind => GH_ParamKind.floating;
    }

    public sealed class Param_Section : GH_Param<GH_Section>
    {
        public Param_Section()
            : base(new GH_InstanceDescription("Section", "Sec", "Cross section.",
                                               Tabs.Category, Tabs.Section)) { }
        public override System.Guid ComponentGuid => new("D1F4C1AA-2A0B-4D3E-9C8E-FEE100000205");
        public override GH_ParamKind Kind => GH_ParamKind.floating;
    }

    public sealed class Param_LinearResult : GH_Param<GH_LinearResult>
    {
        public Param_LinearResult()
            : base(new GH_InstanceDescription("LinearResult", "R", "Linear solve result.",
                                               Tabs.Category, Tabs.Analyze)) { }
        public override System.Guid ComponentGuid => new("D1F4C1AA-2A0B-4D3E-9C8E-FEE100000206");
        public override GH_ParamKind Kind => GH_ParamKind.floating;
    }

    public sealed class Param_DynCollapseResult : GH_Param<GH_DynCollapseResult>
    {
        public Param_DynCollapseResult()
            : base(new GH_InstanceDescription("DynCollapseResult", "DC",
                                               "Dynamic collapse history.",
                                               Tabs.Category, Tabs.Analyze)) { }
        public override System.Guid ComponentGuid => new("D1F4C1AA-2A0B-4D3E-9C8E-FEE100000207");
        public override GH_ParamKind Kind => GH_ParamKind.floating;
    }
}
