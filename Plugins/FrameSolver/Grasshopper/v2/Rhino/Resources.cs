// Resources.cs -- embedded icon loader. Components call Resources.LoadIcon("name") in their
// Icon property override; this finds Resources/Icons/name.png (or .svg) embedded into the
// assembly. SVG is decoded to PNG at load time via Svg-Net (added in C2 when icons exist).
//
// For C1 (current milestone) all icons return a deterministic colored 24x24 placeholder so
// the components show in the ribbon without erroring on null Bitmap. C2 ships real SVGs.

using System.Drawing;
using System.IO;
using System.Reflection;

namespace FrameCore.Gh
{
    internal static class Resources
    {
        /// <summary>
        /// Load <paramref name="name"/> from embedded resources, falling back to a tinted
        /// placeholder. Caches per name.
        /// </summary>
        public static Bitmap? LoadIcon(string name)
        {
            if (_cache.TryGetValue(name, out var hit)) return hit;
            var asm = Assembly.GetExecutingAssembly();
            var resName = asm.GetManifestResourceNames();
            foreach (var rn in resName)
            {
                if (!rn.EndsWith($"Icons.{name}.png", System.StringComparison.OrdinalIgnoreCase) &&
                    !rn.EndsWith($"Icons.{name}.svg", System.StringComparison.OrdinalIgnoreCase))
                    continue;
                using var s = asm.GetManifestResourceStream(rn);
                if (s is null) continue;
                if (rn.EndsWith(".png"))
                {
                    var bmp = new Bitmap(s);
                    _cache[name] = bmp;
                    return bmp;
                }
                // SVG case deferred to C2 (needs Svg-Net dep).
            }
            // Placeholder: 24x24 solid square in a name-derived color so each component is
            // distinguishable on the canvas during C1 dev.
            var ph = MakePlaceholder(name);
            _cache[name] = ph;
            return ph;
        }

        private static Bitmap MakePlaceholder(string name)
        {
            var bmp = new Bitmap(24, 24);
            int hash = 0;
            foreach (var c in name) hash = hash * 31 + c;
            var color = Color.FromArgb(
                100 + System.Math.Abs(hash       ) % 156,
                100 + System.Math.Abs(hash >> 8  ) % 156,
                100 + System.Math.Abs(hash >> 16 ) % 156);
            using var g = Graphics.FromImage(bmp);
            g.Clear(color);
            return bmp;
        }

        private static readonly System.Collections.Generic.Dictionary<string, Bitmap> _cache = new();
    }
}
