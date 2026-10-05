using Microsoft.EntityFrameworkCore.Migrations;

#nullable disable

namespace HvacConfigurator.Api.Migrations
{
    /// <inheritdoc />
    public partial class SensorOutputs : Migration
    {
        /// <inheritdoc />
        protected override void Up(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.AddColumn<string>(
                name: "OutputsJson",
                table: "LibraryItems",
                type: "text",
                nullable: false,
                defaultValue: "[]");
        }

        /// <inheritdoc />
        protected override void Down(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.DropColumn(
                name: "OutputsJson",
                table: "LibraryItems");
        }
    }
}
