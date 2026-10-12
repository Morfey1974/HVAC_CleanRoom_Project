using System.Globalization;
using System.Text.Json;
using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Entities;
using HvacConfigurator.Api.Services;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Plc;

/// <summary>Module channel value key in snapshots: "&lt;module ID&gt;.CH&lt;n&gt;", e.g. "1.01.01.CH2".</summary>
public static class ChannelKey
{
    public static string Of(string moduleId, int channel) => $"{moduleId}.CH{channel}";
}

/// <summary>What the live link needs from the project that is active on site.</summary>
public record ActiveProjectWiring(
    string? PlcName, string? LocId, string? AiId, string? HubId, IReadOnlyList<string> DisplayIds,
    IReadOnlyList<(string Key, double Min, double Max)> Bound,
    IReadOnlyList<(string Id, int Rail, int Place)> AiModules)
{
    public static readonly ActiveProjectWiring Empty = new(null, null, null, null, [], [], []);
}

/// <summary>Last frame of one AI module as the PLC keeps it (rail, place of the sender).</summary>
public record PlcAiSource(int Rail, int Place, double V1, double V2, int Status, long AgeMs);

/// <summary>Last answer of GET /api/ai on the PLC.</summary>
public record PlcAiState(
    DateTime At, double T, double H, int Status, int Counter, bool LocoLink, bool CanInit, long AgeMs,
    long TxHub, long TxHubErr, bool BusOff1, bool BusOff2, bool RtcOk, bool RtcValid, DateTime? RtcTime,
    IReadOnlyList<PlcAiSource> Sources);

/// <summary>Shared between the reader (background) and the router (snapshot builder).</summary>
public class PlcLiveState
{
    public volatile string Mode = "simulator";
    public volatile ActiveProjectWiring Wiring = ActiveProjectWiring.Empty;
    public volatile PlcAiState? Ai;
    public volatile string? Error;
}

/// <summary>
/// Live data source chosen by System → General → data source: the simulator, or the Main PLC over Ethernet.
/// The PLC gateway now forwards one AI measurement (frame 0x301): humidity on channel 1, temperature on channel 2.
/// It is attributed to the first AI module on the scheme of the project active on site.
/// </summary>
public class PlcLinkRouter(PlcSimulator sim, PlcLiveState live) : IPlcLink
{
    private const int AiStHumFault = 0x01, AiStTempFault = 0x02, AiStAdcFault = 0x04, AiStNoLink = 0x80;
    private static readonly TimeSpan PlcStale = TimeSpan.FromSeconds(3);
    private const long FrameStaleMs = 3000;

    public string Mode => live.Mode;
    public IReadOnlyDictionary<string, double> Inputs => sim.Inputs;
    public bool SetInput(string key, double value) => Mode == "simulator" && sim.SetInput(key, value);

    public LiveSnapshot Step() => Mode == "plc" ? FromPlc() : FromSimulator();

    /// <summary>Simulator plus slowly drifting values for every bound channel, so room cards can be tried without hardware.</summary>
    private LiveSnapshot FromSimulator()
    {
        var s = sim.Step();
        var w = live.Wiring;
        if (w.Bound.Count == 0) return s;
        var v = new Dictionary<string, double>(s.Values);
        var t = DateTime.UtcNow.TimeOfDay.TotalSeconds;
        var i = 0;
        foreach (var (key, min, max) in w.Bound)
            v[key] = Math.Round(min + (max - min) * (0.45 + 0.04 * Math.Sin(t / 20 + i++)), 1);
        return s with { Values = v };
    }

    private LiveSnapshot FromPlc()
    {
        var now = DateTime.UtcNow;
        var w = live.Wiring;
        var ai = live.Ai;
        var v = new Dictionary<string, double>();
        var alarms = new List<ActiveAlarm>();
        var online = ai is not null && now - ai.At < PlcStale;

        v["PLC.ONLINE"] = online ? 1 : 0;
        if (!online)
        {
            alarms.Add(new("plcOffline", w.PlcName ?? "PLC"));
            return new LiveSnapshot(now, Mode, false, v, alarms);
        }

        v["LOC.LINK"] = ai!.LocoLink ? 1 : 0;
        v["AI.AGE"] = ai.AgeMs;
        v["AI.CNT"] = ai.Counter;
        v["HUB.TX"] = ai.TxHub;
        v["HUB.TXERR"] = ai.TxHubErr;
        v["CAN1.BUSOFF"] = ai.BusOff1 ? 1 : 0;
        v["CAN2.BUSOFF"] = ai.BusOff2 ? 1 : 0;

        if (!ai.LocoLink) alarms.Add(new("locoNoLink", w.LocId ?? "LOC"));
        if (ai.BusOff2) alarms.Add(new("displayBusOff", w.HubId ?? "HUB"));

        var bound = w.Bound.Select(b => b.Key).ToHashSet();
        var allAnswer = w.AiModules.Count > 0;
        var allAdcOk = true;
        foreach (var (id, rail, place) in w.AiModules)
        {
            var src = ai.Sources.FirstOrDefault(s => s.Rail == rail && s.Place == place);
            var answers = ai.LocoLink && src is not null && src.AgeMs < FrameStaleMs && (src.Status & AiStNoLink) == 0;
            var adcOk = answers && (src!.Status & AiStAdcFault) == 0;
            allAnswer &= answers;
            allAdcOk &= !answers || adcOk;
            if (ai.LocoLink && !answers) alarms.Add(new("aiNoAnswer", id));
            else if (answers && !adcOk) alarms.Add(new("aiAdcFault", id));
            if (!adcOk) continue;

            foreach (var (ch, value, fault) in new[] { (1, src!.V1, AiStHumFault), (2, src.V2, AiStTempFault) })
            {
                var key = ChannelKey.Of(id, ch);
                if ((src.Status & fault) == 0) v[key] = value;
                else if (bound.Contains(key)) alarms.Add(new("channelBroken", key));
            }
        }
        v["AI.LINK"] = allAnswer ? 1 : 0;
        v["AI.ADC"] = allAnswer && allAdcOk ? 1 : 0;
        DateTime? plcTime = ai.RtcValid ? ai.RtcTime + (now - ai.At) : null;
        return new LiveSnapshot(now, Mode, true, v, alarms, plcTime, ai.RtcOk && ai.RtcValid);
    }
}

/// <summary>Reads the data source setting and the active project wiring every few seconds, and polls the PLC once a second.</summary>
public class PlcLiveReader(PlcFirmwareClient plc, PlcLiveState live, IServiceScopeFactory scopes, ILogger<PlcLiveReader> log)
    : BackgroundService
{
    private static readonly TimeSpan Period = TimeSpan.FromSeconds(1);
    private const int ConfigEvery = 5;
    private const double ClockMaxDriftSec = 3;
    private static readonly TimeSpan ClockRetry = TimeSpan.FromSeconds(30);
    private static readonly TimeSpan ClockDailySet = TimeSpan.FromDays(1);
    private DateTime _clockTryAt = DateTime.MinValue;
    private DateTime _clockSetAt = DateTime.MinValue;

    protected override async Task ExecuteAsync(CancellationToken ct)
    {
        using var timer = new PeriodicTimer(Period);
        var n = 0;
        do
        {
            try
            {
                if (n++ % ConfigEvery == 0) await LoadConfigAsync(ct);
                if (live.Mode == "plc") await PollAsync(ct);
            }
            catch (Exception ex) when (ex is not OperationCanceledException)
            {
                log.LogWarning(ex, "PLC live read failed");
            }
        } while (await timer.WaitForNextTickAsync(ct));
    }

    private async Task PollAsync(CancellationToken ct)
    {
        try
        {
            var text = await plc.GetAiAsync(ct);
            if (text is null) return;
            using var doc = JsonDocument.Parse(text);
            var r = doc.RootElement;
            static double D(JsonElement e, string k) => e.TryGetProperty(k, out var x) && x.ValueKind == JsonValueKind.Number ? x.GetDouble() : 0;
            var rtcOk = false;
            var rtcValid = false;
            DateTime? rtcTime = null;
            if (r.TryGetProperty("rtc", out var rtc) && rtc.ValueKind == JsonValueKind.Object)
            {
                rtcOk = D(rtc, "ok") > 0;
                rtcValid = rtcOk && D(rtc, "valid") > 0;
                if (rtc.TryGetProperty("t", out var ts) && DateTime.TryParse(ts.GetString(), CultureInfo.InvariantCulture,
                        DateTimeStyles.AdjustToUniversal | DateTimeStyles.AssumeUniversal, out var parsed))
                    rtcTime = parsed;
                else rtcValid = false;
            }
            var sources = new List<PlcAiSource>();
            if (r.TryGetProperty("src", out var src) && src.ValueKind == JsonValueKind.Array)
                foreach (var s in src.EnumerateArray())
                    sources.Add(new PlcAiSource((int)D(s, "r"), (int)D(s, "p"), D(s, "v1"), D(s, "v2"), (int)D(s, "st"), (long)D(s, "age")));
            var now = DateTime.UtcNow;
            live.Ai = new PlcAiState(now, D(r, "t"), D(r, "h"), (int)D(r, "st"), (int)D(r, "cnt"),
                D(r, "link") > 0, D(r, "init") > 0, (long)D(r, "age"), (long)D(r, "tx_hub"), (long)D(r, "tx_err"),
                D(r, "boff1") > 0, D(r, "boff2") > 0, rtcOk, rtcValid, rtcTime, sources);
            live.Error = null;
            if (rtcOk) await SyncClockAsync(rtcValid ? rtcTime : null, now, ct);
        }
        catch (PlcUnavailableException ex)
        {
            live.Error = ex.Code;
        }
        catch (JsonException)
        {
            live.Error = "plc_bad_answer";
        }
    }

    /// <summary>
    /// Keeps the PLC clock on server time: sets it when it is invalid or drifts by more than a few seconds,
    /// and once a day anyway. Attempts are spaced out so a failing clock is not hammered every second.
    /// </summary>
    private async Task SyncClockAsync(DateTime? plcTime, DateTime now, CancellationToken ct)
    {
        var drift = plcTime is { } t ? Math.Abs((t - now).TotalSeconds) : double.MaxValue;
        var due = drift > ClockMaxDriftSec || now - _clockSetAt > ClockDailySet;
        if (!due || now - _clockTryAt < ClockRetry) return;
        _clockTryAt = now;
        try
        {
            using var doc = JsonDocument.Parse(await plc.RtcSetAsync(DateTime.UtcNow, ct));
            if (doc.RootElement.TryGetProperty("ok", out var ok) && ok.ValueKind == JsonValueKind.True)
            {
                _clockSetAt = now;
                log.LogInformation("PLC clock set (drift was {Drift})", plcTime is null ? "invalid" : $"{drift:F0} s");
            }
        }
        catch (Exception ex) when (ex is PlcUnavailableException or JsonException or HttpRequestException)
        {
            log.LogWarning("PLC clock set failed: {Error}", ex.Message);
        }
    }

    private async Task LoadConfigAsync(CancellationToken ct)
    {
        using var scope = scopes.CreateScope();
        var db = scope.ServiceProvider.GetRequiredService<AppDbContext>();
        var settings = await db.SystemSettings.AsNoTracking().FirstOrDefaultAsync(ct);
        var mode = settings?.PlcMode == "plc" ? "plc" : "simulator";
        if (mode != live.Mode) live.Ai = null;
        live.Mode = mode;

        var project = await db.Projects.AsNoTracking().FirstOrDefaultAsync(p => p.IsActiveOnSite && !p.IsArchived, ct);
        live.Wiring = project is null ? ActiveProjectWiring.Empty : await WiringAsync(db, project.Id, ct);
    }

    private static T? Parse<T>(string json) where T : class
    {
        if (string.IsNullOrEmpty(json)) return null;
        try { return JsonSerializer.Deserialize<T>(json, Localized.Json); }
        catch (JsonException) { return null; }
    }

    public static async Task<ActiveProjectWiring> WiringAsync(AppDbContext db, Guid projectId, CancellationToken ct)
    {
        var modules = (await db.ProjectModules.AsNoTracking().Where(m => m.ProjectId == projectId).ToListAsync(ct))
            .Select(m =>
            {
                var lib = Parse<LibraryItemDto>(m.SnapshotJson);
                var type = lib?.TypeCode ?? 0;
                var kind = ModuleRules.KindOf(type);
                return (Row: m, Lib: lib, Type: type, Id: ModuleRules.IdOf(kind, m.Line, m.Rail, m.Place));
            })
            .Where(x => x.Lib is not null)
            .OrderBy(x => x.Id, StringComparer.Ordinal)
            .ToList();

        string? First(int type) => modules.FirstOrDefault(m => m.Type == type && m.Id.Length > 0).Id;
        var plcName = modules.FirstOrDefault(m => m.Type == 0x01).Lib?.SystemPrefix;

        var sensors = (await db.ProjectEquipment.AsNoTracking().Where(e => e.ProjectId == projectId).ToListAsync(ct))
            .ToDictionary(e => e.Id, e => Parse<LibraryItemDto>(e.SnapshotJson)?.Outputs ?? []);
        var bound = new List<(string, double, double)>();
        foreach (var m in modules.Where(m => m.Id.Length > 0))
        {
            foreach (var c in Parse<List<ModuleChannelDto>>(m.Row.ChannelsJson) ?? [])
            {
                if (c.EquipmentId is not { } eq || !sensors.TryGetValue(eq, out var outs)) continue;
                if (outs.FirstOrDefault(o => o.No == c.Output) is { } o) bound.Add((ChannelKey.Of(m.Id, c.Channel), o.Min, o.Max));
            }
        }

        return new ActiveProjectWiring(plcName, First(0x10), First(0x20), First(0x70),
            modules.Where(m => m.Type == 0x71 && m.Id.Length > 0).Select(m => m.Id).ToList(), bound,
            modules.Where(m => m.Type == 0x20 && m.Id.Length > 0).Select(m => (m.Id, m.Row.Rail, m.Row.Place)).ToList());
    }
}
