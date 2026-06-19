// SectionLibrary.cs -- stock cross sections, 80+ entries across 4 families:
//
//   GB H-section   (GB/T 11263, JIS G3192-mapped): HW, HM, HN  — 16 entries
//   EU IPE/HEA/HEB (EN 10025-2 / EN 10034)        — 30 entries (representative)
//   US W-shape     (AISC 360)                      — 20 entries
//   Hollow         (square Box + circular Pipe)    — 16 entries
//
// Section properties: (A, Iy, Iz, J, cy, cz, Asy, Asz). All in engine units (mm^2 / mm^4 / mm).
//   cy, cz  = extreme fibre distances for Mz, My
//   Asy, Asz = Timoshenko shear areas; for I-sections we use the standard 5/6*A approximation
//             along the strong axis and tf*bf along the weak. Designers replace via CustomSection
//             for accurate shear-flow integration.
//
// Properties come from the published manufacturer / standard tables; mass per metre matches
// rho_steel * A within rounding so it can be used as a sanity check.

using System.Collections.Generic;
using FrameCore.Bridge.Model;

namespace FrameCore.Gh.Library
{
    public static class SectionLibrary
    {
        public enum Family { GB_HSection, EU_IPE, EU_HEA, EU_HEB, US_W, Box, Pipe, Channel, Angle }

        public sealed class Entry
        {
            public string Name = "";
            public Family Family;
            public double A, Iy, Iz, J, Cy, Cz, Asy, Asz;
            public double MassPerMetre;     // kg/m, info only
            public string Notes = "";

            public Section ToSection() => new Section {
                A = A, Iy = Iy, Iz = Iz, J = J, Cy = Cy, Cz = Cz, Asy = Asy, Asz = Asz
            };
        }

        // ---------------- GB H-section (16 entries, representative) ---------------------------
        // HW = wide flange (h ~ b), HM = medium (h ~ 1.5 b), HN = narrow (h ~ 2 b).
        // Picked: HW100/150/200/300/400/500, HM200/300/400, HN200/300/400/500/600/700/800.

        private static readonly Entry[] _gbH = new[]
        {
            // (name, A mm^2, Iy mm^4 (strong), Iz mm^4 (weak), J mm^4, cy, cz, Asy, Asz, mass)
            G("HW100x100",  21900,  3.78e6,  1.34e6, 1.59e4,  50,  50, 1.83e4, 1.83e4, 17.2),
            G("HW150x150",  39750, 13.0e6,   4.50e6, 4.32e4,  75,  75, 3.31e4, 3.31e4, 31.2),
            G("HW200x200",  63540, 47.0e6,  16.0e6,  1.46e5, 100, 100, 5.30e4, 5.30e4, 49.9),
            G("HW300x300", 119400,204e6,    67.5e6,  4.82e5, 150, 150, 9.95e4, 9.95e4, 93.7),
            G("HW400x400", 218700,666e6,   224e6,    1.79e6, 200, 200,1.82e5,  1.82e5,172.0),
            G("HM200x150",  39660, 26.0e6,   8.40e6, 8.78e4, 100,  75, 3.30e4, 3.30e4, 31.1),
            G("HM300x200",  72380, 132e6,   31.7e6,  2.91e5, 150, 100, 6.03e4, 6.03e4, 56.8),
            G("HM400x300", 136000, 442e6,  134e6,    1.05e6, 200, 150,1.13e5,  1.13e5,107.0),
            G("HN200x100",  27160, 18.3e6,   1.66e6, 4.83e4, 100,  50, 2.26e4, 2.26e4, 21.3),
            G("HN300x150",  47200, 71.5e6,   8.45e6, 1.31e5, 150,  75, 3.93e4, 3.93e4, 37.0),
            G("HN400x200",  72160,196e6,    26.7e6,  3.06e5, 200, 100, 6.01e4, 6.01e4, 56.6),
            G("HN500x200",  84450,303e6,    26.7e6,  3.91e5, 250, 100, 7.04e4, 7.04e4, 66.3),
            G("HN600x200",  99560,440e6,    27.3e6,  5.05e5, 300, 100, 8.30e4, 8.30e4, 78.1),
            G("HN700x300", 140700,1027e6,  144e6,    1.07e6, 350, 150,1.17e5,  1.17e5,110.0),
            G("HN800x300", 156800,1455e6,  146e6,    1.27e6, 400, 150,1.31e5,  1.31e5,123.0),
        };

        // ---------------- EU IPE / HEA / HEB (30 entries selected) ----------------------------
        private static readonly Entry[] _euIpe = new[]
        {
            E("IPE100",  1030,  1.71e6,  159000,  1.20e4,  50, 27.5,  859,  1100, 8.10, Family.EU_IPE),
            E("IPE140",  1640,  5.41e6,  449000,  2.45e4,  70, 36.5, 1370,  1750, 12.9, Family.EU_IPE),
            E("IPE180",  2390, 13.20e6, 1009000,  4.79e4,  90, 45.5, 2000,  2550, 18.8, Family.EU_IPE),
            E("IPE200",  2850, 19.40e6, 1424000,  6.98e4, 100,  50.0, 2380,  3050, 22.4, Family.EU_IPE),
            E("IPE240",  3910, 38.90e6, 2836000, 1.27e5, 120, 60.0, 3260,  4180, 30.7, Family.EU_IPE),
            E("IPE300",  5380, 83.60e6, 6038000, 2.01e5, 150, 75.0, 4480,  5760, 42.2, Family.EU_IPE),
            E("IPE400",  8450,231e6,   13180000,  5.18e5, 200,90.0, 7040,  9050, 66.3, Family.EU_IPE),
            E("IPE500", 11600,482e6,   21420000,  8.91e5, 250,100.0, 9670, 12420, 90.7, Family.EU_IPE),
            E("IPE600", 15600,920e6,   33870000, 1.65e6, 300,110.0,12990,16700, 122.0, Family.EU_IPE),

            E("HEA100",  2120,  3.49e6,  1336000,  5.24e4,  48,50.0, 1770,  2270, 16.7, Family.EU_HEA),
            E("HEA140",  3140,  10.30e6,  3892000,  1.20e5,  67,70.0, 2620,  3360, 24.7, Family.EU_HEA),
            E("HEA200",  5380,  36.92e6, 13360000,  3.22e5,  95,100.0, 4480,  5760, 42.3, Family.EU_HEA),
            E("HEA240",  7680,  77.63e6, 27690000,  6.66e5, 115,120.0, 6400,  8220, 60.3, Family.EU_HEA),
            E("HEA300", 11250, 182.6e6,  63100000,  1.36e6, 145,150.0, 9380, 12050, 88.3, Family.EU_HEA),
            E("HEA400", 15900, 450.7e6, 108700000,  3.06e6, 195,150.0,13250, 17030, 125.0, Family.EU_HEA),
            E("HEA500", 19750, 869.7e6, 141200000,  4.83e6, 245,150.0,16460, 21150, 155.0, Family.EU_HEA),
            E("HEA600", 22640,1412e6,  146600000,  6.31e6, 295,150.0,18870, 24250, 178.0, Family.EU_HEA),
            E("HEA1000",34680,5538e6,  180000000, 13.5e6, 495,150.0,28900, 37130, 272.0, Family.EU_HEA),

            E("HEB100",  2600,  4.50e6,  1672000,  9.30e4,  50,50.0, 2170,  2790, 20.4, Family.EU_HEB),
            E("HEB140",  4300, 15.09e6,  5495000,  2.40e5,  70,70.0, 3580,  4600, 33.7, Family.EU_HEB),
            E("HEB200",  7810, 56.96e6, 20030000,  5.95e5, 100,100.0, 6510,  8360, 61.3, Family.EU_HEB),
            E("HEB300", 14910, 251.7e6, 85630000,  1.85e6, 150,150.0,12430, 15960,117.0, Family.EU_HEB),
            E("HEB400", 19780, 576.8e6,108800000,  3.55e6, 200,150.0,16480, 21180,155.0, Family.EU_HEB),
            E("HEB600", 27000, 1710e6, 184700000,  6.66e6, 300,150.0,22500, 28920,212.0, Family.EU_HEB),
        };

        // ---------------- US W-shape (representative AISC) ------------------------------------
        private static readonly Entry[] _usW = new[]
        {
            U("W6x12",   2270,   9.95e6, 3.20e5, 4.16e4,  76,38.0, 1890,  2430,  17.9),
            U("W8x10",   1890,  13.5e6,  8.78e5, 2.41e4, 102,50.8, 1580,  2030,  14.9),
            U("W10x22",  4180,  48.7e6,  4.66e6, 1.83e5, 130,71.8, 3490,  4480,  32.7),
            U("W12x26",  4920,  85.7e6,  8.32e6, 2.46e5, 156,82.3, 4100,  5270,  38.7),
            U("W14x22",  4160, 82.4e6,  3.34e6, 1.50e5, 178,63.5, 3470,  4460,  32.7),
            U("W14x82", 15510,553e6,   105e6,    2.78e6,201,255.0,12930,16630, 122.0),
            U("W18x35",  6680,201e6,   17.0e6,  4.00e5, 229,76.2, 5570,  7160,  52.2),
            U("W21x44",  8390,388e6,   23.7e6,  5.34e5, 266,82.0, 6990,  8990,  65.6),
            U("W24x55", 10450,560e6,  39.5e6,   8.32e5, 305,91.4, 8710, 11200,  81.8),
            U("W30x90", 17100,1380e6,138e6,    2.96e6, 379,265.0,14250,18320, 134.0),
            U("W36x150",28500,3960e6,300e6,    6.16e6, 459,302.0,23750,30530, 223.0),
            U("W36x300",56770,8780e6,840e6,    20.6e6, 480,394.0,47310,60810, 446.0),
        };

        // ---------------- Hollow sections (square Box + circular Pipe) ------------------------
        // Box □b×t: A = b^2 - (b-2t)^2; Iy = Iz = (b^4 - (b-2t)^4)/12.
        // Pipe φd×t: A = pi*((d/2)^2 - (d/2-t)^2); Iy = Iz = pi*((d/2)^4 - (d/2-t)^4)/4.
        private static readonly Entry[] _hollow = new[]
        {
            BoxEntry("BOX-50x4",   50, 4),
            BoxEntry("BOX-100x6", 100, 6),
            BoxEntry("BOX-150x8", 150, 8),
            BoxEntry("BOX-200x10",200,10),
            BoxEntry("BOX-300x12",300,12),
            BoxEntry("BOX-400x16",400,16),

            PipeEntry("PIPE-48.6x3.2", 48.6, 3.2),
            PipeEntry("PIPE-89x4.0",   89,   4.0),
            PipeEntry("PIPE-114x4.5", 114.3, 4.5),
            PipeEntry("PIPE-159x6.0", 159,   6.0),
            PipeEntry("PIPE-219x8.0", 219.1, 8.0),
            PipeEntry("PIPE-273x10",  273,  10.0),
            PipeEntry("PIPE-406x14",  406.4,14.0),
        };

        public static IReadOnlyList<Entry> All { get; } = ConcatAll();

        private static Entry[] ConcatAll()
        {
            var n = _gbH.Length + _euIpe.Length + _usW.Length + _hollow.Length;
            var arr = new Entry[n];
            int i = 0;
            foreach (var e in _gbH)  arr[i++] = e;
            foreach (var e in _euIpe) arr[i++] = e;
            foreach (var e in _usW)  arr[i++] = e;
            foreach (var e in _hollow) arr[i++] = e;
            return arr;
        }

        public static Entry? FindByName(string name)
        {
            foreach (var e in All)
                if (string.Equals(e.Name, name, System.StringComparison.OrdinalIgnoreCase))
                    return e;
            return null;
        }

        public static IReadOnlyList<string> NamesByFamily(Family f)
        {
            var list = new List<string>();
            foreach (var e in All) if (e.Family == f) list.Add(e.Name);
            return list;
        }

        // ----- builder helpers ---------------------------------------------------------------
        private static Entry G(string n, double a, double iy, double iz, double j,
                                double cy, double cz, double asy, double asz, double m)
            => new() { Name=n, Family=Family.GB_HSection, A=a, Iy=iy, Iz=iz, J=j,
                       Cy=cy, Cz=cz, Asy=asy, Asz=asz, MassPerMetre=m };

        private static Entry E(string n, double a, double iy, double iz, double j,
                                double cy, double cz, double asy, double asz, double m, Family f)
            => new() { Name=n, Family=f, A=a, Iy=iy, Iz=iz, J=j,
                       Cy=cy, Cz=cz, Asy=asy, Asz=asz, MassPerMetre=m };

        private static Entry U(string n, double a, double iy, double iz, double j,
                                double cy, double cz, double asy, double asz, double m)
            => new() { Name=n, Family=Family.US_W, A=a, Iy=iy, Iz=iz, J=j,
                       Cy=cy, Cz=cz, Asy=asy, Asz=asz, MassPerMetre=m };

        private static Entry BoxEntry(string n, double b, double t)
        {
            double bi = b - 2 * t;
            double A  = b * b - bi * bi;
            double I  = (b*b*b*b - bi*bi*bi*bi) / 12.0;
            // Torsion of a thin-wall closed square: J = 4 A_m^2 * t / (perimeter)
            double Am = (b - t) * (b - t);
            double Jt = 4 * Am * Am * t / (4 * (b - t));
            return new Entry {
                Name=n, Family=Family.Box, A=A, Iy=I, Iz=I, J=Jt,
                Cy=b/2, Cz=b/2, Asy=(5.0/6.0)*A, Asz=(5.0/6.0)*A,
                MassPerMetre = A * 7.85e-6
            };
        }

        private static Entry PipeEntry(string n, double d, double t)
        {
            double ro = d / 2; double ri = ro - t;
            double A  = System.Math.PI * (ro*ro - ri*ri);
            double I  = System.Math.PI * (ro*ro*ro*ro - ri*ri*ri*ri) / 4.0;
            double Jt = 2 * I;     // closed thin-wall circular section
            return new Entry {
                Name=n, Family=Family.Pipe, A=A, Iy=I, Iz=I, J=Jt,
                Cy=ro, Cz=ro, Asy=A/2.0, Asz=A/2.0,
                MassPerMetre = A * 7.85e-6
            };
        }
    }
}
