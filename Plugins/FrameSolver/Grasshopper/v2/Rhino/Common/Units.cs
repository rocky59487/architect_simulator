// Units.cs -- input-time unit conversion to the engine's fixed N-mm-MPa convention.
//
// The engine is fixed: lengths in mm, forces in N, stresses in MPa (= N/mm^2), densities in
// tonne/mm^3 (so M omega^2 has units of N). This file keeps the engine fixed and lets the GH
// surface accept SI-kN-m or US Imperial without leaking the convention everywhere.
//
// All Display components reverse-translate back to whichever UnitSystem the upstream `Units`
// component produced — Disp shown in mm/m/inch as the user asked, Reactions in N/kN/kip, etc.
//
// We DO NOT do unit math on the wire: every engine call uses N-mm-MPa. Conversion is purely
// a client-side decoration applied at the param-read / param-write boundary.

namespace FrameCore.Gh.Common
{
    public enum UnitSystem
    {
        SI_N_mm_MPa     = 0,    // engine-native
        SI_kN_m_kPa     = 1,
        Imperial_kip_in_ksi = 2
    }

    public static class Units
    {
        // ----- to engine -------------------------------------------------------------------
        public static double LengthToEngine(double v, UnitSystem u) => u switch {
            UnitSystem.SI_kN_m_kPa        => v * 1000.0,
            UnitSystem.Imperial_kip_in_ksi => v * 25.4,
            _ => v
        };

        public static double ForceToEngine(double v, UnitSystem u) => u switch {
            UnitSystem.SI_kN_m_kPa        => v * 1000.0,            // kN -> N
            UnitSystem.Imperial_kip_in_ksi => v * 4448.222,          // kip -> N
            _ => v
        };

        public static double MomentToEngine(double v, UnitSystem u) => u switch {
            UnitSystem.SI_kN_m_kPa        => v * 1.0e6,             // kN*m -> N*mm
            UnitSystem.Imperial_kip_in_ksi => v * 4448.222 * 25.4,   // kip*in -> N*mm
            _ => v
        };

        public static double StressToEngine(double v, UnitSystem u) => u switch {
            UnitSystem.SI_kN_m_kPa        => v * 1.0e-3,            // kPa -> MPa
            UnitSystem.Imperial_kip_in_ksi => v * 6.89476,           // ksi -> MPa
            _ => v
        };

        public static double DensityToEngine(double v, UnitSystem u) => u switch {
            // SI kg/m^3 -> engine tonne/mm^3 (Material rho is engine tonne/mm^3)
            UnitSystem.SI_kN_m_kPa        => v * 1.0e-12,
            // pcf -> tonne/mm^3
            UnitSystem.Imperial_kip_in_ksi => v * 1.602e-14,
            _ => v
        };

        // ----- back from engine ------------------------------------------------------------
        public static double LengthFromEngine(double v, UnitSystem u) => u switch {
            UnitSystem.SI_kN_m_kPa        => v / 1000.0,
            UnitSystem.Imperial_kip_in_ksi => v / 25.4,
            _ => v
        };

        public static double ForceFromEngine(double v, UnitSystem u) => u switch {
            UnitSystem.SI_kN_m_kPa        => v / 1000.0,
            UnitSystem.Imperial_kip_in_ksi => v / 4448.222,
            _ => v
        };

        public static double MomentFromEngine(double v, UnitSystem u) => u switch {
            UnitSystem.SI_kN_m_kPa        => v / 1.0e6,
            UnitSystem.Imperial_kip_in_ksi => v / (4448.222 * 25.4),
            _ => v
        };

        public static double StressFromEngine(double v, UnitSystem u) => u switch {
            UnitSystem.SI_kN_m_kPa        => v * 1000.0,
            UnitSystem.Imperial_kip_in_ksi => v / 6.89476,
            _ => v
        };

        public static string LabelLength(UnitSystem u) => u switch {
            UnitSystem.SI_kN_m_kPa        => "m",
            UnitSystem.Imperial_kip_in_ksi => "in",
            _ => "mm"
        };

        public static string LabelForce(UnitSystem u) => u switch {
            UnitSystem.SI_kN_m_kPa        => "kN",
            UnitSystem.Imperial_kip_in_ksi => "kip",
            _ => "N"
        };

        public static string LabelMoment(UnitSystem u) => u switch {
            UnitSystem.SI_kN_m_kPa        => "kN·m",
            UnitSystem.Imperial_kip_in_ksi => "kip·in",
            _ => "N·mm"
        };

        public static string LabelStress(UnitSystem u) => u switch {
            UnitSystem.SI_kN_m_kPa        => "kPa",
            UnitSystem.Imperial_kip_in_ksi => "ksi",
            _ => "MPa"
        };

        public static string ShortName(UnitSystem u) => u switch {
            UnitSystem.SI_kN_m_kPa        => "SI-kN",
            UnitSystem.Imperial_kip_in_ksi => "Imp",
            _ => "SI"
        };
    }
}
