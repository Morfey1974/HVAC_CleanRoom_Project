using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Entities;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Services;

public class RoomService(AppDbContext db, AuditService audit)
{
    public async Task<List<RoomDto>> ListAsync(Guid projectId, CancellationToken ct) =>
        (await db.Rooms.AsNoTracking().Where(r => r.ProjectId == projectId)
            .OrderBy(r => r.SortOrder).ThenBy(r => r.Name.Ru).ToListAsync(ct))
        .Select(ToDto).ToList();

    public async Task<RoomDto> CreateAsync(Guid projectId, RoomSaveRequest req, CancellationToken ct)
    {
        var room = new Room { ProjectId = projectId };
        Apply(room, req);
        db.Rooms.Add(room);
        audit.Add("room.create", Label(room), newValue: Localized.ToJson(ToDto(room)), reason: req.Reason, projectId: projectId);
        await db.SaveChangesAsync(ct);
        return ToDto(room);
    }

    public async Task<RoomDto?> UpdateAsync(Guid projectId, Guid id, RoomSaveRequest req, CancellationToken ct)
    {
        var room = await db.Rooms.FirstOrDefaultAsync(r => r.Id == id && r.ProjectId == projectId, ct);
        if (room is null) return null;
        var before = Localized.ToJson(ToDto(room));
        Apply(room, req);
        room.UpdatedAt = DateTime.UtcNow;
        audit.Add("room.update", Label(room), before, Localized.ToJson(ToDto(room)), req.Reason, projectId: projectId);
        await db.SaveChangesAsync(ct);
        return ToDto(room);
    }

    public async Task<bool> DeleteAsync(Guid projectId, Guid id, string? reason, CancellationToken ct)
    {
        var room = await db.Rooms.FirstOrDefaultAsync(r => r.Id == id && r.ProjectId == projectId, ct);
        if (room is null) return false;
        audit.Add("room.delete", Label(room), Localized.ToJson(ToDto(room)), reason: reason, projectId: projectId);
        db.Rooms.Remove(room);
        await db.SaveChangesAsync(ct);
        return true;
    }

    private static void Apply(Room r, RoomSaveRequest q)
    {
        r.Name = q.Name.ToEntity();
        r.IsoClass = q.IsoClass;
        r.AreaM2 = q.AreaM2;
        r.HeightM = q.HeightM;
        r.TempSetpointC = q.TempSetpointC;
        r.TempToleranceC = q.TempToleranceC;
        r.RhSetpointPct = q.RhSetpointPct;
        r.RhTolerancePct = q.RhTolerancePct;
        r.PressureSetpointPa = q.PressureSetpointPa;
        r.PressureTolerancePa = q.PressureTolerancePa;
        r.SortOrder = q.SortOrder;
    }

    private static string Label(Room r) => r.Name.Label(r.Id.ToString());

    private static RoomDto ToDto(Room r) => new(
        r.Id, r.Name.ToDto(), r.IsoClass, r.AreaM2, r.HeightM,
        r.TempSetpointC, r.TempToleranceC, r.RhSetpointPct, r.RhTolerancePct,
        r.PressureSetpointPa, r.PressureTolerancePa, r.SortOrder);
}
