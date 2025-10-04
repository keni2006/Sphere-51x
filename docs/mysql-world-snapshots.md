# MySQL World Snapshot Exports

> 🇷🇺 Для русской версии см. [`docs/ru/mysql-world-snapshots.md`](ru/mysql-world-snapshots.md).

## Overview
- SphereServer 0.51x can export a full MySQL world snapshot every time the world saver finishes committing data to the live tables.
- `CWorld::FinalizeStorageSave()` queues the snapshot work through `MySqlStorageService::ScheduleWorldSnapshot()`, allowing the main save to complete while the copy runs on the background worker whenever available.
- If the queue cannot accept the job the server logs a warning and falls back to running the snapshot inline; live tables stay writable because only `SELECT` queries are issued against them.

## When snapshots run
- Snapshots are triggered immediately after the main world-save transaction commits and the save counter increments. The default label is `"World save #<n>"`, matching the new save count.
- Timestamps are rounded down to the configured save period (`SAVEPERIOD`). This ensures recurring saves reuse consistent directory names even if the saver starts a few seconds early or late.
- Administrators can still start manual saves; the helper uses the same schedule and label logic so ad-hoc saves also get a snapshot directory.

## Output location
- Set the `WORLDSAVE` key in `spheredef.ini` to the directory where SphereServer should keep flat-file saves and exported snapshots. The server creates a `mysqlsnapshots/` subdirectory under this path when MySQL snapshots run for the first time.
- Each snapshot is written to `WORLDSAVE/mysqlsnapshots/<timestamp_or_label>/`. The folder name contains the aligned timestamp (`YYYYMMDD_HHMMSS`) followed by an optional sanitized label suffix (for example, `20240314_210000_World_save_12`).
- Dumps stream directly into that directory; there is no temporary working folder or helper script to clean up.
- The Sphere process must be able to create directories and write files under the configured `WORLDSAVE` path. Permission issues are reported in the server log and abort the snapshot.

## Files that are generated
Every snapshot folder contains one SQL dump per world table plus a metadata file:

| File | Contents |
| ---- | -------- |
| `world_objects.sql` | `DROP TABLE`, `CREATE TABLE` (structure) and `INSERT` statements for `<prefix>world_objects`. |
| `world_object_data.sql` | Full dump of `<prefix>world_object_data`. |
| `world_object_components.sql` | Full dump of `<prefix>world_object_components`. |
| `world_object_relations.sql` | Full dump of `<prefix>world_object_relations`. |
| `world_object_audit.sql` | Full dump of `<prefix>world_object_audit`. |
| `gm_pages.sql` | Full dump of `<prefix>gm_pages`. |
| `servers.sql` | Full dump of `<prefix>servers`. |
| `sectors.sql` | Full dump of `<prefix>sectors`. |
| `metadata.txt` | Plain-text key/value pairs describing the label, creation time, object count, save period in seconds and the directory path that was written. |

All SQL files include the table definition retrieved via `SHOW CREATE TABLE` so they can be replayed on a clean schema without additional migrations.

## Database bookkeeping
- The snapshot transaction also inserts a row into `<prefix>world_savepoints` using the generated label, timestamp and object count. This mirrors the previous behaviour that recorded save metadata directly inside MySQL.
- The metadata row is written even though the data lives on disk, letting operators track when the latest snapshot finished and how many objects it contained.

## Failure handling
- Any failure during file creation, data streaming or metadata recording aborts the snapshot transaction and writes an error to the save log channel.
- The world save itself stays committed; the warning clarifies that only the snapshot step failed so administrators can investigate while players keep using the shard.
