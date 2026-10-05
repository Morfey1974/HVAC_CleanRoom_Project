using System.Buffers.Binary;

namespace HvacConfigurator.Api.Services;

public record FirmwareHeader(int ModuleType, int BoardRev, int Version, uint ImageEnd);

public record FirmwareParseResult(byte[]? Data, FirmwareHeader? Header, string? Error);

/// <summary>Module application image: raw .bin from the application start, or the .elf it was linked into.</summary>
public static class FirmwareImage
{
    public const int HeaderOffset = 0x100;
    public const uint HeaderMagic = 0x57465648; // "HVFW"
    public const int HeaderSize = 28;
    /// <summary>One W25Q128 store slot on the PLC minus its descriptor.</summary>
    public const int MaxImageBytes = 512 * 1024 - 4096;
    public const int MaxModuleType = 15;

    public static FirmwareParseResult Parse(byte[] file)
    {
        byte[] image;
        uint? baseAddr = null;

        if (file.Length >= 4 && file[0] == 0x7F && file[1] == (byte)'E' && file[2] == (byte)'L' && file[3] == (byte)'F')
        {
            var elf = FromElf(file);
            if (elf.Error is not null) return new(null, null, elf.Error);
            image = elf.Image!;
            baseAddr = elf.Base;
        }
        else
        {
            image = file;
        }

        if (image.Length < HeaderOffset + HeaderSize) return new(null, null, "too_small");
        var h = image.AsSpan(HeaderOffset);
        if (BinaryPrimitives.ReadUInt32LittleEndian(h) != HeaderMagic) return new(null, null, "no_header");
        var header = new FirmwareHeader(h[5], h[6], BinaryPrimitives.ReadUInt16LittleEndian(h[8..]),
            BinaryPrimitives.ReadUInt32LittleEndian(h[12..]));
        if (header.ModuleType is < 1 or > MaxModuleType) return new(null, null, "bad_type");

        var length = image.Length;
        if (baseAddr is { } b && header.ImageEnd > b + HeaderOffset && header.ImageEnd - b <= (uint)length)
            length = (int)(header.ImageEnd - b);

        var padded = (length + 7) & ~7;
        if (padded > MaxImageBytes) return new(null, null, "too_large");
        var data = new byte[padded];
        Array.Fill(data, (byte)0xFF);
        Array.Copy(image, data, length);
        return new(data, header, null);
    }

    private record ElfResult(byte[]? Image, uint Base, string? Error);

    /// <summary>Flash contents from the loadable segments (physical addresses), gaps filled with 0xFF.</summary>
    private static ElfResult FromElf(byte[] f)
    {
        const uint PtLoad = 1;
        if (f.Length < 52 || f[4] != 1 || f[5] != 1) return new(null, 0, "bad_elf");
        var span = f.AsSpan();
        var phoff = BinaryPrimitives.ReadUInt32LittleEndian(span[0x1C..]);
        var phentsize = BinaryPrimitives.ReadUInt16LittleEndian(span[0x2A..]);
        var phnum = BinaryPrimitives.ReadUInt16LittleEndian(span[0x2C..]);
        if (phentsize < 32 || phoff + (long)phentsize * phnum > f.Length) return new(null, 0, "bad_elf");

        var segs = new List<(uint Addr, uint Off, uint Size)>();
        for (var i = 0; i < phnum; i++)
        {
            var p = span[(int)(phoff + i * phentsize)..];
            var type = BinaryPrimitives.ReadUInt32LittleEndian(p);
            var off = BinaryPrimitives.ReadUInt32LittleEndian(p[4..]);
            var paddr = BinaryPrimitives.ReadUInt32LittleEndian(p[12..]);
            var filesz = BinaryPrimitives.ReadUInt32LittleEndian(p[16..]);
            if (type != PtLoad || filesz == 0 || paddr < 0x08000000u || paddr >= 0x20000000u) continue;
            if (off + (long)filesz > f.Length) return new(null, 0, "bad_elf");
            segs.Add((paddr, off, filesz));
        }
        if (segs.Count == 0) return new(null, 0, "bad_elf");

        var lo = segs.Min(s => s.Addr);
        var hi = segs.Max(s => (long)s.Addr + s.Size);
        if (hi - lo > MaxImageBytes) return new(null, 0, "too_large");
        var img = new byte[hi - lo];
        Array.Fill(img, (byte)0xFF);
        foreach (var s in segs) Array.Copy(f, s.Off, img, s.Addr - lo, s.Size);
        return new(img, lo, null);
    }

    private static readonly uint[] CrcTable = BuildCrcTable();

    private static uint[] BuildCrcTable()
    {
        var t = new uint[256];
        for (uint i = 0; i < 256; i++)
        {
            var c = i;
            for (var k = 0; k < 8; k++) c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }

    /// <summary>CRC-32 (IEEE 802.3), same as fwupd_crc32 on the modules.</summary>
    public static uint Crc32(ReadOnlySpan<byte> data)
    {
        var crc = 0xFFFFFFFFu;
        foreach (var b in data) crc = CrcTable[(crc ^ b) & 0xFF] ^ (crc >> 8);
        return ~crc;
    }

    public static string VersionText(int v) => $"{v >> 8}.{v & 0xFF}";
}
