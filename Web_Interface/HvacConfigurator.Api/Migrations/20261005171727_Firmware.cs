using System;
using Microsoft.EntityFrameworkCore.Migrations;

#nullable disable

namespace HvacConfigurator.Api.Migrations
{
    /// <inheritdoc />
    public partial class Firmware : Migration
    {
        /// <inheritdoc />
        protected override void Up(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.CreateTable(
                name: "FirmwareFiles",
                columns: table => new
                {
                    Id = table.Column<Guid>(type: "uuid", nullable: false),
                    ModuleType = table.Column<int>(type: "integer", nullable: false),
                    BoardRev = table.Column<int>(type: "integer", nullable: false),
                    Version = table.Column<int>(type: "integer", nullable: false),
                    SizeBytes = table.Column<long>(type: "bigint", nullable: false),
                    Crc32 = table.Column<long>(type: "bigint", nullable: false),
                    FileName = table.Column<string>(type: "character varying(260)", maxLength: 260, nullable: false),
                    Notes = table.Column<string>(type: "character varying(1000)", maxLength: 1000, nullable: false),
                    UploadedAt = table.Column<DateTime>(type: "timestamp with time zone", nullable: false),
                    UploadedBy = table.Column<string>(type: "character varying(64)", maxLength: 64, nullable: false),
                    SentToPlcAt = table.Column<DateTime>(type: "timestamp with time zone", nullable: true),
                    Data = table.Column<byte[]>(type: "bytea", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_FirmwareFiles", x => x.Id);
                });

            migrationBuilder.CreateIndex(
                name: "IX_FirmwareFiles_ModuleType_UploadedAt",
                table: "FirmwareFiles",
                columns: new[] { "ModuleType", "UploadedAt" });
        }

        /// <inheritdoc />
        protected override void Down(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.DropTable(
                name: "FirmwareFiles");
        }
    }
}
