namespace HvacConfigurator.Api.Entities;

public class Project
{
    public Guid Id { get; set; } = Guid.NewGuid();
    public LocalizedText Name { get; set; } = new();
    public string Number { get; set; } = "";
    public string Customer { get; set; } = "";
    public string Address { get; set; } = "";
    public string Responsible { get; set; } = "";
    public int? IsoClass { get; set; }
    public string Description { get; set; } = "";
    public bool IsArchived { get; set; }

    /// <summary>The single project whose configuration runs on the site PLC (monitoring, alarms).</summary>
    public bool IsActiveOnSite { get; set; }

    public DateTime CreatedAt { get; set; } = DateTime.UtcNow;
    public string CreatedBy { get; set; } = "";
    public DateTime UpdatedAt { get; set; } = DateTime.UtcNow;

    public List<Room> Rooms { get; set; } = [];
    public List<ProjectDocument> Documents { get; set; } = [];
    public List<ProjectEquipment> Equipment { get; set; } = [];
    public List<ProjectModule> Modules { get; set; } = [];
}
