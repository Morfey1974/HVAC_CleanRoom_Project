using HvacConfigurator.Api.Services;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;

namespace HvacConfigurator.Api.Controllers;

/// <summary>HMI picture library (shared) and HMI screens of a project.</summary>
[ApiController]
[Authorize]
public class HmiController(HmiService hmi) : ControllerBase
{
    [HttpGet("api/hmi/elements")]
    public async Task<IActionResult> Elements([FromQuery] bool archived = false, CancellationToken ct = default) =>
        Ok(await hmi.ListElementsAsync(archived, ct));

    [HttpPost("api/hmi/elements")]
    [Authorize(Roles = "Engineer,Admin")]
    public async Task<IActionResult> CreateElement([FromBody] HmiElementSaveRequest req, CancellationToken ct)
    {
        var (dto, err) = await hmi.CreateElementAsync(req, ct);
        return err is null ? Ok(dto) : BadRequest(new { message = err });
    }

    [HttpPut("api/hmi/elements/{id:guid}")]
    [Authorize(Roles = "Engineer,Admin")]
    public async Task<IActionResult> UpdateElement(Guid id, [FromBody] HmiElementSaveRequest req, CancellationToken ct)
    {
        var (dto, err) = await hmi.UpdateElementAsync(id, req, ct);
        return err switch { null => Ok(dto), "not_found" => NotFound(), _ => BadRequest(new { message = err }) };
    }

    [HttpPost("api/hmi/elements/{id:guid}/archive")]
    [Authorize(Roles = "Engineer,Admin")]
    public async Task<IActionResult> Archive(Guid id, [FromQuery] bool value = true, [FromQuery] string? reason = null, CancellationToken ct = default) =>
        await hmi.SetArchivedAsync(id, value, reason, ct) is { } e ? Ok(e) : NotFound();

    [HttpGet("api/projects/{pid:guid}/hmi-screens")]
    public async Task<IActionResult> Screens(Guid pid, CancellationToken ct) => Ok(await hmi.ListScreensAsync(pid, ct));

    [HttpPost("api/projects/{pid:guid}/hmi-screens")]
    [Authorize(Roles = "Engineer,Admin")]
    public async Task<IActionResult> CreateScreen(Guid pid, [FromBody] HmiScreenSaveRequest req, CancellationToken ct)
    {
        var (dto, err) = await hmi.CreateScreenAsync(pid, req, ct);
        return err switch { null => Ok(dto), "not_found" => NotFound(), _ => BadRequest(new { message = err }) };
    }

    [HttpPut("api/projects/{pid:guid}/hmi-screens/{id:guid}")]
    [Authorize(Roles = "Engineer,Admin")]
    public async Task<IActionResult> UpdateScreen(Guid pid, Guid id, [FromBody] HmiScreenSaveRequest req, CancellationToken ct)
    {
        var (dto, err) = await hmi.UpdateScreenAsync(pid, id, req, ct);
        return err switch { null => Ok(dto), "not_found" => NotFound(), _ => BadRequest(new { message = err }) };
    }

    [HttpDelete("api/projects/{pid:guid}/hmi-screens/{id:guid}")]
    [Authorize(Roles = "Engineer,Admin")]
    public async Task<IActionResult> DeleteScreen(Guid pid, Guid id, [FromQuery] string? reason, CancellationToken ct) =>
        await hmi.DeleteScreenAsync(pid, id, reason, ct) ? NoContent() : NotFound();
}
