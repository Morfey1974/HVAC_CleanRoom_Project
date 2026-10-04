namespace HvacConfigurator.Api.Entities;

/// <summary>
/// A library item used in a project. Keeps a frozen copy of the library item so later library
/// edits never silently change a validated project; the user updates explicitly.
/// </summary>
public class ProjectEquipment
{
    public Guid Id { get; set; } = Guid.NewGuid();
    public Guid ProjectId { get; set; }
    public string Tag { get; set; } = "";
    public int Quantity { get; set; } = 1;
    public Guid? RoomId { get; set; }
    public string Notes { get; set; } = "";

    public Guid? LibraryItemId { get; set; }
    public int LibraryVersion { get; set; }
    public string SnapshotJson { get; set; } = "{}";

    public DateTime UpdatedAt { get; set; } = DateTime.UtcNow;
}
