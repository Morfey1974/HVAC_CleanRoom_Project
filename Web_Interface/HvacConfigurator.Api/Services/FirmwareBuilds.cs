namespace HvacConfigurator.Api.Services;

/// <summary>
/// Build output (.elf) of every module application in the repository the app runs from.
/// Only works on the development computer; elsewhere nothing is found.
/// </summary>
public static class FirmwareBuilds
{
    /// <summary>Firmware module type (fwupd_proto.h) -> application project folder from the repository root.</summary>
    private static readonly (int Type, string Folder)[] Projects =
    [
        (2, "Firmware_Locomotive/Application"),
        (3, "Firmware_HUB_Displays/Application"),
        (4, "Firmware_Displays/Display_TFT4.3/Application"),
        (5, "Firmware_Wagons/Analog_Modul_AI/Application"),
    ];

    private static string? _root;

    /// <summary>Repository root: first parent of the app folder that holds the module projects.</summary>
    private static string? Root()
    {
        if (_root is not null) return _root;
        for (var d = new DirectoryInfo(AppContext.BaseDirectory); d is not null; d = d.Parent)
        {
            if (Directory.Exists(Path.Combine(d.FullName, "Firmware_MainPLC")) &&
                Directory.Exists(Path.Combine(d.FullName, "Shared_Libs")))
                return _root = d.FullName;
        }
        return null;
    }

    /// <summary>Newest .elf in the Debug folder of each module project.</summary>
    public static IEnumerable<(int Type, string Path)> Find()
    {
        var root = Root();
        if (root is null) yield break;
        foreach (var (type, folder) in Projects)
        {
            var debug = Path.Combine(root, folder, "Debug");
            if (!Directory.Exists(debug)) continue;
            var elf = new DirectoryInfo(debug).GetFiles("*.elf").OrderByDescending(f => f.LastWriteTimeUtc).FirstOrDefault();
            if (elf is not null) yield return (type, elf.FullName);
        }
    }
}
