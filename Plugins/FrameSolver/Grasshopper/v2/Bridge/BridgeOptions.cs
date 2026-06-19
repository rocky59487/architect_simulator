// BridgeOptions.cs -- choose which Layer 2 transport carries the v2 frames.
//
// The same FrameSession class drives any of these transports; the wire schema is identical.
// This is the OPEN/CLOSED principle in action: adding a new transport (TCP, named-pipe) only
// adds an enum value + a derived ITransport implementation; the rest of the SDK and the
// hosting Grasshopper components don't change.

using System.Net;

namespace FrameCore.Bridge;

public enum TransportKind
{
    /// <summary>frame_capi_v2.dll loaded in-process via P/Invoke. Lowest latency, the default.</summary>
    CApiV2InProcess,

    /// <summary>
    /// frame_cli_v2.exe subprocess driven over stdin/stdout, framed binary. Use when the host
    /// process cannot load native DLLs (e.g. sandboxed Rhino plugin loader) or when fault
    /// isolation is desired (a crash in the solver does not bring down Rhino).
    /// </summary>
    Stdio,

    /// <summary>
    /// Windows named pipe (\\.\pipe\framecore_v2_*). Long-running daemon shared by several
    /// Rhino sessions on the same machine. B6 milestone — not in B1.
    /// </summary>
    NamedPipe,

    /// <summary>
    /// TCP socket. For cloud / split-machine scenarios. B6 milestone — not in B1.
    /// </summary>
    Tcp,
}

/// <summary>
/// Configuration for opening a <see cref="FrameSession"/>. Use the named-parameter syntax
/// (`new BridgeOptions { Kind = ..., FrameCapiV2DllPath = ... }`) -- the record's positional
/// ctor is intentionally absent so future fields don't break call sites.
/// </summary>
public sealed record BridgeOptions
{
    /// <summary>Which Layer-2 transport to use. Defaults to in-process C ABI v2.</summary>
    public TransportKind Kind { get; init; } = TransportKind.CApiV2InProcess;

    /// <summary>
    /// Absolute path to frame_capi_v2.dll. When null and <see cref="Kind"/> is
    /// <see cref="TransportKind.CApiV2InProcess"/>, the SDK resolves it relative to the
    /// hosting assembly directory (the convention used by .gha component packaging).
    /// </summary>
    public string? FrameCapiV2DllPath { get; init; }

    /// <summary>Absolute path to frame_cli_v2.exe for <see cref="TransportKind.Stdio"/>.</summary>
    public string? FrameCliV2ExePath { get; init; }

    /// <summary>Pipe name (without the <c>\\.\pipe\</c> prefix) for <see cref="TransportKind.NamedPipe"/>.</summary>
    public string? NamedPipeName { get; init; }

    /// <summary>Endpoint for <see cref="TransportKind.Tcp"/>.</summary>
    public IPEndPoint? TcpEndpoint { get; init; }

    /// <summary>
    /// Human-readable client tag, surfaced in the hello handshake's <c>client</c> field. Helps
    /// engine-side telemetry attribute load to specific Grasshopper component versions.
    /// </summary>
    public string ClientTag { get; init; } = "FrameCore.Bridge/2.0";

    /// <summary>
    /// Preferred schema versions in priority order. The hello handshake reports the server's
    /// schemaVer; the SDK records the negotiated version in <see cref="FrameSession.SchemaVersion"/>.
    /// </summary>
    public IReadOnlyList<string> PreferredSchemas { get; init; } = new[] { "2026.06" };

    /// <summary>Whether to accept binary payload frames (raw doubles). Almost always true.</summary>
    public bool WantsBinaryPayloads { get; init; } = true;

    /// <summary>
    /// Timeout for the initial hello handshake. Defaults to 5 s. Longer for cold-start cloud
    /// transports; shorter for in-process where the DLL load itself dominates.
    /// </summary>
    public TimeSpan HandshakeTimeout { get; init; } = TimeSpan.FromSeconds(5);
}
