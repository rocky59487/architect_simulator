// FrameProtocol.cs -- v2 framing primitives (parser + serializer).
//
// On the wire each message is a frame:
//
//   +--------+--------+----------------+------------------+
//   | MAGIC  | FLAGS  | HEADER_LEN(4)  | PAYLOAD_LEN(4)   |   12 bytes fixed
//   +--------+--------+----------------+------------------+
//   | HEADER JSON (UTF-8, no NUL)                          |   HEADER_LEN bytes
//   +------------------------------------------------------+
//   | PAYLOAD (binary; usually raw IEEE-754 doubles LE)    |   PAYLOAD_LEN bytes
//   +------------------------------------------------------+
//
// All integers are little-endian. See docs/specs/S6b_rhino_bridge_v2.md § ②.
//
// Design note: we deliberately keep this file dependency-free (no Newtonsoft, no third-party
// serializers). System.Text.Json handles the header; the payload is raw bytes.

using System.Buffers.Binary;
using System.Text;
using System.Text.Json;

namespace FrameCore.Bridge;

/// <summary>
/// Flag bits in the 16-bit FLAGS word of a wire frame. Layout matches frame_capi_v2.h.
/// </summary>
[Flags]
public enum FrameFlags : ushort
{
    None             = 0,
    EndOfResponse    = 1 << 0,
    HasPayload       = 1 << 1,
    BinaryPayload    = 1 << 2,
    // bits 3..15 reserved -- producers MUST set to 0; consumers MUST ignore unknown bits
    // (forward-compat rule of the protocol).
}

/// <summary>
/// One wire frame in decoded form. Header is the JSON document; payload is raw bytes (zero-length
/// when <see cref="FrameFlags.HasPayload"/> is unset).
/// </summary>
public readonly record struct WireFrame(
    FrameFlags Flags,
    JsonDocument Header,
    ReadOnlyMemory<byte> Payload) : IDisposable
{
    public bool IsEndOfResponse => (Flags & FrameFlags.EndOfResponse) != 0;
    public bool HasPayload      => (Flags & FrameFlags.HasPayload)    != 0;
    public bool IsBinaryPayload => (Flags & FrameFlags.BinaryPayload) != 0;
    public void Dispose() => Header.Dispose();
}

/// <summary>
/// Static helpers for serializing a JSON header + optional payload into the v2 frame layout,
/// and for parsing a contiguous buffer back into <see cref="WireFrame"/> instances.
/// </summary>
public static class FrameProtocol
{
    public const byte Magic0 = (byte)'F';
    public const byte Magic1 = (byte)'C';
    public const int  HeaderFixedBytes = 12;

    /// <summary>
    /// Serialize one frame into a fresh byte array. Convenience for clients that want to hand a
    /// single buffer to <c>frame_v2_send</c>. For high-throughput streams prefer
    /// <see cref="WriteTo"/> into a pooled writer.
    /// </summary>
    public static byte[] Serialize(FrameFlags flags, ReadOnlySpan<byte> headerJson, ReadOnlySpan<byte> payload)
    {
        if (headerJson.Length > int.MaxValue) throw new ArgumentException("header too large", nameof(headerJson));
        if (payload.Length    > int.MaxValue) throw new ArgumentException("payload too large", nameof(payload));

        var hasPayload = payload.Length > 0;
        if (hasPayload) flags |= FrameFlags.HasPayload;

        var total = HeaderFixedBytes + headerJson.Length + payload.Length;
        var buf = new byte[total];
        var span = buf.AsSpan();

        span[0] = Magic0;
        span[1] = Magic1;
        BinaryPrimitives.WriteUInt16LittleEndian(span.Slice(2, 2), (ushort)flags);
        BinaryPrimitives.WriteUInt32LittleEndian(span.Slice(4, 4), (uint)headerJson.Length);
        BinaryPrimitives.WriteUInt32LittleEndian(span.Slice(8, 4), (uint)payload.Length);
        headerJson.CopyTo(span.Slice(HeaderFixedBytes, headerJson.Length));
        if (hasPayload)
            payload.CopyTo(span.Slice(HeaderFixedBytes + headerJson.Length, payload.Length));
        return buf;
    }

    /// <summary>
    /// Write one frame into a pre-allocated writer. Returns the number of bytes written.
    /// Throws <see cref="ArgumentException"/> when the writer cannot hold the frame.
    /// </summary>
    public static int WriteTo(Span<byte> dest, FrameFlags flags,
                              ReadOnlySpan<byte> headerJson, ReadOnlySpan<byte> payload)
    {
        var hasPayload = payload.Length > 0;
        if (hasPayload) flags |= FrameFlags.HasPayload;

        var total = HeaderFixedBytes + headerJson.Length + payload.Length;
        if (dest.Length < total) throw new ArgumentException("destination too small", nameof(dest));

        dest[0] = Magic0;
        dest[1] = Magic1;
        BinaryPrimitives.WriteUInt16LittleEndian(dest.Slice(2, 2), (ushort)flags);
        BinaryPrimitives.WriteUInt32LittleEndian(dest.Slice(4, 4), (uint)headerJson.Length);
        BinaryPrimitives.WriteUInt32LittleEndian(dest.Slice(8, 4), (uint)payload.Length);
        headerJson.CopyTo(dest.Slice(HeaderFixedBytes, headerJson.Length));
        if (hasPayload)
            payload.CopyTo(dest.Slice(HeaderFixedBytes + headerJson.Length, payload.Length));
        return total;
    }

    /// <summary>
    /// Parse one wire frame out of <paramref name="source"/>. The returned frame's Header is a
    /// fresh <see cref="JsonDocument"/> (caller MUST Dispose) and Payload is a slice that
    /// stays valid as long as <paramref name="source"/> stays valid.
    /// </summary>
    /// <param name="source">Buffer containing at least one complete frame at offset 0.</param>
    /// <param name="bytesConsumed">Bytes consumed from <paramref name="source"/> (frame size).</param>
    /// <returns>The decoded frame.</returns>
    /// <exception cref="ProtocolException">malformed magic / lengths / JSON</exception>
    public static WireFrame Parse(ReadOnlyMemory<byte> source, out int bytesConsumed)
    {
        if (source.Length < HeaderFixedBytes)
            throw new ProtocolException("frame shorter than fixed header");

        var span = source.Span;
        if (span[0] != Magic0 || span[1] != Magic1)
            throw new ProtocolException($"bad magic 0x{span[0]:X2}{span[1]:X2}");

        var flags      = (FrameFlags)BinaryPrimitives.ReadUInt16LittleEndian(span.Slice(2, 2));
        var headerLen  = (int)BinaryPrimitives.ReadUInt32LittleEndian(span.Slice(4, 4));
        var payloadLen = (int)BinaryPrimitives.ReadUInt32LittleEndian(span.Slice(8, 4));
        if (headerLen < 0 || payloadLen < 0)
            throw new ProtocolException("negative length");

        var total = HeaderFixedBytes + headerLen + payloadLen;
        if (total > source.Length)
            throw new ProtocolException("frame truncated");

        var headerJson = source.Slice(HeaderFixedBytes, headerLen);
        var payload    = payloadLen > 0
            ? source.Slice(HeaderFixedBytes + headerLen, payloadLen)
            : ReadOnlyMemory<byte>.Empty;

        JsonDocument doc;
        try
        {
            doc = JsonDocument.Parse(headerJson);
        }
        catch (JsonException ex)
        {
            throw new ProtocolException("bad header JSON: " + ex.Message, ex);
        }

        bytesConsumed = total;
        return new WireFrame(flags, doc, payload);
    }

    /// <summary>UTF-8 encode a <see cref="JsonElement"/> for embedding as a header.</summary>
    public static byte[] EncodeHeader(JsonElement headerObject)
    {
        using var ms = new MemoryStream();
        using (var writer = new Utf8JsonWriter(ms, new JsonWriterOptions { Indented = false }))
        {
            headerObject.WriteTo(writer);
        }
        return ms.ToArray();
    }
}

/// <summary>Thrown when a wire frame is malformed.</summary>
public sealed class ProtocolException : Exception
{
    public ProtocolException(string message) : base(message) { }
    public ProtocolException(string message, Exception inner) : base(message, inner) { }
}
