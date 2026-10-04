namespace HvacConfigurator.Api.Entities;

/// <summary>
/// A module placed in the project hardware. Configuration is bound to the place (ID = line.rail.place),
/// not to the serial number, so a same-article replacement needs no reconfiguration (plan 8.3).
/// </summary>
public class ProjectModule
{
    public Guid Id { get; set; } = Guid.NewGuid();
    public Guid ProjectId { get; set; }

    public Guid? LibraryItemId { get; set; }
    public int LibraryVersion { get; set; }
    public string SnapshotJson { get; set; } = "{}";

    public int Line { get; set; }
    public int Rail { get; set; }
    public int Place { get; set; }

    public LocalizedText UserName { get; set; } = new();
    public string ExpectedSerial { get; set; } = "";
    public string Revision { get; set; } = "";
    public string Notes { get; set; } = "";

    /// <summary>Channel settings (signal type per channel), JSON list of ModuleChannelDto.</summary>
    public string ChannelsJson { get; set; } = "";

    /// <summary>Block position on the system scheme.</summary>
    public double SchemeX { get; set; }
    public double SchemeY { get; set; }

    public DateTime UpdatedAt { get; set; } = DateTime.UtcNow;
}

/// <summary>A wire on the system scheme between connection points of two modules.</summary>
public class ProjectLink
{
    public Guid Id { get; set; } = Guid.NewGuid();
    public Guid ProjectId { get; set; }
    public Guid FromModuleId { get; set; }
    public string FromPort { get; set; } = "";
    public Guid ToModuleId { get; set; }
    public string ToPort { get; set; } = "";
    /// <summary>Wire drawing on the scheme: line style and bend points; empty = automatic route.</summary>
    public string RouteJson { get; set; } = "";
    public DateTime CreatedAt { get; set; } = DateTime.UtcNow;
}
