using System.Text.Json;
using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Entities;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Services;

public class LibraryService(AppDbContext db, AuditService audit)
{
    public async Task<List<LibraryItemDto>> ListAsync(string? category, bool archived, CancellationToken ct)
    {
        var q = db.LibraryItems.AsNoTracking().Where(x => x.IsArchived == archived);
        if (!string.IsNullOrEmpty(category)) q = q.Where(x => x.Category == category);
        var items = await q.OrderBy(x => x.Category).ThenBy(x => x.TypeCode).ThenBy(x => x.Code).ToListAsync(ct);
        var usage = await UsageAsync(items.Select(i => i.Id).ToList(), ct);
        return items.Select(i => ToDto(i, usage.GetValueOrDefault(i.Id))).ToList();
    }

    public async Task<LibraryItemDto?> GetAsync(Guid id, CancellationToken ct)
    {
        var i = await db.LibraryItems.AsNoTracking().FirstOrDefaultAsync(x => x.Id == id, ct);
        if (i is null) return null;
        var usage = await UsageAsync([id], ct);
        return ToDto(i, usage.GetValueOrDefault(id));
    }

    public async Task<(LibraryItemDto? dto, string? error)> CreateAsync(LibraryItemSaveRequest req, CancellationToken ct)
    {
        if (await ValidateAsync(null, req, ct) is { } err) return (null, err);
        if (GraphicJson(req, out var graphic) is { } gErr) return (null, gErr);
        var i = new LibraryItem();
        Apply(i, req);
        if (graphic is not null) i.GraphicJson = graphic;
        db.LibraryItems.Add(i);
        audit.Add("library.create", Label(i), newValue: Localized.ToJson(ToDto(i, 0)), reason: req.Reason);
        await db.SaveChangesAsync(ct);
        return (ToDto(i, 0), null);
    }

    public async Task<(LibraryItemDto? dto, string? error)> UpdateAsync(Guid id, LibraryItemSaveRequest req, CancellationToken ct)
    {
        var i = await db.LibraryItems.FirstOrDefaultAsync(x => x.Id == id, ct);
        if (i is null) return (null, "not_found");
        if (await ValidateAsync(id, req, ct) is { } err) return (null, err);
        if (GraphicJson(req, out var graphic) is { } gErr) return (null, gErr);
        var before = Localized.ToJson(ToDto(i, 0));
        Apply(i, req);
        if (graphic is not null) i.GraphicJson = graphic;
        i.Version++;
        i.UpdatedAt = DateTime.UtcNow;
        audit.Add("library.update", Label(i), before, Localized.ToJson(ToDto(i, 0)), req.Reason);
        await db.SaveChangesAsync(ct);
        return (await GetAsync(id, ct), null);
    }

    public async Task<LibraryItemDto?> SetArchivedAsync(Guid id, bool archived, string? reason, CancellationToken ct)
    {
        var i = await db.LibraryItems.FirstOrDefaultAsync(x => x.Id == id, ct);
        if (i is null) return null;
        i.IsArchived = archived;
        i.UpdatedAt = DateTime.UtcNow;
        audit.Add(archived ? "library.archive" : "library.restore", Label(i), reason: reason);
        await db.SaveChangesAsync(ct);
        return await GetAsync(id, ct);
    }

    /// <summary>Own modules follow the article / type code rules of plan 8.3.1.</summary>
    private async Task<string?> ValidateAsync(Guid? id, LibraryItemSaveRequest r, CancellationToken ct)
    {
        if (!LibraryCategories.All.Contains(r.Category)) return "bad_category";
        var code = r.Code?.Trim() ?? "";
        if (code.Length == 0 && !r.Name.HasAny()) return "name_required";
        if (code.Length > 0 && await db.LibraryItems.AnyAsync(x => x.Id != id && x.Code == code, ct)) return "article_taken";

        if (r.Category != LibraryCategories.Module) return null;
        if (!ModuleRules.ArticleFormat().IsMatch(code)) return "article_format";
        if (r.TypeCode is not (>= 1 and <= 255)) return "type_code_required";
        if (!ModuleRules.PrefixFormat().IsMatch(r.SystemPrefix?.Trim() ?? "")) return "prefix_format";
        if ((r.ChannelModes ?? []).Any(m => !ModuleRules.SignalModes.Contains(m))) return "channel_mode";
        if (await db.LibraryItems.AnyAsync(x => x.Id != id && x.Category == LibraryCategories.Module && x.TypeCode == r.TypeCode && !x.IsArchived, ct))
            return "type_code_taken";
        return null;
    }

    /// <summary>No drawing in the request keeps the stored one.</summary>
    private static string? GraphicJson(LibraryItemSaveRequest r, out string? json)
    {
        json = null;
        if (r.Graphic is null) return null;
        if (ModuleGraphics.Validate(r.Graphic, out var clean) is { } err) return err;
        json = ModuleGraphics.ToJson(clean);
        return null;
    }

    private async Task<Dictionary<Guid, int>> UsageAsync(List<Guid> ids, CancellationToken ct)
    {
        var eq = await db.ProjectEquipment.AsNoTracking()
            .Where(e => e.LibraryItemId != null && ids.Contains(e.LibraryItemId.Value))
            .Select(e => new { Lib = e.LibraryItemId!.Value, e.ProjectId }).ToListAsync(ct);
        var mod = await db.ProjectModules.AsNoTracking()
            .Where(e => e.LibraryItemId != null && ids.Contains(e.LibraryItemId.Value))
            .Select(e => new { Lib = e.LibraryItemId!.Value, e.ProjectId }).ToListAsync(ct);
        return eq.Concat(mod).GroupBy(x => x.Lib).ToDictionary(g => g.Key, g => g.Select(x => x.ProjectId).Distinct().Count());
    }

    private static void Apply(LibraryItem i, LibraryItemSaveRequest q)
    {
        i.Category = q.Category;
        i.Code = q.Code.Trim();
        i.Name = q.Name.ToEntity();
        i.Manufacturer = q.Manufacturer?.Trim() ?? "";
        i.Model = q.Model?.Trim() ?? "";
        i.Description = q.Description?.Trim() ?? "";
        var isModule = q.Category == LibraryCategories.Module;
        i.TypeCode = isModule ? q.TypeCode : null;
        i.SystemPrefix = isModule ? q.SystemPrefix?.Trim() ?? "" : "";
        i.ChannelCount = q.ChannelCount is > 0 ? q.ChannelCount : null;
        i.ChannelModes = !isModule ? []
            : q.ChannelModes is null ? i.ChannelModes
            : ModuleRules.SignalModes.Where(q.ChannelModes.Contains).ToList();
        var props = (q.Props ?? []).Where(p => !string.IsNullOrWhiteSpace(p.Key))
            .Select(p => new LibraryPropDto(p.Key.Trim(), p.Value?.Trim() ?? "", p.Unit?.Trim() ?? "")).ToList();
        i.PropsJson = JsonSerializer.Serialize(props, Localized.Json);
    }

    private static string Label(LibraryItem i) => string.IsNullOrWhiteSpace(i.Code) ? i.Name.Label(i.Id.ToString()) : i.Code;

    public static IReadOnlyList<LibraryPropDto> ParseProps(string json)
    {
        try { return JsonSerializer.Deserialize<List<LibraryPropDto>>(json, Localized.Json) ?? []; }
        catch (JsonException) { return []; }
    }

    public static string? KindName(LibraryItem i) =>
        i.Category == LibraryCategories.Module && i.TypeCode is { } tc ? ModuleRules.KindOf(tc).ToString().ToLowerInvariant() : null;

    public static ModuleGraphicDto? GraphicOf(LibraryItem i) =>
        ModuleGraphics.Parse(i.GraphicJson) ??
        (i.Category == LibraryCategories.Module
            ? ModuleGraphics.Default(i.Code, i.TypeCode is { } tc ? ModuleRules.KindOf(tc) : null, i.ChannelCount)
            : null);

    public static LibraryItemDto ToDto(LibraryItem i, int used) => new(
        i.Id, i.Category, i.Code, i.Name.ToDto(), i.Manufacturer, i.Model, i.Description,
        ParseProps(i.PropsJson), i.Version, i.IsArchived, i.UpdatedAt, used,
        i.TypeCode, i.SystemPrefix, i.ChannelCount, KindName(i), GraphicOf(i), i.ChannelModes);
}
