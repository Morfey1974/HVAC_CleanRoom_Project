using HvacConfigurator.Api.Data;
using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Entities;
using Microsoft.EntityFrameworkCore;

namespace HvacConfigurator.Api.Services;

public class DocumentService(AppDbContext db, AuditService audit)
{
    public Task<List<ProjectDocumentDto>> ListAsync(Guid projectId, CancellationToken ct) =>
        db.ProjectDocuments.AsNoTracking().Where(d => d.ProjectId == projectId)
            .OrderBy(d => d.Kind).ThenByDescending(d => d.UploadedAt)
            .Select(d => new ProjectDocumentDto(d.Id, d.Kind, d.FileName, d.ContentType, d.SizeBytes, d.UploadedAt, d.UploadedBy))
            .ToListAsync(ct);

    public async Task<ProjectDocumentDto> UploadAsync(Guid projectId, string kind, IFormFile file, CancellationToken ct)
    {
        using var ms = new MemoryStream();
        await file.CopyToAsync(ms, ct);
        var d = new ProjectDocument
        {
            ProjectId = projectId,
            Kind = kind,
            FileName = Path.GetFileName(file.FileName),
            ContentType = string.IsNullOrWhiteSpace(file.ContentType) ? "application/octet-stream" : file.ContentType,
            SizeBytes = ms.Length,
            Data = ms.ToArray(),
            UploadedBy = audit.CurrentLogin,
        };
        db.ProjectDocuments.Add(d);
        audit.Add("document.upload", d.FileName, newValue: $"{kind}, {d.SizeBytes} B", projectId: projectId);
        await db.SaveChangesAsync(ct);
        return new ProjectDocumentDto(d.Id, d.Kind, d.FileName, d.ContentType, d.SizeBytes, d.UploadedAt, d.UploadedBy);
    }

    public Task<ProjectDocument?> GetFileAsync(Guid projectId, Guid id, CancellationToken ct) =>
        db.ProjectDocuments.AsNoTracking().FirstOrDefaultAsync(d => d.Id == id && d.ProjectId == projectId, ct);

    public async Task<bool> DeleteAsync(Guid projectId, Guid id, string? reason, CancellationToken ct)
    {
        var d = await db.ProjectDocuments.FirstOrDefaultAsync(x => x.Id == id && x.ProjectId == projectId, ct);
        if (d is null) return false;
        audit.Add("document.delete", d.FileName, oldValue: $"{d.Kind}, {d.SizeBytes} B", reason: reason, projectId: projectId);
        db.ProjectDocuments.Remove(d);
        await db.SaveChangesAsync(ct);
        return true;
    }
}
