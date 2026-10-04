namespace HvacConfigurator.Api.Entities;

/// <summary>User-entered name in every UI language; shown in the currently selected one.</summary>
public class LocalizedText
{
    public string Ru { get; set; } = "";
    public string En { get; set; } = "";
    public string He { get; set; } = "";
}
