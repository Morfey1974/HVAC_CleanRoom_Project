namespace HvacConfigurator.Api.Entities;

public static class DocumentKinds
{
    public const string Pid = "pid";
    public const string Aoi = "aoi";
    public const string Other = "other";
    public static readonly string[] All = [Pid, Aoi, Other];
}

public class ProjectDocument
{
    public Guid Id { get; set; } = Guid.NewGuid();
    public Guid ProjectId { get; set; }
    public string Kind { get; set; } = DocumentKinds.Other;
    public string FileName { get; set; } = "";
    public string ContentType { get; set; } = "application/octet-stream";
    public long SizeBytes { get; set; }
    public DateTime UploadedAt { get; set; } = DateTime.UtcNow;
    public string UploadedBy { get; set; } = "";
    public byte[] Data { get; set; } = [];
}
