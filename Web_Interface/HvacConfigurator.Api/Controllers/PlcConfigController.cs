using HvacConfigurator.Api.Authorization;
using HvacConfigurator.Api.Services;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;

namespace HvacConfigurator.Api.Controllers;

[ApiController]
[Route("api/projects/{projectId:guid}/plc-config")]
[Authorize]
public class PlcConfigController(PlcConfigService config) : ControllerBase
{
    /// <summary>Configuration built from the project and what the PLC reports about the one it runs.</summary>
    [HttpGet]
    [HasPermission(Perm.PlcConfigView)]
    public async Task<IActionResult> Get(Guid projectId, CancellationToken ct) =>
        await config.ViewAsync(projectId, ct) is { } v ? Ok(v) : NotFound();

    [HttpPost("upload")]
    [HasPermission(Perm.PlcConfigUpload)]
    public async Task<IActionResult> Upload(Guid projectId, [FromQuery] string? reason, CancellationToken ct) =>
        await config.UploadAsync(projectId, reason, ct) is { } r ? Ok(r) : NotFound();
}
