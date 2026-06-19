// MaterialLibrary.cs -- 11 stock materials covering the four most-common families. Values from
// EN 10025 / GB/T 1591 / ASTM A36/A572 / GB 50010 / Aluminum Association / NDS-2018.
//
// The capacity (cap.comp / cap.tens / cap.shear) is the ELASTIC ALLOWABLE used by the engine's
// D/C screen — NOT an RC ultimate, NOT a code-compliant Mn. Computed from fy via a uniform
// gamma_M = 1.5 for steel and from fc' / fct / fv for concrete (GB 50010 Table 4.1). Designers
// who need code-compliant capacities replace these in the GH definition; the library is a
// "good starting point" not a regulator-blessed table.
//
// Densities are in engine units (tonne/mm^3). Conversion table:
//   steel  7850 kg/m^3 = 7.85e-9 tonne/mm^3
//   concrete 2500 kg/m^3 = 2.5e-9 tonne/mm^3
//   aluminum 2700 kg/m^3 = 2.7e-9 tonne/mm^3
//   wood SPF 380 kg/m^3 = 3.8e-10 tonne/mm^3

using System.Collections.Generic;
using FrameCore.Bridge.Model;

namespace FrameCore.Gh.Library
{
    public static class MaterialLibrary
    {
        public enum Family { Steel, Concrete, Aluminum, Wood }

        public sealed class Entry
        {
            public string Name = "";
            public string Standard = "";
            public Family Family;
            public double E, G, Nu, Rho, Fy;
            public Capacity Cap;
            public string Notes = "";

            public Material ToMaterial()
            {
                return new Material {
                    E = E, G = G, Rho = Rho, Nu = Nu, Cap = Cap
                };
            }
        }

        public static IReadOnlyList<Entry> All { get; } = new[]
        {
            // ---- European structural steel (EN 10025-2) ----
            new Entry { Name="S235", Standard="EN 10025-2", Family=Family.Steel,
                        E=210000, G=80769, Nu=0.3, Rho=7.85e-9, Fy=235,
                        Cap=new Capacity(156, 156, 93),
                        Notes="Mild structural steel, fy=235 MPa, fu=360 MPa." },
            new Entry { Name="S275", Standard="EN 10025-2", Family=Family.Steel,
                        E=210000, G=80769, Nu=0.3, Rho=7.85e-9, Fy=275,
                        Cap=new Capacity(183, 183, 110) },
            new Entry { Name="S355", Standard="EN 10025-2", Family=Family.Steel,
                        E=210000, G=80769, Nu=0.3, Rho=7.85e-9, Fy=355,
                        Cap=new Capacity(236, 236, 142),
                        Notes="Common EU mid-strength; bridges, frames." },

            // ---- Chinese structural steel (GB/T 1591) ----
            new Entry { Name="Q235B", Standard="GB/T 700",  Family=Family.Steel,
                        E=206000, G=79231, Nu=0.3, Rho=7.85e-9, Fy=235,
                        Cap=new Capacity(156, 156, 93) },
            new Entry { Name="Q345B", Standard="GB/T 1591", Family=Family.Steel,
                        E=206000, G=79231, Nu=0.3, Rho=7.85e-9, Fy=345,
                        Cap=new Capacity(229, 229, 138) },
            new Entry { Name="Q420C", Standard="GB/T 1591", Family=Family.Steel,
                        E=206000, G=79231, Nu=0.3, Rho=7.85e-9, Fy=420,
                        Cap=new Capacity(279, 279, 167) },

            // ---- US structural steel (ASTM) ----
            new Entry { Name="A36",       Standard="ASTM A36",      Family=Family.Steel,
                        E=200000, G=76923, Nu=0.3, Rho=7.85e-9, Fy=250,
                        Cap=new Capacity(166, 166, 100) },
            new Entry { Name="A572 Gr50", Standard="ASTM A572 Gr50", Family=Family.Steel,
                        E=200000, G=76923, Nu=0.3, Rho=7.85e-9, Fy=345,
                        Cap=new Capacity(229, 229, 138) },

            // ---- Concrete (GB 50010) ----
            // For concrete, fy stored is design fc (compressive); cap.tens is design fct.
            new Entry { Name="C30", Standard="GB 50010", Family=Family.Concrete,
                        E=30000, G=12500, Nu=0.2, Rho=2.5e-9, Fy=14.3,
                        Cap=new Capacity(14.3, 1.43, 1.5),
                        Notes="fc=14.3 MPa, ft=1.43 MPa; elastic screening only." },
            new Entry { Name="C50", Standard="GB 50010", Family=Family.Concrete,
                        E=34500, G=14375, Nu=0.2, Rho=2.5e-9, Fy=23.1,
                        Cap=new Capacity(23.1, 1.89, 1.83) },

            // ---- Aluminum ----
            new Entry { Name="Al 6061-T6", Standard="AA AMS-QQ-A-200/8", Family=Family.Aluminum,
                        E=69000,  G=26000, Nu=0.33, Rho=2.7e-9, Fy=276,
                        Cap=new Capacity(183, 183, 110) },
        };

        public static Entry? FindByName(string name)
        {
            foreach (var e in All)
                if (string.Equals(e.Name, name, System.StringComparison.OrdinalIgnoreCase))
                    return e;
            return null;
        }

        public static IReadOnlyList<string> Names => System.Array.AsReadOnly(
            System.Linq.Enumerable.ToArray(System.Linq.Enumerable.Select(All, e => e.Name)));
    }
}
