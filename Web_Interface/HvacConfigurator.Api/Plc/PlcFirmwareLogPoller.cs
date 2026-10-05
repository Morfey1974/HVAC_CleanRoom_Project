using System.Text.Json;
using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Services;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Plc;

/// <summary>Copies the PLC firmware-update log (ring of 32 entries) into the audit journal.</summary>
public class PlcFirmwareLogPoller(PlcFirmwareClient plc, IServiceScopeFactory scopes, ILogger<PlcFirmwareLogPoller> log)
    : BackgroundService
{
    private static readonly TimeSpan Period = TimeSpan.FromSeconds(5);
    private static readonly TimeSpan OfflinePeriod = TimeSpan.FromSeconds(30);

    private static readonly Dictionary<int, string> Events = new()
    {
        [1] = "node_new", [2] = "node_lost", [3] = "node_state", [4] = "update_start", [5] = "update_ok",
        [6] = "update_fail", [7] = "started", [8] = "run_start", [9] = "run_end", [10] = "promote",
        [11] = "upload_ok", [12] = "upload_fail", [13] = "run_cancel",
    };

    private long _boot;
    private long _lastId;

    protected override async Task ExecuteAsync(CancellationToken ct)
    {
        while (!ct.IsCancellationRequested)
        {
            var delay = Period;
            try
            {
                if (await plc.GetAddressAsync(ct) is null) delay = OfflinePeriod;
                else if (await plc.GetStatusAsync(_lastId, ct) is { } text) await IngestAsync(text, ct);
            }
            catch (PlcUnavailableException)
            {
                delay = OfflinePeriod;
            }
            catch (Exception ex) when (ex is not OperationCanceledException)
            {
                log.LogWarning(ex, "PLC firmware log poll failed");
            }
            await Task.Delay(delay, ct);
        }
    }

    private static string Marker(long boot) => $"#{boot:x8}-";

    private async Task IngestAsync(string text, CancellationToken ct)
    {
        using var doc = JsonDocument.Parse(text);
        var root = doc.RootElement;
        var boot = root.GetProperty("boot").GetInt64();
        if (boot == 0) return;

        using var scope = scopes.CreateScope();
        var db = scope.ServiceProvider.GetRequiredService<AppDbContext>();

        if (boot != _boot)
        {
            // New PLC boot (or our restart): continue after what the journal already has for this boot.
            var marker = Marker(boot);
            var targets = await db.AuditEntries.AsNoTracking()
                .Where(a => a.Action.StartsWith("plc.fw.") && a.Target.Contains(marker))
                .Select(a => a.Target).ToListAsync(ct);
            _boot = boot;
            _lastId = targets.Select(t => long.TryParse(t[(t.IndexOf(marker, StringComparison.Ordinal) + marker.Length)..].TrimEnd(')'), out var n) ? n : 0)
                .DefaultIfEmpty(0).Max();
            return;
        }

        var audit = scope.ServiceProvider.GetRequiredService<AuditService>();
        var added = false;
        foreach (var e in root.GetProperty("log").EnumerateArray())
        {
            var id = e.GetProperty("id").GetInt64();
            if (id <= _lastId) continue;
            _lastId = id;

            var ev = e.GetProperty("ev").GetInt32();
            var type = e.GetProperty("type").GetInt32();
            var tag = e.GetProperty("tag").GetString() ?? "";
            var err = e.GetProperty("err").GetInt32();
            var step = e.GetProperty("step").GetInt32();
            var from = e.GetProperty("from").GetInt32();
            var to = e.GetProperty("to").GetInt32();

            var target = (tag == "000000" ? $"type {type}" : $"type {type} node {tag}") + $" ({Marker(boot)}{id})";
            audit.Add("plc.fw." + (Events.TryGetValue(ev, out var name) ? name : ev.ToString()), target,
                oldValue: from != 0 ? FirmwareImage.VersionText(from) : null,
                newValue: to != 0 ? FirmwareImage.VersionText(to) : null,
                reason: err != 0 ? $"err 0x{err:x2}, step {step}" : null,
                loginOverride: "PLC");
            added = true;
        }
        if (added) await db.SaveChangesAsync(ct);
    }
}
