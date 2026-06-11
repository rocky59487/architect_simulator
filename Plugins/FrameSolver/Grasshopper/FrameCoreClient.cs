// FrameCoreClient.cs -- minimal reference client for the FrameCore CLI text bridge (S6 J1).
//
// [NOT GATED] This file is NOT built or tested by the engine's verification gate: it needs the
// .NET / Rhino 8 SDK, which is not available in the engine CI. It is a REFERENCE client that a
// Grasshopper component would wrap -- it has NO Rhino dependency itself (pure System.Diagnostics +
// string parsing), so it can be dropped into a GHA project or compiled standalone with `dotnet`.
// The authoritative wire protocol is docs/CLI_PROTOCOL.md; the engine-side end-to-end test that
// DOES gate the protocol is Tools/cli_roundtrip.py.
//
// Usage:
//   var client = new FrameCoreClient(@"...\frame_cli.exe");
//   var input  = new StringBuilder();
//   input.AppendLine("MAT 210000 80769 7850");
//   ... build the model ...
//   var res = client.Solve(input.ToString());
//   double tipUz = res.Disp[1][2];

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.Text;

namespace FrameCore.Bridge
{
    /// <summary>Parsed result of one frame_cli invocation. Only the fields the run emitted are filled.</summary>
    public sealed class FrameResult
    {
        public string Version;                                   // engine build SHA (VERSION handshake)
        public bool Singular;
        public readonly Dictionary<int, double[]> Disp = new Dictionary<int, double[]>();   // nodeId -> [ux..rz]
        public readonly Dictionary<int, double[]> Reactions = new Dictionary<int, double[]>();
        public readonly Dictionary<int, double[]> MemberForces = new Dictionary<int, double[]>(); // id -> 12
        // S6 analysis-mode summaries (null unless that command was run):
        public int[] TensionOnly;                                // [converged, cycled, iters]
        public readonly List<int> Slack = new List<int>();       // tension-only members left slack
        public int[] SizeOpt;                                    // [converged, iters, singular]
        public readonly Dictionary<int, double[]> Areas = new Dictionary<int, double[]>(); // id -> [A, DC]
        public double WeightVolume = double.NaN;                 // sum A*L
        public int[] DynCollapse;                                // [outcome, nEvents, nFrames]; outcome 0=Stable 1=Collapsed 2=MaxSteps 3=Invalid
        public double DynEndTime = double.NaN;
        public readonly string RawStdout;

        internal FrameResult(string raw) { RawStdout = raw; }
    }

    /// <summary>Shells out to frame_cli.exe over stdin/stdout. One instance per exe path; thread-safe per call.</summary>
    public sealed class FrameCoreClient
    {
        private readonly string _exePath;

        public FrameCoreClient(string exePath)
        {
            _exePath = exePath ?? throw new ArgumentNullException(nameof(exePath));
        }

        /// <summary>Run the engine on a model description (CLI text, with or without a trailing END).</summary>
        public FrameResult Solve(string modelText, int timeoutMs = 60000)
        {
            if (!modelText.TrimEnd().EndsWith("END"))
                modelText = modelText.TrimEnd() + "\nEND\n";

            var psi = new ProcessStartInfo(_exePath)
            {
                RedirectStandardInput = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
                CreateNoWindow = true
            };
            using (var p = new Process { StartInfo = psi })
            {
                p.Start();
                p.StandardInput.Write(modelText);
                p.StandardInput.Close();
                string stdout = p.StandardOutput.ReadToEnd();
                if (!p.WaitForExit(timeoutMs))
                {
                    try { p.Kill(); } catch { /* ignore */ }
                    throw new TimeoutException("frame_cli timed out");
                }
                if (p.ExitCode != 0)
                    throw new InvalidOperationException("frame_cli exit " + p.ExitCode + ": " + p.StandardError.ReadToEnd());
                return Parse(stdout);
            }
        }

        private static double D(string s) => double.Parse(s, CultureInfo.InvariantCulture);
        private static int I(string s) => int.Parse(s, CultureInfo.InvariantCulture);

        private static FrameResult Parse(string stdout)
        {
            var r = new FrameResult(stdout);
            foreach (var line in stdout.Split('\n'))
            {
                var t = line.Trim().Split(new[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries);
                if (t.Length == 0) continue;
                switch (t[0])
                {
                    case "VERSION":   r.Version = t.Length > 1 ? t[1] : ""; break;
                    case "SINGULAR":  r.Singular = I(t[1]) != 0; break;
                    case "DISP":      r.Disp[I(t[1])] = Slice(t, 2, 6); break;
                    case "RF":        r.Reactions[I(t[1])] = Slice(t, 2, 6); break;
                    case "MF":        r.MemberForces[I(t[1])] = Slice(t, 2, 12); break;
                    case "TONLY":     r.TensionOnly = new[] { I(t[1]), I(t[2]), I(t[3]) }; break;
                    case "SLACK":     for (int k = 1; k < t.Length; ++k) r.Slack.Add(I(t[k])); break;
                    case "SIZEOPT":   r.SizeOpt = new[] { I(t[1]), I(t[2]), I(t[3]) }; break;
                    case "AREA":      r.Areas[I(t[1])] = new[] { D(t[2]), D(t[3]) }; break;
                    case "WEIGHTVOL": r.WeightVolume = D(t[1]); break;
                    case "DYNC":      r.DynCollapse = new[] { I(t[1]), I(t[2]), I(t[3]) }; r.DynEndTime = D(t[4]); break;
                    // DEVENT / SF / FREQ / PDSTATUS: see docs/CLI_PROTOCOL.md -- add as the GH component needs them.
                }
            }
            return r;
        }

        private static double[] Slice(string[] t, int start, int count)
        {
            var v = new double[count];
            for (int k = 0; k < count; ++k) v[k] = D(t[start + k]);
            return v;
        }
    }
}
