namespace HvacConfigurator.Api.Services;

/// <summary>One action the application can allow or deny. Built-in roles are templates of these keys.</summary>
public static class Perm
{
    public const string ProjectsView = "projects.view";
    public const string ProjectsCreate = "projects.create";
    public const string ProjectsEdit = "projects.edit";
    public const string ProjectsArchive = "projects.archive";
    public const string ProjectsActivate = "projects.activate";
    public const string ProjectsCopy = "projects.copy";

    public const string RoomsView = "rooms.view";
    public const string RoomsEdit = "rooms.edit";

    public const string EquipmentView = "equipment.view";
    public const string EquipmentEdit = "equipment.edit";

    public const string DocumentsView = "documents.view";
    public const string DocumentsUpload = "documents.upload";
    public const string DocumentsDelete = "documents.delete";
    public const string AnalysisEdit = "analysis.edit";

    public const string LibraryView = "library.view";
    public const string LibraryEdit = "library.edit";

    public const string HmiLibraryView = "hmi.library.view";
    public const string HmiLibraryEdit = "hmi.library.edit";
    public const string HmiScreensView = "hmi.screens.view";
    public const string HmiScreensEdit = "hmi.screens.edit";

    public const string HardwareView = "hardware.view";
    public const string HardwareEdit = "hardware.edit";
    public const string SchemeView = "scheme.view";
    public const string SchemeEdit = "scheme.edit";
    public const string SignalsEdit = "signals.edit";
    public const string BindingEdit = "binding.edit";
    public const string RegulatorsEdit = "regulators.edit";

    public const string MonitoringView = "monitoring.view";
    public const string AlarmsView = "alarms.view";
    public const string AlarmsAck = "alarms.ack";
    public const string SimulatorUse = "simulator.use";
    public const string CommissioningEdit = "commissioning.edit";

    public const string SettingsView = "settings.view";
    public const string SettingsEdit = "settings.edit";
    public const string UsersView = "users.view";
    public const string UsersEdit = "users.edit";
    public const string RolesView = "roles.view";
    public const string RolesEdit = "roles.edit";
    public const string AuditView = "audit.view";
    public const string ReportsView = "reports.view";

    public const string FirmwareView = "firmware.view";
    public const string FirmwareManage = "firmware.manage";
    public const string Can3Use = "can3.use";
    public const string PlcConfigView = "plc.config.view";
    public const string PlcConfigUpload = "plc.config.upload";

    public static readonly string[] All =
    [
        ProjectsView, ProjectsCreate, ProjectsEdit, ProjectsArchive, ProjectsActivate, ProjectsCopy,
        RoomsView, RoomsEdit, EquipmentView, EquipmentEdit,
        DocumentsView, DocumentsUpload, DocumentsDelete, AnalysisEdit,
        LibraryView, LibraryEdit, HmiLibraryView, HmiLibraryEdit, HmiScreensView, HmiScreensEdit,
        HardwareView, HardwareEdit, SchemeView, SchemeEdit, SignalsEdit, BindingEdit, RegulatorsEdit,
        MonitoringView, AlarmsView, AlarmsAck, SimulatorUse, CommissioningEdit, ReportsView,
        SettingsView, SettingsEdit, UsersView, UsersEdit, RolesView, RolesEdit, AuditView,
        FirmwareView, FirmwareManage, Can3Use, PlcConfigView, PlcConfigUpload,
    ];

    public static readonly (string Group, string[] Keys)[] Groups =
    [
        ("projects", [ProjectsView, ProjectsCreate, ProjectsEdit, ProjectsArchive, ProjectsActivate, ProjectsCopy]),
        ("rooms", [RoomsView, RoomsEdit]),
        ("equipment", [EquipmentView, EquipmentEdit]),
        ("documents", [DocumentsView, DocumentsUpload, DocumentsDelete, AnalysisEdit]),
        ("library", [LibraryView, LibraryEdit]),
        ("hmi", [HmiLibraryView, HmiLibraryEdit, HmiScreensView, HmiScreensEdit]),
        ("config", [HardwareView, HardwareEdit, SchemeView, SchemeEdit, SignalsEdit, BindingEdit, RegulatorsEdit]),
        ("operation", [MonitoringView, AlarmsView, AlarmsAck, SimulatorUse, CommissioningEdit, ReportsView]),
        ("admin", [SettingsView, SettingsEdit, UsersView, UsersEdit, RolesView, RolesEdit, AuditView]),
        ("service", [FirmwareView, FirmwareManage, Can3Use, PlcConfigView, PlcConfigUpload]),
    ];

    public static readonly Guid ViewerId = Guid.Parse("11111111-1111-4111-8111-111111111111");
    public static readonly Guid OperatorId = Guid.Parse("22222222-2222-4222-8222-222222222222");
    public static readonly Guid EngineerId = Guid.Parse("33333333-3333-4333-8333-333333333333");
    public static readonly Guid AdminId = Guid.Parse("44444444-4444-4444-8444-444444444444");

    /// <summary>Users.edit and Roles.edit stay on the built-in Admin role so the last administrator cannot be locked out.</summary>
    public static readonly string[] AdminLocked = [UsersEdit, RolesEdit];

    public static IReadOnlyList<string> ForBuiltIn(string code) => code switch
    {
        "Viewer" => ViewerSet,
        "Operator" => OperatorSet,
        "Engineer" => EngineerSet,
        "Admin" => All,
        _ => [],
    };

    private static readonly string[] ViewerSet =
    [
        ProjectsView, RoomsView, EquipmentView, DocumentsView, LibraryView,
        HmiLibraryView, HmiScreensView,
        MonitoringView, AlarmsView, ReportsView, SettingsView,
    ];

    private static readonly string[] OperatorSet = [.. ViewerSet, AlarmsAck];

    private static readonly string[] EngineerSet =
    [
        .. OperatorSet,
        ProjectsCreate, ProjectsEdit, ProjectsArchive, ProjectsCopy,
        RoomsEdit, EquipmentEdit, DocumentsUpload, DocumentsDelete, AnalysisEdit,
        LibraryEdit, HmiLibraryEdit, HmiScreensEdit,
        HardwareView, HardwareEdit, SchemeView, SchemeEdit, SignalsEdit, BindingEdit, RegulatorsEdit,
        SimulatorUse, CommissioningEdit, AuditView,
        FirmwareView, FirmwareManage, Can3Use, PlcConfigView, PlcConfigUpload,
    ];

    public static string[] Sanitize(IEnumerable<string>? keys) =>
        (keys ?? []).Where(k => All.Contains(k)).Distinct().OrderBy(k => Array.IndexOf(All, k)).ToArray();
}
