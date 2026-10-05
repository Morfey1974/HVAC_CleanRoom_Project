using System.Net.Http.Headers;
using HvacConfigurator.Api.Data;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Plc;

public class PlcUnavailableException(string code, Exception? inner = null) : Exception(code, inner)
{
    public string Code { get; } = code;
}

/// <summary>HTTP client for the Main PLC (/api/fw/*, /api/ai). The PLC serves one connection at a time.</summary>
public class PlcFirmwareClient(IServiceScopeFactory scopes)
{
    private static readonly HttpClient Http = new() { Timeout = Timeout.InfiniteTimeSpan };
    private static readonly TimeSpan ShortTimeout = TimeSpan.FromSeconds(4);
    /// <summary>The PLC erases up to 512 KB of external flash while receiving.</summary>
    private static readonly TimeSpan UploadTimeout = TimeSpan.FromSeconds(120);

    public async Task<string?> GetAddressAsync(CancellationToken ct)
    {
        using var scope = scopes.CreateScope();
        var db = scope.ServiceProvider.GetRequiredService<AppDbContext>();
        var s = await db.SystemSettings.AsNoTracking().FirstOrDefaultAsync(ct);
        var a = s?.PlcAddress.Trim() ?? "";
        return a.Length == 0 ? null : a;
    }

    private readonly SemaphoreSlim _gate = new(1, 1);

    /// <summary>True while an image is being sent; status then comes from the last answer.</summary>
    public bool Uploading { get; private set; }

    /// <summary>Null when the PLC is busy with another request (an upload) for longer than the wait.</summary>
    public async Task<string?> GetStatusAsync(long logAfter, CancellationToken ct)
    {
        if (!await _gate.WaitAsync(TimeSpan.FromSeconds(1), ct)) return null;
        try { return LastStatus = await SendAsync(HttpMethod.Get, $"/api/fw/status?log={logAfter}", null, ShortTimeout, ct); }
        finally { _gate.Release(); }
    }

    public string? LastStatus { get; private set; }

    /// <summary>Gateway state and the AI measurement (/api/ai). Null when the PLC is busy with another request.</summary>
    public async Task<string?> GetAiAsync(CancellationToken ct)
    {
        if (!await _gate.WaitAsync(TimeSpan.FromMilliseconds(300), ct)) return null;
        try { return await SendAsync(HttpMethod.Get, "/api/ai", null, TimeSpan.FromSeconds(2), ct); }
        finally { _gate.Release(); }
    }

    public async Task<string> UploadAsync(byte[] image, uint crc, CancellationToken ct)
    {
        var body = new ByteArrayContent(image);
        body.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
        await _gate.WaitAsync(ct);
        Uploading = true;
        try { return await SendAsync(HttpMethod.Post, $"/api/fw/upload?size={image.Length}&crc={crc:x8}", body, UploadTimeout, ct); }
        finally { Uploading = false; _gate.Release(); }
    }

    public Task<string> RunAsync(int moduleType, int mode, CancellationToken ct) =>
        CommandAsync($"/api/fw/run?type={moduleType}&mode={mode}", ct);

    public Task<string> CancelAsync(CancellationToken ct) => CommandAsync("/api/fw/cancel", ct);

    private async Task<string> CommandAsync(string path, CancellationToken ct)
    {
        if (!await _gate.WaitAsync(ShortTimeout, ct)) throw new PlcUnavailableException("plc_busy");
        try { return await SendAsync(HttpMethod.Post, path, new ByteArrayContent([]), ShortTimeout, ct); }
        finally { _gate.Release(); }
    }

    private async Task<string> SendAsync(HttpMethod method, string path, HttpContent? body, TimeSpan timeout, CancellationToken ct)
    {
        var address = await GetAddressAsync(ct) ?? throw new PlcUnavailableException("plc_address_missing");
        var baseUri = address.Contains("://") ? address : "http://" + address;
        if (!Uri.TryCreate(new Uri(baseUri.TrimEnd('/') + "/"), path.TrimStart('/'), out var uri))
            throw new PlcUnavailableException("plc_address_invalid");

        using var cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
        cts.CancelAfter(timeout);
        using var req = new HttpRequestMessage(method, uri) { Content = body };
        req.Headers.ConnectionClose = true;
        try
        {
            using var resp = await Http.SendAsync(req, cts.Token);
            var text = await resp.Content.ReadAsStringAsync(cts.Token);
            if (!resp.IsSuccessStatusCode) throw new PlcUnavailableException($"plc_http_{(int)resp.StatusCode}");
            return text;
        }
        catch (OperationCanceledException ex) when (!ct.IsCancellationRequested)
        {
            throw new PlcUnavailableException("plc_timeout", ex);
        }
        catch (HttpRequestException ex)
        {
            throw new PlcUnavailableException("plc_unreachable", ex);
        }
    }
}
