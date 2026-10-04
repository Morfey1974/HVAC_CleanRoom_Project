using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Services;
using Microsoft.AspNetCore.Mvc;

namespace HvacConfigurator.Api.Controllers;

[ApiController]
[Route("api/[controller]")]
public class AuthController(AuthService auth) : ControllerBase
{
    [HttpPost("login")]
    public async Task<IActionResult> Login([FromBody] LoginRequest request, CancellationToken ct)
    {
        var result = await auth.LoginAsync(request, ct);
        return result is null ? Unauthorized(new { message = "invalid_credentials" }) : Ok(result);
    }
}
