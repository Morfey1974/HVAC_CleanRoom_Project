using System.Text.Json;
using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Entities;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Services;

public class SettingsService(AppDbContext db, AuditService audit)
{
    private static readonly string[] Languages = ["he", "ru", "en"];

    public async Task<SettingsDto> GetAsync(CancellationToken ct) => ToDto(await LoadAsync(ct));

    public async Task<PublicSettingsDto> GetPublicAsync(CancellationToken ct)
    {
        var s = await LoadAsync(ct);
        return new PublicSettingsDto(s.SiteName, s.StartupLanguage);
    }

    public async Task<SettingsDto?> UpdateAsync(SettingsDto req, CancellationToken ct)
    {
        if (!Languages.Contains(req.StartupLanguage)) return null;
        var s = await LoadAsync(ct);
        var before = JsonSerializer.Serialize(ToDto(s));
        s.SiteName = req.SiteName.Trim();
        s.StartupLanguage = req.StartupLanguage;
        s.PlcMode = req.PlcMode == "plc" ? "plc" : "simulator";
        s.PlcAddress = req.PlcAddress.Trim();
        s.UpdatedAt = DateTime.UtcNow;
        audit.Add("settings.update", "system", before, JsonSerializer.Serialize(ToDto(s)));
        await db.SaveChangesAsync(ct);
        return ToDto(s);
    }

    private async Task<SystemSettings> LoadAsync(CancellationToken ct) =>
        await db.SystemSettings.FirstOrDefaultAsync(ct) ?? db.SystemSettings.Add(new SystemSettings()).Entity;

    private static SettingsDto ToDto(SystemSettings s) => new(s.SiteName, s.StartupLanguage, s.PlcMode, s.PlcAddress);
}
