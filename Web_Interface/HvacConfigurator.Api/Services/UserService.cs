using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Dto;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Services;

public class UserService(AppDbContext db)
{
    public async Task<List<UserDto>> ListAsync(CancellationToken ct) =>
        await db.Users.AsNoTracking().OrderBy(u => u.Login)
            .Select(u => new UserDto(u.Id, u.Login, u.FullName, u.Role.ToString(), u.IsActive, u.LastLoginAt))
            .ToListAsync(ct);
}
