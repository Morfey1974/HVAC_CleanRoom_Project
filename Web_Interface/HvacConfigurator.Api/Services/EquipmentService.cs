using System.Text.Json;
using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Entities;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Services;

public class EquipmentService(AppDbContext db, AuditService audit)
{
    public async Task<List<ProjectEquipmentDto>> ListAsync(Guid projectId, CancellationToken ct)
    {
        var rows = await db.ProjectEquipment.AsNoTracking().Where(e => e.ProjectId == projectId)
            .OrderBy(e => e.Tag).ToListAsync(ct);
        var libIds = rows.Where(r => r.LibraryItemId != null).Select(r => r.LibraryItemId!.Value).Distinct().ToList();
        var latest = await db.LibraryItems.AsNoTracking().Where(l => libIds.Contains(l.Id))
            .ToDictionaryAsync(l => l.Id, l => l.Version, ct);
        return rows.Select(r => ToDto(r, r.LibraryItemId is { } id && latest.TryGetValue(id, out var v) ? v : null)).ToList();
    }

    public async Task<(ProjectEquipmentDto? dto, string? error)> CreateAsync(Guid projectId, ProjectEquipmentSaveRequest req, CancellationToken ct)
    {
        var e = new ProjectEquipment { ProjectId = projectId };
        if (req.LibraryItemId is { } libId)
        {
            var lib = await db.LibraryItems.AsNoTracking().FirstOrDefaultAsync(l => l.Id == libId, ct);
            if (lib is null) return (null, "library_not_found");
            Freeze(e, lib);
        }
        var err = await ApplyAsync(projectId, e, req, ct);
        if (err is not null) return (null, err);
        db.ProjectEquipment.Add(e);
        audit.Add("equipment.create", e.Tag, newValue: Localized.ToJson(Summary(e)), reason: req.Reason, projectId: projectId);
        await db.SaveChangesAsync(ct);
        return (ToDto(e, e.LibraryVersion == 0 ? null : e.LibraryVersion), null);
    }

    public async Task<(ProjectEquipmentDto? dto, string? error)> UpdateAsync(Guid projectId, Guid id, ProjectEquipmentSaveRequest req, CancellationToken ct)
    {
        var e = await db.ProjectEquipment.FirstOrDefaultAsync(x => x.Id == id && x.ProjectId == projectId, ct);
        if (e is null) return (null, "not_found");
        var before = Localized.ToJson(Summary(e));
        var err = await ApplyAsync(projectId, e, req, ct);
        if (err is not null) return (null, err);
        e.UpdatedAt = DateTime.UtcNow;
        audit.Add("equipment.update", e.Tag, before, Localized.ToJson(Summary(e)), req.Reason, projectId: projectId);
        await db.SaveChangesAsync(ct);
        return ((await ListAsync(projectId, ct)).First(x => x.Id == id), null);
    }

    /// <summary>Explicit, audited upgrade of the frozen copy to the current library version.</summary>
    public async Task<ProjectEquipmentDto?> UpdateFromLibraryAsync(Guid projectId, Guid id, string? reason, CancellationToken ct)
    {
        var e = await db.ProjectEquipment.FirstOrDefaultAsync(x => x.Id == id && x.ProjectId == projectId, ct);
        if (e?.LibraryItemId is null) return null;
        var lib = await db.LibraryItems.AsNoTracking().FirstOrDefaultAsync(l => l.Id == e.LibraryItemId, ct);
        if (lib is null) return null;
        var oldVersion = e.LibraryVersion;
        Freeze(e, lib);
        e.UpdatedAt = DateTime.UtcNow;
        audit.Add("equipment.libraryUpdate", e.Tag, $"v{oldVersion}", $"v{lib.Version}", reason, projectId: projectId);
        await db.SaveChangesAsync(ct);
        return ToDto(e, lib.Version);
    }

    public async Task<bool> DeleteAsync(Guid projectId, Guid id, string? reason, CancellationToken ct)
    {
        var e = await db.ProjectEquipment.FirstOrDefaultAsync(x => x.Id == id && x.ProjectId == projectId, ct);
        if (e is null) return false;
        audit.Add("equipment.delete", e.Tag, Localized.ToJson(Summary(e)), reason: reason, projectId: projectId);
        db.ProjectEquipment.Remove(e);
        await db.SaveChangesAsync(ct);
        return true;
    }

    private async Task<string?> ApplyAsync(Guid projectId, ProjectEquipment e, ProjectEquipmentSaveRequest q, CancellationToken ct)
    {
        if (string.IsNullOrWhiteSpace(q.Tag)) return "tag_required";
        if (q.RoomId is { } rid && !await db.Rooms.AnyAsync(r => r.Id == rid && r.ProjectId == projectId, ct)) return "room_not_found";
        e.Tag = q.Tag.Trim();
        e.Quantity = Math.Max(1, q.Quantity);
        e.RoomId = q.RoomId;
        e.Notes = q.Notes?.Trim() ?? "";
        return null;
    }

    private static void Freeze(ProjectEquipment e, LibraryItem lib)
    {
        e.LibraryItemId = lib.Id;
        e.LibraryVersion = lib.Version;
        e.SnapshotJson = JsonSerializer.Serialize(LibraryService.ToDto(lib, 0), Localized.Json);
    }

    private static object Summary(ProjectEquipment e) => new { e.Tag, e.Quantity, e.RoomId, e.Notes, e.LibraryItemId, e.LibraryVersion };

    private static ProjectEquipmentDto ToDto(ProjectEquipment e, int? latest)
    {
        LibraryItemDto? snap = null;
        if (e.LibraryItemId is not null)
        {
            try { snap = JsonSerializer.Deserialize<LibraryItemDto>(e.SnapshotJson, Localized.Json); }
            catch (JsonException) { }
        }
        return new ProjectEquipmentDto(e.Id, e.Tag, e.Quantity, e.RoomId, e.Notes, e.LibraryItemId, e.LibraryVersion, latest, snap);
    }
}
