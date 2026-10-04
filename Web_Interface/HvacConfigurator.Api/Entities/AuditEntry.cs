namespace HvacConfigurator.Api.Entities;

/// <summary>Append-only user action log (GMP): never updated or deleted.</summary>
public class AuditEntry
{
    public long Id { get; set; }
    public DateTime At { get; set; } = DateTime.UtcNow;
    public Guid? UserId { get; set; }
    public string UserLogin { get; set; } = "";
    public string Source { get; set; } = "";
    public string Action { get; set; } = "";
    public string Target { get; set; } = "";
    public string? OldValue { get; set; }
    public string? NewValue { get; set; }
    public string? Reason { get; set; }
    public Guid? ProjectId { get; set; }
}
