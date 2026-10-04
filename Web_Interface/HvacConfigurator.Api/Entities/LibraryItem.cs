namespace HvacConfigurator.Api.Entities;

public static class LibraryCategories
{
    public const string Module = "module";
    public const string Sensor = "sensor";
    public const string Actuator = "actuator";
    public const string Assembly = "assembly";
    public const string Graphic = "graphic";
    public const string Regulator = "regulator";
    public static readonly string[] All = [Module, Sensor, Actuator, Assembly, Graphic, Regulator];
}

/// <summary>Shared, reusable description of a module / sensor / device. Every edit bumps <see cref="Version"/>.</summary>
public class LibraryItem
{
    public Guid Id { get; set; } = Guid.NewGuid();
    public string Category { get; set; } = LibraryCategories.Sensor;

    /// <summary>Article. For own modules: HC-&lt;TYPE&gt;&lt;CHANNELS&gt;[-&lt;VARIANT&gt;] (plan 8.3.1); for bought devices — manufacturer part number.</summary>
    public string Code { get; set; } = "";

    /// <summary>Module type code stored in module memory (plan 8.3.1); modules only.</summary>
    public int? TypeCode { get; set; }

    /// <summary>System name prefix: AI → AI-03.05 (plan 8.3.5); modules only.</summary>
    public string SystemPrefix { get; set; } = "";

    public int? ChannelCount { get; set; }
    /// <summary>Signal types each channel can be set to (ModuleRules.SignalModes); the project picks one per channel.</summary>
    public List<string> ChannelModes { get; set; } = [];
    public LocalizedText Name { get; set; } = new();
    public string Manufacturer { get; set; } = "";
    public string Model { get; set; } = "";
    public string Description { get; set; } = "";

    /// <summary>JSON array of { key, value, unit } — free-form technical properties.</summary>
    public string PropsJson { get; set; } = "[]";

    /// <summary>JSON <c>ModuleGraphicDto</c>: block look and connection points on the scheme; empty = default drawing.</summary>
    public string GraphicJson { get; set; } = "";

    public int Version { get; set; } = 1;
    public bool IsArchived { get; set; }
    public DateTime UpdatedAt { get; set; } = DateTime.UtcNow;
}
