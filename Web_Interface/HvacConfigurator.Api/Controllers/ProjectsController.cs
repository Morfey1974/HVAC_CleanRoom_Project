using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Entities;
using HvacConfigurator.Api.Services;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;

namespace HvacConfigurator.Api.Controllers;

[ApiController]
[Authorize]
[Route("api/projects")]
public class ProjectsController(ProjectService projects, RoomService rooms, DocumentService docs, EquipmentService equipment, ModuleService modules) : ControllerBase
{
    private const string Editors = "Engineer,Admin";
    private const long MaxUploadBytes = 200L * 1024 * 1024;

    // ---- Projects ----

    [HttpGet]
    public async Task<IActionResult> List([FromQuery] bool archived = false, CancellationToken ct = default) =>
        Ok(await projects.ListAsync(archived, ct));

    [HttpGet("{id:guid}")]
    public async Task<IActionResult> Get(Guid id, CancellationToken ct) =>
        await projects.GetAsync(id, ct) is { } p ? Ok(p) : NotFound();

    [HttpPost]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> Create([FromBody] ProjectSaveRequest req, CancellationToken ct) =>
        !req.Name.HasAny() ? BadRequest(new { message = "name_required" }) : Ok(await projects.CreateAsync(req, ct));

    [HttpPut("{id:guid}")]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> Update(Guid id, [FromBody] ProjectSaveRequest req, CancellationToken ct)
    {
        if (!req.Name.HasAny()) return BadRequest(new { message = "name_required" });
        return await projects.UpdateAsync(id, req, ct) is { } p ? Ok(p) : NotFound();
    }

    [HttpPost("{id:guid}/archive")]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> Archive(Guid id, [FromQuery] bool value = true, [FromQuery] string? reason = null, CancellationToken ct = default) =>
        await projects.SetArchivedAsync(id, value, reason, ct) is { } p ? Ok(p) : NotFound();

    [HttpPost("{id:guid}/activate")]
    [Authorize(Roles = "Admin")]
    public async Task<IActionResult> Activate(Guid id, [FromQuery] string? reason = null, CancellationToken ct = default) =>
        await projects.SetActiveOnSiteAsync(id, reason, ct) is { } p ? Ok(p) : NotFound();

    [HttpPost("{id:guid}/copy")]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> Copy(Guid id, [FromQuery] string? reason = null, CancellationToken ct = default) =>
        await projects.CopyAsync(id, reason, ct) is { } p ? Ok(p) : NotFound();

    // ---- Rooms ----

    [HttpGet("{pid:guid}/rooms")]
    public async Task<IActionResult> Rooms(Guid pid, CancellationToken ct) =>
        await projects.ExistsAsync(pid, ct) ? Ok(await rooms.ListAsync(pid, ct)) : NotFound();

    [HttpPost("{pid:guid}/rooms")]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> CreateRoom(Guid pid, [FromBody] RoomSaveRequest req, CancellationToken ct)
    {
        if (!req.Name.HasAny()) return BadRequest(new { message = "name_required" });
        if (!await projects.ExistsAsync(pid, ct)) return NotFound();
        return Ok(await rooms.CreateAsync(pid, req, ct));
    }

    [HttpPut("{pid:guid}/rooms/{id:guid}")]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> UpdateRoom(Guid pid, Guid id, [FromBody] RoomSaveRequest req, CancellationToken ct)
    {
        if (!req.Name.HasAny()) return BadRequest(new { message = "name_required" });
        return await rooms.UpdateAsync(pid, id, req, ct) is { } r ? Ok(r) : NotFound();
    }

    [HttpDelete("{pid:guid}/rooms/{id:guid}")]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> DeleteRoom(Guid pid, Guid id, [FromQuery] string? reason, CancellationToken ct) =>
        await rooms.DeleteAsync(pid, id, reason, ct) ? NoContent() : NotFound();

    // ---- Documents ----

    [HttpGet("{pid:guid}/documents")]
    public async Task<IActionResult> Documents(Guid pid, CancellationToken ct) =>
        await projects.ExistsAsync(pid, ct) ? Ok(await docs.ListAsync(pid, ct)) : NotFound();

    [HttpPost("{pid:guid}/documents")]
    [Authorize(Roles = Editors)]
    [RequestSizeLimit(MaxUploadBytes)]
    [RequestFormLimits(MultipartBodyLengthLimit = MaxUploadBytes)]
    public async Task<IActionResult> Upload(Guid pid, [FromForm] string kind, IFormFile file, CancellationToken ct)
    {
        if (!DocumentKinds.All.Contains(kind)) return BadRequest(new { message = "bad_kind" });
        if (file is null || file.Length == 0) return BadRequest(new { message = "file_required" });
        if (!await projects.ExistsAsync(pid, ct)) return NotFound();
        return Ok(await docs.UploadAsync(pid, kind, file, ct));
    }

    [HttpGet("{pid:guid}/documents/{id:guid}/file")]
    public async Task<IActionResult> Download(Guid pid, Guid id, CancellationToken ct) =>
        await docs.GetFileAsync(pid, id, ct) is { } d ? File(d.Data, d.ContentType, d.FileName) : NotFound();

    [HttpDelete("{pid:guid}/documents/{id:guid}")]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> DeleteDocument(Guid pid, Guid id, [FromQuery] string? reason, CancellationToken ct) =>
        await docs.DeleteAsync(pid, id, reason, ct) ? NoContent() : NotFound();

    // ---- Hardware modules ----

    [HttpGet("{pid:guid}/modules")]
    public async Task<IActionResult> Modules(Guid pid, CancellationToken ct) =>
        await projects.ExistsAsync(pid, ct) ? Ok(await modules.ListAsync(pid, ct)) : NotFound();

    [HttpPost("{pid:guid}/modules")]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> CreateModule(Guid pid, [FromBody] ProjectModuleSaveRequest req, CancellationToken ct)
    {
        if (!await projects.ExistsAsync(pid, ct)) return NotFound();
        var (dto, err) = await modules.SaveAsync(pid, null, req, ct);
        return err is null ? Ok(dto) : BadRequest(new { message = err });
    }

    [HttpPut("{pid:guid}/modules/{id:guid}")]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> UpdateModule(Guid pid, Guid id, [FromBody] ProjectModuleSaveRequest req, CancellationToken ct)
    {
        var (dto, err) = await modules.SaveAsync(pid, id, req, ct);
        return err switch { null => Ok(dto), "not_found" => NotFound(), _ => BadRequest(new { message = err }) };
    }

    [HttpDelete("{pid:guid}/modules/{id:guid}")]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> DeleteModule(Guid pid, Guid id, [FromQuery] string? reason, CancellationToken ct)
    {
        var (ok, err) = await modules.DeleteAsync(pid, id, reason, ct);
        return ok ? NoContent() : err == "not_found" ? NotFound() : BadRequest(new { message = err });
    }

    [HttpPost("{pid:guid}/modules/{id:guid}/update-from-library")]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> UpdateModuleFromLibrary(Guid pid, Guid id, [FromQuery] string? reason, CancellationToken ct)
    {
        var (dto, err) = await modules.UpdateFromLibraryAsync(pid, id, reason, ct);
        return err switch { null => Ok(dto), "not_found" => NotFound(), _ => BadRequest(new { message = err }) };
    }

    // ---- System scheme ----

    [HttpGet("{pid:guid}/scheme")]
    public async Task<IActionResult> Scheme(Guid pid, CancellationToken ct) =>
        await projects.ExistsAsync(pid, ct) ? Ok(await modules.SchemeAsync(pid, ct)) : NotFound();

    [HttpPost("{pid:guid}/scheme/modules")]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> CreateSchemeModule(Guid pid, [FromBody] SchemeModuleCreateRequest req, CancellationToken ct)
    {
        if (!await projects.ExistsAsync(pid, ct)) return NotFound();
        var (dto, err) = await modules.CreateOnSchemeAsync(pid, req, ct);
        return err is null ? Ok(dto) : BadRequest(new { message = err });
    }

    [HttpPut("{pid:guid}/scheme/modules/{id:guid}/position")]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> MoveSchemeModule(Guid pid, Guid id, [FromBody] SchemePositionRequest req, CancellationToken ct) =>
        await modules.MoveAsync(pid, id, req, ct) ? NoContent() : NotFound();

    [HttpPost("{pid:guid}/scheme/links")]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> CreateLink(Guid pid, [FromBody] ProjectLinkSaveRequest req, CancellationToken ct)
    {
        var (dto, err) = await modules.CreateLinkAsync(pid, req, ct);
        return err switch { null => Ok(dto), "not_found" => NotFound(), _ => BadRequest(new { message = err }) };
    }

    [HttpPut("{pid:guid}/scheme/links/{id:guid}/route")]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> SaveLinkRoute(Guid pid, Guid id, [FromBody] LinkRouteRequest req, CancellationToken ct)
    {
        var (dto, err) = await modules.SaveRouteAsync(pid, id, req, ct);
        return err switch { null => Ok(dto), "not_found" => NotFound(), _ => BadRequest(new { message = err }) };
    }

    [HttpDelete("{pid:guid}/scheme/links/{id:guid}")]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> DeleteLink(Guid pid, Guid id, [FromQuery] string? reason, CancellationToken ct) =>
        await modules.DeleteLinkAsync(pid, id, reason, ct) ? NoContent() : NotFound();

    // ---- Equipment ----

    [HttpGet("{pid:guid}/equipment")]
    public async Task<IActionResult> Equipment(Guid pid, CancellationToken ct) =>
        await projects.ExistsAsync(pid, ct) ? Ok(await equipment.ListAsync(pid, ct)) : NotFound();

    [HttpPost("{pid:guid}/equipment")]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> CreateEquipment(Guid pid, [FromBody] ProjectEquipmentSaveRequest req, CancellationToken ct)
    {
        if (!await projects.ExistsAsync(pid, ct)) return NotFound();
        var (dto, err) = await equipment.CreateAsync(pid, req, ct);
        return err is null ? Ok(dto) : BadRequest(new { message = err });
    }

    [HttpPut("{pid:guid}/equipment/{id:guid}")]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> UpdateEquipment(Guid pid, Guid id, [FromBody] ProjectEquipmentSaveRequest req, CancellationToken ct)
    {
        var (dto, err) = await equipment.UpdateAsync(pid, id, req, ct);
        return err switch { null => Ok(dto), "not_found" => NotFound(), _ => BadRequest(new { message = err }) };
    }

    [HttpPost("{pid:guid}/equipment/{id:guid}/update-from-library")]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> UpdateFromLibrary(Guid pid, Guid id, [FromQuery] string? reason, CancellationToken ct) =>
        await equipment.UpdateFromLibraryAsync(pid, id, reason, ct) is { } e ? Ok(e) : NotFound();

    [HttpDelete("{pid:guid}/equipment/{id:guid}")]
    [Authorize(Roles = Editors)]
    public async Task<IActionResult> DeleteEquipment(Guid pid, Guid id, [FromQuery] string? reason, CancellationToken ct) =>
        await equipment.DeleteAsync(pid, id, reason, ct) ? NoContent() : NotFound();
}
