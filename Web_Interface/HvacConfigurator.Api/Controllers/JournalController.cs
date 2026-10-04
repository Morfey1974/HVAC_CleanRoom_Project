using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Services;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Controllers;

[ApiController]
[Authorize]
[Route("api/[controller]")]
public class JournalController(AuditService audit, AppDbContext db) : ControllerBase
{
    [HttpGet("audit")]
    [Authorize(Roles = "Engineer,Admin")]
    public async Task<IActionResult> Audit([FromQuery] int offset = 0, [FromQuery] int limit = 50, [FromQuery] Guid? projectId = null, CancellationToken ct = default) =>
        Ok(await audit.ListAsync(offset, limit, projectId, ct));

    [HttpGet("alarms")]
    public async Task<IActionResult> Alarms([FromQuery] bool active = false, [FromQuery] int limit = 200, CancellationToken ct = default)
    {
        var q = db.AlarmEvents.AsNoTracking().AsQueryable();
        if (active) q = q.Where(a => a.ClearedAt == null);
        var items = await q.OrderByDescending(a => a.Id).Take(Math.Clamp(limit, 1, 1000))
            .Select(a => new AlarmEventDto(a.Id, a.Code, a.Element, a.StartedAt, a.ClearedAt, a.AckAt, a.AckBy))
            .ToListAsync(ct);
        return Ok(items);
    }

    [HttpPost("alarms/{id:long}/ack")]
    [Authorize(Roles = "Operator,Engineer,Admin")]
    public async Task<IActionResult> Ack(long id, CancellationToken ct)
    {
        var a = await db.AlarmEvents.FirstOrDefaultAsync(x => x.Id == id, ct);
        if (a is null) return NotFound();
        if (a.AckAt is null)
        {
            a.AckAt = DateTime.UtcNow;
            a.AckBy = User.Identity?.Name;
            audit.Add("alarm.ack", a.Code);
            await db.SaveChangesAsync(ct);
        }
        return NoContent();
    }
}
