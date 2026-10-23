namespace HvacConfigurator.Api.Entities;

public static class HmiGroups
{
    /// <summary>Section of an air handling unit: same height for all, glued in a row on the screen.</summary>
    public const string Section = "section";
    public const string Duct = "duct";
    public const string Pipe = "pipe";
    public const string Instrument = "instrument";
    public const string Equipment = "equipment";
    public const string Room = "room";
    public static readonly string[] All = [Section, Duct, Pipe, Instrument, Equipment, Room];
}

public static class HmiStatuses
{
    public const string Draft = "draft";
    public const string Approved = "approved";
    public static readonly string[] All = [Draft, Approved];
}

/// <summary>
/// HMI picture library (plan 5.7), separate from the module library. Kind is the shared equipment vocabulary
/// used by the document assistant; Art is the drawing (built-in drawings of the web app). Every edit bumps Version.
/// </summary>
public class HmiElement
{
    public Guid Id { get; set; } = Guid.NewGuid();
    public string Code { get; set; } = "";
    public string Group { get; set; } = HmiGroups.Section;
    public string Kind { get; set; } = "";
    public string Art { get; set; } = "";
    public LocalizedText Name { get; set; } = new();
    public string Description { get; set; } = "";

    /// <summary>Length on the screen (sections: along the air flow), drawing units.</summary>
    public int Length { get; set; } = 100;

    /// <summary>JSON object of drawing parameters: label, stages… (meaning depends on Art).</summary>
    public string ParamsJson { get; set; } = "{}";

    public string Status { get; set; } = HmiStatuses.Draft;
    public int Version { get; set; } = 1;
    public bool IsArchived { get; set; }
    public string UpdatedBy { get; set; } = "";
    public DateTime UpdatedAt { get; set; } = DateTime.UtcNow;
}

public static class HmiScreenKinds
{
    public const string Ahu = "ahu";
    public const string Rooms = "rooms";
    public static readonly string[] All = [Ahu, Rooms];
}

/// <summary>Screen of a project: an air handling unit glued from library sections, or a row of room tiles.</summary>
public class HmiScreen
{
    public Guid Id { get; set; } = Guid.NewGuid();
    public Guid ProjectId { get; set; }
    public string Kind { get; set; } = HmiScreenKinds.Ahu;
    public LocalizedText Name { get; set; } = new();
    public int SortOrder { get; set; }

    /// <summary>
    /// JSON. ahu: { items, ducts, pipes, instruments, flowTag }.
    /// items = unit sections in airflow order; ducts join inlet/outlet (side in|out);
    /// pipes are free-placed (x, y, rot) and snap at ends; instruments sit on a section, duct or pipe (hostUid).
    /// rooms: { rooms: [{ roomId, bind: { t, rh, dp } }] }.
    /// </summary>
    public string ContentJson { get; set; } = "{}";

    public DateTime UpdatedAt { get; set; } = DateTime.UtcNow;
}
