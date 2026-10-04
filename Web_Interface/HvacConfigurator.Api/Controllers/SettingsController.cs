using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Services;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;

namespace HvacConfigurator.Api.Controllers;

[ApiController]
[Route("api/[controller]")]
public class SettingsController(SettingsService settings, UserService users) : ControllerBase
{
    /// <summary>Available before login: startup language and site name for the login screen.</summary>
    [HttpGet("public")]
    public async Task<IActionResult> Public(CancellationToken ct) => Ok(await settings.GetPublicAsync(ct));

    [HttpGet]
    [Authorize]
    public async Task<IActionResult> Get(CancellationToken ct) => Ok(await settings.GetAsync(ct));

    [HttpPut]
    [Authorize(Roles = "Admin")]
    public async Task<IActionResult> Update([FromBody] SettingsDto req, CancellationToken ct)
    {
        var r = await settings.UpdateAsync(req, ct);
        return r is null ? BadRequest(new { message = "invalid_language" }) : Ok(r);
    }

    [HttpGet("users")]
    [Authorize(Roles = "Admin")]
    public async Task<IActionResult> Users(CancellationToken ct) => Ok(await users.ListAsync(ct));
}
