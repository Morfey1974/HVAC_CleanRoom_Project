namespace HvacConfigurator.Api.Configuration;

public class JwtSettings
{
    public const string SectionName = "Jwt";
    public string Secret { get; set; } = "";
    public string Issuer { get; set; } = "HvacCleanRoom";
    public string Audience { get; set; } = "HvacCleanRoom";
    public int ExpiryMinutes { get; set; } = 480;
}
