using System.Text.Json;
using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Entities;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Services;

public class RoleService(AppDbContext db, AuditService audit)
{
    public PermissionCatalogDto Catalog() =>
        new(Perm.Groups.Select(g => new PermissionGroupDto(g.Group, g.Keys)).ToList());

    public async Task<List<RoleDto>> ListAsync(bool archived, CancellationToken ct)
    {
        var rows = await db.Roles.AsNoTracking().Where(r => r.IsArchived == archived)
            .OrderBy(r => r.IsBuiltIn ? 0 : 1).ThenBy(r => r.Code)
            .Select(r => new { Role = r, Users = r.Users.Count(u => u.IsActive) })
            .ToListAsync(ct);
        return rows.Select(x => ToDto(x.Role, x.Users)).ToList();
    }

    public async Task<(RoleDto? dto, string? error)> CreateAsync(RoleSaveRequest req, CancellationToken ct)
    {
        if (!req.Name.HasAny()) return (null, "name_required");
        var code = await FreeCodeAsync(req.Code, ct);
        if (code is null) return (null, "code_taken");
        var r = new AppRole
        {
            Code = code,
            Name = req.Name.ToEntity(),
            Description = req.Description?.Trim() ?? "",
            PermissionsJson = Localized.ToJson(Perm.Sanitize(req.Permissions)),
        };
        db.Roles.Add(r);
        audit.Add("role.create", r.Code, newValue: r.PermissionsJson, reason: req.Reason);
        await db.SaveChangesAsync(ct);
        return (ToDto(r, 0), null);
    }

    public async Task<(RoleDto? dto, string? error)> UpdateAsync(Guid id, RoleSaveRequest req, CancellationToken ct)
    {
        var r = await db.Roles.Include(x => x.Users).FirstOrDefaultAsync(x => x.Id == id, ct);
        if (r is null) return (null, "not_found");
        if (!req.Name.HasAny()) return (null, "name_required");
        var perms = Perm.Sanitize(req.Permissions);
        if (r.IsBuiltIn && r.Code == "Admin" && Perm.AdminLocked.Any(k => !perms.Contains(k)))
            return (null, "admin_locked");
        if (!r.IsBuiltIn)
        {
            var code = (req.Code ?? r.Code).Trim().ToUpperInvariant();
            if (code.Length == 0) return (null, "code_required");
            if (await db.Roles.AnyAsync(x => x.Id != id && x.Code == code, ct)) return (null, "code_taken");
            r.Code = code;
        }
        var before = r.PermissionsJson;
        r.Name = req.Name.ToEntity();
        r.Description = req.Description?.Trim() ?? "";
        r.PermissionsJson = Localized.ToJson(perms);
        r.UpdatedAt = DateTime.UtcNow;
        audit.Add("role.update", r.Code, before, r.PermissionsJson, req.Reason);
        await db.SaveChangesAsync(ct);
        return (ToDto(r, r.Users.Count(u => u.IsActive)), null);
    }

    public async Task<(RoleDto? dto, string? error)> SetArchivedAsync(Guid id, bool archived, string? reason, CancellationToken ct)
    {
        var r = await db.Roles.Include(x => x.Users).FirstOrDefaultAsync(x => x.Id == id, ct);
        if (r is null) return (null, "not_found");
        if (r.IsBuiltIn) return (null, "builtin");
        if (archived && r.Users.Any(u => u.IsActive)) return (null, "role_in_use");
        r.IsArchived = archived;
        r.UpdatedAt = DateTime.UtcNow;
        audit.Add(archived ? "role.archive" : "role.restore", r.Code, reason: reason);
        await db.SaveChangesAsync(ct);
        return (ToDto(r, r.Users.Count(u => u.IsActive)), null);
    }

    public static string[] Parse(string json)
    {
        try
        {
            return Perm.Sanitize(JsonSerializer.Deserialize<string[]>(json, Localized.Json));
        }
        catch (JsonException)
        {
            return [];
        }
    }

    public static RoleDto ToDto(AppRole r, int users) =>
        new(r.Id, r.Code, r.Name.ToDto(), r.Description, r.IsBuiltIn, r.IsArchived, Parse(r.PermissionsJson), users, r.UpdatedAt);

    private async Task<string?> FreeCodeAsync(string? requested, CancellationToken ct)
    {
        var code = (requested ?? "").Trim().ToUpperInvariant();
        if (code.Length == 0)
        {
            var taken = await db.Roles.Select(x => x.Code).ToListAsync(ct);
            for (var n = 1; ; n++)
            {
                var c = $"ROLE-{n:D2}";
                if (!taken.Contains(c)) return c;
            }
        }
        if (await db.Roles.AnyAsync(x => x.Code == code, ct)) return null;
        return code;
    }
}
