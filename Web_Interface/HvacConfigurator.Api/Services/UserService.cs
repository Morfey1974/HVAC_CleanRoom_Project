using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Entities;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Services;

public class UserService(AppDbContext db, AuditService audit)
{
    public async Task<List<UserDto>> ListAsync(CancellationToken ct) =>
        (await db.Users.AsNoTracking().Include(u => u.RoleRef).OrderBy(u => u.Login).ToListAsync(ct))
        .Select(ToDto).ToList();

    public async Task<(UserDto? dto, string? error)> CreateAsync(UserSaveRequest req, CancellationToken ct)
    {
        if (Validate(req, creating: true) is { } err) return (null, err);
        var login = req.Login.Trim().ToLowerInvariant();
        if (await db.Users.AnyAsync(u => u.Login == login, ct)) return (null, "login_taken");
        if (!await RoleOkAsync(req.RoleId, ct)) return (null, "bad_role");
        var u = new User
        {
            Login = login,
            FullName = req.FullName.Trim(),
            PasswordHash = BCrypt.Net.BCrypt.HashPassword(req.Password!),
            RoleId = req.RoleId,
            IsActive = req.IsActive,
        };
        db.Users.Add(u);
        audit.Add("user.create", u.Login, newValue: req.RoleId.ToString(), reason: req.Reason);
        await db.SaveChangesAsync(ct);
        return (await GetAsync(u.Id, ct), null);
    }

    public async Task<(UserDto? dto, string? error)> UpdateAsync(Guid id, UserSaveRequest req, CancellationToken ct)
    {
        var u = await db.Users.Include(x => x.RoleRef).FirstOrDefaultAsync(x => x.Id == id, ct);
        if (u is null) return (null, "not_found");
        if (Validate(req, creating: false) is { } err) return (null, err);
        var login = req.Login.Trim().ToLowerInvariant();
        if (await db.Users.AnyAsync(x => x.Id != id && x.Login == login, ct)) return (null, "login_taken");
        if (!await RoleOkAsync(req.RoleId, ct)) return (null, "bad_role");
        if (u.IsActive && !req.IsActive && !await CanDropAdminAsync(u, ct)) return (null, "last_admin");
        if (HasAdmin(u) && !await RoleHasAdminAsync(req.RoleId, ct) && !await CanDropAdminAsync(u, ct))
            return (null, "last_admin");
        var before = $"{u.Login}/{u.RoleId}";
        u.Login = login;
        u.FullName = req.FullName.Trim();
        u.RoleId = req.RoleId;
        u.IsActive = req.IsActive;
        if (!string.IsNullOrWhiteSpace(req.Password))
            u.PasswordHash = BCrypt.Net.BCrypt.HashPassword(req.Password);
        audit.Add("user.update", u.Login, before, $"{u.Login}/{u.RoleId}", req.Reason);
        await db.SaveChangesAsync(ct);
        return (await GetAsync(id, ct), null);
    }

    private async Task<UserDto?> GetAsync(Guid id, CancellationToken ct)
    {
        var u = await db.Users.AsNoTracking().Include(x => x.RoleRef).FirstOrDefaultAsync(x => x.Id == id, ct);
        return u is null ? null : ToDto(u);
    }

    private static UserDto ToDto(User u) =>
        new(u.Id, u.Login, u.FullName, u.RoleId, u.RoleRef.Code, u.RoleRef.Name.ToDto(), u.IsActive, u.LastLoginAt);

    private static string? Validate(UserSaveRequest r, bool creating)
    {
        var login = r.Login?.Trim() ?? "";
        if (login.Length is < 2 or > 64) return "bad_login";
        if (string.IsNullOrWhiteSpace(r.FullName)) return "name_required";
        if (creating && string.IsNullOrWhiteSpace(r.Password)) return "password_required";
        if (!string.IsNullOrWhiteSpace(r.Password) && r.Password.Length < 6) return "bad_password";
        return null;
    }

    private async Task<bool> RoleOkAsync(Guid id, CancellationToken ct) =>
        await db.Roles.AnyAsync(r => r.Id == id && !r.IsArchived, ct);

    private static bool HasAdmin(User u) =>
        u.IsActive && RoleService.Parse(u.RoleRef.PermissionsJson).Contains(Perm.UsersEdit);

    private async Task<bool> RoleHasAdminAsync(Guid roleId, CancellationToken ct)
    {
        var json = await db.Roles.Where(r => r.Id == roleId).Select(r => r.PermissionsJson).FirstOrDefaultAsync(ct);
        return json is not null && RoleService.Parse(json).Contains(Perm.UsersEdit);
    }

    /// <summary>False when this is the last active user who can edit users.</summary>
    private async Task<bool> CanDropAdminAsync(User u, CancellationToken ct)
    {
        var others = await db.Users.AsNoTracking().Include(x => x.RoleRef)
            .Where(x => x.Id != u.Id && x.IsActive).ToListAsync(ct);
        return others.Any(HasAdmin);
    }
}
