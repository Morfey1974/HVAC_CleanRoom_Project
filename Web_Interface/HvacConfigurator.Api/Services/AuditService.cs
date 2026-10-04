using System.Security.Claims;
using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Entities;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Services;

public class AuditService(AppDbContext db, IHttpContextAccessor http)
{
    public string CurrentLogin => http.HttpContext?.User.Identity?.Name ?? "";

    public void Add(string action, string target, string? oldValue = null, string? newValue = null,
        string? reason = null, string? loginOverride = null, Guid? userIdOverride = null, Guid? projectId = null)
    {
        var principal = http.HttpContext?.User;
        var sub = principal?.FindFirstValue(ClaimTypes.NameIdentifier) ?? principal?.FindFirstValue("sub");
        db.AuditEntries.Add(new AuditEntry
        {
            UserId = userIdOverride ?? (Guid.TryParse(sub, out var id) ? id : null),
            UserLogin = loginOverride ?? principal?.Identity?.Name ?? "",
            Source = http.HttpContext?.Connection.RemoteIpAddress?.ToString() ?? "",
            Action = action,
            Target = target.Length > 256 ? target[..256] : target,
            OldValue = oldValue,
            NewValue = newValue,
            Reason = string.IsNullOrWhiteSpace(reason) ? null : reason.Trim(),
            ProjectId = projectId,
        });
    }

    public async Task<AuditPage> ListAsync(int offset, int limit, Guid? projectId, CancellationToken ct)
    {
        var q = db.AuditEntries.AsNoTracking();
        if (projectId is not null) q = q.Where(x => x.ProjectId == projectId);
        var total = await q.CountAsync(ct);
        var items = await q.OrderByDescending(x => x.Id).Skip(offset).Take(Math.Clamp(limit, 1, 200))
            .Select(x => new AuditEntryDto(x.Id, x.At, x.UserLogin, x.Source, x.Action, x.Target, x.OldValue, x.NewValue, x.Reason))
            .ToListAsync(ct);
        return new AuditPage(items, total);
    }
}
