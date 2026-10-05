using System.Buffers.Binary;
using System.Text;
using System.Text.Json;
using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Plc;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Services;

public record PlcConfigModuleDto(int Index, Guid ModuleId, string SystemName, string Id, int Type, int Channels);
public record PlcConfigIssueDto(string Code, string? Module, int? Channel, bool Blocking);
public record PlcConfigPreviewDto(string Crc, int Size, string Project, IReadOnlyList<PlcConfigModuleDto> Modules,
    int ChannelCount, IReadOnlyList<PlcConfigIssueDto> Issues);
public record PlcConfigViewDto(PlcConfigPreviewDto Built, bool Online, bool Stale, string? Error, JsonElement? Plc, bool Matches);
public record PlcConfigUploadResult(bool Ok, string? Error, int? PlcError, int? Gen);

/// <summary>
/// Builds the configuration file of a project (layout: Shared_Libs/hvac_cfg.h), uploads it to the Main PLC
/// and reads back how the PLC applied it.
/// </summary>
public class PlcConfigService(AppDbContext db, AuditService audit, PlcFirmwareClient plc)
{
    private const ushort Format = 1;
    private const uint Magic = 0x47464348; // "HCFG"
    private const int HeaderSize = 44, ModuleSize = 8, ChannelSize = 8, ProjectLen = 24;
    private const int MaxModules = 64, MaxChannelsPerModule = 8, MaxSize = 8192;

    private static readonly Dictionary<string, byte> Signals = new()
    {
        ["4-20mA"] = 1, ["0-20mA"] = 2, ["0-10V"] = 3, ["2-10V"] = 4, ["0-5V"] = 5, ["PT100"] = 6, ["PT1000"] = 7, ["NTC10K"] = 8,
    };

    private static readonly Dictionary<string, byte> Quantities = new()
    {
        ["temperature"] = 1, ["humidity"] = 2, ["pressure"] = 3, ["flow"] = 4, ["other"] = 5,
    };

    private sealed record Built(byte[] Blob, uint Crc, PlcConfigPreviewDto Preview);

    private static T? Parse<T>(string json) where T : class
    {
        if (string.IsNullOrEmpty(json)) return null;
        try { return JsonSerializer.Deserialize<T>(json, Localized.Json); }
        catch (JsonException) { return null; }
    }

    private async Task<Built?> BuildAsync(Guid projectId, CancellationToken ct)
    {
        var project = await db.Projects.AsNoTracking().FirstOrDefaultAsync(p => p.Id == projectId, ct);
        if (project is null) return null;

        var issues = new List<PlcConfigIssueDto>();
        var rows = (await db.ProjectModules.AsNoTracking().Where(m => m.ProjectId == projectId).ToListAsync(ct))
            .Select(m =>
            {
                var lib = Parse<LibraryItemDto>(m.SnapshotJson);
                var type = lib?.TypeCode ?? 0;
                var kind = ModuleRules.KindOf(type);
                return (Row: m, Lib: lib, Type: type, Kind: kind, Id: ModuleRules.IdOf(kind, m.Line, m.Rail, m.Place),
                    Name: ModuleRules.SystemName(kind, lib?.SystemPrefix ?? "?", m.Rail, m.Place));
            })
            .Where(x => x.Lib is not null && x.Type > 0 && x.Kind != ModuleKind.Plugin)
            .ToList();

        foreach (var x in rows.Where(x => !ModuleRules.IsPlaced(x.Kind, x.Row.Rail)))
            issues.Add(new("module_unplaced", x.Name, null, false));

        var modules = rows.Where(x => ModuleRules.IsPlaced(x.Kind, x.Row.Rail))
            .OrderBy(x => x.Kind == ModuleKind.Plc ? 0 : x.Kind == ModuleKind.NoId ? 2 : 1)
            .ThenBy(x => x.Id, StringComparer.Ordinal)
            .ThenBy(x => x.Name, StringComparer.Ordinal)
            .ToList();

        if (!modules.Any(x => x.Kind == ModuleKind.Plc)) issues.Add(new("no_plc", null, null, true));
        if (modules.Count > MaxModules) issues.Add(new("too_many_modules", null, null, true));

        var sensors = (await db.ProjectEquipment.AsNoTracking().Where(e => e.ProjectId == projectId).ToListAsync(ct))
            .ToDictionary(e => e.Id, e => Parse<LibraryItemDto>(e.SnapshotJson)?.Outputs ?? []);

        var moduleBytes = new List<byte[]>();
        var channelBytes = new List<byte[]>();
        var dtos = new List<PlcConfigModuleDto>();
        foreach (var (x, idx) in modules.Take(MaxModules).Select((x, i) => (x, i)))
        {
            // Only I/O modules (AI 0x20 .. RL 0x60) have signal channels; for hubs ChannelCount means ports.
            var count = x.Type is >= 0x20 and < 0x70 ? x.Lib!.ChannelCount ?? 0 : 0;
            if (count > MaxChannelsPerModule)
            {
                issues.Add(new("too_many_channels", x.Name, null, true));
                count = MaxChannelsPerModule;
            }
            moduleBytes.Add([(byte)x.Type, (byte)x.Row.Line, (byte)x.Row.Rail, (byte)x.Row.Place, (byte)count, 0, 0, 0]);
            dtos.Add(new(idx, x.Row.Id, x.Name, x.Id, x.Type, count));

            foreach (var c in (Parse<List<ModuleChannelDto>>(x.Row.ChannelsJson) ?? []).Where(c => c.Channel >= 1 && c.Channel <= count))
            {
                SensorOutputDto? output = null;
                if (c.EquipmentId is { } eq && sensors.TryGetValue(eq, out var outs))
                    output = outs.FirstOrDefault(o => o.No == c.Output);
                if (c.EquipmentId is not null && output is null) issues.Add(new("output_not_set", x.Name, c.Channel, false));

                if (string.IsNullOrEmpty(c.Mode) || !Signals.TryGetValue(c.Mode, out var signal))
                {
                    if (output is not null) issues.Add(new("channel_not_set", x.Name, c.Channel, false));
                    continue;
                }
                if (output is not null && output.Signal != c.Mode) issues.Add(new("signal_mismatch", x.Name, c.Channel, false));

                short min = 0, max = 0;
                if (output is not null)
                {
                    if (!TryX10(output.Min, out min) || !TryX10(output.Max, out max))
                        issues.Add(new("range_too_large", x.Name, c.Channel, true));
                }
                var b = new byte[ChannelSize];
                b[0] = (byte)idx;
                b[1] = (byte)c.Channel;
                b[2] = signal;
                b[3] = output is not null && Quantities.TryGetValue(output.Quantity, out var q) ? q : (byte)0;
                BinaryPrimitives.WriteInt16LittleEndian(b.AsSpan(4), min);
                BinaryPrimitives.WriteInt16LittleEndian(b.AsSpan(6), max);
                channelBytes.Add(b);
            }
        }

        var size = HeaderSize + moduleBytes.Count * ModuleSize + channelBytes.Count * ChannelSize;
        if (size > MaxSize) issues.Add(new("too_large", null, null, true));

        var blob = new byte[size];
        var h = blob.AsSpan();
        BinaryPrimitives.WriteUInt32LittleEndian(h, Magic);
        BinaryPrimitives.WriteUInt16LittleEndian(h[4..], Format);
        BinaryPrimitives.WriteUInt16LittleEndian(h[6..], (ushort)moduleBytes.Count);
        BinaryPrimitives.WriteUInt16LittleEndian(h[8..], (ushort)channelBytes.Count);
        var number = Ascii(project.Number);
        Encoding.ASCII.GetBytes(number, h.Slice(12, ProjectLen));
        var pos = HeaderSize;
        foreach (var m in moduleBytes) { m.CopyTo(blob, pos); pos += ModuleSize; }
        foreach (var c in channelBytes) { c.CopyTo(blob, pos); pos += ChannelSize; }

        var crc = FirmwareImage.Crc32(blob);
        return new Built(blob, crc, new PlcConfigPreviewDto($"{crc:x8}", size, number, dtos, channelBytes.Count, issues));
    }

    private static bool TryX10(double v, out short r)
    {
        var x = Math.Round(v * 10);
        r = x is >= short.MinValue and <= short.MaxValue ? (short)x : (short)0;
        return x is >= short.MinValue and <= short.MaxValue;
    }

    private static string Ascii(string s)
    {
        var chars = s.Trim().Select(c => c is >= ' ' and < (char)0x7F ? c : '_').Take(ProjectLen).ToArray();
        return new string(chars);
    }

    public async Task<PlcConfigViewDto?> ViewAsync(Guid projectId, CancellationToken ct)
    {
        var built = await BuildAsync(projectId, ct);
        if (built is null) return null;

        string? text, error = null;
        try { text = await plc.GetConfigStatusAsync(ct); }
        catch (PlcUnavailableException ex) { return new(built.Preview, false, false, ex.Code, null, false); }
        if (text is null) return new(built.Preview, true, true, null, null, false);

        try
        {
            using var doc = JsonDocument.Parse(text);
            var root = doc.RootElement.Clone();
            var matches = root.TryGetProperty("present", out var p) && p.GetInt32() == 1 &&
                          root.TryGetProperty("crc", out var c) && c.GetString() == built.Preview.Crc;
            return new(built.Preview, true, false, error, root, matches);
        }
        catch (JsonException)
        {
            return new(built.Preview, true, false, "plc_bad_answer", null, false);
        }
    }

    /// <summary>Null if the project does not exist.</summary>
    public async Task<PlcConfigUploadResult?> UploadAsync(Guid projectId, string? reason, CancellationToken ct)
    {
        var project = await db.Projects.AsNoTracking().FirstOrDefaultAsync(p => p.Id == projectId, ct);
        if (project is null) return null;
        if (!project.IsActiveOnSite) return new(false, "not_active", null, null);

        var built = (await BuildAsync(projectId, ct))!;
        if (built.Preview.Issues.Any(i => i.Blocking)) return new(false, "blocking_issues", null, null);

        PlcConfigUploadResult result;
        try
        {
            using var doc = JsonDocument.Parse(await plc.UploadConfigAsync(built.Blob, built.Crc, ct));
            var root = doc.RootElement;
            result = root.TryGetProperty("ok", out var ok) && ok.GetBoolean()
                ? new(true, null, null, root.TryGetProperty("gen", out var g) ? g.GetInt32() : null)
                : new(false, "plc_rejected", root.TryGetProperty("err", out var e) ? e.GetInt32() : null, null);
        }
        catch (PlcUnavailableException ex) { result = new(false, ex.Code, null, null); }
        catch (JsonException) { result = new(false, "plc_bad_answer", null, null); }

        audit.Add(result.Ok ? "plc.config.upload" : "plc.config.upload_failed", project.Number,
            newValue: $"crc {built.Preview.Crc}, {built.Preview.Modules.Count} mod, {built.Preview.ChannelCount} ch" +
                      (result.Gen is { } gen ? $", gen {gen}" : ""),
            reason: result.Ok ? reason : $"{result.Error}{(result.PlcError is { } pe ? $" ({pe})" : "")}",
            projectId: projectId);
        await db.SaveChangesAsync(ct);
        return result;
    }
}
