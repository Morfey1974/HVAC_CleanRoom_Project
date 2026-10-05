using System.Text.Json;
using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Entities;
using HvacConfigurator.Api.Plc;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Services;

public record FirmwareSendResult(bool Ok, string? Error, int? PlcError);

public class FirmwareService(AppDbContext db, AuditService audit, PlcFirmwareClient plc)
{
    private static FirmwareFileDto ToDto(FirmwareFile f) => new(
        f.Id, f.ModuleType, f.BoardRev, f.Version, f.SizeBytes, ((uint)f.Crc32).ToString("x8"),
        f.FileName, f.Notes, f.UploadedAt, f.UploadedBy, f.SentToPlcAt);

    private static string Describe(FirmwareFile f) =>
        $"type {f.ModuleType}, board {f.BoardRev}, v{FirmwareImage.VersionText(f.Version)}, {f.SizeBytes} B, crc {(uint)f.Crc32:x8}";

    public async Task<List<FirmwareFileDto>> ListAsync(CancellationToken ct) =>
        (await db.FirmwareFiles.AsNoTracking().OrderBy(f => f.ModuleType).ThenByDescending(f => f.UploadedAt)
            .Select(f => new FirmwareFile
            {
                Id = f.Id, ModuleType = f.ModuleType, BoardRev = f.BoardRev, Version = f.Version, SizeBytes = f.SizeBytes,
                Crc32 = f.Crc32, FileName = f.FileName, Notes = f.Notes, UploadedAt = f.UploadedAt,
                UploadedBy = f.UploadedBy, SentToPlcAt = f.SentToPlcAt,
            })
            .ToListAsync(ct)).Select(ToDto).ToList();

    /// <summary>Returns the stored file, or an error code (too_small, no_header, bad_type, too_large, bad_elf).</summary>
    public async Task<(FirmwareFileDto? File, string? Error)> UploadAsync(IFormFile file, string? notes, CancellationToken ct)
    {
        using var ms = new MemoryStream();
        await file.CopyToAsync(ms, ct);
        var r = FirmwareImage.Parse(ms.ToArray());
        if (r.Error is not null) return (null, r.Error);

        var f = new FirmwareFile
        {
            ModuleType = r.Header!.ModuleType,
            BoardRev = r.Header.BoardRev,
            Version = r.Header.Version,
            SizeBytes = r.Data!.Length,
            Crc32 = FirmwareImage.Crc32(r.Data),
            FileName = Path.GetFileName(file.FileName),
            Notes = (notes ?? "").Trim(),
            UploadedBy = audit.CurrentLogin,
            Data = r.Data,
        };
        db.FirmwareFiles.Add(f);
        audit.Add("firmware.upload", f.FileName, newValue: Describe(f));
        await db.SaveChangesAsync(ct);
        return (ToDto(f), null);
    }

    public Task<FirmwareFile?> GetAsync(Guid id, CancellationToken ct) =>
        db.FirmwareFiles.AsNoTracking().FirstOrDefaultAsync(f => f.Id == id, ct);

    public async Task<bool> DeleteAsync(Guid id, string? reason, CancellationToken ct)
    {
        var f = await db.FirmwareFiles.FirstOrDefaultAsync(x => x.Id == id, ct);
        if (f is null) return false;
        audit.Add("firmware.delete", f.FileName, oldValue: Describe(f), reason: reason);
        db.FirmwareFiles.Remove(f);
        await db.SaveChangesAsync(ct);
        return true;
    }

    /// <summary>Writes the image into the PLC store (slot "new" of its module type).</summary>
    public async Task<FirmwareSendResult?> SendToPlcAsync(Guid id, string? reason, CancellationToken ct)
    {
        var f = await db.FirmwareFiles.FirstOrDefaultAsync(x => x.Id == id, ct);
        if (f is null) return null;

        FirmwareSendResult result;
        try
        {
            var text = await plc.UploadAsync(f.Data, (uint)f.Crc32, ct);
            using var doc = JsonDocument.Parse(text);
            var root = doc.RootElement;
            result = root.TryGetProperty("ok", out var ok) && ok.GetBoolean()
                ? new FirmwareSendResult(true, null, null)
                : new FirmwareSendResult(false, "plc_rejected", root.TryGetProperty("err", out var e) ? e.GetInt32() : null);
        }
        catch (PlcUnavailableException ex)
        {
            result = new FirmwareSendResult(false, ex.Code, null);
        }
        catch (JsonException)
        {
            result = new FirmwareSendResult(false, "plc_bad_answer", null);
        }

        if (result.Ok) f.SentToPlcAt = DateTime.UtcNow;
        audit.Add(result.Ok ? "firmware.send" : "firmware.send_failed", f.FileName, newValue: Describe(f),
            reason: result.Ok ? reason : $"{result.Error}{(result.PlcError is { } pe ? $" ({pe})" : "")}");
        await db.SaveChangesAsync(ct);
        return result;
    }

    /// <summary>Mode 1: only modules with a different version, 2: all modules of the type.</summary>
    public async Task<FirmwareSendResult> RunAsync(int moduleType, int mode, string? reason, CancellationToken ct)
    {
        FirmwareSendResult result;
        try
        {
            using var doc = JsonDocument.Parse(await plc.RunAsync(moduleType, mode, ct));
            var root = doc.RootElement;
            result = root.TryGetProperty("ok", out var ok) && ok.GetBoolean()
                ? new FirmwareSendResult(true, null, null)
                : new FirmwareSendResult(false, "plc_rejected", root.TryGetProperty("err", out var e) ? e.GetInt32() : null);
        }
        catch (PlcUnavailableException ex) { result = new FirmwareSendResult(false, ex.Code, null); }
        catch (JsonException) { result = new FirmwareSendResult(false, "plc_bad_answer", null); }

        audit.Add(result.Ok ? "firmware.run" : "firmware.run_failed", $"type {moduleType}",
            newValue: mode == 2 ? "all" : "different",
            reason: result.Ok ? reason : $"{result.Error}{(result.PlcError is { } pe ? $" ({pe})" : "")}");
        await db.SaveChangesAsync(ct);
        return result;
    }

    public async Task<FirmwareSendResult> CancelAsync(CancellationToken ct)
    {
        FirmwareSendResult result;
        try
        {
            await plc.CancelAsync(ct);
            result = new FirmwareSendResult(true, null, null);
        }
        catch (PlcUnavailableException ex) { result = new FirmwareSendResult(false, ex.Code, null); }

        audit.Add("firmware.cancel", "plc", reason: result.Error);
        await db.SaveChangesAsync(ct);
        return result;
    }
}
