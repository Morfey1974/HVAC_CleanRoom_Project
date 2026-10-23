using HvacConfigurator.Api.Authorization;
using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Services;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;

namespace HvacConfigurator.Api.Controllers;

[ApiController]
[Route("api/[controller]")]
public class SettingsController(SettingsService settings, UserService users, RoleService roles) : ControllerBase
{
    /// <summary>Available before login: startup language and site name for the login screen.</summary>
    [HttpGet("public")]
    public async Task<IActionResult> Public(CancellationToken ct) => Ok(await settings.GetPublicAsync(ct));

    [HttpGet]
    [HasPermission(Perm.SettingsView)]
    public async Task<IActionResult> Get(CancellationToken ct) => Ok(await settings.GetAsync(ct));

    [HttpPut]
    [HasPermission(Perm.SettingsEdit)]
    public async Task<IActionResult> Update([FromBody] SettingsDto req, CancellationToken ct)
    {
        var r = await settings.UpdateAsync(req, ct);
        return r is null ? BadRequest(new { message = "invalid_language" }) : Ok(r);
    }

    [HttpGet("permissions")]
    [HasPermission(Perm.RolesView)]
    public IActionResult PermissionCatalog() => Ok(roles.Catalog());

    [HttpGet("roles")]
    [Authorize]
    public async Task<IActionResult> Roles([FromQuery] bool archived = false, CancellationToken ct = default)
    {
        if (!User.HasClaim("perm", Perm.RolesView) && !User.HasClaim("perm", Perm.UsersView))
            return Forbid();
        return Ok(await roles.ListAsync(archived, ct));
    }

    [HttpPost("roles")]
    [HasPermission(Perm.RolesEdit)]
    public async Task<IActionResult> CreateRole([FromBody] RoleSaveRequest req, CancellationToken ct)
    {
        var (dto, err) = await roles.CreateAsync(req, ct);
        return err is null ? Ok(dto) : BadRequest(new { message = err });
    }

    [HttpPut("roles/{id:guid}")]
    [HasPermission(Perm.RolesEdit)]
    public async Task<IActionResult> UpdateRole(Guid id, [FromBody] RoleSaveRequest req, CancellationToken ct)
    {
        var (dto, err) = await roles.UpdateAsync(id, req, ct);
        return err switch { null => Ok(dto), "not_found" => NotFound(), _ => BadRequest(new { message = err }) };
    }

    [HttpPost("roles/{id:guid}/archive")]
    [HasPermission(Perm.RolesEdit)]
    public async Task<IActionResult> ArchiveRole(Guid id, [FromQuery] bool value = true, [FromQuery] string? reason = null, CancellationToken ct = default)
    {
        var (dto, err) = await roles.SetArchivedAsync(id, value, reason, ct);
        return err switch { null => Ok(dto), "not_found" => NotFound(), _ => BadRequest(new { message = err }) };
    }

    [HttpGet("users")]
    [HasPermission(Perm.UsersView)]
    public async Task<IActionResult> Users(CancellationToken ct) => Ok(await users.ListAsync(ct));

    [HttpPost("users")]
    [HasPermission(Perm.UsersEdit)]
    public async Task<IActionResult> CreateUser([FromBody] UserSaveRequest req, CancellationToken ct)
    {
        var (dto, err) = await users.CreateAsync(req, ct);
        return err is null ? Ok(dto) : BadRequest(new { message = err });
    }

    [HttpPut("users/{id:guid}")]
    [HasPermission(Perm.UsersEdit)]
    public async Task<IActionResult> UpdateUser(Guid id, [FromBody] UserSaveRequest req, CancellationToken ct)
    {
        var (dto, err) = await users.UpdateAsync(id, req, ct);
        return err switch { null => Ok(dto), "not_found" => NotFound(), _ => BadRequest(new { message = err }) };
    }
}
