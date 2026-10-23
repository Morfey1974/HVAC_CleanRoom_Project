using Microsoft.AspNetCore.Authorization;
using Microsoft.Extensions.Options;

namespace HvacConfigurator.Api.Authorization;

/// <summary>Requires the permission claim issued at login from the user's role.</summary>
public sealed class HasPermissionAttribute : AuthorizeAttribute
{
    public HasPermissionAttribute(string permission) => Policy = PermissionPolicyProvider.Prefix + permission;
}

public sealed class PermissionPolicyProvider(IOptions<AuthorizationOptions> options) : IAuthorizationPolicyProvider
{
    public const string Prefix = "perm:";
    private readonly DefaultAuthorizationPolicyProvider _inner = new(options);

    public Task<AuthorizationPolicy> GetDefaultPolicyAsync() => _inner.GetDefaultPolicyAsync();
    public Task<AuthorizationPolicy?> GetFallbackPolicyAsync() => _inner.GetFallbackPolicyAsync();

    public Task<AuthorizationPolicy?> GetPolicyAsync(string policyName)
    {
        if (policyName.StartsWith(Prefix, StringComparison.Ordinal))
        {
            var key = policyName[Prefix.Length..];
            var policy = new AuthorizationPolicyBuilder().RequireAuthenticatedUser().RequireClaim("perm", key).Build();
            return Task.FromResult<AuthorizationPolicy?>(policy);
        }
        return _inner.GetPolicyAsync(policyName);
    }
}
