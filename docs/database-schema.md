# MySQL Storage Architecture and Schema

SphereServer 0.51x ships with an integrated MySQL storage layer that replaces
most legacy `*.scp` persistence. The implementation is split into focused
modules that keep connections, schema evolution and data access logic separate.
This document explains that layout and details every table that the bundled
migrations create.

## Module overview

### Storage façade (`GraySvr/MySqlStorageService.*`)

`MySqlStorageService` is the high-level API exposed to the rest of the server.
It owns the components described below and offers helpers used throughout the
code base (for example `UniversalRecord` for ad-hoc inserts/updates and
`DirtyQueueProcessor` integration for deferred world saves). The façade is the
single point that other systems call when they need to issue SQL, queue world
objects for persistence or read configuration values.

### Connection manager (`GraySvr/Storage/MySql/ConnectionManager.*`)

The connection manager translates `CServerMySQLConfig` into a ready-to-use
connection pool:

- Validates and normalises charset/collation pairs and computes reasonable
  defaults (`utf8mb4` when no charset is provided).
- Manages retries and delays when auto reconnect is enabled. The behaviour is
  driven by the `MYSQL*` keys inside `spheredef.ini` (host, port, database,
  credentials, prefix and charset) plus the reconnect flags stored on
  `CServerMySQLConfig` (`m_fAutoReconnect`, `m_iReconnectTries`,
  `m_iReconnectDelay`).
- Provides scoped access to pooled connections (`MySqlConnectionPool`) and keeps
  track of explicit transactions so that complex save operations can reuse the
  same handle.
- Exposes the effective table charset/collation so that DDL generators can pick
  the right suffix when creating new tables.

### Repository layer (`Storage::Repository`)

To keep SQL text close to the data it manipulates the service builds a small
repository hierarchy inside `MySqlStorageService.cpp`:

- `PreparedStatementRepository` provides a thin wrapper around prepared
  statements. It centralises error handling, parameter binding and connection
  acquisition from the pool.
- `WorldObjectMetaRepository`, `WorldObjectDataRepository`,
  `WorldObjectComponentRepository` and `WorldObjectRelationRepository` map the
  world persistence tables to C++ records. They are responsible for generating
  `INSERT ... ON DUPLICATE KEY UPDATE` statements, decomposing dynamic TAG/VAR
  data and removing orphaned rows.
- Additional repositories follow the same pattern for GM pages, timers, sector
  metadata and other subsystems as they are migrated to SQL.

Developers adding new tables should follow this structure: create a repository
class that inherits from `PreparedStatementRepository`, define the SQL text once
in the constructor and expose typed methods that accept simple record structs.

### Schema manager (`GraySvr/Storage/Schema/SchemaManager.*`)

Schema maintenance is handled by `Storage::Schema::SchemaManager`. The manager
executes the migration chain during start-up and keeps helper routines for
schema bookkeeping:

- Ensures that `<prefix>schema_version` exists and seeds the rows used for
  metadata tracking.
- Applies incremental migrations (`ApplyMigration_0_1`, `_1_2`, `_2_3`, `_3_4`, …) until
  the current revision (`CURRENT_SCHEMA_VERSION`) is reached.
- Extends existing tables with the helper routines `Ensure*Columns`. The logic
  checks for column existence before issuing `ALTER TABLE` statements, which
  makes repeated runs idempotent.
- Provides convenience methods for reading and updating legacy import state,
  world save counters and the "last save completed" flag that the save system
  consumes.

## Making schema changes

1. Update `CURRENT_SCHEMA_VERSION` inside
   `Storage/Schema/SchemaManager.cpp` and add a matching branch in
   `SchemaManager::ApplyMigration`.
2. Implement a new `ApplyMigration_X_Y` method that uses the façade helpers to
   build `CREATE TABLE` / `ALTER TABLE` statements. Prefer
   `storage.GetPrefixedTableName()` and `storage.GetDefaultTableCollationSuffix()`
   so that prefix and charset rules remain consistent.
3. If the migration needs to add columns to existing tables, call
   `EnsureColumnExists` (or follow the pattern in the `Ensure*Columns` helpers)
   to avoid double-applying the change.
4. Update `docs/database-schema.md` (this file) and
   `docs/mysql-schema.sql` so documentation and example dumps match the runtime
   migrations.
5. Bump any tests or deployment scripts that assert the schema version.

Remember that migrations run inside the storage façade with an open connection
managed by the connection manager, so they can freely execute DDL and DML using
`storage.ExecuteQuery()` / `storage.Query()`.

## Schema reference

The current schema version is **5**. Table names below omit the optional prefix
configured through `MYSQLPREFIX`.

### `schema_version`

Tracks migration and operational metadata. Seeded by the schema manager.

| `id` | Purpose | Typical values |
| ---- | ------- | -------------- |
| 1 | Schema revision (`CURRENT_SCHEMA_VERSION`). | `5` |
| 2 | Legacy account import flag (`0` pending, `1` complete). | `0` or `1` |
| 3 | World save counter (incremented for every completed save). | `0+` |
| 4 | World save completion flag (`0` = interrupted, `1` = success). | `0` or `1` |

### Account management

`accounts`
: Primary record for Sphere accounts. Stores credentials, privilege levels,
  chat/e-mail metadata and timestamps. Indexed on the auto-increment primary key
  and `name` (unique).

`account_emails`
: Queue of scheduled account messages (activation, warnings). Uses a composite
  primary key `(account_id, sequence)` with a cascading foreign key back to
  `accounts`.

### Legacy import staging

`characters`
: Minimal representation of imported characters. Holds positional data and links
  back to `accounts`. Used only while migrating flat-file saves.

`items`
: Legacy container for serialized items during the import. Maintains foreign keys
  to both `items.uid` (self-referencing containers) and `characters.uid` for
  ownership tracking.

`item_props`
: Name/value pairs for item properties captured during import. Uses a composite
  primary key `(item_uid, prop)` and cascades deletes from `items`.

### Operational tables

`sectors`
: World sector overrides (light, rain, cold) maintained for optional gameplay
  features. Columns stay unsigned for counters and use nullable fields for
  optional overrides.

`gm_pages`
: Player-submitted GM help requests. Includes account name, reason, timestamps
  and location. Extended as needed via the schema manager.

`servers`
: Shard listings published to the legacy server list. Stores networking
  metadata, contact details, localisation information and ageing statistics.

`timers`
: Auxiliary timers that back scheduled operations from the classic scripts. Each
  row references a persisted world object via `character_uid` or `item_uid`,
  both of which point to `<prefix>world_objects`.`uid`. The `expires_at` column
  stores the remaining world ticks (1 tick = 100 ms) until the timer fires and
  `type` distinguishes character (`1`) from item (`2`) timers. Script engines
  may persist additional payloads in `data` for future extensions.

### World persistence (`schema` version ≥ 3, current version 5)

`world_objects`
: Metadata for every persisted object (characters and items). Stores the base
  object type/subtype, optional display name, ownership references and cached
  location/container data. Indexed by owner and container identifiers to speed
  delta synchronisation.

`world_object_data`
: Serialized object payload (script state) keyed by `object_uid`. Includes a
  checksum to detect divergence.

`world_object_components`
: Normalised representation of dynamic script data (TAG/VAR style properties).
  Composite primary key `(object_uid, component, name, sequence)` preserves
  ordering for repeated tags.

`world_object_relations`
: Parent/child relationships such as equipment slots or nested containers.
  Composite primary key `(parent_uid, child_uid, relation, sequence)` plus an
  index on `child_uid` to speed reverse lookups.

`world_savepoints`
: Bookkeeping table for manual save checkpoints. Records a label, timestamp,
  object count and checksum.

`world_object_audit`
: Optional history table that captures before/after snapshots of object
  mutations together with the change type and timestamp.

## Additional resources

- `docs/mysql-schema.sql` – example dump that mirrors the migrations above.
- `docs/storage-migration.md` – upgrade checklist covering configuration and
  deployment changes.
- `GraySvr/Storage/Schema/SchemaManager.*` – canonical source for migrations.
- `GraySvr/MySqlStorageService.cpp` – reference implementation of repository
  classes and persistence workflows.
