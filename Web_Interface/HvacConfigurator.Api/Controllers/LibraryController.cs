using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Entities;
using HvacConfigurator.Api.Services;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;

namespace HvacConfigurator.Api.Controllers;

[ApiController]
[Authorize]
[Route("api/library")]
public class LibraryController(LibraryService library) : ControllerBase
{
    [HttpGet]
    public async Task<IActionResult> List([FromQuery] string? category, [FromQuery] bool archived = false, CancellationToken ct = default) =>
        Ok(await library.ListAsync(category, archived, ct));

    [HttpGet("{id:guid}")]
    public async Task<IActionResult> Get(Guid id, CancellationToken ct) =>
        await library.GetAsync(id, ct) is { } i ? Ok(i) : NotFound();

    [HttpPost]
    [Authorize(Roles = "Engineer,Admin")]
    public async Task<IActionResult> Create([FromBody] LibraryItemSaveRequest req, CancellationToken ct)
    {
        var (dto, err) = await library.CreateAsync(req, ct);
        return err is null ? Ok(dto) : BadRequest(new { message = err });
    }

    [HttpPut("{id:guid}")]
    [Authorize(Roles = "Engineer,Admin")]
    public async Task<IActionResult> Update(Guid id, [FromBody] LibraryItemSaveRequest req, CancellationToken ct)
    {
        var (dto, err) = await library.UpdateAsync(id, req, ct);
        return err switch { null => Ok(dto), "not_found" => NotFound(), _ => BadRequest(new { message = err }) };
    }

    [HttpPost("{id:guid}/archive")]
    [Authorize(Roles = "Engineer,Admin")]
    public async Task<IActionResult> Archive(Guid id, [FromQuery] bool value = true, [FromQuery] string? reason = null, CancellationToken ct = default) =>
        await library.SetArchivedAsync(id, value, reason, ct) is { } i ? Ok(i) : NotFound();
}
