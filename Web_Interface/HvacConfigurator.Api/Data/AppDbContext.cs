using HvacConfigurator.Api.Entities;
using Microsoft.EntityFrameworkCore;
using Microsoft.EntityFrameworkCore.Metadata.Builders;

namespace HvacConfigurator.Api.Data;

public class AppDbContext(DbContextOptions<AppDbContext> options) : DbContext(options)
{
    public DbSet<User> Users => Set<User>();
    public DbSet<Project> Projects => Set<Project>();
    public DbSet<Room> Rooms => Set<Room>();
    public DbSet<ProjectDocument> ProjectDocuments => Set<ProjectDocument>();
    public DbSet<LibraryItem> LibraryItems => Set<LibraryItem>();
    public DbSet<ProjectEquipment> ProjectEquipment => Set<ProjectEquipment>();
    public DbSet<ProjectModule> ProjectModules => Set<ProjectModule>();
    public DbSet<ProjectLink> ProjectLinks => Set<ProjectLink>();
    public DbSet<AuditEntry> AuditEntries => Set<AuditEntry>();
    public DbSet<AlarmEvent> AlarmEvents => Set<AlarmEvent>();
    public DbSet<SystemSettings> SystemSettings => Set<SystemSettings>();

    private static void LocalizedName<T>(OwnedNavigationBuilder<T, LocalizedText> n, int max) where T : class
    {
        n.Property(p => p.Ru).HasColumnName("NameRu").HasMaxLength(max);
        n.Property(p => p.En).HasColumnName("NameEn").HasMaxLength(max);
        n.Property(p => p.He).HasColumnName("NameHe").HasMaxLength(max);
    }

    protected override void OnModelCreating(ModelBuilder b)
    {
        b.Entity<User>(e =>
        {
            e.HasIndex(x => x.Login).IsUnique();
            e.Property(x => x.Login).HasMaxLength(64);
            e.Property(x => x.FullName).HasMaxLength(128);
            e.Property(x => x.Role).HasConversion<string>().HasMaxLength(16);
        });

        b.Entity<Project>(e =>
        {
            e.OwnsOne(x => x.Name, n => LocalizedName(n, 200));
            e.Property(x => x.Number).HasMaxLength(64);
            e.Property(x => x.Customer).HasMaxLength(200);
            e.Property(x => x.Address).HasMaxLength(300);
            e.Property(x => x.Responsible).HasMaxLength(128);
            e.Property(x => x.Description).HasMaxLength(4000);
            e.Property(x => x.CreatedBy).HasMaxLength(64);
            e.HasMany(x => x.Rooms).WithOne().HasForeignKey(x => x.ProjectId).OnDelete(DeleteBehavior.Cascade);
            e.HasMany(x => x.Documents).WithOne().HasForeignKey(x => x.ProjectId).OnDelete(DeleteBehavior.Cascade);
            e.HasMany(x => x.Equipment).WithOne().HasForeignKey(x => x.ProjectId).OnDelete(DeleteBehavior.Cascade);
            e.HasMany(x => x.Modules).WithOne().HasForeignKey(x => x.ProjectId).OnDelete(DeleteBehavior.Cascade);
        });

        b.Entity<ProjectModule>(e =>
        {
            e.HasIndex(x => new { x.ProjectId, x.Rail, x.Place });
            e.OwnsOne(x => x.UserName, n => LocalizedName(n, 200));
            e.Property(x => x.ExpectedSerial).HasMaxLength(16);
            e.Property(x => x.Revision).HasMaxLength(8);
            e.Property(x => x.Notes).HasMaxLength(1000);
            e.HasOne<LibraryItem>().WithMany().HasForeignKey(x => x.LibraryItemId).OnDelete(DeleteBehavior.SetNull);
        });

        b.Entity<ProjectLink>(e =>
        {
            e.HasIndex(x => x.ProjectId);
            e.Property(x => x.FromPort).HasMaxLength(32);
            e.Property(x => x.ToPort).HasMaxLength(32);
            e.HasOne<Project>().WithMany().HasForeignKey(x => x.ProjectId).OnDelete(DeleteBehavior.Cascade);
            e.HasOne<ProjectModule>().WithMany().HasForeignKey(x => x.FromModuleId).OnDelete(DeleteBehavior.Cascade);
            e.HasOne<ProjectModule>().WithMany().HasForeignKey(x => x.ToModuleId).OnDelete(DeleteBehavior.Cascade);
        });

        b.Entity<Room>(e =>
        {
            e.OwnsOne(x => x.Name, n => LocalizedName(n, 128));
            e.Property(x => x.AreaM2).HasPrecision(8, 2);
            e.Property(x => x.HeightM).HasPrecision(6, 2);
            e.Property(x => x.TempSetpointC).HasPrecision(5, 2);
            e.Property(x => x.TempToleranceC).HasPrecision(5, 2);
            e.Property(x => x.RhSetpointPct).HasPrecision(5, 2);
            e.Property(x => x.RhTolerancePct).HasPrecision(5, 2);
            e.Property(x => x.PressureSetpointPa).HasPrecision(7, 2);
            e.Property(x => x.PressureTolerancePa).HasPrecision(7, 2);
        });

        b.Entity<ProjectDocument>(e =>
        {
            e.HasIndex(x => new { x.ProjectId, x.Kind });
            e.Property(x => x.Kind).HasMaxLength(16);
            e.Property(x => x.FileName).HasMaxLength(260);
            e.Property(x => x.ContentType).HasMaxLength(128);
            e.Property(x => x.UploadedBy).HasMaxLength(64);
        });

        b.Entity<LibraryItem>(e =>
        {
            e.HasIndex(x => x.Category);
            e.OwnsOne(x => x.Name, n => LocalizedName(n, 200));
            e.Property(x => x.Category).HasMaxLength(24);
            e.Property(x => x.Code).HasMaxLength(64);
            e.Property(x => x.SystemPrefix).HasMaxLength(8);
            e.Property(x => x.Manufacturer).HasMaxLength(128);
            e.Property(x => x.Model).HasMaxLength(128);
            e.Property(x => x.Description).HasMaxLength(4000);
        });

        b.Entity<ProjectEquipment>(e =>
        {
            e.Property(x => x.Tag).HasMaxLength(64);
            e.Property(x => x.Notes).HasMaxLength(1000);
            e.HasOne<LibraryItem>().WithMany().HasForeignKey(x => x.LibraryItemId).OnDelete(DeleteBehavior.SetNull);
            e.HasOne<Room>().WithMany().HasForeignKey(x => x.RoomId).OnDelete(DeleteBehavior.SetNull);
        });

        b.Entity<AuditEntry>(e =>
        {
            e.HasIndex(x => x.At);
            e.HasIndex(x => x.ProjectId);
            e.Property(x => x.UserLogin).HasMaxLength(64);
            e.Property(x => x.Source).HasMaxLength(32);
            e.Property(x => x.Action).HasMaxLength(64);
            e.Property(x => x.Target).HasMaxLength(256);
        });

        b.Entity<AlarmEvent>(e =>
        {
            e.HasIndex(x => x.StartedAt);
            e.Property(x => x.Code).HasMaxLength(64);
            e.Property(x => x.Element).HasMaxLength(64);
        });

        b.Entity<SystemSettings>(e =>
        {
            e.Property(x => x.StartupLanguage).HasMaxLength(8);
            e.Property(x => x.PlcMode).HasMaxLength(16);
        });
    }
}
