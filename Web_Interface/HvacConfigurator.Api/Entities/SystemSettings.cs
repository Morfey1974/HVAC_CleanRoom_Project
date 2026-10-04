namespace HvacConfigurator.Api.Entities;

/// <summary>Single-row system settings.</summary>
public class SystemSettings
{
    public int Id { get; set; } = 1;
    public string SiteName { get; set; } = "";
    public string StartupLanguage { get; set; } = "he";
    public string PlcMode { get; set; } = "simulator";
    public string PlcAddress { get; set; } = "";
    public DateTime UpdatedAt { get; set; } = DateTime.UtcNow;
}
