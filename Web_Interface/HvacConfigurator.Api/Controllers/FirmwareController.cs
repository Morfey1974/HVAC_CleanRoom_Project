using HvacConfigurator.Api.Plc;
using HvacConfigurator.Api.Services;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;

namespace HvacConfigurator.Api.Controllers;

[ApiController]
[Route("api/[controller]")]
[Authorize(Roles = "Engineer,Admin")]
public class FirmwareController(FirmwareService firmware, PlcFirmwareClient plc) : ControllerBase
{
    private const long MaxUploadBytes = 8 * 1024 * 1024;

    [HttpGet]
    public async Task<IActionResult> List(CancellationToken ct) => Ok(await firmware.ListAsync(ct));

    /// <summary>Accepts the module .bin (from the application start) or .elf.</summary>
    [HttpPost]
    [RequestSizeLimit(MaxUploadBytes)]
    [RequestFormLimits(MultipartBodyLengthLimit = MaxUploadBytes)]
    public async Task<IActionResult> Upload(IFormFile file, [FromForm] string? notes, CancellationToken ct)
    {
        var (f, err) = await firmware.UploadAsync(file, notes, ct);
        return f is null ? BadRequest(new { message = err }) : Ok(f);
    }

    /// <summary>Module builds in the project folders (development computer only).</summary>
    [HttpGet("builds")]
    public async Task<IActionResult> Builds(CancellationToken ct) => Ok(await firmware.BuildsAsync(ct));

    [HttpPost("from-build")]
    public async Task<IActionResult> FromBuild([FromQuery] int type, CancellationToken ct)
    {
        var (f, err) = await firmware.AddFromBuildAsync(type, ct);
        return f is null ? BadRequest(new { message = err }) : Ok(f);
    }

    [HttpGet("{id:guid}/download")]
    public async Task<IActionResult> Download(Guid id, CancellationToken ct) =>
        await firmware.GetAsync(id, ct) is { } f
            ? File(f.Data, "application/octet-stream", Path.ChangeExtension(f.FileName, ".bin"))
            : NotFound();

    [HttpDelete("{id:guid}")]
    public async Task<IActionResult> Delete(Guid id, [FromQuery] string? reason, CancellationToken ct) =>
        await firmware.DeleteAsync(id, reason, ct) ? NoContent() : NotFound();

    [HttpPost("{id:guid}/send")]
    public async Task<IActionResult> Send(Guid id, [FromQuery] string? reason, CancellationToken ct) =>
        await firmware.SendToPlcAsync(id, reason, ct) is { } r ? Ok(r) : NotFound();

    [HttpPost("run")]
    public async Task<IActionResult> Run([FromQuery] int type, [FromQuery] int mode, [FromQuery] string? reason, CancellationToken ct)
    {
        if (type is < 1 or > FirmwareImage.MaxModuleType || mode is < 1 or > 2) return BadRequest(new { message = "bad_args" });
        return Ok(await firmware.RunAsync(type, mode, reason, ct));
    }

    [HttpPost("rollback")]
    public async Task<IActionResult> Rollback([FromQuery] int type, [FromQuery] string? reason, CancellationToken ct)
    {
        if (type is < 1 or > FirmwareImage.MaxModuleType) return BadRequest(new { message = "bad_args" });
        return Ok(await firmware.RollbackAsync(type, reason, ct));
    }

    /// <summary>New search of module places (IDs) on all lines.</summary>
    [HttpPost("id-walk")]
    public async Task<IActionResult> IdWalk(CancellationToken ct) => Ok(await firmware.IdWalkAsync(ct));

    [HttpPost("cancel")]
    public async Task<IActionResult> Cancel(CancellationToken ct) => Ok(await firmware.CancelAsync(ct));

    /// <summary>PLC firmware state as {online, stale, uploading, error, plc: raw PLC JSON}.</summary>
    [HttpGet("plc")]
    public async Task<IActionResult> PlcStatus([FromQuery] long log = 0, CancellationToken ct = default)
    {
        string? text;
        try
        {
            text = await plc.GetStatusAsync(log, ct);
        }
        catch (PlcUnavailableException ex)
        {
            return Content($"{{\"online\":false,\"stale\":false,\"uploading\":false,\"error\":\"{ex.Code}\",\"plc\":null}}", "application/json");
        }
        var stale = text is null;
        text ??= plc.LastStatus;
        var up = plc.Uploading ? "true" : "false";
        return Content(text is null
                ? $"{{\"online\":true,\"stale\":true,\"uploading\":{up},\"error\":null,\"plc\":null}}"
                : $"{{\"online\":true,\"stale\":{(stale ? "true" : "false")},\"uploading\":{up},\"error\":null,\"plc\":{text}}}",
            "application/json");
    }
}
