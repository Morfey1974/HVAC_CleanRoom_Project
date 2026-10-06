using System.Text.RegularExpressions;
using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Plc;
using HvacConfigurator.Api.Services;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;

namespace HvacConfigurator.Api.Controllers;

/// <summary>PLC CAN3 line: bench check and the doors master (Shared_Libs/hvac_can.h, DM_*).</summary>
[ApiController]
[Route("api/[controller]")]
[Authorize(Roles = "Engineer,Admin")]
public partial class Can3Controller(PlcFirmwareClient plc, AuditService audit, AppDbContext db) : ControllerBase
{
    [GeneratedRegex("^([0-9a-fA-F]{2}){0,8}$")]
    private static partial Regex HexBytes();

    /// <summary>{online, stale, error, plc: raw PLC JSON}.</summary>
    [HttpGet]
    public async Task<IActionResult> Status(CancellationToken ct)
    {
        string? text;
        try { text = await plc.GetCan3Async(ct); }
        catch (PlcUnavailableException ex)
        {
            return Content($"{{\"online\":false,\"stale\":false,\"error\":\"{ex.Code}\",\"plc\":null}}", "application/json");
        }
        return Content(text is null
            ? "{\"online\":true,\"stale\":true,\"error\":null,\"plc\":null}"
            : $"{{\"online\":true,\"stale\":false,\"error\":null,\"plc\":{text}}}", "application/json");
    }

    [HttpPost("send")]
    public async Task<IActionResult> Send([FromQuery] int id, [FromQuery] string? data, CancellationToken ct)
    {
        data ??= "";
        if (id is < 0 or > 0x7FF || !HexBytes().IsMatch(data)) return BadRequest(new { message = "bad_args" });
        var text = await Command(() => plc.Can3SendAsync(id, data.ToLowerInvariant(), ct));
        audit.Add("can3.send", $"0x{id:x3}", newValue: data.ToLowerInvariant());
        await db.SaveChangesAsync(ct);
        return Content(text, "application/json");
    }

    [HttpPost("doors-cmd")]
    public async Task<IActionResult> DoorsCmd([FromQuery] int cmd, [FromQuery] int mask = 0xFF, CancellationToken ct = default)
    {
        if (cmd is < 0 or > 0xFF || mask is < 0 or > 0xFF) return BadRequest(new { message = "bad_args" });
        var text = await Command(() => plc.DoorsCmdAsync(cmd, mask, ct));
        audit.Add("doors.cmd", $"cmd {cmd}", newValue: $"mask 0x{mask:x2}");
        await db.SaveChangesAsync(ct);
        return Content(text, "application/json");
    }

    private static async Task<string> Command(Func<Task<string>> call)
    {
        try { return await call(); }
        catch (PlcUnavailableException ex) { return $"{{\"ok\":false,\"error\":\"{ex.Code}\"}}"; }
    }
}
