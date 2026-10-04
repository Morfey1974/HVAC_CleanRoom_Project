namespace HvacConfigurator.Api.Plc;

/// <summary>
/// Simulated PLC for the demo object (NanoMotion 26000017-HVAC-W: package AHU 600 cfm + ISO 7 room).
/// Tag names follow "&lt;unit&gt;.&lt;element&gt;.&lt;signal&gt;".
/// </summary>
public class PlcSimulator : IPlcLink
{
    private readonly Lock _lock = new();
    private readonly Dictionary<string, double> _in = new()
    {
        ["run"] = 1, ["fanSp"] = 80, ["stages"] = 1, ["vrfMode"] = 0, ["vrfCap"] = 50,
        ["outT"] = 12, ["dirt"] = 35, ["sp"] = 21,
        ["fVsd"] = 0, ["fBelt"] = 0, ["fTt"] = 0, ["fVrf"] = 0,
    };

    private double _roomT = 20.5, _supplyT = 18;
    private bool _tsTrip;

    public string Mode => "simulator";

    public IReadOnlyDictionary<string, double> Inputs
    {
        get { lock (_lock) return new Dictionary<string, double>(_in); }
    }

    public bool SetInput(string key, double value)
    {
        lock (_lock)
        {
            if (!_in.ContainsKey(key) || double.IsNaN(value)) return false;
            _in[key] = value;
            return true;
        }
    }

    public LiveSnapshot Step()
    {
        lock (_lock)
        {
            bool run = _in["run"] > 0, fVsd = _in["fVsd"] > 0, fBelt = _in["fBelt"] > 0, fTt = _in["fTt"] > 0, fVrf = _in["fVrf"] > 0;
            double fan = _in["fanSp"], stages = _in["stages"], vrfMode = _in["vrfMode"], vrfCap = _in["vrfCap"];
            double outT = _in["outT"], dirt = _in["dirt"], sp = _in["sp"];

            var fanOn = run && !fVsd && fan > 0;
            var flowPct = fanOn && !fBelt ? fan * (1 - dirt / 250) : 0;
            var flowOk = flowPct > 15;
            if (!run) _tsTrip = false;
            if (run && stages > 0 && fanOn && fBelt) _tsTrip = true;
            var stagesOn = run && flowOk && !_tsTrip ? stages : 0;
            var vrfOn = run && vrfMode > 0 && !fVrf && flowOk;
            var vrfDelta = vrfOn ? (vrfMode == 1 ? -12 : 12) * vrfCap / 100 : 0;
            var k = flowOk ? 600 / Math.Max(150, 600 * flowPct / 100) : 0;
            var targetSupply = flowOk ? outT + vrfDelta + stagesOn * 3.2 * Math.Min(k, 3) : _roomT;
            _supplyT += (targetSupply - _supplyT) * 0.15;
            var roomTarget = flowOk ? 0.55 * _supplyT + 0.45 * 22 : 22 + (outT - 22) * 0.1;
            _roomT += (roomTarget - _roomT) * 0.03;
            var f2 = Math.Pow(flowPct / 100, 2);

            var v = new Dictionary<string, double>
            {
                ["OUT.T"] = outT,
                ["AHU1.RUN"] = run ? 1 : 0,
                ["AHU1.FAN.RUN"] = fanOn ? 1 : 0,
                ["AHU1.FAN.SPD"] = fanOn ? fan : 0,
                ["AHU1.VSD.HZ"] = fanOn ? fan * 0.5 : 0,
                ["AHU1.VSD.FAULT"] = fVsd && run ? 1 : 0,
                ["AHU1.FS.FLOW"] = flowOk ? 1 : 0,
                ["AHU1.FLOW.PCT"] = Math.Round(flowPct, 1),
                ["AHU1.HTR.STAGES"] = stagesOn,
                ["AHU1.TS.TRIP"] = _tsTrip ? 1 : 0,
                ["AHU1.VRF.MODE"] = vrfMode,
                ["AHU1.VRF.ON"] = vrfOn ? 1 : 0,
                ["AHU1.VRF.CAP"] = vrfCap,
                ["AHU1.VRF.FAULT"] = fVrf && run && vrfMode > 0 ? 1 : 0,
                ["AHU1.FLT.DIRT"] = dirt,
                ["AHU1.FLT1.DP"] = Math.Round((40 + dirt * 2.2) * f2),
                ["AHU1.FLT2.DP"] = Math.Round((120 + dirt * 3.5) * f2),
                ["AHU1.SUP.T"] = Math.Round(_supplyT, 1),
                ["AHU1.SUP.P"] = Math.Round(250 * f2),
                ["AHU1.SUP.CFM"] = Math.Round(600 * flowPct / 100),
                ["ROOM1.T"] = Math.Round(_roomT, 1),
                ["ROOM1.SP"] = sp,
                ["ROOM1.FFU.RUN"] = run ? 1 : 0,
            };

            // A broken sensor has no value: clients show it as "no data".
            if (fTt) v.Remove("ROOM1.T");

            var alarms = new List<ActiveAlarm>();
            if (fVsd && run) alarms.Add(new("vsdFault", "vsd"));
            if (fanOn && !flowOk) alarms.Add(new("noFlow", "fs"));
            if (_tsTrip) alarms.Add(new("heaterOverheat", "ts"));
            if (dirt >= 80) alarms.Add(new("filtersDirty", "filters"));
            if (fTt) alarms.Add(new("roomTtBroken", "tt2"));
            if (fVrf && run && vrfMode > 0) alarms.Add(new("vrfFault", "vrf"));
            if (!fTt && run && Math.Abs(_roomT - sp) > 2) alarms.Add(new("roomTempOutOfBand", "room"));

            return new LiveSnapshot(DateTime.UtcNow, Mode, true, v, alarms);
        }
    }
}
