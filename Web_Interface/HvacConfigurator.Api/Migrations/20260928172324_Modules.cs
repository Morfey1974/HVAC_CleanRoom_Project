using System;
using Microsoft.EntityFrameworkCore.Migrations;

#nullable disable

namespace HvacConfigurator.Api.Migrations
{
    /// <inheritdoc />
    public partial class Modules : Migration
    {
        /// <inheritdoc />
        protected override void Up(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.Sql("""DELETE FROM "LibraryItems" WHERE "Code" IN ('MAIN-PLC','MOD-AI','MOD-HUB-DISP');""");

            migrationBuilder.AddColumn<int>(
                name: "ChannelCount",
                table: "LibraryItems",
                type: "integer",
                nullable: true);

            migrationBuilder.AddColumn<string>(
                name: "SystemPrefix",
                table: "LibraryItems",
                type: "character varying(8)",
                maxLength: 8,
                nullable: false,
                defaultValue: "");

            migrationBuilder.AddColumn<int>(
                name: "TypeCode",
                table: "LibraryItems",
                type: "integer",
                nullable: true);

            migrationBuilder.CreateTable(
                name: "ProjectModules",
                columns: table => new
                {
                    Id = table.Column<Guid>(type: "uuid", nullable: false),
                    ProjectId = table.Column<Guid>(type: "uuid", nullable: false),
                    LibraryItemId = table.Column<Guid>(type: "uuid", nullable: true),
                    LibraryVersion = table.Column<int>(type: "integer", nullable: false),
                    SnapshotJson = table.Column<string>(type: "text", nullable: false),
                    Line = table.Column<int>(type: "integer", nullable: false),
                    Rail = table.Column<int>(type: "integer", nullable: false),
                    Place = table.Column<int>(type: "integer", nullable: false),
                    NameRu = table.Column<string>(type: "character varying(200)", maxLength: 200, nullable: false),
                    NameEn = table.Column<string>(type: "character varying(200)", maxLength: 200, nullable: false),
                    NameHe = table.Column<string>(type: "character varying(200)", maxLength: 200, nullable: false),
                    ExpectedSerial = table.Column<string>(type: "character varying(16)", maxLength: 16, nullable: false),
                    Revision = table.Column<string>(type: "character varying(8)", maxLength: 8, nullable: false),
                    Notes = table.Column<string>(type: "character varying(1000)", maxLength: 1000, nullable: false),
                    UpdatedAt = table.Column<DateTime>(type: "timestamp with time zone", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_ProjectModules", x => x.Id);
                    table.ForeignKey(
                        name: "FK_ProjectModules_LibraryItems_LibraryItemId",
                        column: x => x.LibraryItemId,
                        principalTable: "LibraryItems",
                        principalColumn: "Id",
                        onDelete: ReferentialAction.SetNull);
                    table.ForeignKey(
                        name: "FK_ProjectModules_Projects_ProjectId",
                        column: x => x.ProjectId,
                        principalTable: "Projects",
                        principalColumn: "Id",
                        onDelete: ReferentialAction.Cascade);
                });

            migrationBuilder.CreateIndex(
                name: "IX_ProjectModules_LibraryItemId",
                table: "ProjectModules",
                column: "LibraryItemId");

            migrationBuilder.CreateIndex(
                name: "IX_ProjectModules_ProjectId_Rail_Place",
                table: "ProjectModules",
                columns: new[] { "ProjectId", "Rail", "Place" });
        }

        /// <inheritdoc />
        protected override void Down(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.DropTable(
                name: "ProjectModules");

            migrationBuilder.DropColumn(
                name: "ChannelCount",
                table: "LibraryItems");

            migrationBuilder.DropColumn(
                name: "SystemPrefix",
                table: "LibraryItems");

            migrationBuilder.DropColumn(
                name: "TypeCode",
                table: "LibraryItems");
        }
    }
}
