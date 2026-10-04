namespace HvacConfigurator.Api.Plc;

public record ActiveAlarm(string Code, string Element);

/// <summary>One frame of live data pushed to Monitor clients.</summary>
public record LiveSnapshot(
    DateTime At,
    string Mode,
    bool Connected,
    IReadOnlyDictionary<string, double> Values,
    IReadOnlyList<ActiveAlarm> Alarms);

/// <summary>Source of live data: PLC simulator now, real PLC over Ethernet later.</summary>
public interface IPlcLink
{
    string Mode { get; }
    LiveSnapshot Step();
    bool SetInput(string key, double value);
    IReadOnlyDictionary<string, double> Inputs { get; }
}
