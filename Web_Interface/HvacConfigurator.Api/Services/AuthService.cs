using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Dto;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Services;

public class AuthService(AppDbContext db, JwtTokenService jwt, AuditService audit)
{
    public async Task<AuthResponse?> LoginAsync(LoginRequest req, CancellationToken ct)
    {
        var login = req.Login.Trim().ToLowerInvariant();
        var user = await db.Users.Include(u => u.RoleRef).FirstOrDefaultAsync(u => u.Login == login && u.IsActive, ct);
        if (user is null || user.RoleRef.IsArchived || !BCrypt.Net.BCrypt.Verify(req.Password, user.PasswordHash))
        {
            audit.Add("auth.loginFailed", login, loginOverride: login);
            await db.SaveChangesAsync(ct);
            return null;
        }

        user.LastLoginAt = DateTime.UtcNow;
        audit.Add("auth.login", user.Login, loginOverride: user.Login, userIdOverride: user.Id);
        await db.SaveChangesAsync(ct);
        var perms = RoleService.Parse(user.RoleRef.PermissionsJson);
        return new AuthResponse(
            jwt.Create(user, perms), user.Id, user.Login, user.FullName, user.RoleRef.Code,
            user.RoleId, user.RoleRef.Name.ToDto(), perms);
    }
}
