using System.Text.Json;
using System.Text.RegularExpressions;
using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Dto;

namespace HvacConfigurator.Api.Services;

/// <summary>
/// Module block drawing for the system scheme: validation and the default drawing of the own module range.
/// Port roles drive automatic numbering when wires are drawn (plan 8.3.3):
/// line (PLC CAN line N) ↔ uplink (rail head); cableOut (head) ↔ uplink (next head on the cable);
/// railOut (locomotive) ↔ chainIn (first wagon); chainOut ↔ chainIn (next wagon); hubPort N ↔ hubIn (display).
/// </summary>
public static partial class ModuleGraphics
{
    public static readonly string[] PortTypes = ["can", "rs485", "eth", "pwr24", "ai", "ao", "di", "do", "relay", "usb", "other"];
    public static readonly string[] PortDirs = ["in", "out", "bi"];
    public static readonly string[] PortRoles = ["line", "uplink", "cableOut", "railOut", "chainIn", "chainOut", "hubPort", "hubIn"];

    [GeneratedRegex("^#[0-9a-fA-F]{6}$")]
    private static partial Regex ColorFormat();

    [GeneratedRegex("^[A-Za-z0-9_-]{1,32}$")]
    private static partial Regex PortIdFormat();

    public static ModuleGraphicDto? Parse(string json)
    {
        if (string.IsNullOrWhiteSpace(json)) return null;
        try { return JsonSerializer.Deserialize<ModuleGraphicDto>(json, Localized.Json); }
        catch (JsonException) { return null; }
    }

    public static string ToJson(ModuleGraphicDto? g) => g is null ? "" : JsonSerializer.Serialize(g, Localized.Json);

    /// <summary>Returns an error code or null; <paramref name="clean"/> is the normalized drawing.</summary>
    public static string? Validate(ModuleGraphicDto g, out ModuleGraphicDto clean)
    {
        clean = g;
        if (g.Width is < 80 or > 1600 || g.Height is < 50 or > 1200) return "graphic_size";
        foreach (var c in new[] { g.Fill, g.Header, g.HeaderText, g.TextColor, g.Border })
            if (!ColorFormat().IsMatch(c ?? "")) return "graphic_color";
        var ports = g.Ports ?? [];
        if (ports.Count > 96) return "graphic_ports";
        var ids = new HashSet<string>();
        var cleanPorts = new List<GraphicPortDto>();
        foreach (var p in ports)
        {
            if (!PortIdFormat().IsMatch(p.Id ?? "") || !ids.Add(p.Id!)) return "graphic_port_id";
            if (!PortTypes.Contains(p.Type) || !PortDirs.Contains(p.Dir)) return "graphic_port_type";
            if (p.Role is { Length: > 0 } r && !PortRoles.Contains(r)) return "graphic_port_role";
            if (p.Color is { Length: > 0 } pc && !ColorFormat().IsMatch(pc)) return "graphic_color";
            cleanPorts.Add(p with
            {
                Label = Trim(p.Label, 32),
                X = Math.Clamp(p.X, 0, 1),
                Y = Math.Clamp(p.Y, 0, 1),
                Color = string.IsNullOrEmpty(p.Color) ? null : p.Color,
                Role = string.IsNullOrEmpty(p.Role) ? null : p.Role,
            });
        }
        var labels = g.Labels ?? [];
        if (labels.Count > 48) return "graphic_labels";
        var cleanLabels = new List<GraphicLabelDto>();
        foreach (var l in labels)
        {
            if (l.Color is { Length: > 0 } lc && !ColorFormat().IsMatch(lc)) return "graphic_color";
            cleanLabels.Add(l with
            {
                Id = Trim(l.Id, 32),
                Text = Trim(l.Text, 80),
                X = Math.Clamp(l.X, 0, 1),
                Y = Math.Clamp(l.Y, 0, 1),
                Size = Math.Clamp(l.Size, 8, 32),
                Color = string.IsNullOrEmpty(l.Color) ? null : l.Color,
            });
        }
        clean = g with { Title = Trim(g.Title, 40), Subtitle = Trim(g.Subtitle, 80), Ports = cleanPorts, Labels = cleanLabels };
        return null;
    }

    private static string Trim(string? s, int max)
    {
        var t = (s ?? "").Trim();
        return t.Length > max ? t[..max] : t;
    }

    public static GraphicPortDto? FindPort(LibraryItemDto? lib, string portId) => lib?.Graphic?.Ports.FirstOrDefault(p => p.Id == portId);

    /// <summary>Wires join points of the same type; two pure inputs or two pure outputs cannot be joined.</summary>
    public static bool Compatible(GraphicPortDto a, GraphicPortDto b) =>
        a.Type == b.Type && !(a.Dir == "in" && b.Dir == "in") && !(a.Dir == "out" && b.Dir == "out");

    // ---- default drawings of the own range (plan 8.3.1) ----

    private static GraphicPortDto P(string id, string label, string type, string dir, double x, double y, string? role = null, int? line = null, int? index = null) =>
        new(id, label, type, dir, x, y, null, role, line, index);

    private static ModuleGraphicDto Block(int w, int h, string fill, string header, List<GraphicPortDto> ports) =>
        new(w, h, fill, header, "#ffffff", "#1f2937", header, "", "", true, ports, []);

    private static IEnumerable<GraphicPortDto> Row(string prefix, string label, string type, string dir, int count, double y = 1)
    {
        for (var i = 1; i <= count; i++)
            yield return P($"{prefix}{i}", $"{label}{i}", type, dir, (i - 0.5) / count, y);
    }

    private static List<GraphicPortDto> WagonChain() =>
    [
        P("can_in", "CAN in", "can", "bi", 1, 0.32, "chainIn"),
        P("can_out", "CAN out", "can", "bi", 0, 0.32, "chainOut"),
        P("pwr_in", "24V in", "pwr24", "in", 1, 0.62),
        P("pwr_out", "24V out", "pwr24", "out", 0, 0.62),
    ];

    private static ModuleGraphicDto Wagon(int channels, string prefix, string label, string type, string dir, string fill = "#fef3c7", string header = "#d97706") =>
        Block(Math.Max(120, 26 * channels + 20), 150, fill, header, [.. WagonChain(), .. Row(prefix, label, type, dir, channels)]);

    public static ModuleGraphicDto Default(string article, ModuleKind? kind, int? channels) => article switch
    {
        "HC-PLC" => Block(360, 170, "#dcfce7", "#16a34a",
        [
            P("rs485_1", "RS485-1", "rs485", "bi", 0.12, 0), P("rs485_2", "RS485-2", "rs485", "bi", 0.26, 0), P("rs485_3", "RS485-3", "rs485", "bi", 0.40, 0),
            P("eth", "Ethernet", "eth", "bi", 0.58, 0), P("usb", "USB", "usb", "bi", 0.72, 0), P("pwr_in", "24V", "pwr24", "in", 0.9, 0),
            P("can1", "CAN1 · AI", "can", "bi", 0.2, 1, "line", 1), P("can2", "CAN2 · Displays", "can", "bi", 0.5, 1, "line", 2),
            P("can3", "CAN3 · Bus", "can", "bi", 0.8, 1, "line", 3),
        ]),
        "HC-DPLC" => Block(320, 140, "#fee2e2", "#dc2626",
        [
            P("can_in", "CAN in", "can", "bi", 0.78, 0), P("can_out", "CAN out", "can", "bi", 0.92, 0), P("pwr_in", "24V", "pwr24", "in", 0.6, 0),
            .. Row("door", "D", "other", "bi", 8),
        ]),
        "HC-LOC" => Block(160, 160, "#ede9fe", "#7c3aed",
        [
            P("can2_in", "CAN2 in", "can", "bi", 0.3, 0, "uplink"), P("can2_out", "CAN2 out", "can", "bi", 0.7, 0, "cableOut"),
            P("can1", "CAN1", "can", "bi", 0, 0.35, "railOut"), P("pwr_in", "24V in", "pwr24", "in", 1, 0.35), P("pwr_out", "24V out", "pwr24", "out", 0, 0.7),
        ]),
        "HC-AI2" => Wagon(2, "ai", "AI", "ai", "in"),
        "HC-AO2" => Wagon(2, "ao", "AO", "ao", "out"),
        "HC-DI8" => Wagon(8, "di", "DI", "di", "in", "#e0f2fe", "#0369a1"),
        "HC-DO8" => Wagon(8, "do", "DO", "do", "out", "#fce7f3", "#be185d"),
        "HC-RL4" => Wagon(4, "rl", "K", "relay", "out", "#f1f5f9", "#475569"),
        "HC-HUBD9" => Block(340, 140, "#ffedd5", "#ea580c",
        [
            P("can2_in", "CAN2 in", "can", "bi", 0.8, 0, "uplink"), P("can2_out", "CAN2 out", "can", "bi", 1, 0.45, "cableOut"), P("pwr_in", "24V", "pwr24", "in", 0.6, 0),
            .. Enumerable.Range(1, ModuleRules.HubPorts).Select(i => P($"port{i}", $"P{i}", "can", "bi", (i - 0.5) / ModuleRules.HubPorts, 1, "hubPort", index: i)),
        ]),
        "HC-TFT43" => Block(180, 120, "#e0f2fe", "#0284c7", [P("can", "CAN", "can", "bi", 0.5, 1, "hubIn"), P("pwr_in", "24V", "pwr24", "in", 0.85, 1)]),
        "HC-HMI10" => Block(180, 120, "#dbeafe", "#2563eb",
            [P("rs485", "RS485", "rs485", "bi", 0.3, 1), P("eth", "Ethernet", "eth", "bi", 0.6, 1), P("pwr_in", "24V", "pwr24", "in", 0.88, 1)]),
        _ => kind switch
        {
            ModuleKind.Wagon => Wagon(Math.Clamp(channels ?? 2, 1, 32), "ch", "", "other", "bi"),
            ModuleKind.Head => Block(160, 150, "#ede9fe", "#7c3aed",
                [P("up", "CAN in", "can", "bi", 0.3, 0, "uplink"), P("out", "CAN out", "can", "bi", 0.7, 0, "cableOut"), P("rail", "CAN", "can", "bi", 0, 0.4, "railOut")]),
            ModuleKind.Display => Block(180, 120, "#e0f2fe", "#0284c7", [P("can", "CAN", "can", "bi", 0.5, 1, "hubIn")]),
            _ => Block(180, 110, "#f8fafc", "#475569", []),
        },
    };
}
