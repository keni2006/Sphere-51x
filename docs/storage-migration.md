# MySQL storage migration guide

This guide walks administrators through upgrading an existing SphereServer 0.51x
installation to the refactored MySQL storage stack. The new code path introduces
an explicit storage façade, a connection manager with pooling/retry logic and a
schema manager that centralises migrations.

## What's new?

- **Storage façade (`MySqlStorageService`)** – orchestrates account imports,
  world saves and repository calls. Other subsystems interact with MySQL solely
  through this façade.
- **Connection manager** – normalises character sets, honours the
  auto-reconnect policy found in `CServerMySQLConfig` and keeps a pool of active
  connections so that saves can reuse handles safely.
- **Repository layer** – consolidates SQL used for accounts, world objects,
  timers and GM pages. Each repository owns its prepared statements, reducing
  duplication and improving error reporting.
- **Schema manager** – applies migrations up to schema version **5**, extends
  legacy tables when new columns are required and records world-save status in
  dedicated rows of `<prefix>schema_version`.

If you previously referenced `CWorldStorageMySQL.cpp`, all of that logic now
lives across `GraySvr/MySqlStorageService.cpp`,
`GraySvr/Storage/MySql/ConnectionManager.*` and
`GraySvr/Storage/Schema/SchemaManager.*`.

## Configuration checklist

1. Review the `[SPHERE]` section in `spheredef.ini`:
   - `MYSQL`, `MYSQLHOST`, `MYSQLPORT`, `MYSQLUSER`, `MYSQLPASS`, `MYSQLDB` and
     `MYSQLPREFIX` continue to govern how the server connects to the database.
   - `MYSQLCHARSET` now feeds directly into the connection manager. You can
     optionally append a collation (e.g. `utf8mb4 utf8mb4_unicode_ci`) and the
     manager will derive the charset/charset-collation pair automatically.
2. Advanced connection behaviour is controlled through the fields on
   `CServerMySQLConfig`:
   - `m_fAutoReconnect` (defaults to `true`)
   - `m_iReconnectTries` (defaults to `3` attempts)
   - `m_iReconnectDelay` (defaults to `5` seconds)
   The stock configuration keeps these defaults; adjust them in code or via your
   deployment tooling if you need stricter policies.
3. Build systems can still override include/library paths using the environment
   variables `MYSQL_INCLUDE_DIR` and `MYSQL_LIB_DIR`. These are picked up by the
   Visual Studio project as well as the makefiles.

## Pre-upgrade steps

1. **Back up your database and legacy save files.** Dump the MySQL schema and
   keep copies of `sphereworld.scp`, `sphereaccu.scp` and any other custom save
   scripts.
2. **Upgrade the binaries.** Rebuild or download the release that contains the
   new storage façade.
3. **Verify configuration.** Double-check the keys listed above and ensure the
   database user still has permission to create tables and run migrations.
4. **Plan downtime.** The first boot after the upgrade runs the schema manager,
   which may perform `ALTER TABLE` statements. Schedule a maintenance window if
   you run a production shard.

## Migration workflow

1. Start your MySQL server and confirm connectivity using the credentials in
   `spheredef.ini`.
2. Launch the upgraded SphereServer binary. During initialisation the connection
   manager establishes the pool and `SchemaManager::EnsureSchema` migrates the
   database to the latest revision.
3. If legacy account scripts are still present, the façade will import them once
   and mark `<prefix>schema_version`.`id = 2` as complete.
4. Trigger a world save (`SAVE 0`) or wait for the background saver to run. The
   repository layer writes `world_objects`, `world_object_data` and related tables
   using prepared statements.
   - Existing installations that previously used the `type` column on
     `<prefix>world_object_relations` are automatically migrated. The schema
     manager renames the legacy column to `relation` so existing relationship
     data continues to load before the first save touches the table.
   - Older `world_savepoints` tables gain a `label` column (renamed from
     `save_reason`) and the new `objects_count` field so snapshot metadata can be
     recorded without errors.
5. Inspect the migration status:

   ```sql
   SELECT id, version FROM `<prefix>schema_version` ORDER BY id;
   ```

   - `id = 1` should equal `4`.
   - `id = 2` becomes `1` after the legacy import finishes.
   - `id = 3` increments with every save.
   - `id = 4` reports whether the most recent save completed successfully.

6. Review the server logs. Any connection failures are reported before the
   server falls back to the legacy flat-file loader.

## Post-upgrade tips

- Keep `docs/mysql-schema.sql` and `docs/database-schema.md` handy; they mirror
  the runtime schema and are updated alongside migrations.
- Use the new `docs/storage-migration.md` checklist whenever you roll out a new
  build so you remember to validate configuration and schema status.
- When extending the schema, implement a new migration in
  `SchemaManager::ApplyMigration` and document the change before deploying.

Upgrading to the refactored storage layer results in cleaner separation of
concerns, easier schema evolution and more predictable world saves. Take your
time to validate the configuration once and the new tooling will handle the rest.
