using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Entities;
using Microsoft.AspNetCore.SignalR;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Plc;

/// <summary>Polls the PLC link, pushes snapshots to clients and records alarm start/clear in the alarm journal.</summary>
public class LivePoller(IPlcLink plc, LiveState state, IHubContext<LiveHub> hub, IServiceScopeFactory scopes, ILogger<LivePoller> log)
    : BackgroundService
{
    private static readonly TimeSpan Period = TimeSpan.FromMilliseconds(500);
    private HashSet<string> _active = [];

    protected override async Task ExecuteAsync(CancellationToken ct)
    {
        await RestoreActiveAsync(ct);
        using var timer = new PeriodicTimer(Period);
        while (await timer.WaitForNextTickAsync(ct))
        {
            try
            {
                var snap = plc.Step();
                state.Last = snap;
                await hub.Clients.All.SendAsync("snapshot", snap, ct);
                await TrackAlarmsAsync(snap, ct);
            }
            catch (Exception ex) when (ex is not OperationCanceledException)
            {
                log.LogError(ex, "Live poll failed");
            }
        }
    }

    private async Task RestoreActiveAsync(CancellationToken ct)
    {
        using var scope = scopes.CreateScope();
        var db = scope.ServiceProvider.GetRequiredService<AppDbContext>();
        _active = (await db.AlarmEvents.Where(a => a.ClearedAt == null).Select(a => a.Code).ToListAsync(ct)).ToHashSet();
    }

    private async Task TrackAlarmsAsync(LiveSnapshot snap, CancellationToken ct)
    {
        var now = snap.Alarms.Select(a => a.Code).ToHashSet();
        if (now.SetEquals(_active)) return;

        using var scope = scopes.CreateScope();
        var db = scope.ServiceProvider.GetRequiredService<AppDbContext>();
        foreach (var a in snap.Alarms.Where(a => !_active.Contains(a.Code)))
            db.AlarmEvents.Add(new AlarmEvent { Code = a.Code, Element = a.Element, StartedAt = snap.At });
        var cleared = _active.Except(now).ToList();
        if (cleared.Count > 0)
        {
            var open = await db.AlarmEvents.Where(a => a.ClearedAt == null && cleared.Contains(a.Code)).ToListAsync(ct);
            foreach (var e in open) e.ClearedAt = snap.At;
        }
        await db.SaveChangesAsync(ct);
        _active = now;
        await hub.Clients.All.SendAsync("alarmsChanged", ct);
    }
}
