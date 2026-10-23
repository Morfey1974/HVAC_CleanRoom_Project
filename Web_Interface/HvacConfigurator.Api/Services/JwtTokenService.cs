using System.IdentityModel.Tokens.Jwt;
using System.Security.Claims;
using System.Text;
using HvacConfigurator.Api.Configuration;
using HvacConfigurator.Api.Entities;
using Microsoft.Extensions.Options;
using Microsoft.IdentityModel.Tokens;

namespace HvacConfigurator.Api.Services;

public class JwtTokenService(IOptions<JwtSettings> options)
{
    private readonly JwtSettings _s = options.Value;

    public string Create(User user, IReadOnlyList<string> permissions)
    {
        var claims = new List<Claim>
        {
            new(JwtRegisteredClaimNames.Sub, user.Id.ToString()),
            new(ClaimTypes.Name, user.Login),
            new(ClaimTypes.Role, user.RoleRef.Code),
        };
        claims.AddRange(permissions.Select(p => new Claim("perm", p)));
        var key = new SymmetricSecurityKey(Encoding.UTF8.GetBytes(_s.Secret));
        var token = new JwtSecurityToken(
            _s.Issuer,
            _s.Audience,
            claims,
            expires: DateTime.UtcNow.AddMinutes(_s.ExpiryMinutes),
            signingCredentials: new SigningCredentials(key, SecurityAlgorithms.HmacSha256));
        return new JwtSecurityTokenHandler().WriteToken(token);
    }
}
