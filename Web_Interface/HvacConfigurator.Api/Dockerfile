FROM mcr.microsoft.com/dotnet/sdk:9.0 AS build
WORKDIR /src
COPY ["HvacConfigurator.Api/HvacConfigurator.Api.csproj", "HvacConfigurator.Api/"]
RUN dotnet restore "HvacConfigurator.Api/HvacConfigurator.Api.csproj"
COPY HvacConfigurator.Api/ HvacConfigurator.Api/
WORKDIR /src/HvacConfigurator.Api
RUN dotnet publish -c Release -o /app/publish /p:UseAppHost=false

FROM mcr.microsoft.com/dotnet/aspnet:9.0 AS final
WORKDIR /app
COPY --from=build /app/publish .
EXPOSE 8080
ENTRYPOINT ["dotnet", "HvacConfigurator.Api.dll"]
