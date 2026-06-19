// FrameModel.cs -- strongly typed structural model. The whole point of v2 is "client never touches
// strings": users build a FrameModel via builders, the SDK serializes it to JSON for the model.set
// request. Forward-compatibility is built in: extra serialized fields the engine doesn't recognise
// land harmlessly (the engine ignores unknown JSON fields), and the engine can add result fields
// the SDK doesn't recognise (LinearResult.RawHeader exposes them).
//
// CONVENTIONS (mirror docs/CLI_PROTOCOL.md and FrameTypes.h)
//   * Units: N, mm, MPa.
//   * DOF order per node: [Ux, Uy, Uz, Rx, Ry, Rz].
//   * Axial N is COMPRESSION-POSITIVE in member end forces.
//   * Materials/Sections are referenced by INDEX, not pointer -- the builder hands out
//     MaterialRef/SectionRef so the user can't get the index wrong.

namespace FrameCore.Bridge.Model;

public enum Dof { Ux = 0, Uy = 1, Uz = 2, Rx = 3, Ry = 4, Rz = 5 }

/// <summary>Strongly-typed reference into FrameModel.Materials.</summary>
public readonly record struct MaterialRef(int Index);
/// <summary>Strongly-typed reference into FrameModel.Sections.</summary>
public readonly record struct SectionRef(int Index);
/// <summary>Strongly-typed reference into FrameModel.Nodes (carries id, not raw index).</summary>
public readonly record struct NodeRef(int Id);
/// <summary>Strongly-typed reference into FrameModel.Members.</summary>
public readonly record struct MemberRef(int Id);
/// <summary>Strongly-typed reference into FrameModel.Shells.</summary>
public readonly record struct ShellRef(int Id);

public sealed class Material
{
    public required double E   { get; init; }   // Young's modulus, MPa
    public required double G   { get; init; }   // shear modulus, MPa
    public required double Rho { get; init; }   // density, tonne/mm^3 (engine units)
    public double Nu { get; init; }             // Poisson, only used by SMAT (shells)

    public Capacity? Cap { get; init; }         // optional allowable cap for size opt / D-C

    /// <summary>Beam material: nu defaults to 0 (the CLI MAT convention).</summary>
    public static Material Steel(double E = 210000, double G = 80769, double rho = 7.85e-9)
        => new() { E = E, G = G, Rho = rho };

    /// <summary>Shell material: takes nu explicitly (SMAT convention).</summary>
    public static Material Shell(double E, double nu, double G)
        => new() { E = E, G = G, Rho = 0, Nu = nu };
}

public readonly record struct Capacity(double Comp, double Tens, double Shear);

public sealed class Section
{
    public required double A   { get; init; }   // area
    public required double Iy  { get; init; }
    public required double Iz  { get; init; }
    public required double J   { get; init; }
    public required double Cy  { get; init; }   // extreme fibre dist for Mz
    public required double Cz  { get; init; }
    public required double Asy { get; init; }   // shear area (Timoshenko)
    public required double Asz { get; init; }

    /// <summary>Rectangular section convenience: b = width along local z, d = depth along local y.</summary>
    public static Section Rectangular(double b, double d) => new()
    {
        A   = b * d,
        Iy  = b * d * d * d / 12.0,
        Iz  = d * b * b * b / 12.0,
        J   = (1.0 / 3.0) * b * d * d * d * (1 - 0.63 * (d / b)),   // rough St-Venant for b >= d
        Cy  = d / 2,
        Cz  = b / 2,
        Asy = (5.0 / 6.0) * b * d,
        Asz = (5.0 / 6.0) * b * d
    };
}

public sealed class Node
{
    public required int Id { get; init; }
    public required double X { get; init; }
    public required double Y { get; init; }
    public required double Z { get; init; }
    // P2.7: Fixed/Prescribed are still bool[]/double[] for clean engine-side serialisation,
    // but the builder deep-copies on Add and FrameModelBuilder.Build() deep-copies again,
    // so the array inside the built FrameModel is owned exclusively by the model.
    /// <summary>Boolean support flags per DOF (true = fixed). Length 6, [Ux..Rz].</summary>
    public bool[] Fixed { get; init; } = new bool[6];
    /// <summary>Prescribed displacement values for fixed DOFs (default 0). Length 6.</summary>
    public double[] Prescribed { get; init; } = new double[6];
}

public sealed class Member
{
    public required int Id { get; init; }
    public required NodeRef I { get; init; }
    public required NodeRef J { get; init; }
    public required MaterialRef Material { get; init; }
    public required SectionRef Section { get; init; }
    public (double X, double Y, double Z) RefVec { get; init; } = (0, 0, 1);
    public bool Active { get; init; } = true;
    public bool TensionOnly { get; init; } = false;
    /// <summary>End releases per DOF, length 12 ([node_i 6][node_j 6]). See Node remark on P2.7 ownership.</summary>
    public bool[] Release { get; init; } = new bool[12];
}

public sealed class ShellQuad
{
    public required int Id { get; init; }
    public required NodeRef N0 { get; init; }
    public required NodeRef N1 { get; init; }
    public required NodeRef N2 { get; init; }
    public required NodeRef N3 { get; init; }
    public required MaterialRef Material { get; init; }
    public required double Thickness { get; init; }
    public bool Active { get; init; } = true;
}

public sealed class NodalLoad
{
    public required NodeRef Node { get; init; }
    /// <summary>[Fx, Fy, Fz, Mx, My, Mz], length 6.</summary>
    public required double[] Components { get; init; }
}

public sealed class MemberUdl
{
    public required MemberRef Member { get; init; }
    /// <summary>Local UDL (wx, wy, wz).</summary>
    public (double Wx, double Wy, double Wz) Local { get; init; }
}

public sealed class ShellPressure
{
    public required ShellRef Shell { get; init; }
    public required double P { get; init; }
}

public sealed class PlasticHinge
{
    public required MemberRef Member { get; init; }
    /// <summary>4=Myi, 5=Mzi, 10=Myj, 11=Mzj.</summary>
    public required int Dof { get; init; }
    public required double Mp { get; init; }
}

/// <summary>The whole structural model. Build via <see cref="FrameModelBuilder"/>.</summary>
public sealed class FrameModel
{
    public IReadOnlyList<Material> Materials { get; }
    public IReadOnlyList<Section>  Sections  { get; }
    public IReadOnlyList<Node>     Nodes     { get; }
    public IReadOnlyList<Member>   Members   { get; }
    public IReadOnlyList<ShellQuad> Shells   { get; }
    public IReadOnlyList<NodalLoad>     NodalLoads     { get; }
    public IReadOnlyList<MemberUdl>     MemberUdls     { get; }
    public IReadOnlyList<ShellPressure> ShellPressures { get; }
    public IReadOnlyList<PlasticHinge>  Hinges         { get; }

    // P2.7: ctor wraps every list in a defensive new List<> so even a future caller that
    // bypasses FrameModelBuilder (e.g. an import path that hands us raw lists) cannot mutate
    // a built model by changing the source list afterwards.
    internal FrameModel(
        IReadOnlyList<Material> mats, IReadOnlyList<Section> secs,
        IReadOnlyList<Node> nodes, IReadOnlyList<Member> mems, IReadOnlyList<ShellQuad> shells,
        IReadOnlyList<NodalLoad> nl, IReadOnlyList<MemberUdl> udls,
        IReadOnlyList<ShellPressure> sps, IReadOnlyList<PlasticHinge> hinges)
    {
        Materials      = new List<Material>(mats);
        Sections       = new List<Section>(secs);
        Nodes          = new List<Node>(nodes);
        Members        = new List<Member>(mems);
        Shells         = new List<ShellQuad>(shells);
        NodalLoads     = new List<NodalLoad>(nl);
        MemberUdls     = new List<MemberUdl>(udls);
        ShellPressures = new List<ShellPressure>(sps);
        Hinges         = new List<PlasticHinge>(hinges);
    }
}
