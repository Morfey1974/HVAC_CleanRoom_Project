namespace HvacConfigurator.Api.Entities;

/// <summary>Named set of permissions. Four built-in templates cannot be deleted.</summary>
public class AppRole
{
    public Guid Id { get; set; } = Guid.NewGuid();
    public string Code { get; set; } = "";
    public LocalizedText Name { get; set; } = new();
    public string Description { get; set; } = "";
    public bool IsBuiltIn { get; set; }
    public bool IsArchived { get; set; }
    public string PermissionsJson { get; set; } = "[]";
    public DateTime UpdatedAt { get; set; } = DateTime.UtcNow;
    public ICollection<User> Users { get; set; } = [];
}
