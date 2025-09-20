SphereServer 0.51x Quick Start
==============================

This document summarises the options that need to be reviewed before the
GraySvr executable can be launched. It focuses on the modern MySQL backed
persistence layer that replaces the legacy `*.scp` save files.

1. Configure `spheredef.ini`
----------------------------

Open `GraySvr/spheredef.ini` and update the following keys in the `[SPHERE]`
section:

- `SERVIP`, `SERVPORT`, `SERVNAME`, `TIMEZONE`, `ADMINEMAIL` – the standard
  networking and reporting settings.
- `MULFILES`, `SCPFILES`, `SPEECHFILES`, `WORLDSAVE`, `LOG` – absolute paths to
  the client data, script packs and optional backup folders.
- `MYSQL` and its companion parameters – see the next section for details.

> **Tip:** `WORLDSAVE` is still used for compatibility reasons (account script
> imports, backups). World state persistence no longer depends on
> `sphereworld.scp` or `sphereaccu.scp` when MySQL is enabled.

2. MySQL backend settings
-------------------------

The server can now persist accounts, world objects, GM pages, timers and server
listings to MySQL. Minimum requirements:

- MySQL Server **5.7** (or MariaDB **10.3**) with InnoDB and `utf8mb4`.
- MySQL Connector/C (libmysqlclient) **5.7**+ or an equivalent MariaDB client
  library available at build time.

Configuration keys (`[SPHERE]` section of `spheredef.ini`):

| Key          | Description |
| ------------ | ----------- |
| `MYSQL`      | Enables the database backend when set to `1`. A value of `0` keeps the classic flat-file behaviour. |
| `MYSQLHOST`  | Host name or IP address. Defaults to `localhost`. |
| `MYSQLPORT`  | Port number, default `3306`. |
| `MYSQLUSER`  | Database user that has privileges to create tables and run DDL/DML. |
| `MYSQLPASS`  | Password for the user above. |
| `MYSQLDB`    | Database/schema name where tables are created. |
| `MYSQLPREFIX`| Optional string prepended to every table (e.g. `sphere_`). Leave blank to skip the prefix. |

During compilation make sure the MySQL headers and import libraries are
available. The Visual Studio project honours the `MYSQL_INCLUDE_DIR` and
`MYSQL_LIB_DIR` environment variables but you may also edit the project settings
manually.

3. First boot and migration workflow
------------------------------------

1. Start the database server and confirm that the configured user can create
   tables inside the chosen schema.
2. Launch GraySvr. On the first run with `MYSQL=1` the server runs the
   migrations in `CWorldStorageMySQL.cpp`. The resulting schema is documented in
   `docs/database-schema.md` and the example `docs/mysql-schema.sql` script.
3. Account data is imported automatically the first time the server connects to
   MySQL **while** the legacy `sphereaccu.scp` / `sphereacct.scp` files are still
   available in `WORLDSAVE`. Existing rows are preserved.
4. Trigger a save (`SAVE 0`) or wait for the periodic saver. This uploads the
   complete world state to the database, replacing the `sphereworld.scp` flow.
   Interrupted saves resume on the next run.
5. Verify the migration status with:

   ```sql
   SELECT id, version FROM `<prefix>schema_version` ORDER BY id;
   ```

   - `id = 1` reflects the schema revision (should be **3**).
   - `id = 2` flips to **1** after the legacy account import finishes.
   - `id = 3` stores the world save counter.
   - `id = 4` indicates whether the last world save completed successfully.

4. Additional notes
-------------------

- The console will log failures if the server cannot reach MySQL; in that case
  it falls back to the flat-file loader and leaves the database untouched.
- Use `/ACCOUNT UPDATE` after the first import to push recent account changes to
  MySQL.
- GM pages, server listings and timers are also mirrored in MySQL. The
  `Ensure*Columns` routines in `CWorldStorageMySQL.cpp` will extend existing
  tables automatically when newer builds add fields.

Good luck and welcome back to 0.51x shard development!
