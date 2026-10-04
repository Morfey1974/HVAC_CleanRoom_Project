namespace HvacConfigurator.Api.Entities;

public class Room
{
    public Guid Id { get; set; } = Guid.NewGuid();
    public Guid ProjectId { get; set; }
    public LocalizedText Name { get; set; } = new();
    public int? IsoClass { get; set; }
    public decimal? AreaM2 { get; set; }
    public decimal? HeightM { get; set; }
    public decimal? TempSetpointC { get; set; }
    public decimal? TempToleranceC { get; set; }
    public decimal? RhSetpointPct { get; set; }
    public decimal? RhTolerancePct { get; set; }
    public decimal? PressureSetpointPa { get; set; }
    public decimal? PressureTolerancePa { get; set; }
    public int SortOrder { get; set; }
    public DateTime UpdatedAt { get; set; } = DateTime.UtcNow;
}
