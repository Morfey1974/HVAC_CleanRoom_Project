using System;
using HvacConfigurator.Api.Services;
using Microsoft.EntityFrameworkCore.Migrations;

#nullable disable

namespace HvacConfigurator.Api.Migrations
{
    /// <inheritdoc />
    public partial class Roles : Migration
    {
        /// <inheritdoc />
        protected override void Up(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.CreateTable(
                name: "Roles",
                columns: table => new
                {
                    Id = table.Column<Guid>(type: "uuid", nullable: false),
                    Code = table.Column<string>(type: "character varying(64)", maxLength: 64, nullable: false),
                    NameRu = table.Column<string>(type: "character varying(120)", maxLength: 120, nullable: false),
                    NameEn = table.Column<string>(type: "character varying(120)", maxLength: 120, nullable: false),
                    NameHe = table.Column<string>(type: "character varying(120)", maxLength: 120, nullable: false),
                    Description = table.Column<string>(type: "character varying(500)", maxLength: 500, nullable: false),
                    IsBuiltIn = table.Column<bool>(type: "boolean", nullable: false),
                    IsArchived = table.Column<bool>(type: "boolean", nullable: false),
                    PermissionsJson = table.Column<string>(type: "text", nullable: false),
                    UpdatedAt = table.Column<DateTime>(type: "timestamp with time zone", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_Roles", x => x.Id);
                });

            migrationBuilder.CreateIndex(
                name: "IX_Roles_Code",
                table: "Roles",
                column: "Code",
                unique: true);

            InsertRole(migrationBuilder, Perm.ViewerId, "Viewer", "Просмотр", "View", "צפייה", "Только смотреть");
            InsertRole(migrationBuilder, Perm.OperatorId, "Operator", "Оператор", "Operator", "מפעיל", "Квитирование аварий и уставки");
            InsertRole(migrationBuilder, Perm.EngineerId, "Engineer", "Инженер", "Engineer", "מהנדס", "Конфигуратор, библиотеки, загрузка в ПЛК");
            InsertRole(migrationBuilder, Perm.AdminId, "Admin", "Администратор", "Administrator", "מנהל", "Пользователи, роли, настройки системы");

            migrationBuilder.AddColumn<Guid>(
                name: "RoleId",
                table: "Users",
                type: "uuid",
                nullable: true);

            migrationBuilder.Sql(
                """
                UPDATE "Users" SET "RoleId" = CASE "Role"
                  WHEN 'Viewer' THEN '11111111-1111-4111-8111-111111111111'::uuid
                  WHEN 'Operator' THEN '22222222-2222-4222-8222-222222222222'::uuid
                  WHEN 'Engineer' THEN '33333333-3333-4333-8333-333333333333'::uuid
                  ELSE '44444444-4444-4444-8444-444444444444'::uuid
                END;
                """);

            migrationBuilder.DropColumn(
                name: "Role",
                table: "Users");

            migrationBuilder.AlterColumn<Guid>(
                name: "RoleId",
                table: "Users",
                type: "uuid",
                nullable: false);

            migrationBuilder.CreateIndex(
                name: "IX_Users_RoleId",
                table: "Users",
                column: "RoleId");

            migrationBuilder.AddForeignKey(
                name: "FK_Users_Roles_RoleId",
                table: "Users",
                column: "RoleId",
                principalTable: "Roles",
                principalColumn: "Id",
                onDelete: ReferentialAction.Restrict);
        }

        /// <inheritdoc />
        protected override void Down(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.AddColumn<string>(
                name: "Role",
                table: "Users",
                type: "character varying(16)",
                maxLength: 16,
                nullable: false,
                defaultValue: "Viewer");

            migrationBuilder.Sql(
                """
                UPDATE "Users" u SET "Role" = LEFT(r."Code", 16)
                FROM "Roles" r
                WHERE r."Id" = u."RoleId";
                """);

            migrationBuilder.DropForeignKey(
                name: "FK_Users_Roles_RoleId",
                table: "Users");

            migrationBuilder.DropIndex(
                name: "IX_Users_RoleId",
                table: "Users");

            migrationBuilder.DropColumn(
                name: "RoleId",
                table: "Users");

            migrationBuilder.DropTable(
                name: "Roles");
        }

        private static void InsertRole(MigrationBuilder migrationBuilder, Guid id, string code, string ru, string en, string he, string desc) =>
            migrationBuilder.InsertData(
                table: "Roles",
                columns: ["Id", "Code", "NameRu", "NameEn", "NameHe", "Description", "IsBuiltIn", "IsArchived", "PermissionsJson", "UpdatedAt"],
                values: [id, code, ru, en, he, desc, true, false, Localized.ToJson(Perm.ForBuiltIn(code)), DateTime.UtcNow]);
    }
}
