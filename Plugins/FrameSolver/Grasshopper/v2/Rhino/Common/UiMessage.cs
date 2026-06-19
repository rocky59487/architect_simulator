// UiMessage.cs -- wire-tip status formatter. Every component sets `Component.Message` to a short
// formatted line at the end of SolveInstance; this helper keeps the format consistent across
// 80 components (S6c § 3.2).
//
// Format:   "[profile] [units] · [time]ms · [verdict]"
//   profile   "S" (simple) or "A" (advanced)
//   units     "SI" / "SI-kN" / "Imp"
//   time      integer milliseconds
//   verdict   "OK" / "{n} defaults" / "{n}/{m} errors" / "solving..." / "cancelled"
//
// GH itself paints the message strip in the color matching the message level: pass the right
// GH_RuntimeMessageLevel up alongside.

using FrameCore.Bridge;
using Grasshopper.Kernel;

namespace FrameCore.Gh.Common
{
    public static class UiMessage
    {
        public static (string text, GH_RuntimeMessageLevel level) Format(
            BridgeProfile profile, UnitSystem units, long elapsedMs,
            int defaults, int errors, int warnings, bool solving, bool cancelled)
        {
            var p = profile == BridgeProfile.Advanced ? "A" : "S";
            var u = Units.ShortName(units);

            if (cancelled) return ($"{p} {u} · cancelled", GH_RuntimeMessageLevel.Remark);
            if (solving)   return ($"{p} {u} · solving...", GH_RuntimeMessageLevel.Remark);
            if (errors > 0) return ($"{p} {u} · {elapsedMs}ms · {errors}/{errors + warnings} errors",
                                     GH_RuntimeMessageLevel.Error);
            if (defaults > 0)
                return ($"{p} {u} · {elapsedMs}ms · {defaults} defaults",
                         GH_RuntimeMessageLevel.Warning);
            if (warnings > 0)
                return ($"{p} {u} · {elapsedMs}ms · {warnings} warnings",
                         GH_RuntimeMessageLevel.Warning);
            return ($"{p} {u} · {elapsedMs}ms · ✓", GH_RuntimeMessageLevel.Remark);
        }
    }
}
