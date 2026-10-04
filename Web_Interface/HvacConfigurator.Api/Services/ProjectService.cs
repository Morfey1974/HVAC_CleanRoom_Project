using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Entities;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Services;

public class ProjectService(AppDbContext db, AuditService audit)
{
    private static IQueryable<ProjectDto> Project(IQueryable<Project> q) => q.Select(p => new ProjectDto(
        p.Id, new LocalizedTextDto(p.Name.Ru, p.Name.En, p.Name.He), p.Number, p.Customer, p.Address, p.Responsible,
        p.IsoClass, p.Description, p.IsArchived, p.IsActiveOnSite, p.CreatedAt, p.CreatedBy, p.UpdatedAt,
        p.Rooms.Count, p.Documents.Count, p.Equipment.Count, p.Modules.Count));

    public Task<List<ProjectDto>> ListAsync(bool archived, CancellationToken ct) =>
        Project(db.Projects.AsNoTracking().Where(p => p.IsArchived == archived).OrderByDescending(p => p.UpdatedAt)).ToListAsync(ct);

    public Task<ProjectDto?> GetAsync(Guid id, CancellationToken ct) =>
        Project(db.Projects.AsNoTracking().Where(p => p.Id == id)).FirstOrDefaultAsync(ct);

    public Task<bool> ExistsAsync(Guid id, CancellationToken ct) => db.Projects.AnyAsync(p => p.Id == id, ct);

    public async Task<ProjectDto> CreateAsync(ProjectSaveRequest req, CancellationToken ct)
    {
        var p = new Project { CreatedBy = audit.CurrentLogin };
        Apply(p, req);
        db.Projects.Add(p);
        audit.Add("project.create", p.Name.Label(p.Id.ToString()), newValue: Localized.ToJson(Snapshot(p)), reason: req.Reason, projectId: p.Id);
        await db.SaveChangesAsync(ct);
        return (await GetAsync(p.Id, ct))!;
    }

    public async Task<ProjectDto?> UpdateAsync(Guid id, ProjectSaveRequest req, CancellationToken ct)
    {
        var p = await db.Projects.FirstOrDefaultAsync(x => x.Id == id, ct);
        if (p is null) return null;
        var before = Localized.ToJson(Snapshot(p));
        Apply(p, req);
        p.UpdatedAt = DateTime.UtcNow;
        audit.Add("project.update", p.Name.Label(p.Id.ToString()), before, Localized.ToJson(Snapshot(p)), req.Reason, projectId: p.Id);
        await db.SaveChangesAsync(ct);
        return await GetAsync(id, ct);
    }

    public async Task<ProjectDto?> SetArchivedAsync(Guid id, bool archived, string? reason, CancellationToken ct)
    {
        var p = await db.Projects.FirstOrDefaultAsync(x => x.Id == id, ct);
        if (p is null) return null;
        p.IsArchived = archived;
        if (archived) p.IsActiveOnSite = false;
        p.UpdatedAt = DateTime.UtcNow;
        audit.Add(archived ? "project.archive" : "project.restore", p.Name.Label(p.Id.ToString()), reason: reason, projectId: p.Id);
        await db.SaveChangesAsync(ct);
        return await GetAsync(id, ct);
    }

    /// <summary>Only one project runs on the site PLC at a time.</summary>
    public async Task<ProjectDto?> SetActiveOnSiteAsync(Guid id, string? reason, CancellationToken ct)
    {
        var p = await db.Projects.FirstOrDefaultAsync(x => x.Id == id && !x.IsArchived, ct);
        if (p is null) return null;
        var previous = await db.Projects.Where(x => x.IsActiveOnSite && x.Id != id).ToListAsync(ct);
        foreach (var o in previous) o.IsActiveOnSite = false;
        p.IsActiveOnSite = true;
        audit.Add("project.activate", p.Name.Label(p.Id.ToString()),
            oldValue: string.Join(", ", previous.Select(o => o.Name.Label(o.Id.ToString()))), reason: reason, projectId: p.Id);
        await db.SaveChangesAsync(ct);
        return await GetAsync(id, ct);
    }

    public async Task<ProjectDto?> CopyAsync(Guid id, string? reason, CancellationToken ct)
    {
        var src = await db.Projects.AsNoTracking()
            .Include(x => x.Rooms).Include(x => x.Equipment).Include(x => x.Documents).Include(x => x.Modules)
            .AsSplitQuery()
            .FirstOrDefaultAsync(x => x.Id == id, ct);
        if (src is null) return null;

        var copy = new Project
        {
            Name = new LocalizedText
            {
                Ru = Suffix(src.Name.Ru, " (копия)"),
                En = Suffix(src.Name.En, " (copy)"),
                He = Suffix(src.Name.He, " (עותק)"),
            },
            Number = src.Number, Customer = src.Customer, Address = src.Address, Responsible = src.Responsible,
            IsoClass = src.IsoClass, Description = src.Description, CreatedBy = audit.CurrentLogin,
        };
        var roomMap = new Dictionary<Guid, Guid>();
        foreach (var r in src.Rooms)
        {
            var nr = new Room
            {
                ProjectId = copy.Id, Name = new LocalizedText { Ru = r.Name.Ru, En = r.Name.En, He = r.Name.He },
                IsoClass = r.IsoClass, AreaM2 = r.AreaM2, HeightM = r.HeightM,
                TempSetpointC = r.TempSetpointC, TempToleranceC = r.TempToleranceC,
                RhSetpointPct = r.RhSetpointPct, RhTolerancePct = r.RhTolerancePct,
                PressureSetpointPa = r.PressureSetpointPa, PressureTolerancePa = r.PressureTolerancePa, SortOrder = r.SortOrder,
            };
            roomMap[r.Id] = nr.Id;
            copy.Rooms.Add(nr);
        }
        foreach (var e in src.Equipment)
            copy.Equipment.Add(new ProjectEquipment
            {
                ProjectId = copy.Id, Tag = e.Tag, Quantity = e.Quantity, Notes = e.Notes,
                RoomId = e.RoomId is { } rid && roomMap.TryGetValue(rid, out var nrid) ? nrid : null,
                LibraryItemId = e.LibraryItemId, LibraryVersion = e.LibraryVersion, SnapshotJson = e.SnapshotJson,
            });
        // Serial numbers are per physical unit: the copy keeps places and names, not serials.
        foreach (var m in src.Modules)
            copy.Modules.Add(new ProjectModule
            {
                ProjectId = copy.Id, LibraryItemId = m.LibraryItemId, LibraryVersion = m.LibraryVersion, SnapshotJson = m.SnapshotJson,
                Line = m.Line, Rail = m.Rail, Place = m.Place, Revision = m.Revision, Notes = m.Notes,
                UserName = new LocalizedText { Ru = m.UserName.Ru, En = m.UserName.En, He = m.UserName.He },
            });
        foreach (var d in src.Documents)
            copy.Documents.Add(new ProjectDocument
            {
                ProjectId = copy.Id, Kind = d.Kind, FileName = d.FileName, ContentType = d.ContentType,
                SizeBytes = d.SizeBytes, Data = d.Data, UploadedBy = audit.CurrentLogin,
            });

        db.Projects.Add(copy);
        audit.Add("project.copy", copy.Name.Label(copy.Id.ToString()), oldValue: src.Name.Label(src.Id.ToString()), reason: reason, projectId: copy.Id);
        await db.SaveChangesAsync(ct);
        return await GetAsync(copy.Id, ct);
    }

    private static string Suffix(string s, string suffix) => string.IsNullOrWhiteSpace(s) ? s : s + suffix;

    private static void Apply(Project p, ProjectSaveRequest q)
    {
        p.Name = q.Name.ToEntity();
        p.Number = q.Number?.Trim() ?? "";
        p.Customer = q.Customer?.Trim() ?? "";
        p.Address = q.Address?.Trim() ?? "";
        p.Responsible = q.Responsible?.Trim() ?? "";
        p.IsoClass = q.IsoClass;
        p.Description = q.Description?.Trim() ?? "";
    }

    private static object Snapshot(Project p) => new
    {
        name = p.Name.ToDto(), p.Number, p.Customer, p.Address, p.Responsible, p.IsoClass, p.Description,
    };
}
