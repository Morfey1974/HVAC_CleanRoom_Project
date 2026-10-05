using System.Text.Json;
using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Entities;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Services;

/// <summary>Project hardware: modules on their places with IDs and system names (plan 8.3), the system scheme and its wires.</summary>
public class ModuleService(AppDbContext db, AuditService audit)
{
    private sealed record Placed(ProjectModule Row, LibraryItemDto? Lib, ModuleKind Kind)
    {
        public bool IsPlaced => ModuleRules.IsPlaced(Kind, Row.Rail);
    }

    public async Task<List<ProjectModuleDto>> ListAsync(Guid projectId, CancellationToken ct)
    {
        var rows = await db.ProjectModules.AsNoTracking().Where(m => m.ProjectId == projectId).ToListAsync(ct);
        var latest = await LatestVersionsAsync(rows, ct);
        return rows.Select(Place)
            .OrderBy(p => p.Kind == ModuleKind.Plc ? 0 : p.Kind == ModuleKind.NoId ? 2 : !p.IsPlaced ? 3 : 1)
            .ThenBy(p => p.Row.Rail).ThenBy(p => p.Row.Place)
            .Select(p => ToDto(p, p.Row.LibraryItemId is { } id && latest.TryGetValue(id, out var v) ? v : null))
            .ToList();
    }

    public async Task<SchemeDto> SchemeAsync(Guid projectId, CancellationToken ct)
    {
        var links = await db.ProjectLinks.AsNoTracking().Where(l => l.ProjectId == projectId).OrderBy(l => l.CreatedAt).ToListAsync(ct);
        return new SchemeDto(await ListAsync(projectId, ct), links.Select(ToDto).ToList());
    }

    public async Task<(ProjectModuleDto? dto, string? error)> SaveAsync(Guid projectId, Guid? id, ProjectModuleSaveRequest req, CancellationToken ct)
    {
        ProjectModule? row = null;
        if (id is not null)
        {
            row = await db.ProjectModules.FirstOrDefaultAsync(m => m.Id == id && m.ProjectId == projectId, ct);
            if (row is null) return (null, "not_found");
        }

        LibraryItemDto? lib;
        if (row is null || row.LibraryItemId != req.LibraryItemId)
        {
            var item = await db.LibraryItems.AsNoTracking().FirstOrDefaultAsync(l => l.Id == req.LibraryItemId, ct);
            if (item is null || item.Category != LibraryCategories.Module || item.TypeCode is null) return (null, "not_a_module");
            lib = LibraryService.ToDto(item, 0);
        }
        else lib = Snapshot(row);
        if (lib?.TypeCode is not { } typeCode) return (null, "not_a_module");

        var kind = ModuleRules.KindOf(typeCode);
        var (line, rail, place) = kind switch
        {
            ModuleKind.Plc => (0, 0, 0),
            ModuleKind.NoId => (0, 0, 0),
            _ when req.Rail == 0 => (0, 0, 0),
            ModuleKind.Head => (req.Line, req.Rail, 0),
            _ => (req.Line, req.Rail, req.Place),
        };

        var others = (await db.ProjectModules.AsNoTracking().Where(m => m.ProjectId == projectId && m.Id != id).ToListAsync(ct))
            .Select(Place).ToList();
        if (Check(kind, line, rail, place, others) is { } placeErr) return (null, placeErr);

        var serial = req.ExpectedSerial?.Trim() ?? "";
        if (serial.Length > 0 && !ModuleRules.SerialFormat().IsMatch(serial)) return (null, "serial_format");
        if (serial.Length > 0 && others.Any(o => o.Row.ExpectedSerial == serial)) return (null, "serial_taken");

        // No channel list in the request keeps the current settings that still fit the module.
        var channelInput = req.Channels ?? (row is null ? [] : ChannelsOf(row));
        if (CleanChannels(channelInput, lib, req.Channels is not null, out var channels) is { } chErr) return (null, chErr);
        if (req.Channels is not null && await CheckBindingsAsync(projectId, channels, ct) is { } bErr) return (null, bErr);

        var isNew = row is null;
        var before = isNew ? null : Localized.ToJson(Summary(Place(row!)));
        row ??= new ProjectModule { ProjectId = projectId };
        if (isNew || row.LibraryItemId != req.LibraryItemId)
        {
            row.LibraryItemId = lib.Id;
            row.LibraryVersion = lib.Version;
            row.SnapshotJson = JsonSerializer.Serialize(lib, Localized.Json);
        }
        row.Line = line;
        row.Rail = rail;
        row.Place = place;
        row.UserName = req.UserName.ToEntity();
        row.ExpectedSerial = serial;
        row.Revision = req.Revision?.Trim().ToUpperInvariant() ?? "";
        row.Notes = req.Notes?.Trim() ?? "";
        row.ChannelsJson = channels.Count == 0 ? "" : JsonSerializer.Serialize(channels, Localized.Json);
        row.UpdatedAt = DateTime.UtcNow;
        if (isNew)
        {
            (row.SchemeX, row.SchemeY) = AutoPosition(kind, rail, place);
            db.ProjectModules.Add(row);
        }

        var placed = Place(row);
        audit.Add(isNew ? "module.create" : "module.update", SystemName(placed), before, Localized.ToJson(Summary(placed)), req.Reason, projectId: projectId);
        await db.SaveChangesAsync(ct);
        return (ToDto(placed, lib.Version), null);
    }

    /// <summary>Module dropped from the palette onto the scheme. Heads, wagons and displays stay unplaced until wired.</summary>
    public async Task<(ProjectModuleDto? dto, string? error)> CreateOnSchemeAsync(Guid projectId, SchemeModuleCreateRequest req, CancellationToken ct)
    {
        var item = await db.LibraryItems.AsNoTracking().FirstOrDefaultAsync(l => l.Id == req.LibraryItemId, ct);
        if (item is null || item.Category != LibraryCategories.Module || item.TypeCode is null) return (null, "not_a_module");
        var kind = ModuleRules.KindOf(item.TypeCode.Value);
        if (kind == ModuleKind.Plugin) return (null, "plugin_module");
        if (kind == ModuleKind.Plc &&
            (await db.ProjectModules.AsNoTracking().Where(m => m.ProjectId == projectId).ToListAsync(ct)).Select(Place).Any(p => p.Kind == ModuleKind.Plc))
            return (null, "plc_exists");
        var lib = LibraryService.ToDto(item, 0);
        var row = new ProjectModule
        {
            ProjectId = projectId, LibraryItemId = item.Id, LibraryVersion = item.Version,
            SnapshotJson = JsonSerializer.Serialize(lib, Localized.Json),
            SchemeX = req.X, SchemeY = req.Y,
        };
        db.ProjectModules.Add(row);
        var placed = Place(row);
        audit.Add("module.create", SystemName(placed), newValue: Localized.ToJson(Summary(placed)), projectId: projectId);
        await db.SaveChangesAsync(ct);
        return (ToDto(placed, item.Version), null);
    }

    public async Task<bool> MoveAsync(Guid projectId, Guid id, SchemePositionRequest req, CancellationToken ct)
    {
        var row = await db.ProjectModules.FirstOrDefaultAsync(m => m.Id == id && m.ProjectId == projectId, ct);
        if (row is null) return false;
        row.SchemeX = Math.Round(req.X, 1);
        row.SchemeY = Math.Round(req.Y, 1);
        await db.SaveChangesAsync(ct);
        return true;
    }

    /// <summary>Takes the current library version; wires to connection points that no longer exist are removed.</summary>
    public async Task<(ProjectModuleDto? dto, string? error)> UpdateFromLibraryAsync(Guid projectId, Guid id, string? reason, CancellationToken ct)
    {
        var row = await db.ProjectModules.FirstOrDefaultAsync(m => m.Id == id && m.ProjectId == projectId, ct);
        if (row is null) return (null, "not_found");
        var item = row.LibraryItemId is { } libId ? await db.LibraryItems.AsNoTracking().FirstOrDefaultAsync(l => l.Id == libId, ct) : null;
        if (item is null) return (null, "library_not_found");
        var before = Localized.ToJson(Summary(Place(row)));
        var from = row.LibraryVersion;
        var lib = LibraryService.ToDto(item, 0);
        row.LibraryVersion = item.Version;
        row.SnapshotJson = JsonSerializer.Serialize(lib, Localized.Json);
        row.UpdatedAt = DateTime.UtcNow;

        CleanChannels(ChannelsOf(row), lib, false, out var kept);
        row.ChannelsJson = kept.Count == 0 ? "" : JsonSerializer.Serialize(kept, Localized.Json);

        var ports = (lib.Graphic?.Ports ?? []).Select(p => p.Id).ToHashSet();
        var stale = await db.ProjectLinks.Where(l => (l.FromModuleId == id && !ports.Contains(l.FromPort)) || (l.ToModuleId == id && !ports.Contains(l.ToPort))).ToListAsync(ct);
        db.ProjectLinks.RemoveRange(stale);

        var placed = Place(row);
        audit.Add("module.libraryUpdate", SystemName(placed), before,
            Localized.ToJson(new { from, to = item.Version, removedLinks = stale.Count }), reason, projectId: projectId);
        await db.SaveChangesAsync(ct);
        return (ToDto(placed, item.Version), null);
    }

    public async Task<(bool ok, string? error)> DeleteAsync(Guid projectId, Guid id, string? reason, CancellationToken ct)
    {
        var row = await db.ProjectModules.FirstOrDefaultAsync(m => m.Id == id && m.ProjectId == projectId, ct);
        if (row is null) return (false, "not_found");
        var placed = Place(row);
        if (placed.Kind == ModuleKind.Head && row.Rail > 0 &&
            await db.ProjectModules.AnyAsync(m => m.ProjectId == projectId && m.Id != id && m.Rail == row.Rail && m.Place > 0, ct))
            return (false, "rail_not_empty");
        audit.Add("module.delete", SystemName(placed), Localized.ToJson(Summary(placed)), reason: reason, projectId: projectId);
        db.ProjectModules.Remove(row);
        await db.SaveChangesAsync(ct);
        return (true, null);
    }

    // ---- wires ----

    public async Task<(SchemeDto? dto, string? error)> CreateLinkAsync(Guid projectId, ProjectLinkSaveRequest req, CancellationToken ct)
    {
        if (req.FromModuleId == req.ToModuleId) return (null, "link_same_module");
        var rows = await db.ProjectModules.Where(m => m.ProjectId == projectId).ToListAsync(ct);
        var a = rows.FirstOrDefault(m => m.Id == req.FromModuleId);
        var b = rows.FirstOrDefault(m => m.Id == req.ToModuleId);
        if (a is null || b is null) return (null, "not_found");
        var pa = Place(a);
        var pb = Place(b);
        var portA = ModuleGraphics.FindPort(pa.Lib, req.FromPort);
        var portB = ModuleGraphics.FindPort(pb.Lib, req.ToPort);
        if (portA is null || portB is null) return (null, "link_port");
        if (!ModuleGraphics.Compatible(portA, portB)) return (null, "link_incompatible");

        var links = await db.ProjectLinks.Where(l => l.ProjectId == projectId).ToListAsync(ct);
        bool Uses(Guid m, string p) => links.Any(l => (l.FromModuleId == m && l.FromPort == p) || (l.ToModuleId == m && l.ToPort == p));
        if (links.Any(l => (l.FromModuleId == a.Id && l.FromPort == req.FromPort && l.ToModuleId == b.Id && l.ToPort == req.ToPort) ||
                           (l.FromModuleId == b.Id && l.FromPort == req.ToPort && l.ToModuleId == a.Id && l.ToPort == req.FromPort)))
            return (null, "link_exists");
        // Power may be distributed from one terminal to several modules; data points take one wire.
        if (portA.Type != "pwr24" && (Uses(a.Id, req.FromPort) || Uses(b.Id, req.ToPort))) return (null, "link_port_busy");

        var link = new ProjectLink { ProjectId = projectId, FromModuleId = a.Id, FromPort = req.FromPort, ToModuleId = b.Id, ToPort = req.ToPort };
        db.ProjectLinks.Add(link);
        links.Add(link);
        audit.Add("link.create", $"{SystemName(pa)}:{portA.Label} → {SystemName(pb)}:{portB.Label}", projectId: projectId);

        foreach (var (row, before) in AutoPlace(rows, links))
        {
            var placed = Place(row);
            row.UpdatedAt = DateTime.UtcNow;
            audit.Add("module.autoPlace", SystemName(placed), before, Localized.ToJson(Summary(placed)), projectId: projectId);
        }
        await db.SaveChangesAsync(ct);
        return (await SchemeAsync(projectId, ct), null);
    }

    public async Task<bool> DeleteLinkAsync(Guid projectId, Guid id, string? reason, CancellationToken ct)
    {
        var link = await db.ProjectLinks.FirstOrDefaultAsync(l => l.Id == id && l.ProjectId == projectId, ct);
        if (link is null) return false;
        var rows = await db.ProjectModules.AsNoTracking().Where(m => m.Id == link.FromModuleId || m.Id == link.ToModuleId).ToListAsync(ct);
        string End(Guid m, string port)
        {
            var r = rows.FirstOrDefault(x => x.Id == m);
            if (r is null) return port;
            var p = Place(r);
            return $"{SystemName(p)}:{ModuleGraphics.FindPort(p.Lib, port)?.Label ?? port}";
        }
        audit.Add("link.delete", $"{End(link.FromModuleId, link.FromPort)} → {End(link.ToModuleId, link.ToPort)}", reason: reason, projectId: projectId);
        db.ProjectLinks.Remove(link);
        await db.SaveChangesAsync(ct);
        return true;
    }

    /// <summary>
    /// Numbering by wires (plan 8.3.3): only unplaced modules are numbered, placed ones are never moved.
    /// Repeats until nothing changes, so the order in which wires were drawn does not matter.
    /// </summary>
    private static List<(ProjectModule row, string before)> AutoPlace(List<ProjectModule> rows, List<ProjectLink> links)
    {
        var changed = new List<(ProjectModule, string)>();
        var cache = rows.ToDictionary(r => r.Id, Place);
        for (var pass = 0; pass < 64; pass++)
        {
            var any = false;
            foreach (var l in links)
            {
                if (!cache.TryGetValue(l.FromModuleId, out var a) || !cache.TryGetValue(l.ToModuleId, out var b)) continue;
                var pa = ModuleGraphics.FindPort(a.Lib, l.FromPort);
                var pb = ModuleGraphics.FindPort(b.Lib, l.ToPort);
                if (pa is null || pb is null) continue;
                foreach (var (src, sp, dst, dp) in new[] { (a, pa, b, pb), (b, pb, a, pa) })
                {
                    if (dst.IsPlaced || !src.IsPlaced) continue;
                    var before = Localized.ToJson(Summary(dst));
                    if (!TryPlace(src, sp, dst, dp, cache.Values)) continue;
                    cache[dst.Row.Id] = Place(dst.Row);
                    changed.Add((dst.Row, before));
                    any = true;
                }
            }
            if (!any) break;
        }
        return changed;
    }

    private static bool TryPlace(Placed src, GraphicPortDto sp, Placed dst, GraphicPortDto dp, IEnumerable<Placed> all)
    {
        var list = all.Where(p => p.IsPlaced && p.Row.Id != dst.Row.Id).ToList();
        int FreeRail() => Enumerable.Range(1, ModuleRules.MaxRail).FirstOrDefault(r => !list.Any(p => p.Kind == ModuleKind.Head && p.Row.Rail == r));
        int FreePlace(int rail, int first, int max) =>
            Enumerable.Range(first, Math.Max(0, max - first + 1)).Concat(Enumerable.Range(1, max))
                .FirstOrDefault(n => !list.Any(p => p.Row.Rail == rail && p.Row.Place == n && p.Kind != ModuleKind.Head));
        bool IsHub(Placed p) => p.Lib?.TypeCode is { } tc && ModuleRules.IsHub(tc);

        var r = dst.Row;
        switch (sp.Role, dp.Role)
        {
            case ("line", "uplink") when dst.Kind == ModuleKind.Head:
            case ("cableOut", "uplink") when dst.Kind == ModuleKind.Head && src.Kind == ModuleKind.Head:
            {
                var rail = FreeRail();
                if (rail == 0) return false;
                r.Line = sp.Role == "line" ? Math.Clamp(sp.Line ?? 1, 1, ModuleRules.MaxLine) : src.Row.Line;
                r.Rail = rail;
                r.Place = 0;
                return true;
            }
            case ("railOut", "chainIn") when dst.Kind == ModuleKind.Wagon && src.Kind == ModuleKind.Head && !IsHub(src):
            case ("chainOut", "chainIn") when dst.Kind == ModuleKind.Wagon && src.Kind == ModuleKind.Wagon:
            {
                var start = src.Kind == ModuleKind.Head ? 1 : Math.Min(src.Row.Place + 1, ModuleRules.MaxWagonPlace);
                var place = FreePlace(src.Row.Rail, start, ModuleRules.MaxWagonPlace);
                if (place == 0) return false;
                (r.Line, r.Rail, r.Place) = (src.Row.Line, src.Row.Rail, place);
                return true;
            }
            case ("hubPort", "hubIn") when dst.Kind == ModuleKind.Display && IsHub(src):
            {
                var place = FreePlace(src.Row.Rail, Math.Clamp(sp.Index ?? 1, 1, ModuleRules.HubPorts), ModuleRules.HubPorts);
                if (place == 0) return false;
                (r.Line, r.Rail, r.Place) = (src.Row.Line, src.Row.Rail, place);
                return true;
            }
            default:
                return false;
        }
    }

    // ---- rules and mapping ----

    /// <summary>Placement rules of plan 8.3.3: unique places, heads at 00, wagons behind a locomotive, displays on hub ports.</summary>
    private static string? Check(ModuleKind kind, int line, int rail, int place, List<Placed> others)
    {
        switch (kind)
        {
            case ModuleKind.Plugin:
                return "plugin_module";
            case ModuleKind.Plc:
                return others.Any(o => o.Kind == ModuleKind.Plc) ? "plc_exists" : null;
            case ModuleKind.NoId:
                return null;
        }
        if (rail == 0) return null;

        if (line is < 1 or > ModuleRules.MaxLine) return "line_range";
        if (rail is < 1 or > ModuleRules.MaxRail) return "rail_range";

        var head = others.FirstOrDefault(o => o.Kind == ModuleKind.Head && o.Row.Rail == rail);
        if (kind == ModuleKind.Head)
        {
            if (head is not null) return "rail_taken";
            // Rail numbers run through the site; members of this rail keep their line in sync with the head.
            return others.Any(o => o.Row.Rail == rail && o.Row.Line != line) ? "rail_line_mismatch" : null;
        }

        if (head is null) return "no_head";
        if (head.Row.Line != line) return "rail_line_mismatch";
        var hubRail = head.Lib?.TypeCode is { } htc && ModuleRules.IsHub(htc);
        if (kind == ModuleKind.Display)
        {
            if (!hubRail) return "display_needs_hub";
            if (place is < 1 or > ModuleRules.HubPorts) return "port_range";
        }
        else
        {
            if (hubRail) return "hub_only_displays";
            if (place is < 1 or > ModuleRules.MaxWagonPlace) return "place_range";
        }
        return others.Any(o => o.Row.Rail == rail && o.Row.Place == place && o.Kind != ModuleKind.NoId && o.Kind != ModuleKind.Plc)
            ? "place_taken" : null;
    }

    /// <summary>Initial scheme position for modules added from the table: rails as rows, wagons to the left of the locomotive.</summary>
    private static (double x, double y) AutoPosition(ModuleKind kind, int rail, int place) => kind switch
    {
        ModuleKind.Plc => (900, 40),
        ModuleKind.NoId => (300, 40),
        _ when rail == 0 => (80, 40),
        ModuleKind.Head => (1300, 320 + (rail - 1) * 300),
        ModuleKind.Display => (1300 - place * 200, 320 + (rail - 1) * 300 + 220),
        _ => (1300 - place * 170, 320 + (rail - 1) * 300),
    };

    private async Task<Dictionary<Guid, int>> LatestVersionsAsync(List<ProjectModule> rows, CancellationToken ct)
    {
        var libIds = rows.Where(r => r.LibraryItemId != null).Select(r => r.LibraryItemId!.Value).Distinct().ToList();
        return await db.LibraryItems.AsNoTracking().Where(l => libIds.Contains(l.Id)).ToDictionaryAsync(l => l.Id, l => l.Version, ct);
    }

    private static LibraryItemDto? Snapshot(ProjectModule m)
    {
        LibraryItemDto? lib;
        try { lib = JsonSerializer.Deserialize<LibraryItemDto>(m.SnapshotJson, Localized.Json); }
        catch (JsonException) { return null; }
        // Copies frozen before drawings existed get the default drawing of their article.
        if (lib is { Graphic: null, Category: LibraryCategories.Module })
            lib = lib with { Graphic = ModuleGraphics.Default(lib.Code, lib.TypeCode is { } tc ? ModuleRules.KindOf(tc) : null, lib.ChannelCount) };
        return lib;
    }

    private static Placed Place(ProjectModule m)
    {
        var lib = Snapshot(m);
        return new Placed(m, lib, lib?.TypeCode is { } tc ? ModuleRules.KindOf(tc) : ModuleKind.Wagon);
    }

    private static List<ModuleChannelDto> ChannelsOf(ProjectModule m)
    {
        if (string.IsNullOrEmpty(m.ChannelsJson)) return [];
        try { return JsonSerializer.Deserialize<List<ModuleChannelDto>>(m.ChannelsJson, Localized.Json) ?? []; }
        catch (JsonException) { return []; }
    }

    /// <summary>
    /// Keeps channels 1..ChannelCount with a mode the module supports. Strict (settings sent by the user): anything else is an error;
    /// otherwise (library changed) settings that no longer fit are dropped and the channel shows as "not set".
    /// </summary>
    private static string? CleanChannels(IEnumerable<ModuleChannelDto> input, LibraryItemDto lib, bool strict, out List<ModuleChannelDto> clean)
    {
        var modes = lib.ChannelModes ?? [];
        var count = lib.ChannelCount ?? 0;
        var map = new SortedDictionary<int, ModuleChannelDto>();
        foreach (var c in input)
        {
            var mode = c.Mode?.Trim() ?? "";
            // A sensor without an output yet is kept: the output is picked next.
            var bound = c.EquipmentId is not null;
            if (mode.Length == 0 && !bound) continue;
            var inRange = c.Channel >= 1 && c.Channel <= count;
            var modeOk = mode.Length == 0 || modes.Contains(mode);
            if ((!inRange || !modeOk) && strict)
            {
                clean = [];
                return !inRange ? "channel_range" : "channel_mode";
            }
            if (!inRange) continue;
            map[c.Channel] = new ModuleChannelDto(c.Channel, modeOk ? mode : "", bound ? c.EquipmentId : null, bound ? c.Output : null);
        }
        clean = map.Values.ToList();
        return null;
    }

    /// <summary>A bound channel must point to a sensor of this project and one of its outputs.</summary>
    private async Task<string?> CheckBindingsAsync(Guid projectId, IReadOnlyList<ModuleChannelDto> channels, CancellationToken ct)
    {
        var ids = channels.Where(c => c.EquipmentId is not null).Select(c => c.EquipmentId!.Value).Distinct().ToList();
        if (ids.Count == 0) return null;
        var rows = await db.ProjectEquipment.AsNoTracking().Where(e => e.ProjectId == projectId && ids.Contains(e.Id))
            .Select(e => new { e.Id, e.SnapshotJson }).ToListAsync(ct);
        foreach (var c in channels.Where(c => c.EquipmentId is not null))
        {
            var row = rows.FirstOrDefault(r => r.Id == c.EquipmentId);
            if (row is null) return "binding_equipment";
            LibraryItemDto? snap = null;
            try { snap = JsonSerializer.Deserialize<LibraryItemDto>(row.SnapshotJson, Localized.Json); }
            catch (JsonException) { }
            if (snap?.Outputs is not { Count: > 0 } outs) return "binding_output";
            if (c.Output is not null && !outs.Any(o => o.No == c.Output)) return "binding_output";
        }
        return null;
    }

    private static string SystemName(Placed p) => ModuleRules.SystemName(p.Kind, p.Lib?.SystemPrefix ?? "?", p.Row.Rail, p.Row.Place);

    private static object Summary(Placed p) => new
    {
        id = ModuleRules.IdOf(p.Kind, p.Row.Line, p.Row.Rail, p.Row.Place),
        systemName = SystemName(p),
        article = p.Lib?.Code,
        serial = p.Row.ExpectedSerial,
        p.Row.Revision,
        userName = p.Row.UserName.ToDto(),
        channels = ChannelsOf(p.Row).Select(c => c.EquipmentId is null ? $"{c.Channel}:{c.Mode}" : $"{c.Channel}:{c.Mode}:{c.EquipmentId}/{c.Output}"),
    };

    private static ProjectModuleDto ToDto(Placed p, int? latest) => new(
        p.Row.Id, p.Row.LibraryItemId, p.Row.LibraryVersion, latest, p.Lib, p.Kind.ToString().ToLowerInvariant(),
        p.Row.Line, p.Row.Rail, p.Row.Place, ModuleRules.IdOf(p.Kind, p.Row.Line, p.Row.Rail, p.Row.Place), SystemName(p),
        p.Row.UserName.ToDto(), p.Row.ExpectedSerial, p.Row.Revision, p.Row.Notes, p.IsPlaced, p.Row.SchemeX, p.Row.SchemeY, ChannelsOf(p.Row));

    private sealed record Route(string Style, List<SchemePointDto> Points);

    private static readonly string[] LinkStyles = ["step", "straight", "curve"];
    private const int MaxBends = 40;

    private static ProjectLinkDto ToDto(ProjectLink l)
    {
        Route? r = null;
        if (!string.IsNullOrEmpty(l.RouteJson))
            try { r = JsonSerializer.Deserialize<Route>(l.RouteJson, Localized.Json); }
            catch (JsonException) { }
        return new(l.Id, l.FromModuleId, l.FromPort, l.ToModuleId, l.ToPort, r?.Style ?? "step", r?.Points ?? []);
    }

    /// <summary>Wire drawing only (style and bends): layout, like block positions, so it is not written to the journal.</summary>
    public async Task<(ProjectLinkDto? dto, string? error)> SaveRouteAsync(Guid projectId, Guid id, LinkRouteRequest req, CancellationToken ct)
    {
        var link = await db.ProjectLinks.FirstOrDefaultAsync(l => l.Id == id && l.ProjectId == projectId, ct);
        if (link is null) return (null, "not_found");
        var style = req.Style ?? "step";
        if (!LinkStyles.Contains(style)) return (null, "link_style");
        var points = (req.Points ?? []).Select(p => new SchemePointDto(Math.Round(p.X, 1), Math.Round(p.Y, 1))).ToList();
        if (points.Count > MaxBends) return (null, "link_bends");
        link.RouteJson = style == "step" && points.Count == 0 ? "" : JsonSerializer.Serialize(new Route(style, points), Localized.Json);
        await db.SaveChangesAsync(ct);
        return (ToDto(link), null);
    }
}
