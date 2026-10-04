using System;
using Microsoft.EntityFrameworkCore.Migrations;

#nullable disable

namespace HvacConfigurator.Api.Migrations
{
    /// <inheritdoc />
    public partial class Scheme : Migration
    {
        /// <inheritdoc />
        protected override void Up(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.AddColumn<double>(
                name: "SchemeX",
                table: "ProjectModules",
                type: "double precision",
                nullable: false,
                defaultValue: 0.0);

            migrationBuilder.AddColumn<double>(
                name: "SchemeY",
                table: "ProjectModules",
                type: "double precision",
                nullable: false,
                defaultValue: 0.0);

            migrationBuilder.AddColumn<string>(
                name: "GraphicJson",
                table: "LibraryItems",
                type: "text",
                nullable: false,
                defaultValue: "");

            migrationBuilder.CreateTable(
                name: "ProjectLinks",
                columns: table => new
                {
                    Id = table.Column<Guid>(type: "uuid", nullable: false),
                    ProjectId = table.Column<Guid>(type: "uuid", nullable: false),
                    FromModuleId = table.Column<Guid>(type: "uuid", nullable: false),
                    FromPort = table.Column<string>(type: "character varying(32)", maxLength: 32, nullable: false),
                    ToModuleId = table.Column<Guid>(type: "uuid", nullable: false),
                    ToPort = table.Column<string>(type: "character varying(32)", maxLength: 32, nullable: false),
                    CreatedAt = table.Column<DateTime>(type: "timestamp with time zone", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_ProjectLinks", x => x.Id);
                    table.ForeignKey(
                        name: "FK_ProjectLinks_ProjectModules_FromModuleId",
                        column: x => x.FromModuleId,
                        principalTable: "ProjectModules",
                        principalColumn: "Id",
                        onDelete: ReferentialAction.Cascade);
                    table.ForeignKey(
                        name: "FK_ProjectLinks_ProjectModules_ToModuleId",
                        column: x => x.ToModuleId,
                        principalTable: "ProjectModules",
                        principalColumn: "Id",
                        onDelete: ReferentialAction.Cascade);
                    table.ForeignKey(
                        name: "FK_ProjectLinks_Projects_ProjectId",
                        column: x => x.ProjectId,
                        principalTable: "Projects",
                        principalColumn: "Id",
                        onDelete: ReferentialAction.Cascade);
                });

            migrationBuilder.CreateIndex(
                name: "IX_ProjectLinks_FromModuleId",
                table: "ProjectLinks",
                column: "FromModuleId");

            migrationBuilder.CreateIndex(
                name: "IX_ProjectLinks_ProjectId",
                table: "ProjectLinks",
                column: "ProjectId");

            migrationBuilder.CreateIndex(
                name: "IX_ProjectLinks_ToModuleId",
                table: "ProjectLinks",
                column: "ToModuleId");
        }

        /// <inheritdoc />
        protected override void Down(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.DropTable(
                name: "ProjectLinks");

            migrationBuilder.DropColumn(
                name: "SchemeX",
                table: "ProjectModules");

            migrationBuilder.DropColumn(
                name: "SchemeY",
                table: "ProjectModules");

            migrationBuilder.DropColumn(
                name: "GraphicJson",
                table: "LibraryItems");
        }
    }
}
