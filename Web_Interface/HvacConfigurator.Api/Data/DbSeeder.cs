using System.Text.Json;
using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Entities;
using HvacConfigurator.Api.Services;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Data;

public static class DbSeeder
{
    public static async Task SeedAsync(AppDbContext db, IConfiguration config)
    {
        if (!await db.Users.AnyAsync())
        {
            var password = config["Seed:AdminPassword"] ?? "admin";
            db.Users.Add(new User
            {
                Login = "admin",
                FullName = "Administrator",
                PasswordHash = BCrypt.Net.BCrypt.HashPassword(password),
                Role = UserRole.Admin,
            });
        }

        if (!await db.SystemSettings.AnyAsync())
            db.SystemSettings.Add(new SystemSettings());

        if (!await db.LibraryItems.AnyAsync())
            db.LibraryItems.AddRange(StarterLibrary());

        await RenameArticlesAsync(db);
        var existing = await db.LibraryItems.Select(x => x.Code).ToListAsync();
        db.LibraryItems.AddRange(StandardModules().Where(m => !existing.Contains(m.Code)));

        if (!await db.Projects.AnyAsync())
        {
            var p = new Project
            {
                Name = new LocalizedText { Ru = "NanoMotion — чистая комната", En = "NanoMotion — clean room", He = "NanoMotion — חדר נקי" },
                Number = "DEMO-001",
                Customer = "NanoMotion",
                IsoClass = 7,
                Description = "Демо-проект: приточная установка 600 cfm, VRF, электронагрев, FFU.",
                IsActiveOnSite = true,
                CreatedBy = "system",
            };
            p.Rooms.Add(new Room
            {
                ProjectId = p.Id,
                Name = new LocalizedText { Ru = "Комната расширения", En = "Extension room", He = "חדר הרחבה" },
                IsoClass = 7,
                AreaM2 = 35m,
                HeightM = 2.7m,
                TempSetpointC = 21m,
                TempToleranceC = 2m,
            });
            db.Projects.Add(p);
        }

        await db.SaveChangesAsync();
        await SeedDemoHardwareAsync(db);
        await SeedDemoSchemeAsync(db);
        await RenameOwnManufacturerAsync(db);
        await RemoveObsoleteArticlesAsync(db);
        await SeedChannelModesAsync(db);
    }

    /// <summary>Analog channels are set per channel to one signal type; it goes to the module firmware.</summary>
    private static readonly Dictionary<string, List<string>> StandardChannelModes = new()
    {
        ["HC-AI2"] = ["0-10V", "4-20mA"],
        ["HC-AO2"] = ["0-10V", "4-20mA"],
    };

    /// <summary>Library items created before channel modes existed get them in place, together with project copies of the same version.</summary>
    private static async Task SeedChannelModesAsync(AppDbContext db)
    {
        var codes = StandardChannelModes.Keys.ToList();
        var items = await db.LibraryItems.Where(l => codes.Contains(l.Code)).ToListAsync();
        var changed = false;
        foreach (var item in items.Where(i => i.ChannelModes.Count == 0))
        {
            item.ChannelModes = StandardChannelModes[item.Code];
            var snapshot = JsonSerializer.Serialize(LibraryService.ToDto(item, 0), Localized.Json);
            foreach (var m in await db.ProjectModules.Where(m => m.LibraryItemId == item.Id && m.LibraryVersion == item.Version).ToListAsync())
                m.SnapshotJson = snapshot;
            changed = true;
        }
        if (changed) await db.SaveChangesAsync();
    }

    private const string OwnManufacturer = "DCM Doors Control Making";
    private const string OldOwnManufacturer = "HVAC CleanRoom";

    /// <summary>Early seeds used a placeholder manufacturer; fix library items and the copies frozen in projects.</summary>
    private static async Task RenameOwnManufacturerAsync(AppDbContext db)
    {
        var items = await db.LibraryItems.Where(l => l.Manufacturer == OldOwnManufacturer).ToListAsync();
        foreach (var i in items) i.Manufacturer = OwnManufacturer;
        var modules = await db.ProjectModules.Where(m => m.SnapshotJson.Contains(OldOwnManufacturer)).ToListAsync();
        foreach (var m in modules) m.SnapshotJson = m.SnapshotJson.Replace(OldOwnManufacturer, OwnManufacturer);
        if (items.Count > 0 || modules.Count > 0) await db.SaveChangesAsync();
    }

    /// <summary>Early articles with a wrong channel count: corrected in place (same item, same version) together with project copies.</summary>
    private static readonly Dictionary<string, string> RenamedArticles = new() { ["HC-AI8"] = "HC-AI2", ["HC-AO4"] = "HC-AO2" };

    private static async Task RenameArticlesAsync(AppDbContext db)
    {
        var olds = RenamedArticles.Keys.ToList();
        var items = await db.LibraryItems.Where(l => olds.Contains(l.Code)).ToListAsync();
        if (items.Count == 0) return;
        var std = StandardModules().ToDictionary(m => m.Code);
        foreach (var item in items)
        {
            var target = std[RenamedArticles[item.Code]];
            if (await db.LibraryItems.AnyAsync(l => l.Code == target.Code)) continue;
            item.Code = target.Code;
            item.Name = target.Name;
            item.ChannelCount = target.ChannelCount;
            item.PropsJson = target.PropsJson;
            var snapshot = JsonSerializer.Serialize(LibraryService.ToDto(item, 0), Localized.Json);
            foreach (var m in await db.ProjectModules.Where(m => m.LibraryItemId == item.Id).ToListAsync())
                m.SnapshotJson = snapshot;
        }
        await db.SaveChangesAsync();
    }

    /// <summary>Demo rails for the demo project: PLC, locomotive rail 01 with I/O, display hub rail 02.</summary>
    private static async Task SeedDemoHardwareAsync(AppDbContext db)
    {
        var demo = await db.Projects.FirstOrDefaultAsync(p => p.Number == "DEMO-001" && !p.IsArchived);
        if (demo is null || await db.ProjectModules.AnyAsync(m => m.ProjectId == demo.Id)) return;
        var lib = await db.LibraryItems.Where(l => l.Category == LibraryCategories.Module).ToDictionaryAsync(l => l.Code);
        (string article, int line, int rail, int place, string ru)[] layout =
        [
            ("HC-PLC", 0, 0, 0, "Шкаф управления"),
            ("HC-LOC", 1, 1, 0, "Щит AHU"),
            ("HC-AI2", 1, 1, 1, "Датчики AHU и комнаты"),
            ("HC-AO2", 1, 1, 2, "Частотник вентилятора"),
            ("HC-DI8", 1, 1, 3, "Состояния и аварии"),
            ("HC-RL4", 1, 1, 4, "Ступени нагревателя"),
            ("HC-HUBD9", 2, 2, 0, "Дисплеи у дверей"),
            ("HC-TFT43", 2, 2, 1, "Вход в комнату"),
        ];
        foreach (var (article, line, rail, place, ru) in layout)
        {
            if (!lib.TryGetValue(article, out var item)) continue;
            db.ProjectModules.Add(new ProjectModule
            {
                ProjectId = demo.Id, LibraryItemId = item.Id, LibraryVersion = item.Version,
                SnapshotJson = JsonSerializer.Serialize(LibraryService.ToDto(item, 0), Localized.Json),
                Line = line, Rail = rail, Place = place,
                UserName = new LocalizedText { Ru = ru },
            });
        }
        await db.SaveChangesAsync();
    }

    /// <summary>Scheme of the demo rails: block positions and wires between default connection points.</summary>
    private static async Task SeedDemoSchemeAsync(AppDbContext db)
    {
        var demo = await db.Projects.FirstOrDefaultAsync(p => p.Number == "DEMO-001" && !p.IsArchived);
        if (demo is null || await db.ProjectLinks.AnyAsync(l => l.ProjectId == demo.Id)) return;
        var rows = await db.ProjectModules.Where(m => m.ProjectId == demo.Id).ToListAsync();
        if (rows.Count == 0 || rows.Any(r => r.SchemeX != 0 || r.SchemeY != 0)) return;
        var libs = await db.LibraryItems.Where(l => l.Category == LibraryCategories.Module).ToDictionaryAsync(l => l.Id, l => l.Code);
        var byArticle = rows.Where(r => r.LibraryItemId is { } id && libs.ContainsKey(id))
            .GroupBy(r => libs[r.LibraryItemId!.Value]).ToDictionary(g => g.Key, g => g.First());

        (string article, double x, double y)[] positions =
        [
            ("HC-PLC", 600, 40), ("HC-LOC", 1150, 330), ("HC-AI2", 960, 330), ("HC-AO2", 800, 330),
            ("HC-DI8", 540, 330), ("HC-RL4", 380, 330), ("HC-HUBD9", 560, 620), ("HC-TFT43", 500, 860),
        ];
        foreach (var (article, x, y) in positions)
            if (byArticle.TryGetValue(article, out var m)) (m.SchemeX, m.SchemeY) = (x, y);

        (string from, string fromPort, string to, string toPort)[] wires =
        [
            ("HC-PLC", "can1", "HC-LOC", "can2_in"), ("HC-LOC", "can1", "HC-AI2", "can_in"), ("HC-AI2", "can_out", "HC-AO2", "can_in"),
            ("HC-AO2", "can_out", "HC-DI8", "can_in"), ("HC-DI8", "can_out", "HC-RL4", "can_in"),
            ("HC-PLC", "can2", "HC-HUBD9", "can2_in"), ("HC-HUBD9", "port1", "HC-TFT43", "can"),
        ];
        foreach (var (from, fromPort, to, toPort) in wires)
            if (byArticle.TryGetValue(from, out var a) && byArticle.TryGetValue(to, out var b))
                db.ProjectLinks.Add(new ProjectLink { ProjectId = demo.Id, FromModuleId = a.Id, FromPort = fromPort, ToModuleId = b.Id, ToPort = toPort });
        await db.SaveChangesAsync();
    }

    private static LibraryItem Item(string cat, string code, string ru, string en, string he, string manufacturer, string model, params (string k, string v, string u)[] props) => new()
    {
        Category = cat,
        Code = code,
        Name = new LocalizedText { Ru = ru, En = en, He = he },
        Manufacturer = manufacturer,
        Model = model,
        PropsJson = JsonSerializer.Serialize(props.Select(p => new LibraryPropDto(p.k, p.v, p.u)), Localized.Json),
    };

    private static LibraryItem Module(string article, int typeCode, string prefix, int? channels, string ru, string en, string he, params (string k, string v, string u)[] props)
    {
        var i = Item(LibraryCategories.Module, article, ru, en, he, OwnManufacturer, "", props);
        i.TypeCode = typeCode;
        i.SystemPrefix = prefix;
        i.ChannelCount = channels;
        return i;
    }

    /// <summary>Own module range, plan 8.3.1. Added on every start if missing (matched by article).</summary>
    private static IEnumerable<LibraryItem> StandardModules() =>
    [
        Module("HC-PLC", 0x01, "PLC", null, "Главный ПЛК", "Main PLC", "בקר ראשי",
            ("МК", "STM32H723", ""), ("CAN FD", "3", "линии"), ("Ethernet", "1", ""), ("RS-485", "3", ""), ("QSPI Flash", "16", "MB")),
        Module("HC-DPLC", 0x02, "DPLC", null, "Doors PLC", "Doors PLC", "בקר דלתות", ("Связь", "CAN3, вне цепочки ID", "")),
        Module("HC-LOC", 0x10, "LOC", null, "Локомотив (головной модуль рейки)", "Locomotive (rail head)", "קטר (ראש מסילה)",
            ("CAN1", "шина рейки", ""), ("CAN2", "кабель к ПЛК", "")),
        Module("HC-AI2", 0x20, "AI", 2, "2 аналоговых входа", "2 analog inputs", "2 כניסות אנלוגיות", ("Сигнал", "0–10 V / 4–20 mA / PT", "")),
        Module("HC-AO2", 0x30, "AO", 2, "2 аналоговых выхода", "2 analog outputs", "2 יציאות אנלוגיות", ("Сигнал", "0–10 V / 4–20 mA", "")),
        Module("HC-DI8", 0x40, "DI", 8, "8 дискретных входов 24 V", "8 digital inputs 24 V", "8 כניסות דיגיטליות 24V", ("Сигнал", "24", "V")),
        Module("HC-DO8", 0x50, "DO", 8, "8 транзисторных выходов 24 V", "8 transistor outputs 24 V", "8 יציאות טרנזיסטור 24V", ("Сигнал", "24", "V")),
        Module("HC-RL4", 0x60, "RL", 4, "4 реле", "4 relays", "4 ממסרים", ("Назначение", "силовые ON/OFF цепи", "")),
        Module("HC-HUBD9", 0x70, "HUBD", 9, "Хаб дисплеев на 9 портов", "Display hub, 9 ports", "רכזת תצוגות 9 יציאות", ("Опрос ID", "звезда, 9 портов", "")),
        Module("HC-TFT43", 0x71, "TFT", null, "Дисплей 4.3\"", "Display 4.3\"", "תצוגה 4.3\"", ("Подключение", "порт хаба", "")),
        Module("HC-HMI10", 0x80, "HMI", null, "Панель HMI 10\"", "HMI panel 10\"", "פאנל HMI 10\""),
    ];

    /// <summary>Plug-in boards (0x9_) are parts of the main modules, not a product line item.</summary>
    private static readonly string[] RemovedArticles = ["HC-PWRCAN", "HC-CAN2"];

    private static async Task RemoveObsoleteArticlesAsync(AppDbContext db)
    {
        var ids = await db.LibraryItems.Where(l => RemovedArticles.Contains(l.Code)).Select(l => l.Id).ToListAsync();
        if (ids.Count == 0) return;
        var used = await db.ProjectModules.Where(m => m.LibraryItemId != null && ids.Contains(m.LibraryItemId.Value)).Select(m => m.LibraryItemId!.Value)
            .Union(db.ProjectEquipment.Where(e => e.LibraryItemId != null && ids.Contains(e.LibraryItemId.Value)).Select(e => e.LibraryItemId!.Value))
            .ToListAsync();
        await db.LibraryItems.Where(l => ids.Contains(l.Id) && !used.Contains(l.Id)).ExecuteDeleteAsync();
    }

    private static IEnumerable<LibraryItem> StarterLibrary() =>
    [
        Item(LibraryCategories.Sensor, "TT-4-20", "Датчик температуры канальный", "Duct temperature transmitter", "משדר טמפרטורה לתעלה", "", "",
            ("Диапазон", "-30…70", "°C"), ("Выход", "4–20", "мА")),
        Item(LibraryCategories.Sensor, "DPT-4-20", "Датчик перепада давления", "Differential pressure transmitter", "משדר הפרש לחצים", "", "",
            ("Диапазон", "0…500", "Pa"), ("Выход", "4–20", "мА")),
        Item(LibraryCategories.Sensor, "FS", "Реле потока воздуха", "Air flow switch", "מפסק זרימת אוויר", "", "",
            ("Выход", "сухой контакт", "")),
        Item(LibraryCategories.Sensor, "PI", "Дифманометр (местный)", "Differential pressure gauge", "מד הפרש לחצים", "", "",
            ("Диапазон", "0…500", "Pa")),
        Item(LibraryCategories.Actuator, "FAN-VSD", "Вентилятор с частотником", "Fan with VSD", "מאוורר עם ממיר תדר", "", "",
            ("Управление", "0–10", "В"), ("Авария", "DI", "")),
        Item(LibraryCategories.Actuator, "EH-3x1.2", "Электронагреватель 3 ступени", "Electric heater, 3 stages", "מחמם חשמלי 3 דרגות", "", "",
            ("Мощность", "3×1.2", "кВт"), ("Термостат", "TS, DI", "")),
        Item(LibraryCategories.Actuator, "VRF-OU", "VRF наружный блок", "VRF outdoor unit", "יחידה חיצונית VRF", "", "",
            ("Связь", "Modbus RTU", "")),
        Item(LibraryCategories.Regulator, "PID-T", "ПИД температуры", "Temperature PID", "PID טמפרטורה", "", "",
            ("Вход", "TT", ""), ("Выход", "0–100", "%")),
    ];
}
