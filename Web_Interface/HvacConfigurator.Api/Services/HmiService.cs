using System.Text.Json;
using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Entities;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Services;

public record HmiElementDto(
    Guid Id, string Code, string Group, string Kind, string Art, LocalizedTextDto Name, string Description,
    int Length, JsonElement Params, string Status, int Version, bool IsArchived, string UpdatedBy, DateTime UpdatedAt,
    int UsedInScreens);

public record HmiElementSaveRequest(
    string? Code, string Group, string Kind, string Art, LocalizedTextDto Name, string? Description,
    int Length, JsonElement? Params, string Status, string? Reason);

public record HmiScreenDto(Guid Id, string Kind, LocalizedTextDto Name, int SortOrder, JsonElement Content, DateTime UpdatedAt);

public record HmiScreenSaveRequest(string Kind, LocalizedTextDto Name, int SortOrder, JsonElement? Content, string? Reason);

/// <summary>HMI picture library and project HMI screens (plan 5.7).</summary>
public class HmiService(AppDbContext db, AuditService audit)
{
    public const int MinLength = 20;
    public const int MaxLength = 600;

    // ---------- library ----------

    public async Task<List<HmiElementDto>> ListElementsAsync(bool archived, CancellationToken ct)
    {
        var items = await db.HmiElements.AsNoTracking().Where(x => x.IsArchived == archived)
            .OrderBy(x => x.Group).ThenBy(x => x.Code).ToListAsync(ct);
        var usage = await UsageAsync(ct);
        return items.Select(i => ToDto(i, usage.GetValueOrDefault(i.Id))).ToList();
    }

    public async Task<HmiElementDto?> GetElementAsync(Guid id, CancellationToken ct)
    {
        var i = await db.HmiElements.AsNoTracking().FirstOrDefaultAsync(x => x.Id == id, ct);
        return i is null ? null : ToDto(i, (await UsageAsync(ct)).GetValueOrDefault(id));
    }

    public async Task<(HmiElementDto? dto, string? error)> CreateElementAsync(HmiElementSaveRequest req, CancellationToken ct)
    {
        if (await ValidateAsync(null, req, ct) is { } err) return (null, err);
        var i = new HmiElement();
        Apply(i, req);
        if (i.Code.Length == 0) i.Code = await FreeCodeAsync(i.Kind, ct);
        db.HmiElements.Add(i);
        audit.Add("hmi.element.create", i.Code, newValue: Localized.ToJson(ToDto(i, 0)), reason: req.Reason);
        await db.SaveChangesAsync(ct);
        return (ToDto(i, 0), null);
    }

    public async Task<(HmiElementDto? dto, string? error)> UpdateElementAsync(Guid id, HmiElementSaveRequest req, CancellationToken ct)
    {
        var i = await db.HmiElements.FirstOrDefaultAsync(x => x.Id == id, ct);
        if (i is null) return (null, "not_found");
        if (await ValidateAsync(id, req, ct) is { } err) return (null, err);
        var before = Localized.ToJson(ToDto(i, 0));
        var wasApproved = i.Status == HmiStatuses.Approved;
        Apply(i, req);
        if (i.Code.Length == 0) i.Code = await FreeCodeAsync(i.Kind, ct);
        i.Version++;
        audit.Add(!wasApproved && i.Status == HmiStatuses.Approved ? "hmi.element.approve" : "hmi.element.update",
            i.Code, before, Localized.ToJson(ToDto(i, 0)), req.Reason);
        await db.SaveChangesAsync(ct);
        return (await GetElementAsync(id, ct), null);
    }

    public async Task<HmiElementDto?> SetArchivedAsync(Guid id, bool archived, string? reason, CancellationToken ct)
    {
        var i = await db.HmiElements.FirstOrDefaultAsync(x => x.Id == id, ct);
        if (i is null) return null;
        i.IsArchived = archived;
        i.UpdatedAt = DateTime.UtcNow;
        audit.Add(archived ? "hmi.element.archive" : "hmi.element.restore", i.Code, reason: reason);
        await db.SaveChangesAsync(ct);
        return await GetElementAsync(id, ct);
    }

    private async Task<string?> ValidateAsync(Guid? id, HmiElementSaveRequest r, CancellationToken ct)
    {
        if (!HmiGroups.All.Contains(r.Group)) return "bad_group";
        if (!HmiStatuses.All.Contains(r.Status)) return "bad_status";
        if (string.IsNullOrWhiteSpace(r.Kind) || string.IsNullOrWhiteSpace(r.Art)) return "art_required";
        if (!r.Name.HasAny()) return "name_required";
        if (r.Length is < MinLength or > MaxLength) return "bad_length";
        if (r.Params is { } p && p.ValueKind != JsonValueKind.Object) return "bad_params";
        var code = r.Code?.Trim() ?? "";
        if (code.Length > 0 && await db.HmiElements.AnyAsync(x => x.Id != id && x.Code == code, ct)) return "code_taken";
        return null;
    }

    private async Task<string> FreeCodeAsync(string kind, CancellationToken ct)
    {
        var prefix = $"HMI-{kind.ToUpperInvariant()}-";
        var taken = await db.HmiElements.Where(x => x.Code.StartsWith(prefix)).Select(x => x.Code).ToListAsync(ct);
        for (var n = 1; ; n++)
        {
            var c = $"{prefix}{n:D2}";
            if (!taken.Contains(c)) return c;
        }
    }

    private void Apply(HmiElement i, HmiElementSaveRequest q)
    {
        i.Code = q.Code?.Trim() ?? "";
        i.Group = q.Group;
        i.Kind = q.Kind.Trim();
        i.Art = q.Art.Trim();
        i.Name = q.Name.ToEntity();
        i.Description = q.Description?.Trim() ?? "";
        i.Length = q.Length;
        i.ParamsJson = q.Params is { } p ? p.GetRawText() : "{}";
        i.Status = q.Status;
        i.UpdatedBy = audit.CurrentLogin;
        i.UpdatedAt = DateTime.UtcNow;
    }

    /// <summary>Number of project screens that hold a copy of each element.</summary>
    private async Task<Dictionary<Guid, int>> UsageAsync(CancellationToken ct)
    {
        var res = new Dictionary<Guid, int>();
        var contents = await db.HmiScreens.AsNoTracking().Where(s => s.Kind == HmiScreenKinds.Ahu).Select(s => s.ContentJson).ToListAsync(ct);
        foreach (var c in contents)
        {
            foreach (var id in ElementIds(c).Distinct()) res[id] = res.GetValueOrDefault(id) + 1;
        }
        return res;
    }

    private static IEnumerable<Guid> ElementIds(string content)
    {
        JsonDocument doc;
        try { doc = JsonDocument.Parse(content); }
        catch (JsonException) { yield break; }
        using (doc)
        {
            if (!doc.RootElement.TryGetProperty("items", out var items) || items.ValueKind != JsonValueKind.Array) yield break;
            foreach (var it in items.EnumerateArray())
            {
                if (it.TryGetProperty("elementId", out var e) && e.TryGetGuid(out var g)) yield return g;
            }
        }
    }

    private static JsonElement ParseObject(string json)
    {
        try
        {
            using var doc = JsonDocument.Parse(string.IsNullOrWhiteSpace(json) ? "{}" : json);
            return doc.RootElement.Clone();
        }
        catch (JsonException)
        {
            using var empty = JsonDocument.Parse("{}");
            return empty.RootElement.Clone();
        }
    }

    private static HmiElementDto ToDto(HmiElement i, int used) => new(
        i.Id, i.Code, i.Group, i.Kind, i.Art, i.Name.ToDto(), i.Description, i.Length, ParseObject(i.ParamsJson),
        i.Status, i.Version, i.IsArchived, i.UpdatedBy, i.UpdatedAt, used);

    // ---------- project screens ----------

    public async Task<List<HmiScreenDto>> ListScreensAsync(Guid projectId, CancellationToken ct) =>
        (await db.HmiScreens.AsNoTracking().Where(s => s.ProjectId == projectId)
            .OrderBy(s => s.SortOrder).ThenBy(s => s.Name.Ru).ToListAsync(ct))
        .Select(ToDto).ToList();

    public async Task<(HmiScreenDto? dto, string? error)> CreateScreenAsync(Guid projectId, HmiScreenSaveRequest req, CancellationToken ct)
    {
        if (ValidateScreen(req) is { } err) return (null, err);
        if (!await db.Projects.AnyAsync(p => p.Id == projectId, ct)) return (null, "not_found");
        var s = new HmiScreen { ProjectId = projectId };
        ApplyScreen(s, req);
        db.HmiScreens.Add(s);
        audit.Add("hmi.screen.create", s.Name.Label(s.Kind), newValue: s.ContentJson, reason: req.Reason, projectId: projectId);
        await db.SaveChangesAsync(ct);
        return (ToDto(s), null);
    }

    public async Task<(HmiScreenDto? dto, string? error)> UpdateScreenAsync(Guid projectId, Guid id, HmiScreenSaveRequest req, CancellationToken ct)
    {
        var s = await db.HmiScreens.FirstOrDefaultAsync(x => x.Id == id && x.ProjectId == projectId, ct);
        if (s is null) return (null, "not_found");
        if (ValidateScreen(req) is { } err) return (null, err);
        var before = s.ContentJson;
        ApplyScreen(s, req);
        audit.Add("hmi.screen.update", s.Name.Label(s.Kind), before, s.ContentJson, req.Reason, projectId: projectId);
        await db.SaveChangesAsync(ct);
        return (ToDto(s), null);
    }

    public async Task<bool> DeleteScreenAsync(Guid projectId, Guid id, string? reason, CancellationToken ct)
    {
        var s = await db.HmiScreens.FirstOrDefaultAsync(x => x.Id == id && x.ProjectId == projectId, ct);
        if (s is null) return false;
        audit.Add("hmi.screen.delete", s.Name.Label(s.Kind), s.ContentJson, reason: reason, projectId: projectId);
        db.HmiScreens.Remove(s);
        await db.SaveChangesAsync(ct);
        return true;
    }

    private static string? ValidateScreen(HmiScreenSaveRequest r)
    {
        if (!HmiScreenKinds.All.Contains(r.Kind)) return "bad_kind";
        if (r.Content is { } c && c.ValueKind != JsonValueKind.Object) return "bad_content";
        return null;
    }

    private static void ApplyScreen(HmiScreen s, HmiScreenSaveRequest q)
    {
        s.Kind = q.Kind;
        s.Name = q.Name.ToEntity();
        s.SortOrder = q.SortOrder;
        s.ContentJson = q.Content is { } c ? c.GetRawText() : "{}";
        s.UpdatedAt = DateTime.UtcNow;
    }

    private static HmiScreenDto ToDto(HmiScreen s) => new(s.Id, s.Kind, s.Name.ToDto(), s.SortOrder, ParseObject(s.ContentJson), s.UpdatedAt);
}
