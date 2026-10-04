namespace HvacConfigurator.Api.Dto;

public record LoginRequest(string Login, string Password);

public record AuthResponse(string Token, Guid UserId, string Login, string FullName, string Role);

public record LocalizedTextDto(string Ru, string En, string He);

public record RoomDto(
    Guid Id,
    LocalizedTextDto Name,
    int? IsoClass,
    decimal? AreaM2,
    decimal? HeightM,
    decimal? TempSetpointC,
    decimal? TempToleranceC,
    decimal? RhSetpointPct,
    decimal? RhTolerancePct,
    decimal? PressureSetpointPa,
    decimal? PressureTolerancePa,
    int SortOrder);

public record RoomSaveRequest(
    LocalizedTextDto Name,
    int? IsoClass,
    decimal? AreaM2,
    decimal? HeightM,
    decimal? TempSetpointC,
    decimal? TempToleranceC,
    decimal? RhSetpointPct,
    decimal? RhTolerancePct,
    decimal? PressureSetpointPa,
    decimal? PressureTolerancePa,
    int SortOrder,
    string? Reason);

public record AuditEntryDto(
    long Id, DateTime At, string UserLogin, string Source, string Action,
    string Target, string? OldValue, string? NewValue, string? Reason);

public record AuditPage(IReadOnlyList<AuditEntryDto> Items, int Total);

public record SettingsDto(string SiteName, string StartupLanguage, string PlcMode, string PlcAddress);

public record PublicSettingsDto(string SiteName, string StartupLanguage);

public record UserDto(Guid Id, string Login, string FullName, string Role, bool IsActive, DateTime? LastLoginAt);

public record ProjectDto(
    Guid Id, LocalizedTextDto Name, string Number, string Customer, string Address, string Responsible,
    int? IsoClass, string Description, bool IsArchived, bool IsActiveOnSite,
    DateTime CreatedAt, string CreatedBy, DateTime UpdatedAt,
    int RoomCount, int DocumentCount, int EquipmentCount, int ModuleCount);

public record ProjectSaveRequest(
    LocalizedTextDto Name, string? Number, string? Customer, string? Address, string? Responsible,
    int? IsoClass, string? Description, string? Reason);

public record ProjectDocumentDto(Guid Id, string Kind, string FileName, string ContentType, long SizeBytes, DateTime UploadedAt, string UploadedBy);

public record LibraryPropDto(string Key, string Value, string Unit);

public record LibraryItemDto(
    Guid Id, string Category, string Code, LocalizedTextDto Name, string Manufacturer, string Model,
    string Description, IReadOnlyList<LibraryPropDto> Props, int Version, bool IsArchived, DateTime UpdatedAt,
    int UsedInProjects, int? TypeCode = null, string SystemPrefix = "", int? ChannelCount = null, string? Kind = null,
    ModuleGraphicDto? Graphic = null, IReadOnlyList<string>? ChannelModes = null);

/// <summary>ChannelModes: signal types a module channel can be set to (e.g. 0-10V, 4-20mA); empty = no choice.</summary>
public record LibraryItemSaveRequest(
    string Category, string Code, LocalizedTextDto Name, string? Manufacturer, string? Model,
    string? Description, IReadOnlyList<LibraryPropDto>? Props, string? Reason,
    int? TypeCode = null, string? SystemPrefix = null, int? ChannelCount = null, ModuleGraphicDto? Graphic = null,
    IReadOnlyList<string>? ChannelModes = null);

/// <summary>Setting of one module channel in the project; Mode goes to the module firmware.</summary>
public record ModuleChannelDto(int Channel, string Mode);

/// <summary>Connection point on a module block. X/Y are fractions 0..1 of the block size.</summary>
public record GraphicPortDto(
    string Id, string Label, string Type, string Dir, double X, double Y,
    string? Color = null, string? Role = null, int? Line = null, int? Index = null);

public record GraphicLabelDto(string Id, string Text, double X, double Y, string? Color = null, int Size = 11, bool Bold = false);

/// <summary>How a module looks on the system scheme and where its connection points are.</summary>
public record ModuleGraphicDto(
    int Width, int Height, string Fill, string Header, string HeaderText, string TextColor, string Border,
    string Title, string Subtitle, bool ShowArticle, IReadOnlyList<GraphicPortDto> Ports, IReadOnlyList<GraphicLabelDto> Labels);

public record ProjectModuleDto(
    Guid Id, Guid? LibraryItemId, int LibraryVersion, int? LibraryLatestVersion, LibraryItemDto? Snapshot,
    string Kind, int Line, int Rail, int Place, string ModuleId, string SystemName,
    LocalizedTextDto UserName, string ExpectedSerial, string Revision, string Notes,
    bool Placed = true, double SchemeX = 0, double SchemeY = 0, IReadOnlyList<ModuleChannelDto>? Channels = null);

public record ProjectLinkDto(Guid Id, Guid FromModuleId, string FromPort, Guid ToModuleId, string ToPort, string Style = "step", IReadOnlyList<SchemePointDto>? Points = null);

public record SchemePointDto(double X, double Y);

/// <summary>Style: step (right angles), straight, curve. Points: bend points in scheme coordinates.</summary>
public record LinkRouteRequest(string? Style, IReadOnlyList<SchemePointDto>? Points);

public record ProjectLinkSaveRequest(Guid FromModuleId, string FromPort, Guid ToModuleId, string ToPort);

public record SchemeDto(IReadOnlyList<ProjectModuleDto> Modules, IReadOnlyList<ProjectLinkDto> Links);

public record SchemePositionRequest(double X, double Y);

/// <summary>Module dropped from the palette: not placed yet, the ID is assigned by its connections.</summary>
public record SchemeModuleCreateRequest(Guid LibraryItemId, double X, double Y);

public record ProjectModuleSaveRequest(
    Guid LibraryItemId, int Line, int Rail, int Place, LocalizedTextDto? UserName,
    string? ExpectedSerial, string? Revision, string? Notes, string? Reason,
    IReadOnlyList<ModuleChannelDto>? Channels = null);

public record ProjectEquipmentDto(
    Guid Id, string Tag, int Quantity, Guid? RoomId, string Notes,
    Guid? LibraryItemId, int LibraryVersion, int? LibraryLatestVersion, LibraryItemDto? Snapshot);

public record ProjectEquipmentSaveRequest(Guid? LibraryItemId, string Tag, int Quantity, Guid? RoomId, string? Notes, string? Reason);

public record AlarmEventDto(long Id, string Code, string Element, DateTime StartedAt, DateTime? ClearedAt, DateTime? AckAt, string? AckBy);
