// SteelFromLibraryComponent.cs -- pick a stock steel from MaterialLibrary by name. The
// value-list of available names is exposed via right-click "Pick steel grade", driving the
// underlying text param. Mirrors Karamba's "Material from Library" UX.

using System;
using System.Drawing;
using System.Linq;
using FrameCore.Bridge.Model;
using FrameCore.Gh.Common;
using FrameCore.Gh.Library;
using Grasshopper.Kernel;
using Grasshopper.Kernel.Parameters;

namespace FrameCore.Gh.Components.Material
{
    public sealed class SteelFromLibraryComponent : GH_Component
    {
        public SteelFromLibraryComponent()
            : base("Steel from Library", "Steel",
                   "Pick a stock structural-steel grade (S355 / Q345 / A572 Gr50 / ...).",
                   Tabs.Category, Tabs.Material) { }

        public override Guid ComponentGuid => new("D1F4C1AA-2A0B-4D3E-9C8E-FEE100000301");
        protected override Bitmap? Icon => Resources.LoadIcon("material-steel");

        protected override void RegisterInputParams(GH_InputParamManager p)
        {
            p.AddTextParameter("Grade", "G",
                "Steel grade name. Stock: " + string.Join(" / ", MaterialLibrary.All
                    .Where(e => e.Family == MaterialLibrary.Family.Steel)
                    .Select(e => e.Name)),
                GH_ParamAccess.item, "S355");
        }

        protected override void RegisterOutputParams(GH_OutputParamManager p)
        {
            p.AddParameter(new GhParameters.Param_Material(), "Material", "M",
                "Steel material with E/G/ρ/ν/cap populated.", GH_ParamAccess.item);
            p.AddTextParameter("Info", "I",
                "Standard, fy, density (info only).", GH_ParamAccess.item);
        }

        protected override void SolveInstance(IGH_DataAccess da)
        {
            string grade = "S355";
            da.GetData(0, ref grade);
            var entry = MaterialLibrary.FindByName(grade);
            if (entry is null || entry.Family != MaterialLibrary.Family.Steel)
            {
                AddRuntimeMessage(GH_RuntimeMessageLevel.Error,
                    $"Unknown steel grade '{grade}'. Available: " +
                    string.Join(", ", MaterialLibrary.All
                        .Where(e => e.Family == MaterialLibrary.Family.Steel)
                        .Select(e => e.Name)));
                return;
            }
            da.SetData(0, new GH_Material(entry.ToMaterial()));
            da.SetData(1, $"{entry.Standard} · fy={entry.Fy} MPa · ρ={entry.Rho * 1e12:0} kg/m³");
            Message = entry.Name;
        }

        // Right-click → drop a Value List preset with the steel names. Karamba-style discovery.
        public override void AddedToDocument(GH_Document document)
        {
            base.AddedToDocument(document);
            // Future: auto-attach a ValueList to input 0 with the grade choices.
        }
    }
}
