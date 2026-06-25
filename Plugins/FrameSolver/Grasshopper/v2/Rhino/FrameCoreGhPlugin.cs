// FrameCoreGhPlugin.cs -- Grasshopper assembly entry. Discovered by Grasshopper at load time;
// the GUI then enumerates GH_Component subclasses in this assembly and places them in their
// declared Categories/Subcategories. Tabs are the *Subcategory* strings; the *Category*
// "FrameCore" is one ribbon.
//
// We do nothing else here -- no static state, no engine load. The first OpenFrameCore component
// the user drops on the canvas allocates a CApiV2Transport and runs the hello handshake. This
// keeps Rhino startup fast (no DLL load until needed) and lets users uninstall by deleting the
// .gha without engine residue.

using System;
using System.Drawing;
using Grasshopper;
using Grasshopper.Kernel;

namespace FrameCore.Gh
{
    public sealed class FrameCoreGhInfo : GH_AssemblyInfo
    {
        public override string  Name        => "FrameCore";
        public override Bitmap? Icon        => Resources.LoadIcon("framecore-assembly");
        public override string  Description =>
            "FrameCore — 3D elastic FEA with progressive collapse, MITC4 shells, supernodal solver, " +
            "and a Karamba3D-compatible bridge.";
        public override Guid    Id          => new Guid("D1F4C1AA-2A0B-4D3E-9C8E-FEE100C0DE01");
        public override string  AuthorName  => "FrameCore Team";
        public override string  AuthorContact => "https://github.com/rocky59487/architect-";
        public override string  Version     => "2.0.0-alpha.1";

        // Used when ranking ribbon tabs (lower = leftmost).
        public override GH_LibraryLicense AssemblyLicense => GH_LibraryLicense.opensource;
    }

    /// <summary>
    /// Centralised constants shared by every FrameCore component (tab names, colors, defaults).
    /// Pull from here so renaming a tab is one edit, not 80.
    /// </summary>
    internal static class Tabs
    {
        public const string Category = "FrameCore";

        public const string Setup     = "1. Setup";
        public const string Material  = "2. Material";
        public const string Section   = "3. Section";
        public const string Geometry  = "4. Geometry";
        public const string Boundary  = "5. Boundary";
        public const string Load      = "6. Load";
        public const string Analyze   = "7. Analyze";
        public const string Inspect   = "8. Inspect";
        public const string Display   = "9. Display";
        public const string Advanced  = "10. Advanced";
        public const string IO        = "11. IO";

        // Tab tint colors (S6c § 3.1)
        public static readonly Color TintSetup    = Color.FromArgb(0x4A, 0x90, 0xE2);
        public static readonly Color TintMaterial = Color.FromArgb(0x7B, 0x8A, 0x8B);
        public static readonly Color TintSection  = Color.FromArgb(0xF5, 0xA6, 0x23);
        public static readonly Color TintGeometry = Color.FromArgb(0x7E, 0xD3, 0x21);
        public static readonly Color TintBoundary = Color.FromArgb(0x90, 0x13, 0xFE);
        public static readonly Color TintLoad     = Color.FromArgb(0xD0, 0x02, 0x1B);
        public static readonly Color TintAnalyze  = Color.FromArgb(0x00, 0x3F, 0x87);
        public static readonly Color TintInspect  = Color.FromArgb(0xFF, 0xD7, 0x00);
        public static readonly Color TintDisplay  = Color.FromArgb(0x39, 0xC1, 0x2D);
        public static readonly Color TintAdvanced = Color.FromArgb(0x1E, 0x1E, 0x1E);
        public static readonly Color TintIO       = Color.FromArgb(0x95, 0xA5, 0xA6);
    }
}
