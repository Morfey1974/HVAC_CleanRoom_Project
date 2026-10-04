using System.Text.Json;
using HvacConfigurator.Api.Dto;
using HvacConfigurator.Api.Entities;

namespace HvacConfigurator.Api.Services;

public static class Localized
{
    public static readonly JsonSerializerOptions Json = new(JsonSerializerDefaults.Web);

    public static LocalizedTextDto ToDto(this LocalizedText t) => new(t.Ru, t.En, t.He);

    public static LocalizedText ToEntity(this LocalizedTextDto? t) =>
        new() { Ru = t?.Ru?.Trim() ?? "", En = t?.En?.Trim() ?? "", He = t?.He?.Trim() ?? "" };

    public static bool HasAny(this LocalizedTextDto? t) =>
        t is not null && (!string.IsNullOrWhiteSpace(t.Ru) || !string.IsNullOrWhiteSpace(t.En) || !string.IsNullOrWhiteSpace(t.He));

    public static string Label(this LocalizedText t, string fallback) =>
        new[] { t.Ru, t.En, t.He }.FirstOrDefault(s => !string.IsNullOrWhiteSpace(s)) ?? fallback;

    public static string ToJson(object o) => JsonSerializer.Serialize(o, Json);
}
