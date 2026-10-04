namespace HvacConfigurator.Api.Entities;

public class AlarmEvent
{
    public long Id { get; set; }
    public string Code { get; set; } = "";
    public string Element { get; set; } = "";
    public DateTime StartedAt { get; set; } = DateTime.UtcNow;
    public DateTime? ClearedAt { get; set; }
    public DateTime? AckAt { get; set; }
    public string? AckBy { get; set; }
}
