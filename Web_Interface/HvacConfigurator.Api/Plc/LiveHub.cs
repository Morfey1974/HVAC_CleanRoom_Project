using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Services;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.SignalR;

namespace HvacConfigurator.Api.Plc;

[Authorize]
public class LiveHub(IPlcLink plc, LiveState state, AuditService audit, AppDbContext db) : Hub
{
    public override async Task OnConnectedAsync()
    {
        if (state.Last is { } snap)
            await Clients.Caller.SendAsync("snapshot", snap);
        await Clients.Caller.SendAsync("simInputs", plc.Inputs);
        await base.OnConnectedAsync();
    }

    [Authorize(Roles = "Engineer,Admin")]
    public async Task<bool> SetSimInput(string key, double value)
    {
        var before = plc.Inputs.TryGetValue(key, out var b) ? b : (double?)null;
        if (!plc.SetInput(key, value)) return false;
        audit.Add("sim.setInput", key, before?.ToString(System.Globalization.CultureInfo.InvariantCulture),
            value.ToString(System.Globalization.CultureInfo.InvariantCulture));
        await db.SaveChangesAsync();
        await Clients.All.SendAsync("simInputs", plc.Inputs);
        return true;
    }
}

/// <summary>Latest snapshot shared between the poller and newly connected clients.</summary>
public class LiveState
{
    public volatile LiveSnapshot? Last;
}
