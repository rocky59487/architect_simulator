// GooWrappers.cs -- GH_Goo wrappers for FrameCore.Bridge POCO types. Required for GH wire
// type-safety: a wire of `IGH_Goo<FrameSession>` is red on the canvas if a user tries to
// connect it to anything other than another FrameSession sink.
//
// Goo is GH's "any type" envelope. CastFrom / CastTo bridge to numbers/strings/Curves/Points
// where natural (e.g. an Inspect output cast to a number list). Every wrapper here is minimal —
// real production .gha grows these with Duplicate / EmitProxy / GetTypeAlias on demand.

using FrameCore.Bridge;
using FrameCore.Bridge.Model;
using FrameCore.Bridge.Result;
using Grasshopper.Kernel.Types;

namespace FrameCore.Gh.Common
{
    public sealed class GH_FrameSession : GH_Goo<FrameSession>
    {
        public GH_FrameSession() { }
        public GH_FrameSession(FrameSession s) { Value = s; }
        public override bool IsValid => Value is { } v && v.EngineBuildSha.Length > 0;
        public override string TypeName => "FrameSession";
        public override string TypeDescription => "FrameCore engine session (transport + handshake).";
        public override IGH_Goo Duplicate() => new GH_FrameSession(Value);
        public override string ToString()
            => Value is null ? "<null FrameSession>" : $"FrameSession[{Value.EngineVersion}, {Value.Profile}]";
    }

    public sealed class GH_EngineSession : GH_Goo<string>
    {
        public GH_EngineSession() { }
        public GH_EngineSession(string id) { Value = id; }
        public override bool IsValid => !string.IsNullOrEmpty(Value);
        public override string TypeName => "EngineSession";
        public override string TypeDescription => "Engine-side session id (factor reuse handle).";
        public override IGH_Goo Duplicate() => new GH_EngineSession(Value!);
        public override string ToString() => Value ?? "<null EngineSession>";
    }

    public sealed class GH_FrameModel : GH_Goo<FrameModel>
    {
        public GH_FrameModel() { }
        public GH_FrameModel(FrameModel m) { Value = m; }
        public override bool IsValid => Value is not null;
        public override string TypeName => "FrameModel";
        public override string TypeDescription => "Strongly-typed structural model (nodes, members, shells, ...).";
        public override IGH_Goo Duplicate() => new GH_FrameModel(Value!);
        public override string ToString()
            => Value is null ? "<null FrameModel>"
                : $"FrameModel[{Value.Nodes.Count} nodes, {Value.Members.Count} members, {Value.Shells.Count} shells]";
    }

    public sealed class GH_Material : GH_Goo<Material>
    {
        public GH_Material() { }
        public GH_Material(Material m) { Value = m; }
        public override bool IsValid => Value is { E: > 0 };
        public override string TypeName => "Material";
        public override string TypeDescription => "Beam or shell material (E, G, rho, nu, cap).";
        public override IGH_Goo Duplicate() => new GH_Material(Value!);
        public override string ToString() => Value is null ? "<null Material>" : $"Material[E={Value.E}]";
    }

    public sealed class GH_Section : GH_Goo<Section>
    {
        public GH_Section() { }
        public GH_Section(Section s) { Value = s; }
        public override bool IsValid => Value is { A: > 0 };
        public override string TypeName => "Section";
        public override string TypeDescription => "Cross section (A, Iy, Iz, J, cy, cz, Asy, Asz).";
        public override IGH_Goo Duplicate() => new GH_Section(Value!);
        public override string ToString() => Value is null ? "<null Section>" : $"Section[A={Value.A}]";
    }

    public sealed class GH_LinearResult : GH_Goo<LinearResult>
    {
        public GH_LinearResult() { }
        public GH_LinearResult(LinearResult r) { Value = r; }
        public override bool IsValid => Value is not null;
        public override string TypeName => "LinearResult";
        public override string TypeDescription => "Linear static solve result (disp, reactions, member/shell forces).";
        public override IGH_Goo Duplicate() => new GH_LinearResult(Value!);
        public override string ToString()
            => Value is null ? "<null LinearResult>"
                : $"LinearResult[singular={Value.Singular}, {Value.Disp.Count} nodes]";
    }

    public sealed class GH_DynCollapseResult : GH_Goo<DynCollapseResult>
    {
        public GH_DynCollapseResult() { }
        public GH_DynCollapseResult(DynCollapseResult r) { Value = r; }
        public override bool IsValid => Value is not null;
        public override string TypeName => "DynCollapseResult";
        public override string TypeDescription => "Dynamic progressive-collapse history.";
        public override IGH_Goo Duplicate() => new GH_DynCollapseResult(Value!);
        public override string ToString()
            => Value is null ? "<null DynCollapseResult>"
                : $"DynCollapse[outcome={Value.Outcome}, {Value.NEvents} events, {Value.NFrames} frames]";
    }
}
