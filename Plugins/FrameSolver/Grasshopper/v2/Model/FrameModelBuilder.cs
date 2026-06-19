// FrameModelBuilder.cs -- fluent builder that hands out typed refs so the user can't transpose
// indices, forget a fixed flag, or refer to a not-yet-added node. Builds an immutable FrameModel.
//
// The builder DOES NOT serialize anything; it just collects strongly typed records. FrameSession
// converts the model to a model.set request body when the user calls SetModelAsync.

namespace FrameCore.Bridge.Model;

public sealed class FrameModelBuilder
{
    private readonly List<Material>      _mats   = new();
    private readonly List<Section>       _secs   = new();
    private readonly List<Node>          _nodes  = new();
    private readonly List<Member>        _mems   = new();
    private readonly List<ShellQuad>     _shells = new();
    private readonly List<NodalLoad>     _nl     = new();
    private readonly List<MemberUdl>     _udls   = new();
    private readonly List<ShellPressure> _sps    = new();
    private readonly List<PlasticHinge>  _hinges = new();

    // ----- Materials & Sections ---------------------------------------------------------------

    public MaterialRef AddMaterial(Material m) { _mats.Add(m); return new MaterialRef(_mats.Count - 1); }
    public SectionRef  AddSection(Section s)   { _secs.Add(s); return new SectionRef(_secs.Count - 1); }

    /// <summary>Steel beam material with default values (210000 / 80769 / 7.85e-9).</summary>
    public MaterialRef AddSteel() => AddMaterial(Material.Steel());

    /// <summary>Rectangular beam section convenience.</summary>
    public SectionRef AddRectSection(double b, double d) => AddSection(Section.Rectangular(b, d));

    // ----- Nodes ------------------------------------------------------------------------------

    /// <summary>Add a node with explicit support flags. Arrays are defensively copied (P2.7).</summary>
    public NodeRef AddNode(int id, double x, double y, double z, bool[]? fixedFlags = null,
                           double[]? prescribed = null)
    {
        var fix = fixedFlags ?? new bool[6];
        var pre = prescribed ?? new double[6];
        if (fix.Length != 6) throw new ArgumentException("fixed[] must be length 6", nameof(fixedFlags));
        if (pre.Length != 6) throw new ArgumentException("prescribed[] must be length 6", nameof(prescribed));
        _nodes.Add(new Node {
            Id = id, X = x, Y = y, Z = z,
            Fixed      = (bool[])fix.Clone(),
            Prescribed = (double[])pre.Clone()
        });
        return new NodeRef(id);
    }

    /// <summary>Free node (no support).</summary>
    public NodeRef AddFreeNode(int id, double x, double y, double z) => AddNode(id, x, y, z);

    /// <summary>Fully fixed support (all 6 DOFs).</summary>
    public NodeRef AddFixedNode(int id, double x, double y, double z) =>
        AddNode(id, x, y, z, new[] { true, true, true, true, true, true });

    /// <summary>Pinned support (translations fixed, rotations free).</summary>
    public NodeRef AddPinnedNode(int id, double x, double y, double z) =>
        AddNode(id, x, y, z, new[] { true, true, true, false, false, false });

    // ----- Members & Shells -------------------------------------------------------------------

    public MemberRef AddMember(int id, NodeRef i, NodeRef j, MaterialRef mat, SectionRef sec,
                               (double X, double Y, double Z)? refVec = null,
                               bool active = true, bool tensionOnly = false,
                               bool[]? release = null)
    {
        var r = release ?? new bool[12];
        if (r.Length != 12) throw new ArgumentException("release[] must be length 12", nameof(release));
        _mems.Add(new Member {
            Id = id, I = i, J = j, Material = mat, Section = sec,
            RefVec = refVec ?? (0, 0, 1),
            Active = active, TensionOnly = tensionOnly,
            Release = (bool[])r.Clone()      // P2.7: defensive copy
        });
        return new MemberRef(id);
    }

    public ShellRef AddShell(int id, NodeRef n0, NodeRef n1, NodeRef n2, NodeRef n3,
                             MaterialRef mat, double thickness, bool active = true)
    {
        _shells.Add(new ShellQuad {
            Id = id, N0 = n0, N1 = n1, N2 = n2, N3 = n3,
            Material = mat, Thickness = thickness, Active = active
        });
        return new ShellRef(id);
    }

    // ----- Loads ------------------------------------------------------------------------------

    public void AddNodalLoad(NodeRef node, double fx = 0, double fy = 0, double fz = 0,
                             double mx = 0, double my = 0, double mz = 0)
        // The array is fresh (`new[] {...}`), so no Clone() needed — but kept here for parity
        // with the Add* contract: every Add stores arrays the builder OWNS exclusively.
        => _nl.Add(new NodalLoad { Node = node, Components = new[] { fx, fy, fz, mx, my, mz } });

    public void AddUdl(MemberRef m, double wx, double wy, double wz)
        => _udls.Add(new MemberUdl { Member = m, Local = (wx, wy, wz) });

    public void AddShellPressure(ShellRef s, double p)
        => _sps.Add(new ShellPressure { Shell = s, P = p });

    public void AddHinge(MemberRef m, int dof, double mp)
        => _hinges.Add(new PlasticHinge { Member = m, Dof = dof, Mp = mp });

    // ----- Build ------------------------------------------------------------------------------

    /// <summary>
    /// Assemble the immutable model. Performs LOCAL sanity checks only (length 6/12 arrays,
    /// no duplicate ids); the engine runs the authoritative <c>FrameModel::validate()</c>
    /// inside model.set and returns VALIDATION_FAILED with the same diagnostic string.
    ///
    /// P2.7 fix: every list AND every array inside Node/Member/NodalLoad/PlasticHinge is
    /// deep-copied into the model so subsequent edits to this builder (or to the caller's
    /// original arrays) cannot mutate the FrameModel after Build() returns. Materials/Sections
    /// are POCO records with init-only properties, so a shallow list copy is sufficient.
    /// </summary>
    public FrameModel Build()
    {
        AssertUniqueIds(_nodes.Select(n => n.Id), "node");
        AssertUniqueIds(_mems.Select(m => m.Id),  "member");
        AssertUniqueIds(_shells.Select(s => s.Id), "shell");

        var nodesCopy = new List<Node>(_nodes.Count);
        foreach (var n in _nodes)
            nodesCopy.Add(new Node
            {
                Id = n.Id, X = n.X, Y = n.Y, Z = n.Z,
                Fixed      = (bool[])n.Fixed.Clone(),
                Prescribed = (double[])n.Prescribed.Clone()
            });

        var memsCopy = new List<Member>(_mems.Count);
        foreach (var m in _mems)
            memsCopy.Add(new Member
            {
                Id = m.Id, I = m.I, J = m.J,
                Material = m.Material, Section = m.Section,
                RefVec = m.RefVec, Active = m.Active, TensionOnly = m.TensionOnly,
                Release = (bool[])m.Release.Clone()
            });

        var shellsCopy = new List<ShellQuad>(_shells);    // ShellQuad has no internal arrays

        var nlCopy = new List<NodalLoad>(_nl.Count);
        foreach (var l in _nl)
            nlCopy.Add(new NodalLoad { Node = l.Node, Components = (double[])l.Components.Clone() });

        var udlsCopy = new List<MemberUdl>(_udls);        // MemberUdl is value-type tuple
        var spsCopy  = new List<ShellPressure>(_sps);
        var hingesCopy = new List<PlasticHinge>(_hinges);

        return new FrameModel(
            new List<Material>(_mats), new List<Section>(_secs),
            nodesCopy, memsCopy, shellsCopy,
            nlCopy, udlsCopy, spsCopy, hingesCopy);
    }

    private static void AssertUniqueIds(IEnumerable<int> ids, string kind)
    {
        var seen = new HashSet<int>();
        foreach (var id in ids)
            if (!seen.Add(id))
                throw new InvalidOperationException($"duplicate {kind} id {id}");
    }
}
