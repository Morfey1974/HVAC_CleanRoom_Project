namespace HvacConfigurator.Api.Entities;

/// <summary>Module application image (HVFW header at +0x100), padded to a multiple of 8 bytes.</summary>
public class FirmwareFile
{
    public Guid Id { get; set; } = Guid.NewGuid();
    public int ModuleType { get; set; }
    public int BoardRev { get; set; }
    /// <summary>major &lt;&lt; 8 | minor</summary>
    public int Version { get; set; }
    public long SizeBytes { get; set; }
    /// <summary>CRC-32 (IEEE) of Data.</summary>
    public long Crc32 { get; set; }
    public string FileName { get; set; } = "";
    public string Notes { get; set; } = "";
    public DateTime UploadedAt { get; set; } = DateTime.UtcNow;
    public string UploadedBy { get; set; } = "";
    public DateTime? SentToPlcAt { get; set; }
    public byte[] Data { get; set; } = [];
}
