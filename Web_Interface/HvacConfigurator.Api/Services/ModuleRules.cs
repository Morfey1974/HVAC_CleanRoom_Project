using System.Text.RegularExpressions;

namespace HvacConfigurator.Api.Services;

/// <summary>How a module takes part in identification (plan 8.3.3–8.3.4), derived from its type code.</summary>
public enum ModuleKind
{
    Plc,      // 0x01 — always 0.00.00, one per project
    NoId,     // Doors PLC, HMI — outside the ID chain
    Head,     // locomotive / display hub — place 00 of its rail
    Display,  // 4.3" display — port 1..9 of a hub rail
    Wagon,    // I/O module — place 1..31 of a locomotive rail
    Plugin,   // plug-in board — part of another module, not placed
}

/// <summary>Numbering rules from the plan, section 8.3. Single source for validation and naming.</summary>
public static partial class ModuleRules
{
    public const int MaxLine = 3, MaxRail = 30, MaxWagonPlace = 31, HubPorts = 9;

    /// <summary>Channel signal types known to the module firmware.</summary>
    public static readonly string[] SignalModes = ["0-10V", "2-10V", "0-5V", "0-20mA", "4-20mA", "PT100", "PT1000", "NTC10K"];

    [GeneratedRegex(@"^HC-[A-Z]+[0-9]*(-[A-Z0-9]+)?$")]
    public static partial Regex ArticleFormat();

    [GeneratedRegex(@"^[0-9]{2}(0[1-9]|[1-4][0-9]|5[0-3])-[0-9]{4}$")]
    public static partial Regex SerialFormat();

    [GeneratedRegex(@"^[A-Z][A-Z0-9]{1,7}$")]
    public static partial Regex PrefixFormat();

    public static ModuleKind KindOf(int typeCode) => typeCode switch
    {
        0x01 => ModuleKind.Plc,
        0x02 => ModuleKind.NoId,
        >= 0x80 and <= 0x8F => ModuleKind.NoId,
        0x10 or 0x70 => ModuleKind.Head,
        0x71 => ModuleKind.Display,
        >= 0x90 and <= 0x9F => ModuleKind.Plugin,
        _ => ModuleKind.Wagon,
    };

    public static bool IsHub(int typeCode) => typeCode == 0x70;

    /// <summary>Heads, wagons and displays dropped on the scheme get their place from their wires; rail 0 = not placed yet.</summary>
    public static bool IsPlaced(ModuleKind kind, int rail) => kind is ModuleKind.Plc or ModuleKind.NoId || rail > 0;

    public static string IdOf(ModuleKind kind, int line, int rail, int place) =>
        kind == ModuleKind.NoId || !IsPlaced(kind, rail) ? "" : $"{line}.{rail:00}.{place:00}";

    /// <summary>PLC → "PLC"; head → "LOC-03"; others → "AI-03.05"; not placed → "AI-?".</summary>
    public static string SystemName(ModuleKind kind, string prefix, int rail, int place) => kind switch
    {
        ModuleKind.Plc => prefix,
        ModuleKind.NoId => prefix,
        _ when rail == 0 => $"{prefix}-?",
        ModuleKind.Head => $"{prefix}-{rail:00}",
        _ => $"{prefix}-{rail:00}.{place:00}",
    };
}
