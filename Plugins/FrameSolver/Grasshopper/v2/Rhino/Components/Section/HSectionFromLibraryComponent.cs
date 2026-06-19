// HSectionFromLibraryComponent.cs -- pick a stock H/IPE/HEA/HEB/W from SectionLibrary.

using System;
using System.Drawing;
using FrameCore.Bridge.Model;
using FrameCore.Gh.Common;
using FrameCore.Gh.Library;
using Grasshopper.Kernel;

namespace FrameCore.Gh.Components.Section
{
    public sealed class HSectionFromLibraryComponent : GH_Component
    {
        public HSectionFromLibraryComponent()
            : base("H-Section from Library", "H-Sect",
                   "Pick a stock H-section (GB HW/HM/HN, EU IPE/HEA/HEB, US W).",
                   Tabs.Category, Tabs.Section) { }

        public override Guid ComponentGuid => new("D1F4C1AA-2A0B-4D3E-9C8E-FEE100000401");
        protected override Bitmap? Icon => Resources.LoadIcon("section-hshape");

        protected override void RegisterInputParams(GH_InputParamManager p)
        {
            p.AddTextParameter("Name", "N",
                "Section name (case-insensitive). Examples: HW400x400, IPE300, HEA200, W14x82.",
                GH_ParamAccess.item, "IPE300");
        }

        protected override void RegisterOutputParams(GH_OutputParamManager p)
        {
            p.AddParameter(new GhParameters.Param_Section(), "Section", "S",
                "Cross section with A/Iy/Iz/J/cy/cz/Asy/Asz populated.", GH_ParamAccess.item);
            p.AddTextParameter("Info", "I",
                "Family, mass/metre (info only).", GH_ParamAccess.item);
        }

        protected override void SolveInstance(IGH_DataAccess da)
        {
            string name = "IPE300";
            da.GetData(0, ref name);
            var entry = SectionLibrary.FindByName(name);
            if (entry is null)
            {
                AddRuntimeMessage(GH_RuntimeMessageLevel.Error, $"Unknown section '{name}'.");
                return;
            }
            da.SetData(0, new GH_Section(entry.ToSection()));
            da.SetData(1, $"{entry.Family} · m/L = {entry.MassPerMetre:F1} kg/m · A = {entry.A:F0} mm²");
            Message = entry.Name;
        }
    }
}
